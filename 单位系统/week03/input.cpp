module course.input;
import std;
namespace units {
namespace {
bool space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}
Error located(Error e, std::size_t position, std::size_t length, std::string stage) {
    e.position = position;
    e.length = length;
    e.stage = std::move(stage);
    return e;
}
class UnitParser {
    std::string_view source_;
    const UnitRegistry &registry_;
    std::size_t pos_ = 0, offset_, depth_ = 0;
    std::span<const UnitBinding> pinned_;
    bool require_pins_;
    std::vector<UnitBinding> bindings_;
    void skip() {
        while (pos_ < source_.size() && space(source_[pos_]))
            ++pos_;
    }
    Result<UnitDefinition> factor() {
        skip();
        auto start = pos_;
        if (++depth_ > 64)
            return std::unexpected(at("complexity", "unit nesting limit", start));
        struct Guard {
            std::size_t &depth;
            ~Guard() {
                --depth;
            }
        } guard{depth_};
        Result<UnitDefinition> value = std::unexpected(at("unknown_unit", "unit expected", pos_));
        if (pos_ < source_.size() && source_[pos_] == '(') {
            ++pos_;
            value = product();
            skip();
            if (!value)
                return value;
            if (pos_ == source_.size() || source_[pos_] != ')')
                return std::unexpected(at("syntax", "missing unit ')'", pos_));
            ++pos_;
        } else {
            while (pos_ < source_.size() && !space(source_[pos_]) &&
                   std::string_view{"*/^()[]"}.find(source_[pos_]) == std::string_view::npos &&
                   !source_.substr(pos_).starts_with("²") && !source_.substr(pos_).starts_with("³"))
                ++pos_;
            auto symbol = source_.substr(start, pos_ - start);
            if (symbol.empty())
                return std::unexpected(at("unknown_unit", "unit expected", start));
            const UnitBinding *pin = nullptr;
            for (const auto &b : pinned_)
                if (b.position == offset_ + start && b.length == symbol.size()) {
                    if (pin)
                        return std::unexpected(at("binding", "duplicate unit binding", start));
                    pin = &b;
                }
            if (pin) {
                value = registry_.find_id(pin->unit.id, pin->unit.version);
                if (value && value->symbol != symbol)
                    return std::unexpected(
                        at("binding", "unit binding does not match source", start));
            } else if (require_pins_)
                return std::unexpected(at("binding", "missing literal unit binding", start));
            else
                value = registry_.find(symbol);
            if (!value)
                return std::unexpected(
                    located(value.error(), offset_ + start, symbol.size(), "unit"));
            bindings_.push_back({offset_ + start, symbol.size(), {value->id, value->version}});
        }
        if (!value)
            return value;
        skip();
        int exponent = 1;
        bool powered = false;
        if (source_.substr(pos_).starts_with("²") || source_.substr(pos_).starts_with("³")) {
            exponent = source_.substr(pos_).starts_with("²") ? 2 : 3;
            pos_ += 2;
            powered = true;
        } else if (pos_ < source_.size() && source_[pos_] == '^') {
            powered = true;
            ++pos_;
            skip();
            auto begin = pos_;
            if (pos_ < source_.size() && (source_[pos_] == '-' || source_[pos_] == '+'))
                ++pos_;
            while (pos_ < source_.size() && source_[pos_] >= '0' && source_[pos_] <= '9')
                ++pos_;
            auto text = source_.substr(begin, pos_ - begin);
            if (text.starts_with('+'))
                text.remove_prefix(1);
            auto parsed = std::from_chars(text.data(), text.data() + text.size(), exponent);
            if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
                return std::unexpected(at("exponent", "integer unit exponent expected", begin));
        }
        if (powered) {
            value = unit_power(*value, exponent);
            if (!value)
                return std::unexpected(
                    located(value.error(), offset_ + start, pos_ - start, "unit"));
        }
        return value;
    }
    Result<UnitDefinition> product() {
        auto value = factor();
        while (value) {
            skip();
            if (pos_ >= source_.size() || (source_[pos_] != '*' && source_[pos_] != '/'))
                break;
            char op = source_[pos_++];
            auto rhs = factor();
            if (!rhs)
                return rhs;
            value = unit_product(op, *value, *rhs);
            if (!value)
                return std::unexpected(located(value.error(), offset_ + pos_ - 1, 1, "unit"));
        }
        return value;
    }
    Error at(std::string code, std::string message, std::size_t p) {
        return located(error(std::move(code), std::move(message)), offset_ + p,
                       p < source_.size() ? 1 : 0, "unit");
    }

