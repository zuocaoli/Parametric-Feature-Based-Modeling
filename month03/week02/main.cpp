import std;
import month03.week02.dependencies;

int main(int argc, char **) {
    using namespace month03::week02;
    try {
        if (argc != 1) {
            throw std::invalid_argument("usage: demo");
        }
        const PropertyKey length{1, "length"}, width{1, "width"}, area{1, "area"};
        double length_value = 100, width_value = 1, area_value = 0;
        const std::map<PropertyKey, std::function<void()>> calculations{
            {area, [&] { area_value = length_value * width_value; }},
            {width, [&] { width_value = length_value / 2; }}};
        calculations.at(area)();
        calculations.at(width)();
        std::cout << "creation order: width=" << width_value << ", area=" << area_value << '\n';
        width_value = 1;
        area_value = 0;
        DependencyGraph graph;
        graph.replace_dependencies(area, {length, width});
        graph.replace_dependencies(width, {length});
        const auto order = graph.topological_order();
        if (!order) {
            throw std::runtime_error(order.error().message);
        }
        std::cout << "calculation order:";
        for (const auto &key : *order) {
            if (const auto found = calculations.find(key); found != calculations.end()) {
                std::cout << ' ' << key_text(key);
                found->second();
            }
        }
        std::cout << "\nwidth=" << width_value << ", area=" << area_value << '\n';
        const auto cyclic = graph.try_replace_dependencies(length, {width});
        if (cyclic) {
            throw std::runtime_error("cycle was accepted");
        }
        std::cout << "rejected cycle:";
        for (const auto &key : cyclic.error().cycle) {
            std::cout << ' ' << key_text(key);
        }
        std::cout << '\n';
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
