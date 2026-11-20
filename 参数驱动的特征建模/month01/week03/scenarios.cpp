module course_scenarios;

import std;
import feature_model;

namespace cad {
namespace {
constexpr RectangleProfile plateProfile{0, 0, 100, 60};

constexpr RectangleProfile pocketProfile{40, 25, 20, 10};

void check(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

void near(double actual, double expected, double tolerance, std::string_view description) {
    if (!(std::abs(actual - expected) <= tolerance)) {
        throw std::runtime_error(std::string(description) + ": expected " + std::to_string(expected) + ", got " + std::to_string(actual));
    }
}

const Geometry &result_of(const FeatureModel &model, FeatureId id) {
    const auto *result = model.result(id);
    check(result != nullptr, "expected current geometry");
    return *result;
}

void show(std::ostream &out, const FeatureModel &model) {
    for (const auto &step : model.last_report()) {
        out << "feature " << step.id.value << ' ' << state_name(step.state) << " count=" << step.executions << " (" << step.message << ")\n";
    }
}

void check_step(const FeatureModel &model, std::size_t index, FeatureId id, FeatureState state, std::size_t executions) {
    const auto &report = model.last_report();
    check(index < report.size(), "missing report step");
    const auto &step = report[index];
    check(step.id == id && step.state == state && step.executions == executions, "unexpected report step: " + step.message);
}

void single_extrusion(std::ostream &out) {
    FeatureModel model;
    const auto plate = model.add_extrusion(plateProfile, model.parameter("height"));
    model.rebuild();
    near(bounds(result_of(model, plate))[5], 20, 1e-6, "initial height");
    near(volume(result_of(model, plate)), 120000, 1e-4, "initial volume");
    check_step(model, 0, plate, FeatureState::Valid, 1);

    check(model.set_parameters({{"t", 15.0}}) == SetResult::Changed, "update t");
    const auto &geometry = result_of(model, plate);
    const auto height = bounds(geometry)[5];
    const auto mass = volume(geometry);
    near(height, 30, 1e-6, "updated height");
    near(mass, 180000, 1e-4, "updated volume");
    check_step(model, 0, plate, FeatureState::Valid, 1);
    out << "t=10 -> 15 mm, plate height=20 -> " << height << " mm, plate volume=120000 -> " << mass << " mm^3\n";
    show(out, model);
}

struct PlateAndPocket {
    FeatureId plate;
    FeatureId pocket;
};

PlateAndPocket add_pocket(FeatureModel &model) {
    const auto plate = model.add_extrusion(plateProfile, model.parameter("height"));
    return {plate, model.add_cut(pocketProfile, model.parameter("depth"), plate)};
}

void rebuild_order(std::ostream &out) {
    FeatureModel model;
    const auto [plate, pocket] = add_pocket(model);
    model.rebuild();
    near(volume(result_of(model, pocket)), 119000, 1e-4, "initial pocketed volume");
    check(model.set_parameters({{"t", 15.0}}) == SetResult::Changed, "update t");
    check(model.last_report().size() == 2, "both features need rebuilding");
    check_step(model, 0, plate, FeatureState::Valid, 1);
    check_step(model, 1, pocket, FeatureState::Valid, 1);
    const auto mass = volume(result_of(model, pocket));
    near(mass, 179000, 1e-4, "cut should use new base");
    out << "pocketed volume=119000 -> " << mass << " mm^3\n";
    show(out, model);
}

void incremental_rebuild(std::ostream &out) {
    FeatureModel model;
    const auto [plate, pocket] = add_pocket(model);
    model.rebuild();
    const auto originalPlate = result_of(model, plate).solid;
    check(model.set_parameters({{"depth", 8.0}}) == SetResult::Changed, "update depth");
    check(model.last_report().size() == 1, "only the pocket should rebuild");
    check_step(model, 0, pocket, FeatureState::Valid, 1);
    near(volume(result_of(model, pocket)), 118400, 1e-4, "deeper pocket");
    check(result_of(model, plate).solid.IsSame(originalPlate), "reuse the upstream solid");
    out << "depth edit\n";
    show(out, model);

    check(model.set_parameters({{"t", 15.0}, {"depth", 6.0}}) == SetResult::Changed, "edit one batch");
    check(model.last_report().size() == 2, "batch rebuilds both features once");
    check_step(model, 0, plate, FeatureState::Valid, 1);
    check_step(model, 1, pocket, FeatureState::Valid, 1);
    near(volume(result_of(model, pocket)), 178800, 1e-4, "geometry after batched edit");
    out << "batched edit\n";
    show(out, model);

    check(model.set_parameters({{"t", 15.0}, {"depth", 6.0}}) == SetResult::Unchanged, "same input");
    check(model.last_report().empty(), "unchanged input should skip geometry");
    out << "repeating the same inputs executed no features\n";
}

constexpr Scenario courseScenarios[] = {
    {"single-extrusion", single_extrusion},
    {"rebuild-order", rebuild_order},
    {"incremental-rebuild", incremental_rebuild},
};
}

std::span<const Scenario> scenarios() { return courseScenarios; }
}
