import std;
import course.schema;
using namespace units;
void check(bool ok, std::string_view s) {
    if (!ok)
        throw std::runtime_error(std::string{s});
}
int main() try {
    UnitsApi api;
    auto q = api.parse("1000 mm")->value;
    auto unchanged = q;
    api.schemas().select("MeterDecimal").value();
    check(api.format(q)->text == "1.0000 m", "meter schema");
    api.schemas().select(3).value();
    check(api.format(q)->text == "39.3701 in" && q == unchanged, "imperial no model write");
    api.schemas().select("Centimeter").value();
    auto a = api.parse("500000 mm2")->value;
    check(api.format(a, {0})->text == "5000 cm^2", "area factor");
    api.schemas().select("AdaptiveLength").value();
    for (double v : {999, 1000, -1000}) {
        auto t = api.format(*Quantity::make(v, Unit::length()), {0});
        check(t && t->factor == (std::abs(v) < 1000 ? 1 : 1000), "threshold boundary");
    }
    auto unknown = *Quantity::make(2, Unit::force());
    check(api.format(unknown, {0})->unit_string == "mm*kg*s^-2", "fallback internal unit");
    auto before = api.schemas().current().spec().id;
    check(!api.schemas().select("missing") && api.schemas().current().spec().id == before,
          "unknown selection preserves active schema");
    check(!api.schemas().add({200, "Bad", {"mm", 1}, {{Unit::length(), {{1, "mm", 1}}}}}),
          "default required");
    check(api.schemas()
              .add({201, "Teaching", {"cm", 1}, {{Unit::length(), {{0, "cm", 10}}}}})
              .has_value(),
          "data-only schema extension");
    api.schemas().select("Teaching").value();
    check(api.format(q, {0})->text == "100 cm", "new data uses same algorithm");
    check(api.format(q, {0, false, true}, UnitRef{"mm", 1})->text == "1,000 mm",
          "grouping and override");
    check(!api.format(q, {18}) && !api.format(q, {}, UnitRef{"kg", 1}), "format errors");
    UnitConfig context_config;
    context_config.schema = "MeterDecimal";
    auto area_context = api.capture({Unit::area(), 0, {}}, context_config, "area", 1);
    check(area_context && commit_edit("2", api.registry(), *area_context, 1)->value() == 2000000,
          "compound input unit frozen from schema");
    api.schemas()
        .add({202, "BasicCmDisplayMm", {"cm", 1}, {{Unit::length(), {{0, "mm", 1}}}}})
        .value();
    context_config.schema = "BasicCmDisplayMm";
    auto ordinary = api.capture({Unit::length(), 0, {}}, context_config, "length", 1);
    check(ordinary && ordinary->frozen_unit->scale == 10 &&
              commit_edit("2", api.registry(), *ordinary, 1)->value() == 20,
          "ordinary length input uses schema basic length");
    context_config.input_follows_display = true;
    auto displayed = api.capture({Unit::length(), 0, {}}, context_config, "length", 1);
    check(displayed && commit_edit("2", api.registry(), *displayed, 1)->value() == 2,
          "follow display uses millimetres instead of basic centimetres");
    context_config.input_overrides["length"] = {"in", 1};
    auto overridden = api.capture({Unit::length(), 0, {}}, context_config, "length", 1);
    check(overridden && commit_edit("2", api.registry(), *overridden, 1)->value() == 50.8,
          "property input override retains precedence");
    std::println("schema: passed");
} catch (const std::exception &e) {
    std::println(std::cerr, "{}", e.what());
    return 1;
}
