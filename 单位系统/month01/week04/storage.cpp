module course.storage;
import std;
namespace units {
namespace {
constexpr std::size_t max_records = 1024;
struct Failure {
    Error value;
};
[[noreturn]] void reject(std::string code, std::string message) {
    auto e = error(std::move(code), std::move(message));
    e.stage = "storage";
    throw Failure{e};
}
template <class T> T take(Result<T> result) {
    if (!result) {
        auto e = result.error();
        e.stage = "storage";
        throw Failure{e};
    }
    return std::move(*result);
}
void take(Result<void> result) {
    if (!result) {
        auto e = result.error();
        e.stage = "storage";
        throw Failure{e};
    }
}
std::string read_bytes(std::istream &input) {
    std::string bytes;
    std::array<char, 4096> buffer;
    while (input) {
        input.read(buffer.data(), buffer.size());
        bytes.append(buffer.data(), static_cast<std::size_t>(input.gcount()));
        if (bytes.size() > 1024 * 1024)
            reject("size", "document exceeds 1 MiB teaching limit");
    }
    if (input.bad() || (!input.eof() && input.fail()))
        reject("io", "failed to read document");
    return bytes;
}
class Reader {
    std::istringstream in_;

  public:
    explicit Reader(std::string bytes) : in_(std::move(bytes)) {
        in_.imbue(std::locale::classic());
    }
    std::string word() {
        std::string text;
        if (!(in_ >> text))
            reject("syntax", "unexpected end of document");
        return text;
    }
    void token(std::string_view expected) {
        if (word() != expected)
            reject("syntax", "expected " + std::string{expected});
    }
    std::string quoted() {
        in_ >> std::ws;
        if (in_.peek() != '"')
            reject("syntax", "expected quoted string");
        std::string text;
        if (!(in_ >> std::quoted(text)))
            reject("syntax", "invalid quoted string");
        return text;
    }
    template <class T> T number() {
        auto text = word();
        T value{};
        auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
            reject("number", "invalid numeric field");
        if constexpr (std::is_floating_point_v<T>)
            if (!std::isfinite(value))
                reject("number", "nonfinite numeric field");
        return value;
    }
    std::size_t count() {
        auto n = number<std::size_t>();
        if (n > max_records)
            reject("size", "record count exceeds 1024");
        return n;
    }
    bool boolean() {
        auto n = number<unsigned>();
        if (n > 1)
            reject("syntax", "boolean must be 0 or 1");
        return n == 1;
    }
    void finish() {
        token("END");
        in_ >> std::ws;
        if (!in_.eof())
            reject("syntax", "unexpected trailing data");
    }
};
void write_unit(std::ostream &out, Unit unit) {
    for (int e : unit.exponents())
        out << e << ' ';
}
Unit read_unit(Reader &r) {
    std::array<int, 8> e;
    for (int &v : e)
        v = r.number<int>();
    return take(Unit::make(e));
}
void write_bounds(std::ostream &out, const QuantitySpec &spec) {
    out << spec.lower.has_value() << ' ';
    if (spec.lower)
        out << *spec.lower << ' ';
    out << spec.upper.has_value() << ' ';
    if (spec.upper)
        out << *spec.upper << ' ';
}
QuantitySpec read_bounds(Reader &r, Unit unit) {
    QuantitySpec s{unit, {}, {}};
    if (r.boolean())
        s.lower = r.number<double>();
    if (r.boolean())
        s.upper = r.number<double>();
    return s;
}
Quantity placeholder(const QuantitySpec &spec) {
    if (spec.lower && spec.upper && *spec.lower > *spec.upper)
        reject("constraint", "invalid bound order");
    double v = std::clamp(1., spec.lower.value_or(std::numeric_limits<double>::lowest()),
                          spec.upper.value_or(std::numeric_limits<double>::max()));
    auto q = take(Quantity::make(v, spec.unit));
    take(validate(q, spec));
    return q;
}
void write_ref(std::ostream &out, const UnitRef &ref) {
    out << std::quoted(ref.id) << ' ' << ref.version;
}
UnitRef read_ref(Reader &r) {
    auto id = r.quoted();
    auto version = r.number<unsigned>();
    return {std::move(id), version};
}
void write_key(std::ostream &out, const PropertyKey &key) {
    out << key.object << ' ' << std::quoted(key.name);
}
PropertyKey read_key(Reader &r) {
    auto id = r.number<std::uint64_t>();
    auto name = r.quoted();
    return {id, std::move(name)};
}
void write_metadata(std::ostream &out, const MetadataValue &value) {
    std::visit(
        [&](const auto &v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::string>)
                out << "string " << std::quoted(v);
            else if constexpr (std::is_same_v<T, bool>)
                out << "bool " << v;
            else
                out << "integer " << v;
        },
        value);
}
MetadataValue read_metadata(Reader &r, std::string tag) {
    if (tag == "string")
        return r.quoted();
    if (tag == "bool")
        return r.boolean();
    if (tag == "integer")
        return r.number<std::int64_t>();
    reject("type", "unsupported metadata type");
}
struct StoredFormula {
    PropertyKey key;
    std::string source;
    VariableMap aliases;
    std::vector<UnitBinding> units;
};
Model read_model(Reader &r) {
    r.token("INTERNAL_UNITS");
    if (r.quoted() != "mm kg s A K mol cd deg")
        reject("units", "unexpected internal units");
    r.token("HIGH_WATER");
    auto high = r.number<std::uint64_t>();
    Model model{false};
    r.token("UNITS");
    auto count = r.count();
    for (std::size_t i = 0; i < count; ++i) {
        r.token("UNIT");
        auto id = r.quoted();
        auto version = r.number<unsigned>();
        auto symbol = r.quoted();
        auto t = read_unit(r);
        auto scale = r.number<double>();
        auto source = r.quoted();
        take(model.register_unit({id, version, symbol, t, scale, source, false}));
    }
    UnitConfig config;
    r.token("CONFIG");
    config.schema = r.quoted();
    config.input = read_ref(r);
    config.display = read_ref(r);
    config.precision = r.number<int>();
    config.significant = r.boolean();
    config.group_digits = r.boolean();
    config.input_follows_display = r.boolean();
    for (auto [label, map] :
         std::array<std::pair<const char *, std::map<std::string, UnitRef> *>, 2>{
             {{"INPUT_OVERRIDES", &config.input_overrides},
              {"DISPLAY_OVERRIDES", &config.display_overrides}}}) {
        r.token(label);
        auto n = r.count();
        map->clear();
        for (std::size_t i = 0; i < n; ++i) {
            r.token("OVERRIDE");
            auto name = r.quoted();
            auto ref = read_ref(r);
            if (!map->emplace(name, ref).second)
                reject("duplicate", "duplicate unit override");
        }
    }
    r.token("PROPERTIES");
    count = r.count();
    std::vector<StoredFormula> formulas;
    for (std::size_t i = 0; i < count; ++i) {
        r.token("PROPERTY");
        auto key = read_key(r);
        auto type = read_unit(r);
        auto spec = read_bounds(r, type);
        auto mode = r.word();
        if (mode == "INPUT") {
            auto value = take(Quantity::make(r.number<double>(), type));
            take(model.add_property(key, value, spec));
        } else if (mode == "FORMULA") {
            take(model.add_property(key, placeholder(spec), spec));
            StoredFormula f{key, r.quoted(), {}, {}};
            r.token("ALIASES");
            auto n = r.count();
            for (std::size_t j = 0; j < n; ++j) {
                r.token("ALIAS");
                auto name = r.quoted();
                auto ref = read_key(r);
                if (!f.aliases.emplace(name, ref).second)
                    reject("duplicate", "duplicate alias");
            }
            r.token("UNIT_BINDINGS");
            n = r.count();
            for (std::size_t j = 0; j < n; ++j) {
                r.token("BIND");
                auto position = r.number<std::size_t>();
                auto length = r.number<std::size_t>();
                auto unit = read_ref(r);
                f.units.push_back({position, length, unit});
            }
            formulas.push_back(std::move(f));
        } else
            reject("syntax", "expected INPUT or FORMULA");
    }
    r.token("METADATA");
    count = r.count();
    std::set<PropertyKey> metadata;
    for (std::size_t i = 0; i < count; ++i) {
        r.token("ITEM");
        auto key = read_key(r);
        if (!metadata.insert(key).second)
            reject("duplicate", "duplicate metadata");
        auto value = read_metadata(r, r.word());
        take(model.set_metadata(key, std::move(value)));
    }
    r.finish();
    take(model.set_high_water(high));
    take(model.configure(config));
    for (const auto &f : formulas)
        take(model.bind(f.key, f.source, f.aliases, f.units, true));
    take(model.check_graph());
    model.recompute(true);
    return model;
}
} // namespace
Result<std::string> serialize(const Model &m) {
    try {
        take(m.check_graph());
        const auto &c = m.config();
        if (m.properties().size() > max_records || m.metadata().size() > max_records ||
            m.registry().custom_definitions().size() > max_records ||
            c.input_overrides.size() > max_records || c.display_overrides.size() > max_records)
            reject("size", "record count exceeds 1024");
        for (const auto &[key, p] : m.properties())
            if (p.formula &&
                (p.formula->aliases.size() > max_records || p.formula->units.size() > max_records))
                reject("size", "formula record count exceeds 1024");
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << std::setprecision(std::numeric_limits<double>::max_digits10);
        out << "CAD_DOCUMENT\nINTERNAL_UNITS \"mm kg s A K mol cd deg\"\nHIGH_WATER "
            << m.high_water() << '\n';
        auto defs = m.registry().custom_definitions();
        out << "UNITS " << defs.size() << '\n';
        for (const auto &d : defs) {
            out << "UNIT " << std::quoted(d.id) << ' ' << d.version << ' ' << std::quoted(d.symbol)
                << ' ';
            write_unit(out, d.unit);
            out << ' ' << d.scale << ' ' << std::quoted(d.source) << '\n';
        }
        out << "CONFIG ";
        out << std::quoted(c.schema) << ' ';
        write_ref(out, c.input);
        out << ' ';
        write_ref(out, c.display);
        out << ' ' << c.precision << ' ' << c.significant << ' ' << c.group_digits << ' '
            << c.input_follows_display << '\n';
        for (auto [label, map] :
             std::array<std::pair<const char *, const std::map<std::string, UnitRef> *>, 2>{
                 {{"INPUT_OVERRIDES", &c.input_overrides},
                  {"DISPLAY_OVERRIDES", &c.display_overrides}}}) {
            out << label << ' ' << map->size() << '\n';
            for (const auto &[name, ref] : *map) {
                out << "OVERRIDE " << std::quoted(name) << ' ';
                write_ref(out, ref);
                out << '\n';
            }
        }
        out << "PROPERTIES " << m.properties().size() << '\n';
        for (const auto &[key, p] : m.properties()) {
            out << "PROPERTY ";
            write_key(out, key);
            out << ' ';
            write_unit(out, p.spec.unit);
            write_bounds(out, p.spec);
            if (!p.formula)
                out << " INPUT " << p.value.value() << '\n';
            else {
                const auto &f = *p.formula;
                out << " FORMULA " << std::quoted(f.source) << "\nALIASES " << f.aliases.size()
                    << '\n';
                for (const auto &[name, ref] : f.aliases) {
                    out << "ALIAS " << std::quoted(name) << ' ';
                    write_key(out, ref);
                    out << '\n';
                }
                out << "UNIT_BINDINGS " << f.units.size() << '\n';
                for (const auto &b : f.units) {
                    out << "BIND " << b.position << ' ' << b.length << ' ';
                    write_ref(out, b.unit);
                    out << '\n';
                }
            }
        }
        out << "METADATA " << m.metadata().size() << '\n';
        for (const auto &[key, value] : m.metadata()) {
            out << "ITEM ";
            write_key(out, key);
            out << ' ';
            write_metadata(out, value);
            out << '\n';
        }
        out << "END\n";
        if (!out)
            reject("io", "serialization failed");
        auto bytes = out.str();
        if (bytes.size() > 1024 * 1024)
            reject("size", "document exceeds 1 MiB teaching limit");
        return bytes;
    } catch (const Failure &f) {
        return std::unexpected(f.value);
    } catch (const std::exception &e) {
        return std::unexpected(error("storage", e.what()));
    }
}
Result<Model> deserialize(std::istream &input) {
    try {
        Reader r{read_bytes(input)};
        r.token("CAD_DOCUMENT");
        return read_model(r);
    } catch (const Failure &f) {
        return std::unexpected(f.value);
    } catch (const std::exception &e) {
        return std::unexpected(error("storage", e.what()));
    }
}
Result<void> save_document(Document &d, std::ostream &out) {
    auto bytes = serialize(d.model());
    if (!bytes)
        return std::unexpected(bytes.error());
    try {
        out.write(bytes->data(), static_cast<std::streamsize>(bytes->size()));
        out.flush();
        if (!out)
            return std::unexpected(error("io", "write failed"));
        d.mark_saved();
        return {};
    } catch (const std::exception &e) {
        return std::unexpected(error("io", e.what()));
    }
}
Result<void> load_document(Document &d, std::istream &input) {
    auto candidate = deserialize(input);
    if (!candidate)
        return std::unexpected(candidate.error());
    return d.replace(std::move(*candidate));
}
Result<void> save_file(Document &d, const std::filesystem::path &path) {
    auto bytes = serialize(d.model());
    if (!bytes)
        return std::unexpected(bytes.error());
    auto temp = path;
    temp += ".tmp-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    std::ofstream out(temp, std::ios::binary | std::ios::out | std::ios::noreplace);
    if (!out)
        return std::unexpected(error("io", "cannot create temporary output"));
    out.write(bytes->data(), static_cast<std::streamsize>(bytes->size()));
    out.flush();
    bool ok = out.good();
    out.close();
    ok = ok && !out.fail();
    std::error_code ec;
    if (ok)
        std::filesystem::rename(temp, path, ec);
    if (!ok || ec) {
        std::error_code ignored;
        std::filesystem::remove(temp, ignored);
        return std::unexpected(error("io", "failed to commit document file"));
    }
    d.mark_saved();
    return {};
}
Result<void> load_file(Document &d, const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return std::unexpected(error("io", "cannot open document"));
    return load_document(d, in);
}
Result<RectangleExchange> export_rectangle(const Model &m, std::string unit,
                                           const Quantity &tolerance) {
    auto u = parse_unit(unit, m.registry());
    if (!u)
        return std::unexpected(u.error());
    if (u->definition.unit != Unit::length() || tolerance.unit() != Unit::length() ||
        tolerance.value() <= 0)
        return std::unexpected(
            error("exchange", "exchange requires Length and positive tolerance"));
    auto l = m.value({1, "length"}), w = m.value({1, "width"});
    if (!l)
        return std::unexpected(l.error());
    if (!w)
        return std::unexpected(w.error());
    auto length = to_unit(*l, u->definition), width = to_unit(*w, u->definition),
         tol = to_unit(tolerance, u->definition);
    if (!length || !width || !tol)
        return std::unexpected(error("exchange", "conversion failed"));
    return RectangleExchange{std::move(unit), *length, *width, *tol};
}
Result<ImportedRectangle> import_rectangle(const RectangleExchange &data, const UnitRegistry &r) {
    if (data.unit.empty())
        return std::unexpected(error("exchange", "explicit input unit is required"));
    auto u = parse_unit(data.unit, r);
    if (!u)
        return std::unexpected(u.error());
    if (u->definition.unit != Unit::length())
        return std::unexpected(error("exchange", "length unit required"));
    auto l = from_unit(data.length, u->definition), w = from_unit(data.width, u->definition),
         t = from_unit(data.tolerance, u->definition);
    if (!l || !w || !t || l->value() <= 0 || w->value() <= 0 || t->value() <= 0)
        return std::unexpected(error("exchange", "invalid exchange quantities"));
    double norm = 0;
    for (double v : data.normal) {
        if (!std::isfinite(v))
            return std::unexpected(error("exchange", "invalid normal"));
        norm += v * v;
    }
    if (std::abs(norm - 1) > 1e-12)
        return std::unexpected(error("exchange", "normal must be normalized"));
    return ImportedRectangle{*l, *w, *t, data.normal};
}
} // namespace units
