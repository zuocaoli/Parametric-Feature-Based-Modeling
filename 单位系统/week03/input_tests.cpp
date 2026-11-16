import std;
import course.schema;
using namespace units;
void check(bool ok, std::string_view s) {
    if (!ok)
        throw std::runtime_error(std::string{s});
}
int main() try {
    UnitsApi api;
    auto &r = api.registry();
    check(api.parse("25mm")->value.value() == 25 && api.parse("2 in")->value.value() == 50.8,
          "number+unit");
    auto pressure = api.parse("2 N/mm^2");
    check(pressure && pressure->value.value() == 2000 && pressure->value.unit() == Unit::pressure(),
          "unbracketed compound");
    check(api.parse("2 [N/mm^2]")->value.unit().dimensionless() &&
              api.parse("2 [N/mm^2]")->value.value() == 2,
          "brackets are comments");
    check(api.parse("1e-3 m")->value.value() == 1 &&
              api.parse("2 mm²")->value.unit() == Unit::area(),
          "scientific and UTF8");
    check(!api.parse("1e309 mm") && !api.parse("--1 mm") && !api.parse("2 M") && !api.parse("2 [x"),
          "input diagnostics");
    check(!scan_number("1", 2) && !api.parse(""), "numeric bounds");
    UnitConfig config;
    auto context = *api.capture({Unit::length(), 0, {}}, config, "length", 1);
    config.schema = "MeterDecimal";
    check(commit_edit("20", r, context, 1)->value() == 20, "frozen mm context");
    check(commit_edit("3", r, *api.capture(context.spec, config, "length", 1), 1)->value() == 3000,
          "new schema context");
    check(!commit_edit("20", r, context, 2), "generation conflict");
    auto bad = parse_quantity("2 kg", r, context);
    check(!bad && bad.error().position == 2 && bad.error().target == "length", "assignment range");
    auto utf = api.parse("2 µm");
    check(utf && utf->bindings.front().position == 2 && utf->bindings.front().length == 3,
          "original UTF8 byte span");
    check(!parse_unit("mm^9999999999999999", r), "exponent limit");
    auto pins = api.parse("2 in")->bindings;
    check(parse_quantity("2 in", r, {}, 0, pins, true)->value.value() == 50.8, "stable binding");
    std::println("input: passed");
} catch (const std::exception &e) {
    std::println(std::cerr, "{}", e.what());
    return 1;
}
