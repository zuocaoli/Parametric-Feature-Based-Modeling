module course.quantity;
import std;
namespace units {
namespace {
Error mismatch(Unit expected, Unit actual) {
    auto e = error("unit_mismatch", "incompatible dimensions");
    e.expected = describe(expected);
    e.actual = describe(actual);
    return e;
}
} // namespace
Quantity::Quantity(double v, Unit u) : value_(v), unit_(u) {}
Result<Quantity> Quantity::make(double v, Unit u) {
    if (!std::isfinite(v))
        return std::unexpected(error("nonfinite", "quantity must be finite"));
    return Quantity{v == 0 ? 0 : v, u};
}
double Quantity::value() const {
    return value_;
}
Unit Quantity::unit() const {
    return unit_;
}
Result<void> validate(const Quantity &q, const QuantitySpec &s) {
    if (q.unit() != s.unit)
        return std::unexpected(mismatch(s.unit, q.unit()));
    if ((s.lower && !std::isfinite(*s.lower)) || (s.upper && !std::isfinite(*s.upper)) ||
        (s.lower && s.upper && *s.lower > *s.upper))
        return std::unexpected(error("constraint", "invalid bounds"));
    if ((s.lower && q.value() < *s.lower) || (s.upper && q.value() > *s.upper))
        return std::unexpected(error("range", "property value outside bounds"));
    return {};
}
Result<Quantity> calculate(char op, const Quantity &a, const Quantity &b) {
    Unit unit = a.unit();
    double v = 0;
    if (op == '+' || op == '-') {
        if (a.unit() != b.unit())
            return std::unexpected(mismatch(a.unit(), b.unit()));
        v = op == '+' ? a.value() + b.value() : a.value() - b.value();
    } else if (op == '*' || op == '/') {
        auto u = unit_product(op, a.unit(), b.unit());
        if (!u)
            return std::unexpected(u.error());
        unit = *u;
        if (op == '/' && b.value() == 0)
            return std::unexpected(error("division_by_zero", "division by zero"));
        v = op == '*' ? a.value() * b.value() : a.value() / b.value();
    } else
        return std::unexpected(error("operator", "unknown arithmetic operator"));
    return Quantity::make(v, unit);
}
Result<Quantity> power(const Quantity &q, double n) {
    auto u = unit_power(q.unit(), n);
    if (!u)
        return std::unexpected(u.error());
    if (q.value() == 0 && n < 0)
        return std::unexpected(error("division_by_zero", "negative power of zero"));
    if (q.value() < 0 && std::trunc(n) != n)
        return std::unexpected(error("domain", "fractional power of negative value"));
    return Quantity::make(std::pow(q.value(), n), *u);
}
Result<Quantity> square_root(const Quantity &q) {
    return power(q, .5);
}
Result<Quantity> sine(const Quantity &q) {
    if (q.unit() != Unit::angle() && !q.unit().dimensionless())
        return std::unexpected(mismatch(Unit::angle(), q.unit()));
    return Quantity::make(std::sin(q.value() * std::numbers::pi / 180));
}
UnitRegistry::UnitRegistry() {
    auto add_builtin = [&](std::string id, Unit u, double scale, std::string symbol = "") {
        if (symbol.empty())
            symbol = id;
        auto r = add({id, 1, symbol, u, scale, "builtin-mm-deg-v1", true});
        if (!r)
            throw std::logic_error(r.error().message);
    };
    add_builtin("1", {}, 1);
    for (auto [symbol, scale] : std::array<std::pair<const char *, double>, 9>{{{"mm", 1},
                                                                                {"cm", 10},
                                                                                {"m", 1000},
                                                                                {"km", 1e6},
                                                                                {"um", 1e-3},
                                                                                {"µm", 1e-3},
                                                                                {"nm", 1e-6},
                                                                                {"in", 25.4},
                                                                                {"ft", 304.8}}})
        add_builtin(symbol, Unit::length(), scale);
    add_builtin("mm2", Unit::area(), 1);
    add_builtin("cm2", Unit::area(), 100);
    add_builtin("m2", Unit::area(), 1e6);
    add_builtin("in2", Unit::area(), 25.4 * 25.4);
    add_builtin("mm3", Unit::volume(), 1);
    add_builtin("m3", Unit::volume(), 1e9);
    add_builtin("kg", Unit::mass(), 1);
    add_builtin("g", Unit::mass(), .001);
    add_builtin("s", Unit::time(), 1);
    add_builtin("min", Unit::time(), 60);
    add_builtin("deg", Unit::angle(), 1);
    add_builtin("degree-symbol", Unit::angle(), 1, "°");
    add_builtin("rad", Unit::angle(), 180 / std::numbers::pi);
    add_builtin("K", Unit::temperature(), 1);
    add_builtin("A", *Unit::make({0, 0, 0, 1, 0, 0, 0, 0}), 1);
    add_builtin("mol", *Unit::make({0, 0, 0, 0, 0, 1, 0, 0}), 1);
    add_builtin("cd", *Unit::make({0, 0, 0, 0, 0, 0, 1, 0}), 1);
    add_builtin("N", Unit::force(), 1000);
    add_builtin("Pa", Unit::pressure(), .001);
    add_builtin("MPa", Unit::pressure(), 1000);
    add_builtin("J", Unit::work(), 1e6);
}
Result<void> UnitRegistry::add(UnitDefinition d) {
    if (d.id.empty() || d.symbol.empty() || !d.version || !std::isfinite(d.scale) || d.scale <= 0)
        return std::unexpected(error("invalid_unit", "invalid unit definition"));
    if (definitions_.contains({d.id, d.version}))
        return std::unexpected(error("unit_conflict", "duplicate id/version"));
    for (const auto &[key, old] : definitions_) {
        (void)key;
        if (old.id == d.id && (old.builtin || d.builtin || old.symbol != d.symbol ||
                               old.unit != d.unit || d.version <= old.version))
            return std::unexpected(
                error("unit_conflict", "unit identity is immutable; version must increase"));
        if (old.symbol == d.symbol && old.id != d.id)
            return std::unexpected(error("unit_conflict", "symbol belongs to another id"));
    }
    definitions_.emplace(std::pair{d.id, d.version}, std::move(d));
    return {};
}
Result<UnitDefinition> UnitRegistry::find(std::string_view s) const {
    std::optional<UnitDefinition> found;
    for (const auto &[key, d] : definitions_) {
        (void)key;
        if (d.symbol == s && (!found || d.version > found->version))
            found = d;
    }
    if (!found)
        return std::unexpected(error("unknown_unit", "unknown unit: " + std::string{s}));
    return *found;
}
Result<UnitDefinition> UnitRegistry::find_id(std::string_view id, unsigned v) const {
    auto it = definitions_.find({std::string{id}, v});
    if (it == definitions_.end())
        return std::unexpected(error("missing_definition", "missing unit id/version"));
    return it->second;
}
std::vector<UnitDefinition> UnitRegistry::custom_definitions() const {
    std::vector<UnitDefinition> out;
    for (const auto &[key, d] : definitions_) {
        (void)key;
        if (!d.builtin)
            out.push_back(d);
    }
    return out;
}
Result<Quantity> from_unit(double v, const UnitDefinition &u) {
    if (!std::isfinite(u.scale) || u.scale <= 0)
        return std::unexpected(error("invalid_unit", "invalid conversion scale"));
    return Quantity::make(v * u.scale, u.unit);
}
Result<double> to_unit(const Quantity &q, const UnitDefinition &u) {
    if (q.unit() != u.unit)
        return std::unexpected(mismatch(u.unit, q.unit()));
    if (!std::isfinite(u.scale) || u.scale <= 0)
        return std::unexpected(error("invalid_unit", "invalid conversion scale"));
    double v = q.value() / u.scale;
    if (!std::isfinite(v))
        return std::unexpected(error("nonfinite", "conversion overflow"));
    return v;
}
Result<UnitDefinition> unit_product(char op, const UnitDefinition &a, const UnitDefinition &b) {
    auto qa = from_unit(1, a), qb = from_unit(1, b);
    if (!qa || !qb)
        return std::unexpected(error("invalid_unit", "invalid component unit"));
    auto q = calculate(op, *qa, *qb);
    if (!q)
        return std::unexpected(q.error());
    if (q->value() <= 0)
        return std::unexpected(error("invalid_unit", "scale underflow"));
    return UnitDefinition{a.id + op + b.id, 1, a.symbol + op + b.symbol, q->unit(), q->value()};
}
Result<UnitDefinition> unit_power(const UnitDefinition &u, double n) {
    auto base = from_unit(1, u);
    if (!base)
        return std::unexpected(base.error());
    auto q = power(*base, n);
    if (!q)
        return std::unexpected(q.error());
    if (q->value() <= 0)
        return std::unexpected(error("invalid_unit", "scale underflow"));
    return UnitDefinition{u.id + "^" + std::to_string(n), 1, u.symbol + "^" + std::to_string(n),
                          q->unit(), q->value()};
}
bool nearly_equal(const Quantity &a, const Quantity &b, double abs, double rel) {
    return a.unit() == b.unit() && std::isfinite(abs) && std::isfinite(rel) && abs >= 0 &&
           rel >= 0 &&
           std::abs(a.value() - b.value()) <=
               std::max(abs, rel * std::max(std::abs(a.value()), std::abs(b.value())));
}
} // namespace units
