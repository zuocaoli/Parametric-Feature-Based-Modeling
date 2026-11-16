import std;
import course.property;
using namespace units;
void check(bool ok, std::string_view s) {
    if (!ok)
        throw std::runtime_error(std::string{s});
}
int main() try {
    UnitRegistry r;
    PropertyQuantity p{*Quantity::make(0, Unit::length())};
    check(p.set_script(std::string{"1.5 in"}, r).has_value() &&
              std::abs(p.quantity().value() - 38.1) < 1e-12,
          "inches normalize before assignment");
    check(p.set_script(1.5, r).has_value() && p.quantity().value() == 1.5 &&
              p.quantity().unit() == Unit::length(),
          "script numeric uses property unit");
    check(p.set_script(std::string{"2"}, r).has_value() && p.quantity().value() == 2,
          "dimensionless bypass");
    auto before = p.quantity();
    check(!p.set_script(std::string{"3 kg"}, r) && p.quantity() == before,
          "script mismatch atomic");
    p.set_internal(-5).value();
    check(p.quantity().value() == -5, "raw channel does not clip");
    p.set_expression(*Quantity::make(4, Unit::area())).value();
    check(p.quantity().unit() == Unit::area(), "generic expression updates Unit");
    PropertyQuantityConstraint bounded{*Quantity::make(2, Unit::length()), 0, 10};
    bounded.set_script(20., r).value();
    check(bounded.quantity().value() == 10, "script clips");
    bounded.set_internal(20).value();
    check(bounded.quantity().value() == 20, "raw channel bypasses constraints");
    bounded.set_expression(*Quantity::make(-2, Unit::length())).value();
    check(bounded.quantity().value() == -2, "expression channel bypasses constraints");
    auto old = bounded.quantity();
    check(!bounded.set_script(std::string{"2 kg"}, r) && bounded.quantity() == old,
          "clipping does not bypass unit check");
    std::println("property channels: passed");
} catch (const std::exception &e) {
    std::println(std::cerr, "{}", e.what());
    return 1;
}
