import std;
import month02.week03.evaluator;
import month02.week03.rectangle;

int main() {
    using namespace month02::week03;

    try {
        const Rectangle rectangle{100, 1};

        // 在这里修改表达式，重新编译后运行。
        constexpr std::string_view input = "length / 2 + 5";

        const auto result =
            calculate(input, [&](std::string_view name) { return rectangle.get(name); });

        if (!result) {
            const auto &error = result.error();
            std::cerr << stage_name(error.stage) << " at byte " << error.position << ": "
                      << error.message << '\n';
            return 1;
        }

        std::cout << input << " = " << *result << '\n';
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
