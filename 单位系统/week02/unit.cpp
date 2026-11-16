module course.unit;
import std;
namespace units {
Error error(std::string code, std::string message) {
    return {std::move(code), std::move(message), "unit", 0, 0, {}, {}, {}};
}
Unit::Unit(std::array<int, 8> exponents) : exponents_(exponents) {}
Result<Unit> Unit::make(std::array<int, 8> exponents) {
    for (int e : exponents)
        if (e < -8 || e >= 8)
            return std::unexpected(error("exponent_range", "unit exponent must be in [-8,7]"));
    return Unit{exponents};
}
const std::array<int, 8> &Unit::exponents() const {
    return exponents_;
}
bool Unit::dimensionless() const {
    return *this == Unit{};
}
Unit Unit::length() {
    return *make({1, 0, 0, 0, 0, 0, 0, 0});
}
Unit Unit::area() {
    return *make({2, 0, 0, 0, 0, 0, 0, 0});
}
Unit Unit::volume() {
    return *make({3, 0, 0, 0, 0, 0, 0, 0});
}
Unit Unit::mass() {
    return *make({0, 1, 0, 0, 0, 0, 0, 0});
}
Unit Unit::time() {
    return *make({0, 0, 1, 0, 0, 0, 0, 0});
}
Unit Unit::temperature() {
    return *make({0, 0, 0, 0, 1, 0, 0, 0});
}
Unit Unit::angle() {
    return *make({0, 0, 0, 0, 0, 0, 0, 1});
}
Unit Unit::force() {
    return *make({1, 1, -2, 0, 0, 0, 0, 0});
}
Unit Unit::pressure() {
    return *make({-1, 1, -2, 0, 0, 0, 0, 0});
}
Unit Unit::work() {
    return *make({2, 1, -2, 0, 0, 0, 0, 0});
}
Unit Unit::moment() {
    return work();
}
Result<Unit> unit_product(char op, Unit a, Unit b) {
    if (op != '*' && op != '/')
        return std::unexpected(error("operator", "unit algebra supports * and /"));
    auto exps = a.exponents();
    for (std::size_t i = 0; i < 8; ++i)
        exps[i] += (op == '*' ? 1 : -1) * b.exponents()[i];
    return Unit::make(exps);
}
Result<Unit> unit_power(Unit unit, double n) {
    if (!std::isfinite(n))
        return std::unexpected(error("exponent", "exponent must be finite"));
    auto exps = unit.exponents();
    for (int &e : exps) {
        auto value = e * n;
        if (!std::isfinite(value) || value < -8 - 1e-12 || value > 7 + 1e-12)
            return std::unexpected(error("exponent_range", "powered exponent outside [-8,7]"));
        auto integer = std::round(value);
        if (std::abs(value - integer) > 1e-12)
            return std::unexpected(
                error("fractional_dimension", "result requires fractional dimension"));
        e = static_cast<int>(integer);
    }
    return Unit::make(exps);
}
Result<Unit> unit_root(Unit u, unsigned n) {
    if (n == 0)
        return std::unexpected(error("exponent", "zeroth root is undefined"));
    return unit_power(u, 1.0 / n);
}
std::string unit_name(Unit u) {
    if (u.dimensionless())
        return "Ratio";
    for (const auto &[unit, name] :
         std::array<std::pair<Unit, const char *>, 11>{{{Unit::length(), "Length"},
                                                        {Unit::area(), "Area"},
                                                        {Unit::volume(), "Volume"},
                                                        {Unit::mass(), "Mass"},
                                                        {Unit::time(), "Time"},
                                                        {Unit::angle(), "Angle"},
                                                        {Unit::temperature(), "Temperature"},
                                                        {Unit::force(), "Force"},
                                                        {Unit::pressure(), "Pressure"},
                                                        {Unit::work(), "Work"},
                                                        {Unit::moment(), "Moment"}}})
        if (u == unit)
            return name;
    return "Compound";
}
std::string internal_symbol(Unit u) {
    constexpr std::array symbols{"mm", "kg", "s", "A", "K", "mol", "cd", "deg"};
    std::string result;
    for (std::size_t i = 0; i < 8; ++i)
        if (int e = u.exponents()[i]; e != 0) {
            if (!result.empty())
                result += '*';
            result += symbols[i];
            if (e != 1)
                result += '^' + std::to_string(e);
        }
    return result.empty() ? "1" : result;
}
std::string describe(Unit u) {
    auto result = unit_name(u) + " [";
    for (int e : u.exponents())
        result += std::to_string(e) + " ";
    return result + "]";
}
} // namespace units
