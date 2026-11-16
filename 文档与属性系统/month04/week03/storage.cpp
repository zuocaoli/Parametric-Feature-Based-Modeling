module month06.week03.storage;

import std;

namespace month06::week03 {
namespace {
EngineError error(std::string message) {
    return {std::move(message), {}};
}
[[noreturn]] void reject(std::string message) {
    throw std::runtime_error(std::move(message));
}
void token(std::istream &input, std::string_view expected) {
    std::string actual;
    if (!(input >> actual) || actual != expected) {
        reject("expected " + std::string{expected});
    }
}
std::string word(std::istream &input) {
    std::string result;
    if (!(input >> result)) {
        reject("unexpected end of file");
    }
    return result;
}
ObjectId unsigned_number(std::istream &input) {
    const auto text = word(input);
    ObjectId value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
        reject("invalid unsigned integer");
    }
    return value;
}
std::size_t count(std::istream &input) {
    const auto value = unsigned_number(input);
    if (value > 100000) {
        reject("record count exceeds teaching format limit");
    }
    return static_cast<std::size_t>(value);
}
std::string quoted(std::istream &input) {
    input >> std::ws;
    if (input.peek() != '"') {
        reject("expected quoted string");
    }
    std::string value;
    if (!(input >> std::quoted(value))) {
        reject("invalid quoted string");
    }
    return value;
}
Value read_value(std::istream &input) {
    const auto tag = word(input);
    if (tag == "string") {
        return quoted(input);
    }
    const auto text = word(input);
    if (tag == "bool") {
        if (text == "0") {
            return false;
        }
        if (text == "1") {
            return true;
        }
        reject("invalid boolean");
    }
    if (tag == "double") {
        double value = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
            !std::isfinite(value)) {
            reject("invalid finite double");
        }
        return value;
    }
    if (tag == "integer") {
        std::int64_t value = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
            reject("invalid signed integer");
        }
        return value;
    }
    reject("unknown value type");
}
void write_value(std::ostream &output, const Value &value) {
    std::visit(
        [&](const auto &item) {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, std::string>) {
                output << "string " << std::quoted(item);
            } else if constexpr (std::is_same_v<T, bool>) {
                output << "bool " << (item ? 1 : 0);
            } else if constexpr (std::is_same_v<T, double>) {
                output << "double " << item;
            } else {
                output << "integer " << item;
            }
        },
        value);
}
const std::set<std::string> property_names{"length",  "width", "area", "segments",
                                           "visible", "name",  "x",    "y"};
const std::set<std::string> numeric_names{"length", "width", "area", "segments", "x", "y"};
EngineSnapshot read_snapshot(std::istream &input) {
    token(input, "CAD_DOCUMENT");
    if (unsigned_number(input) != 2) {
        reject("unsupported document version");
    }
    EngineSnapshot snapshot;
    token(input, "HIGH_WATER");
    snapshot.high_water = unsigned_number(input);
    token(input, "OBJECTS");
    const auto objects = count(input);
    for (std::size_t i = 0; i < objects; ++i) {
        token(input, "OBJECT");
        token(input, "rectangle");
        ObjectSnapshot object{unsigned_number(input), {}};
        token(input, "VALUES");
        const auto values = count(input);
        for (std::size_t j = 0; j < values; ++j) {
            token(input, "PROPERTY");
            auto key = quoted(input);
            auto value = read_value(input);
            object.values.push_back({std::move(key), std::move(value)});
        }
        snapshot.objects.push_back(std::move(object));
    }
    token(input, "FORMULAS");
    const auto formulas = count(input);
    for (std::size_t i = 0; i < formulas; ++i) {
        token(input, "FORMULA");
        FormulaSnapshot formula{{unsigned_number(input), quoted(input)}, quoted(input), {}};
        token(input, "ALIASES");
        const auto aliases = count(input);
        for (std::size_t j = 0; j < aliases; ++j) {
            token(input, "ALIAS");
            auto alias = quoted(input);
            PropertyKey key{unsigned_number(input), quoted(input)};
            if (!formula.aliases.emplace(std::move(alias), std::move(key)).second) {
                reject("duplicate variable mapping");
            }
        }
        // 缺失对象可恢复，但引用必须是本课程真实的数值属性键。
        auto tree = parse(formula.source);
        if (!tree) {
            reject("invalid formula syntax: " + tree.error().message);
        }
        for (const auto &key :
             resolve_dependencies(**tree, formula.target.object, formula.aliases)) {
            if (!numeric_names.contains(key.name)) {
                reject("invalid numeric property reference");
            }
        }
        snapshot.formulas.push_back(std::move(formula));
    }
    token(input, "END");
    if (!input.eof()) {
        input >> std::ws;
    }
    if (input.bad() || !input.eof()) {
        reject("trailing data or input failure");
    }
    for (const auto &object : snapshot.objects) {
        std::set<std::string> names;
        for (const auto &value : object.values) {
            if (!names.insert(value.name).second) {
                reject("duplicate input property");
            }
        }
        for (const auto &formula : snapshot.formulas) {
            if (formula.target.object == object.id && !names.insert(formula.target.name).second) {
                reject("bound property also stored as input or duplicate formula");
            }
        }
        if (names != property_names) {
            reject("missing or unknown property");
        }
    }
    return snapshot;
}
} // namespace

