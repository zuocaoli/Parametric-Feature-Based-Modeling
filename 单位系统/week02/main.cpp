import std;
import course.schema;
using namespace units;
int main() {
    UnitsApi api;
    auto q = api.parse("1000 mm")->value;
    for (auto name : {"MeterDecimal", "ImperialDecimal", "Centimeter"}) {
        api.schemas().select(name).value();
        std::println("{}: {} (internal={})", name, api.format(q)->text, q.value());
    }
    for (auto source : {"2 N/mm^2", "2 [N/mm^2]"}) {
        auto parsed = api.parse(source);
        if (!parsed) {
            std::println(std::cerr, "{}", diagnostic(parsed.error()));
            return 1;
        }
        std::println("{} -> internal={} {}", source, parsed->value.value(),
                     internal_symbol(parsed->value.unit()));
    }
    api.schemas().select("AdaptiveLength").value();
    for (double v : {999, 1000, -1000})
        std::println("boundary {} -> {}", v,
                     api.format(*Quantity::make(v, Unit::length()), {0})->text);
}
