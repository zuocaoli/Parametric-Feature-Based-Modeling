import std;
import course_scenarios;

int main() {
    try {
        for (const auto &scenario : cad::scenarios()) {
            std::cout << '\n' << scenario.name << '\n';
            scenario.run(std::cout);
        }
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
