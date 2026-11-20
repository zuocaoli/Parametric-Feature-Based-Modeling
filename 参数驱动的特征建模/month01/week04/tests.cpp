import std;
import course_scenarios;

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "expected one scenario name\n";
        return 1;
    }
    for (const auto &scenario : cad::scenarios()) {
        if (scenario.name == argv[1]) {
            try {
                scenario.run(std::cout);
                return 0;
            } catch (const std::exception &error) {
                std::cerr << scenario.name << ": " << error.what() << '\n';
                return 1;
            }
        }
    }
    std::cerr << "unknown scenario: " << argv[1] << '\n';
    return 1;
}
