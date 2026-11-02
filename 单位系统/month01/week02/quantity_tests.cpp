import std;
import course.quantity;
using namespace units;
void check(bool ok, std::string_view message) {
    if (!ok)
        throw std::runtime_error(std::string{message});
}
int main() try {
    UnitRegistry r;
    auto q = [&](double v, std::string_view s) { return *from_unit(v, *r.find(s)); };
    auto l = q(1000, "mm"), w = q(50, "cm"), a = *calculate('*', l, w);
    check(l.value() == 1000 && w.value() == 500 && a.value() == 500000 && a.unit() == Unit::area(),
          "rectangle internal mm");
    check(calculate('+', l, w)->value() == 1500 && calculate('/', l, w)->value() == 2,
          "arithmetic");
    check(!calculate('+', l, a) && !calculate('/', l, q(0, "mm")), "dimension and division errors");
    check(calculate('+', q(1, "in"), q(1, "mm"))->value() == 26.4, "symbol is not dimension");
    check(Unit::work() == Unit::moment(), "same exponents mean equal Unit");
    check(Unit::angle().exponents()[7] == 1 && !Unit::angle().dimensionless(), "eighth dimension");
    check(!Unit::make({8, 0, 0, 0, 0, 0, 0, 0}) &&
              Unit::make({-8, 0, 0, 0, 0, 0, 0, 0}).has_value(),
          "exponent bounds");
    check(!unit_product('*', *unit_power(Unit::length(), 7), Unit::length()), "algebra overflow");
    check(unit_root(Unit::area(), 2).value() == Unit::length() && !unit_root(Unit::length(), 2) &&
              !unit_root(Unit{}, 0),
          "root dimensions");
    check(power(q(4, "mm2"), .5)->value() == 2 && !power(l, .5),
          "fractional power must preserve integer dimensions");
    check(q(1, "N").value() == 1000 && q(1, "Pa").value() == .001, "derived base scales");
    check(std::abs(q(1, "rad").value() - 180 / std::numbers::pi) < 1e-12 &&
              sine(q(90, "deg"))->value() == 1 && sine(q(90, "1"))->value() == 1,
          "degree boundary and FreeCAD App rule");
    check(!sine(l) && !Quantity::make(std::numeric_limits<double>::infinity()), "invalid values");
    check(validate(q(0, "mm"), {Unit::length(), 0, {}}).has_value() &&
              !validate(q(-1, "mm"), {Unit::length(), 0, {}}),
          "range belongs to property");
    check(!validate(l, {Unit::area(), {}, {}}), "property unit mismatch");
    check(!r.add({"bad", 1, "bad", Unit::length(), 0}) &&
              !r.add({"alias", 1, "mm", Unit::length(), 1}),
          "registry invariants");
    check(r.add({"pitch", 1, "pitch", Unit::length(), 10}).has_value(), "custom definition");
    auto bound = *r.find("pitch");
    check(r.add({"pitch", 2, "pitch", Unit::length(), 20}).has_value() && bound.scale == 10 &&
              r.find_id("pitch", 1)->scale == 10,
          "stable version");
    check(!r.add({"mm", 2, "mm", Unit::length(), 2}), "builtin immutable");
    check(!nearly_equal(l, l, std::numeric_limits<double>::infinity()), "finite tolerance");
    std::println("quantity: passed");
} catch (const std::exception &e) {
    std::println(std::cerr, "{}", e.what());
    return 1;
}
