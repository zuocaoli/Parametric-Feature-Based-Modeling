import std;
import course.document;
using namespace units;
void show(const Document &d) {
    for (auto name : {"length", "width", "area"}) {
        const auto &p = d.model().properties().at({1, name});
        std::println("{}={} {} [{}]", name, p.value.value(), internal_symbol(p.value.unit()),
                     state_name(p.state));
    }
    std::println("recomputed={}, history={}", d.last_report().size(), d.position());
}
int main(int argc, char **argv) {
    if (argc > 1) {
        if (argc == 2 && std::string_view{argv[1]} == "--help") {
            std::println("demo: 属性三通道、带单位公式、重算和恢复");
            return 0;
        }
        return 2;
    }
    UnitRegistry r;
    PropertyQuantity p{*Quantity::make(1, Unit::length())};
    p.set_script(std::string{"1.5 in"}, r).value();
    std::println("script 1.5 in -> {} mm", p.quantity().value());
    p.set_expression(*Quantity::make(4, Unit::area())).value();
    std::println("generic expression -> {}", unit_name(p.quantity().unit()));
    Document d;
    show(d);
    d.transact([](Model &m) {
         return m.set_value({1, "length"}, *Quantity::make(2000, Unit::length()));
     }).value();
    show(d);
    auto bad = d.transact([](Model &m) { return m.bind({1, "width"}, "length+1 kg"); });
    std::println("illegal formula: {}", bad.error().code);
    d.undo().value();
    show(d);
    d.transact([](Model &m) { return m.bind({1, "width"}, "length/ratio"); }).value();
    d.transact([](Model &m) { return m.set_value({1, "ratio"}, *Quantity::make(0)); }).value();
    show(d);
    d.transact([](Model &m) { return m.set_value({1, "ratio"}, *Quantity::make(4)); }).value();
    show(d);
}
