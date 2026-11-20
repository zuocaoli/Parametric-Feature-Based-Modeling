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

PlateAndPocket add_pocket(FeatureModel &model, CutSupport support = CutSupport::FixedPlane) {
    const auto plate = model.add_extrusion(plateProfile, model.parameter("height"));
    return {plate, model.add_cut(pocketProfile, model.parameter("depth"), plate, support)};
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

void failure_recovery(std::ostream &out) {
    FeatureModel model;
    const auto [plate, pocket] = add_pocket(model);
    model.rebuild();
    check(model.bind_height("2 * t / (segments - 1)").has_value(), "bind alternate height");
    check(model.last_report().empty(), "equivalent valid height should skip geometry");

    check(model.set_parameters({{"segments", std::int64_t{1}}}) == SetResult::Changed, "trigger formula failure");
    check_step(model, 0, plate, FeatureState::Blocked, 0);
    check_step(model, 1, pocket, FeatureState::Blocked, 0);
    check(model.last_report()[0].message.find("division by zero") != std::string::npos, "report formula failure");
    check(!model.result(plate) && !model.result(pocket), "both current geometries are unavailable");
    const auto *cache = model.cached(pocket);
    check(cache != nullptr, "keep recent successful geometry");
    near(volume(*cache), 119000, 1e-4, "cached pocket");
    out << "division by zero\n";
    show(out, model);

    check(model.set_parameters({{"segments", std::int64_t{2}}, {"t", 0.0}}) == SetResult::Changed, "trigger geometry failure");
    check_step(model, 0, plate, FeatureState::Failed, 1);
    check_step(model, 1, pocket, FeatureState::Blocked, 0);
    check(model.last_report()[0].message.find("height must be positive") != std::string::npos, "report geometry failure");
    out << "zero extrusion height\n";
    show(out, model);

    check(model.set_parameters({{"t", 10.0}}) == SetResult::Changed, "restore the original height");
    check_step(model, 0, plate, FeatureState::Valid, 1);
    check_step(model, 1, pocket, FeatureState::Valid, 1);
    near(volume(result_of(model, pocket)), 119000, 1e-4, "restored pocket");
    out << "original height recovered\n";
    show(out, model);
}

void check_top_pocket(const Geometry &geometry, double height, double depth) {
    near(bounds(geometry)[5], height, 1e-6, "top elevation");
    check(!contains_material(geometry, 50, 30, height - depth / 2), "pocket must open at the current top");
    check(contains_material(geometry, 50, 30, (height - depth) / 2), "material must remain below the pocket");
    check(!contains_material(geometry, 50, 30, height - depth + 0.05), "void just above the pocket floor");
    check(contains_material(geometry, 50, 30, height - depth - 0.05), "material just below the pocket floor");
    check(contains_material(geometry, 10, 10, height - depth / 2), "material must remain beside the pocket");
}

void face_reference(std::ostream &out) {
    FeatureModel model;
    const auto [plate, pocket] = add_pocket(model, CutSupport::ExtrusionEnd);
    model.rebuild();
    check_top_pocket(result_of(model, pocket), 20, 5);
    check(model.set_parameters({{"t", 15.0}}) == SetResult::Changed, "update t");
    const auto &base = result_of(model, plate);
    const auto &pocketGeometry = result_of(model, pocket);
    check_top_pocket(pocketGeometry, 30, 5);
    near(volume(pocketGeometry), 179000, 1e-4, "same cut volume at the new top");

    Geometry missing = base;
    missing.endFaces.clear();
    const auto missingCut = cut(missing, pocketProfile, 5.0, CutSupport::ExtrusionEnd);
    check(!missingCut && missingCut.error() == "extrusion end face is missing", "report missing role");
    const auto face = resolve_end_face(base.endFaces);
    check(face.has_value(), "the demonstration extrusion yields one end face");
    const auto other = extrude(plateProfile, 20);
    check(other.has_value() && other->endFaces.size() == 1, "second candidate fixture");
    Geometry ambiguous = base;
    ambiguous.endFaces.push_back(other->endFaces.front());
    const auto ambiguousCut = cut(ambiguous, pocketProfile, 5.0, CutSupport::ExtrusionEnd);
    check(!ambiguousCut && ambiguousCut.error() == "extrusion end face is ambiguous", "report ambiguous role");

    const auto unsupported = model.add_cut(pocketProfile, model.parameter("depth"), pocket, CutSupport::ExtrusionEnd);
    model.rebuild();
    check_step(model, 0, unsupported, FeatureState::Failed, 1);
    check(!model.result(unsupported), "a cut cannot publish a shape when its support is missing");
    out << "top pocket moved from z=20 to z=30 mm; missing and ambiguous supports were detected\n";
    show(out, model);
}

struct ComparisonModel {
    FeatureModel model;
    PlateAndPocket body = add_pocket(model, CutSupport::ExtrusionEnd);
    FeatureId independent = model.add_extrusion({200, 0, 12, 9}, model.parameter("independentHeight"));
};

void equal_geometry(const Geometry &incremental, const Geometry &full) {
    const auto incrementalBounds = bounds(incremental);
    const auto fullBounds = bounds(full);
    for (std::size_t i = 0; i < incrementalBounds.size(); ++i) {
        near(incrementalBounds[i], fullBounds[i], 1e-6, "incremental vs. full bounds");
    }
    near(volume(incremental), volume(full), 1e-4, "incremental vs. full volume");
}

void compare_results(const ComparisonModel &incremental, const ComparisonModel &full, double height, double depth) {
    equal_geometry(result_of(incremental.model, incremental.body.plate), result_of(full.model, full.body.plate));
    const auto &incrementalPocket = result_of(incremental.model, incremental.body.pocket);
    const auto &fullPocket = result_of(full.model, full.body.pocket);
    equal_geometry(incrementalPocket, fullPocket);
    check_top_pocket(incrementalPocket, height, depth);
    check_top_pocket(fullPocket, height, depth);
    near(volume(incrementalPocket), 6000 * height - 200 * depth, 1e-4, "analytical pocket volume");
    equal_geometry(result_of(incremental.model, incremental.independent), result_of(full.model, full.independent));
}

void full_comparison(std::ostream &out) {
    ComparisonModel incremental;
    ComparisonModel full;
    incremental.model.rebuild();
    full.model.rebuild(RebuildMode::Full);
    compare_results(incremental, full, 20, 5);

    auto edit = [&](const std::vector<Update> &changes) {
        check(incremental.model.set_parameters(changes) == SetResult::Changed, "incremental edit");
        check(full.model.set_parameters(changes, RebuildMode::Full) == SetResult::Changed, "full edit");
        check(full.model.last_report().size() == 3, "full mode visits all features");
        check_step(full.model, 2, full.independent, FeatureState::Valid, 1);
        for (const auto &step : incremental.model.last_report()) {
            check(step.id != incremental.independent, "independent branch must be reused");
        }
    };

    edit({{"t", 15.0}});
    check(incremental.model.last_report().size() == 2, "height edit visits the whole dependent chain");
    compare_results(incremental, full, 30, 5);
    edit({{"depth", 8.0}});
    check(incremental.model.last_report().size() == 1, "depth edit visits the pocket only");
    check_step(incremental.model, 0, incremental.body.pocket, FeatureState::Valid, 1);
    compare_results(incremental, full, 30, 8);
    edit({{"t", 12.0}, {"depth", 6.0}});
    compare_results(incremental, full, 24, 6);

    edit({{"t", 0.0}});
    check_step(incremental.model, 0, incremental.body.plate, FeatureState::Failed, 1);
    check_step(incremental.model, 1, incremental.body.pocket, FeatureState::Blocked, 0);
    check_step(full.model, 0, full.body.plate, FeatureState::Failed, 1);
    check_step(full.model, 1, full.body.pocket, FeatureState::Blocked, 0);
    check(!incremental.model.result(incremental.body.pocket) && !full.model.result(full.body.pocket),
          "failed rebuilds cannot expose stale current geometry");
    const auto *incrementalCache = incremental.model.cached(incremental.body.pocket);
    const auto *fullCache = full.model.cached(full.body.pocket);
    check(incrementalCache && fullCache, "both rebuild modes retain the last successful shape");
    equal_geometry(*incrementalCache, *fullCache);

    edit({{"t", 12.0}});
    compare_results(incremental, full, 24, 6);
    out << "full and incremental results agree after independent, batched and failed edits\n";
    out << "incremental recovery\n";
    show(out, incremental.model);
    out << "full recovery\n";
    show(out, full.model);
}

constexpr Scenario courseScenarios[] = {
    {"single-extrusion", single_extrusion}, {"rebuild-order", rebuild_order},   {"incremental-rebuild", incremental_rebuild},
    {"failure-recovery", failure_recovery}, {"face-reference", face_reference}, {"full-comparison", full_comparison},
};
}

std::span<const Scenario> scenarios() { return courseScenarios; }
}
