import std;
import month03.week01.dependencies;

int main(int argc, char **) {

    using namespace month03::week01;
    try {

        if (argc != 1) {
            throw std::invalid_argument("usage: demo");
        }

        auto tree = parse("(length + length) / 2 + source_width");
        if (!tree) {
            throw std::runtime_error(tree.error().message);
        }

        // 打印这个语法树中的所有变量
        std::cout << "variables:";
        for (const auto &name : collect_variables(**tree)) {
            std::cout << ' ' << name;
        }
        std::cout << '\n';

        std::cout << "=============================================================" << std::endl;

        //
        // ==============================================================
        //

        DependencyGraph graph;

        // 建立或者更新 {1, "width"} 的依赖图，得到的就是 <{1, "width"}, {{1, "length"}, {2, "width"}}>，
        // 表示：对象 1 的 width 依赖对象 1 的 length 和对象 2 的 width。
        graph.replace_dependencies(
            // 要设置这个 PropertyKey 的依赖图。
            {1, "width"},
            // tree: 这个 target 是通过 tree 这个表达式计算出来的，这个表达式中的变量就是这个 target 依赖的。
            // owner: width 默认属于那个对象。
            // aliases：指定特殊变量对应哪个实际属性。这里让 source_width 对应对象 2 的 width，支持跨对象引用，
            // source_width 只是普通变量名，程序不会根据名字自动猜出它指向谁，因此需要别名表。
            resolve_dependencies(**tree, 1, {{"source_width", {2, "width"}}}));

        // 建立或者更新 {1, "area"} 的依赖图。
        graph.replace_dependencies({1, "area"}, {{1, "length"}, {1, "width"}});

        //
        // ===================================================================
        //

        // 打印 {1, "width"} 依赖的属性
        std::cout << "dependencies 1.width:";
        for (const auto &key : graph.dependencies({1, "width"})) {
            std::cout << ' ' << key_text(key);
        }

        //
        std::cout << "\ndependents 1.width:";
        for (const auto &key : graph.dependents({1, "width"})) {
            std::cout << ' ' << key_text(key);
        }

        //
        std::cout << '\n';

    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
