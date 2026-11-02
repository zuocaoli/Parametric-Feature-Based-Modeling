module course.property;
import std;
namespace units {
PropertyQuantity::PropertyQuantity(Quantity q, QuantityFormat f)
    : value_(q.value()), unit_(q.unit()), format_(f) {}
Quantity PropertyQuantity::quantity() const {
    return *Quantity::make(value_, unit_);
}
QuantityFormat PropertyQuantity::format() const {
    return format_;
}
Result<void> PropertyQuantity::set_internal(double v) {
    auto q = Quantity::make(v, unit_);
    if (!q)
        return std::unexpected(q.error());
    value_ = v;
    return {};
}
Result<Quantity> PropertyQuantity::script_quantity(const ScriptValue &v,
                                                   const UnitRegistry &r) const {
    return std::visit(
        [&](const auto &item) -> Result<Quantity> {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, double>)
                return Quantity::make(item, unit_);
            else if constexpr (std::is_same_v<T, Quantity>)
                return item;
            else {
                auto q = parse_quantity(item, r);
                if (!q)
                    return std::unexpected(q.error());
                return q->value;
            }
        },
        v);
}
Result<void> PropertyQuantity::set_script(const ScriptValue &v, const UnitRegistry &r) {
    auto q = script_quantity(v, r);
    if (!q)
        return std::unexpected(q.error());
    if (!q->unit().dimensionless() && q->unit() != unit_)
        return std::unexpected(error("unit_mismatch", "script value incompatible with property"));
    value_ = q->value();
    return {};
}
Result<void> PropertyQuantity::set_expression(const Quantity &q) {
    if (!q.unit().dimensionless())
        unit_ = q.unit();
    value_ = q.value();
    return {};
}
PropertyQuantityConstraint::PropertyQuantityConstraint(Quantity q, double lo, double hi,
                                                       double step)
    : PropertyQuantity(q), lower_(lo), upper_(hi), step_(step) {
    if (!std::isfinite(lo) || !std::isfinite(hi) || !std::isfinite(step) || lo > hi || step <= 0)
        throw std::invalid_argument("invalid constraints");
}
Result<void> PropertyQuantityConstraint::set_script(const ScriptValue &v, const UnitRegistry &r) {
    auto q = script_quantity(v, r);
    if (!q)
        return std::unexpected(q.error());
    if (!q->unit().dimensionless() && q->unit() != unit_)
        return std::unexpected(error("unit_mismatch", "script value incompatible with property"));
    value_ = std::clamp(q->value(), lower_, upper_);
    return {};
}
double PropertyQuantityConstraint::minimum() const {
    return lower_;
}
double PropertyQuantityConstraint::maximum() const {
    return upper_;
}
double PropertyQuantityConstraint::step() const {
    return step_;
}
} // namespace units