std::expected<void, EngineError> save_document(const Document &document, std::ostream &output) {
    try {
        const auto snapshot = document.model().snapshot();
        std::ostringstream buffer;
        buffer.imbue(std::locale::classic());
        buffer << std::setprecision(std::numeric_limits<double>::max_digits10);
        buffer << "CAD_DOCUMENT 2\nHIGH_WATER " << snapshot.high_water << "\nOBJECTS "
               << snapshot.objects.size() << '\n';
        KeySet bound;
        for (const auto &formula : snapshot.formulas) {
            bound.insert(formula.target);
        }
        for (const auto &object : snapshot.objects) {
            std::size_t values = 0;
            for (const auto &value : object.values) {
                values += !bound.contains({object.id, value.name});
            }
            buffer << "OBJECT rectangle " << object.id << "\nVALUES " << values << '\n';
            for (const auto &value : object.values) {
                if (bound.contains({object.id, value.name})) {
                    continue;
                }
                buffer << "PROPERTY " << std::quoted(value.name) << ' ';
                write_value(buffer, value.value);
                buffer << '\n';
            }
        }
        buffer << "FORMULAS " << snapshot.formulas.size() << '\n';
        for (const auto &formula : snapshot.formulas) {
            buffer << "FORMULA " << formula.target.object << ' ' << std::quoted(formula.target.name)
                   << ' ' << std::quoted(formula.source) << "\nALIASES " << formula.aliases.size()
                   << '\n';
            for (const auto &[alias, key] : formula.aliases) {
                buffer << "ALIAS " << std::quoted(alias) << ' ' << key.object << ' '
                       << std::quoted(key.name) << '\n';
            }
        }
        buffer << "END\n";
        if (!buffer) {
            return std::unexpected(error("failed to serialize document"));
        }
        // write 不受调用者 width/locale/数字格式影响。
        const auto bytes = buffer.str();
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (!output) {
            return std::unexpected(error("failed to write document"));
        }
        return {};
    } catch (const std::exception &failure) {
        return std::unexpected(error(failure.what()));
    }
}
std::expected<void, EngineError> load_document(Document &document, std::istream &input) {
    try {
        const auto snapshot = read_snapshot(input);
        auto candidate = RecomputeEngine::restore(snapshot, true);
        if (!candidate) {
            return std::unexpected(candidate.error());
        }
        return document.replace_model(std::move(*candidate));
    } catch (const std::exception &failure) {
        return std::unexpected(error(failure.what()));
    }
}
} // namespace month06::week03
