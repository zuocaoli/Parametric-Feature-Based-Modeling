import std;
import month03.week02.dependencies;

int main(int argc, char **) {
    using namespace month03::week02;
    try {
        if (argc != 1) {
            throw std::invalid_argument("usage: demo");
        }

        const PropertyKey length{1, "length"};
        const PropertyKey width{1, "width"};
        const PropertyKey area{1, "area"};

        double length_value = 100, width_value = 1, area_value = 0;

        //
        // <area, area的表达式>
        // <width, width的表达式>
        //
        const std::map<PropertyKey, std::function<void()>> calculations{
            //
            {area, [&] { area_value = length_value * width_value; }}, //
            {width, [&] { width_value = length_value / 2; }}          //
        };

        // 从 calculations 中取出计算函数，然后调用它.
        calculations.at(area)();
        calculations.at(width)();
        // 这里先调用面积计算，然后调用width计算，但是width干更新后，面积数值也就过时了，
        // 所以计算应该是有一个顺序的。
        std::cout << "creation order: width=" << width_value << ", area=" << area_value << '\n';

        width_value = 1;
        area_value = 0;

        // =====================================================================================
        // 上面是引出问题，下面开始解决问题：
        // =====================================================================================
        std::cout << "=================================" << "上面是引出问题，下面开始解决问题：" << "=================================" << '\n';

        //
        DependencyGraph graph;
        // 建立或者更新 {1, "area"} 的依赖图。
        graph.replace_dependencies(area, {length, width});
        // 建立或者更新 {1, "width"} 的依赖图，得到的就是 <{1, "width"}, {{1, "length"}}>，
        // 表示：对象 1 的 width 依赖对象 1 的 length。
        graph.replace_dependencies(width, {length});

        // 对节点进行拓扑排序
        const auto order = graph.topological_order();
        if (!order) {
            throw std::runtime_error(order.error().message);
        }

        std::cout << "calculation order:";
        // 按拓扑顺序遍历属性，找到对应的计算函数并执行,
        // 也就是拓扑顺序决定了计算顺序。
        for (const auto &key : *order) {
            if (const auto found = calculations.find(key); found != calculations.end()) {
                std::cout << ' ' << key_text(key);
                found->second();
            }
        }
        std::cout << "\nwidth=" << width_value << ", area=" << area_value << '\n';

        // 测试： 故意改变属性依赖，以建立循环依赖，然后看结果
        const auto cyclic = graph.try_replace_dependencies(length, {width});
        if (cyclic) {
            // 如果 cyclic 是true, 说明修改成功了，说明循环没有被正确拦截，需要抛出异常，终止程序。
            throw std::runtime_error("cycle was accepted");
        }

        // 将循环区间打印出来
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
