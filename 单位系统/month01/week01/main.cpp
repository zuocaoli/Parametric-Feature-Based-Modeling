import std;
import course.quantity;
using namespace units;
int main(int argc, char **argv) {
    if (argc > 1) {
        if (argc == 2 && std::string_view{argv[1]} == "--help") {
            std::println("demo: 八维Unit、毫米/度Quantity、矩形及N/Pa/角度边界");
            return 0;
        }
        return 2;
    }
    UnitRegistry r;
    auto l = *from_unit(1000, *r.find("mm")), w = *from_unit(50, *r.find("cm"));
    std::println("length={} mm, width={} mm, area={} mm²", l.value(), w.value(),
                 calculate('*', l, w)->value());
    std::println("length+width={} mm, length/width={}", calculate('+', l, w)->value(),
                 calculate('/', l, w)->value());
    std::println("length+area: {}", calculate('+', l, *calculate('*', l, w)).error().code);
    std::println("1 N internal={}, 1 Pa internal={}", from_unit(1, *r.find("N"))->value(),
                 from_unit(1, *r.find("Pa"))->value());
    std::println("1 rad internal={} deg, sin(90 deg)={}", from_unit(1, *r.find("rad"))->value(),
                 sine(*from_unit(90, *r.find("deg")))->value());
}
