import std;
import course.schema;
using namespace units;
int main(int argc, char **argv) {
    UnitsApi api;
    if (argc > 1) {
        if (argc == 2 && std::string_view{argv[1]} == "--help") {
            std::println("demo [数量文本]：如 '2 N/mm^2'；默认演示schema表和阈值");
            return 0;
        }
        if (argc != 2)
            return 2;
        auto q = api.parse(argv[1]);
        if (!q) {
            std::println(std::cerr, "{}", diagnostic(q.error()));
            return 1;
        }
        std::println("internal={} {}", q->value.value(), internal_symbol(q->value.unit()));
        return 0;
    }
    auto q = api.parse("1000 mm")->value;
    for (auto name : {"MeterDecimal", "ImperialDecimal", "Centimeter"}) {
        api.schemas().select(name).value();
        std::println("{}: {} (internal={})", name, api.format(q)->text, q.value());
    }
    std::println("2 N/mm^2 -> internal={}", api.parse("2 N/mm^2")->value.value());
    api.schemas().select("AdaptiveLength").value();
    for (double v : {999, 1000, -1000})
        std::println("boundary {} -> {}", v,
                     api.format(*Quantity::make(v, Unit::length()), {0})->text);
}
