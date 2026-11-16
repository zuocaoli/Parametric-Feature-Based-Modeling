module course.schema;
import std;
namespace units {
namespace {
Result<std::string> number_text(double value, QuantityFormat format) {
    if (!std::isfinite(value) || format.precision < 0 || format.precision > 17 ||
        (format.significant && format.precision == 0))
        return std::unexpected(error("format", "invalid number or precision"));
    std::ostringstream out;
    out.imbue(std::locale::classic());
    if (!format.significant)
        out << std::fixed;
    out << std::setprecision(format.precision) << value;
    auto text = out.str();
    if (format.group_digits && text.find_first_of("eE") == std::string::npos) {
        auto dot = text.find('.');
        std::size_t end = dot == std::string::npos ? text.size() : dot;
        std::size_t start = text.starts_with('-') ? 1 : 0;
        for (std::size_t i = end; i > start + 3;) {
            i -= 3;
            text.insert(i, ",");
        }
    }
    return text;
}
Result<Translation> render(double internal, double factor, std::string symbol,
                           QuantityFormat format) {
    auto text = number_text(internal / factor, format);
    if (!text)
        return std::unexpected(text.error());
    return Translation{internal / factor, factor, symbol,
                       *text + (symbol == "1" ? "" : " " + symbol)};
}
Result<void> valid_schema(const UnitsSchemaSpec &s) {
    if (s.name.empty() || s.basic_length.id.empty() || s.basic_length.version == 0)
        return std::unexpected(error("schema", "missing schema identity or basic length"));
    for (const auto &[unit, rows] : s.translations) {
        (void)unit;
        if (rows.empty())
            return std::unexpected(error("schema", "empty translation rules"));
        double previous = 0;
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const auto &r = rows[i];
            if (!std::isfinite(r.factor) || r.factor <= 0 || !std::isfinite(r.threshold) ||
                r.threshold < 0 || r.unit_string.empty())
                return std::unexpected(error("schema", "invalid translation rule"));
            if (r.threshold == 0) {
                if (i + 1 != rows.size())
                    return std::unexpected(error("schema", "default rule must be last"));
            } else {
                if (r.threshold <= previous)
                    return std::unexpected(error("schema", "thresholds must increase"));
                previous = r.threshold;
            }
        }
        if (rows.back().threshold != 0)
            return std::unexpected(error("schema", "missing default rule"));
    }
    return {};
}
} // namespace
UnitsSchema::UnitsSchema(UnitsSchemaSpec s) : spec_(std::move(s)) {
    auto r = valid_schema(spec_);
    if (!r)
        throw std::invalid_argument(r.error().message);
}
const UnitsSchemaSpec &UnitsSchema::spec() const {
    return spec_;
}
Result<Translation> UnitsSchema::translate(const Quantity &q, QuantityFormat format) const {
    auto it = spec_.translations.find(q.unit());
    if (it == spec_.translations.end())
        return render(q.value(), 1, internal_symbol(q.unit()), format);
    for (const auto &row : it->second)
        if (row.threshold == 0 || row.threshold * (1 - 1e-12) > std::abs(q.value()))
            return render(q.value(), row.factor, row.unit_string, format);
    return std::unexpected(error("schema", "no matching rule"));
}
UnitsSchemas::UnitsSchemas() {
    auto fixed = [](std::string symbol, double factor) {
        return std::vector<UnitTranslationSpec>{{0, std::move(symbol), factor}};
    };
    auto add_builtin = [&](UnitsSchemaSpec s) {
        auto r = add(std::move(s));
        if (!r)
            throw std::logic_error(r.error().message);
    };
    add_builtin({0,
                 "Internal",
                 {"mm", 1},
                 {{Unit::length(), fixed("mm", 1)},
                  {Unit::area(), fixed("mm^2", 1)},
                  {Unit::volume(), fixed("mm^3", 1)},
                  {Unit::angle(), fixed("deg", 1)}}});
    add_builtin({9,
                 "MeterDecimal",
                 {"m", 1},
                 {{Unit::length(), fixed("m", 1000)},
                  {Unit::area(), fixed("m^2", 1e6)},
                  {Unit::volume(), fixed("m^3", 1e9)},
                  {Unit::force(), fixed("N", 1000)},
                  {Unit::pressure(), fixed("Pa", .001)},
                  {Unit::angle(), fixed("deg", 1)}}});
    add_builtin({3,
                 "ImperialDecimal",
                 {"in", 1},
                 {{Unit::length(), fixed("in", 25.4)},
                  {Unit::area(), fixed("in^2", 25.4 * 25.4)},
                  {Unit::angle(), fixed("deg", 1)}}});
    add_builtin({5,
                 "Centimeter",
                 {"cm", 1},
                 {{Unit::length(), fixed("cm", 10)}, {Unit::area(), fixed("cm^2", 100)}}});
    add_builtin(
        {100, "AdaptiveLength", {"mm", 1}, {{Unit::length(), {{1000, "mm", 1}, {0, "m", 1000}}}}});
}
Result<void> UnitsSchemas::add(UnitsSchemaSpec spec) {
    auto v = valid_schema(spec);
    if (!v)
        return v;
    if (schemas_.contains(spec.id))
        return std::unexpected(error("schema", "duplicate schema id"));
    for (const auto &[id, s] : schemas_) {
        (void)id;
        if (s.spec().name == spec.name)
            return std::unexpected(error("schema", "duplicate schema name"));
    }
    auto id = spec.id;
    schemas_.emplace(id, UnitsSchema{std::move(spec)});
    return {};
}
Result<void> UnitsSchemas::select(std::string_view name) {
    for (const auto &[id, s] : schemas_)
        if (s.spec().name == name) {
            current_ = id;
            return {};
        }
    return std::unexpected(error("schema", "unknown schema"));
}
Result<void> UnitsSchemas::select(unsigned id) {
    if (!schemas_.contains(id))
        return std::unexpected(error("schema", "unknown schema id"));
    current_ = id;
    return {};
}
const UnitsSchema &UnitsSchemas::current() const {
    return schemas_.at(current_);
}
Result<UnitsSchema> UnitsSchemas::get(std::string_view name) const {
    for (const auto &[id, s] : schemas_) {
        (void)id;
        if (s.spec().name == name)
            return s;
    }
    return std::unexpected(error("schema", "unknown schema"));
}
std::vector<std::string> UnitsSchemas::names() const {
    std::vector<std::string> result;
    for (const auto &[id, s] : schemas_) {
        (void)id;
        result.push_back(s.spec().name);
    }
    return result;
}
UnitsApi::UnitsApi(UnitRegistry r) : registry_(std::move(r)) {}
UnitRegistry &UnitsApi::registry() {
    return registry_;
}
const UnitRegistry &UnitsApi::registry() const {
    return registry_;
}
UnitsSchemas &UnitsApi::schemas() {
    return schemas_;
}
const UnitsSchemas &UnitsApi::schemas() const {
    return schemas_;
}
Result<ParsedQuantity> UnitsApi::parse(std::string_view source,
                                       const std::optional<InputContext> &context) const {
    return parse_quantity(source, registry_, context);
}
Result<Translation> UnitsApi::format(const Quantity &q, QuantityFormat format,
                                     std::optional<UnitRef> override) const {
    if (override && !override->id.empty()) {
        auto u = registry_.find_id(override->id, override->version);
        if (!u)
            return std::unexpected(u.error());
        auto n = to_unit(q, *u);
        if (!n)
            return std::unexpected(n.error());
        return render(q.value(), u->scale, u->symbol, format);
    }
    return schemas_.current().translate(q, format);
}
Result<InputContext> UnitsApi::capture(QuantitySpec spec, const UnitConfig &config,
                                       std::string target, std::uint64_t generation) const {
    auto schema = schemas_.get(config.schema);
    if (!schema)
        return std::unexpected(schema.error());
    auto effective = config;
    if (spec.unit != Unit::length()) {
        effective.input = {"", 1};
        effective.display = {"", 1};
    }
    auto context =
        begin_edit(spec, effective, std::move(target), generation, schema->spec().basic_length);
    bool explicit_input =
        effective.input_overrides.contains(context.target) || !effective.input.id.empty() ||
        (effective.input_follows_display &&
         (!effective.display.id.empty() || effective.display_overrides.contains(context.target)));
    Result<UnitDefinition> unit = std::unexpected(error("input_unit", "unresolved input unit"));
    if (explicit_input || (spec.unit == Unit::length() && !effective.input_follows_display))
        unit = registry_.find_id(context.input_unit.id, context.input_unit.version);
    else {
        auto translated = schema->translate(*Quantity::make(1, spec.unit));
        if (!translated)
            return std::unexpected(translated.error());
        auto parsed = parse_unit(translated->unit_string, registry_);
        if (!parsed)
            return std::unexpected(parsed.error());
        unit = parsed->definition;
    }
    if (!unit)
        return std::unexpected(unit.error());
    if (unit->unit != spec.unit)
        return std::unexpected(error("unit_mismatch", "input unit incompatible with property"));
    context.input_unit = {unit->id, unit->version};
    context.frozen_unit = *unit;
    return context;
}
} // namespace units