  public:
    UnitParser(std::string_view s, const UnitRegistry &r, std::size_t offset,
               std::span<const UnitBinding> pins, bool require)
        : source_(s), registry_(r), offset_(offset), pinned_(pins), require_pins_(require) {}
    Result<ParsedUnit> run() {
        if (source_.size() > 4096)
            return std::unexpected(at("complexity", "unit text too long", 0));
        auto d = product();
        if (!d)
            return std::unexpected(d.error());
        skip();
        if (pos_ != source_.size())
            return std::unexpected(at("syntax", "unexpected unit text", pos_));
        return ParsedUnit{*d, std::move(bindings_)};
    }
};
} // namespace
Result<ScannedNumber> scan_number(std::string_view source, std::size_t position) {
    if (position >= source.size() ||
        !((source[position] >= '0' && source[position] <= '9') || source[position] == '.'))
        return std::unexpected(located(error("number", "unsigned numeric token expected"), position,
                                       position < source.size() ? 1 : 0, "lexical"));
    double value = 0;
    const auto begin = source.data() + position;
    auto end =
        std::from_chars(begin, source.data() + source.size(), value, std::chars_format::general);
    if (end.ec != std::errc{} || end.ptr == begin || !std::isfinite(value))
        return std::unexpected(located(error("number", "invalid or overflowing number"), position,
                                       std::max<std::size_t>(1, end.ptr - begin), "lexical"));
    return ScannedNumber{value, static_cast<std::size_t>(end.ptr - source.data())};
}
Result<ParsedUnit> parse_unit(std::string_view source, const UnitRegistry &registry,
                              std::size_t offset, std::span<const UnitBinding> pinned,
                              bool require) {
    return UnitParser{source, registry, offset, pinned, require}.run();
}
Result<ParsedQuantity> parse_quantity(std::string_view source, const UnitRegistry &registry,
                                      const std::optional<InputContext> &context,
                                      std::size_t offset, std::span<const UnitBinding> pinned,
                                      bool require) {
    auto clean = mask_comments(source);
    if (!clean)
        return std::unexpected(clean.error());
    source = *clean;
    if (source.size() > 4096)
        return std::unexpected(error("complexity", "quantity text too long"));
    std::size_t pos = 0;
    while (pos < source.size() && space(source[pos]))
        ++pos;
    double sign = 1;
    if (pos < source.size() && (source[pos] == '+' || source[pos] == '-')) {
        if (source[pos] == '-')
            sign = -1;
        ++pos;
    }
    auto n = scan_number(source, pos);
    if (!n) {
        auto e = n.error();
        e.position += offset;
        return std::unexpected(e);
    }
    pos = n->end;
    while (pos < source.size() && space(source[pos]))
        ++pos;
    std::size_t end = source.size();
    while (end > pos && space(source[end - 1]))
        --end;
    Result<Quantity> value = Quantity::make(sign * n->value);
    std::vector<UnitBinding> bindings;
    std::size_t unit_start = pos;
    if (pos < end) {
        std::string_view unit;
        unit = source.substr(pos, end - pos);
        auto u = parse_unit(unit, registry, offset + pos, pinned, require);
        if (!u)
            return std::unexpected(u.error());
        value = from_unit(sign * n->value, u->definition);
        bindings = std::move(u->bindings);
    } else if (context) {
        auto u = context->frozen_unit
                     ? Result<UnitDefinition>{*context->frozen_unit}
                     : registry.find_id(context->input_unit.id, context->input_unit.version);
        if (!u)
            return std::unexpected(u.error());
        value = from_unit(sign * n->value, *u);
    }
    if (!value)
        return std::unexpected(located(value.error(), offset, source.size(), "conversion"));
    if (context) {
        auto v = validate(*value, context->spec);
        if (!v) {
            auto e = located(v.error(), offset + unit_start, end - unit_start, "assignment");
            e.target = context->target;
            return std::unexpected(e);
        }
    }
    return ParsedQuantity{*value, std::move(bindings)};
}
InputContext begin_edit(QuantitySpec spec, const UnitConfig &config, std::string target,
                        std::uint64_t generation, UnitRef schema_input) {
    UnitRef input =
        config.input_follows_display && !config.display.id.empty() ? config.display : config.input;
    if (input.id.empty())
        input = spec.unit == Unit::angle()  ? UnitRef{"deg", 1}
                : spec.unit.dimensionless() ? UnitRef{"1", 1}
                                            : schema_input;
    if (config.input_overrides.contains(target))
        input = config.input_overrides.at(target);
    else if (config.input_follows_display && config.display_overrides.contains(target))
        input = config.display_overrides.at(target);
    return {spec, 0, 1, input, generation, config.version, std::move(target), {}};
}
Result<Quantity> commit_edit(std::string_view source, const UnitRegistry &registry,
                             const InputContext &context, std::uint64_t generation) {
    if (context.document_generation != generation)
        return std::unexpected(error("edit_conflict", "document or target was replaced"));
    auto q = parse_quantity(source, registry, context);
    if (!q)
        return std::unexpected(q.error());
    return q->value;
}
Result<std::string> mask_comments(std::string_view source) {
    std::string result{source};
    bool comment = false;
    std::size_t start = 0;
    for (std::size_t i = 0; i < result.size(); ++i) {
        if (!comment && result[i] == '[') {
            comment = true;
            start = i;
            result[i] = ' ';
        } else if (comment) {
            if (result[i] == ']')
                comment = false;
            result[i] = ' ';
        } else if (result[i] == ']')
            return std::unexpected(
                located(error("syntax", "unexpected comment terminator"), i, 1, "lexical"));
    }
    if (comment)
        return std::unexpected(located(error("syntax", "unterminated quantity comment"), start,
                                       source.size() - start, "lexical"));
    return result;
}
std::string diagnostic(const Error &e) {
    return e.code + " (" + e.stage + ", bytes " + std::to_string(e.position) + ":" +
           std::to_string(e.length) + "): " + e.message +
           (e.expected.empty() ? "" : "; expected " + e.expected + ", actual " + e.actual);
}
} // namespace units
