import std;
import month02.week02.parser;

int main() {
    using namespace month02::week02;
    // 在这里修改表达式，保存后重新运行 ./run.sh。
    constexpr std::string_view expression = "-(2 + 3) * 4";

    try {
        auto result = parse(expression);

        if (!result) {
            const auto &error = result.error();
            std::cerr << stage_name(error.stage) << " at byte " << error.position << ": "
                      << error.message << '\n';
            return 1;
        }

        std::cout << describe(**result) << '\n';
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
