export module course_scenarios;

import std;

export namespace cad {
struct Scenario {
    std::string_view name;
    void (*run)(std::ostream &);
};

std::span<const Scenario> scenarios();
}
