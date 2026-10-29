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

        std::cout << "========================================================================"
                  << std::endl;

        //
        // ===================================================================================================
        //
        DependencyGraph graph;
        graph.replace_dependencies(
            //
            {1, "width"},
            //
            resolve_dependencies(**tree, //
                                 1,      //
                                 {{"source_width", {2, "width"}}}));

        graph.replace_dependencies({1, "area"}, {{1, "length"}, {1, "width"}});

        //
        std::cout << "dependencies 1.width:";
        for (const auto &key : graph.dependencies({1, "width"})) {
            std::cout << ' ' << key_text(key);
        }

        std::cout << "\ndependents 1.width:";
        for (const auto &key : graph.dependents({1, "width"})) {
            std::cout << ' ' << key_text(key);
        }

        std::cout << '\n';

    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
