import std;
import course_scenarios;

int main() {

    try {

        //
        for (const auto &scenario : cad::scenarios()) {

            // 打印当前课程
            std::cout << '\n' << scenario.name << '\n';

            // 执行当前课程
            scenario.run(std::cout);
        }

        return 0;

    } catch (const std::exception &error) {

        //
        std::cerr << error.what() << '\n';
        return 1;
    }
}
