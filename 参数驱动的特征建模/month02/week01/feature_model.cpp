module feature_model;

import std;

namespace cad {
using month03::week04::ComputeState;
using month03::week04::KeySet;

namespace {
PropertyKey graph_key(FeatureId id) { return {id.value, "shape"}; }

std::optional<double> numeric_value(const std::optional<month03::week04::Value> &value) {
    if (!value) {
        return std::nullopt;
    }
    if (const auto *number = std::get_if<double>(&*value)) {
        return *number;
    }
    return std::nullopt;
}
}

std::string_view state_name(FeatureState state) {
    switch (state) {
    case FeatureState::Dirty:
        return "Dirty";
    case FeatureState::Valid:
        return "Valid";
    case FeatureState::Failed:
        return "Failed";
    case FeatureState::Blocked:
        return "Blocked";
    case FeatureState::MissingReference:
        return "MissingReference";
    }
    std::unreachable();
}

FeatureModel::FeatureModel() : parameterId_(parameters_.create_model_parameters()) {
    const auto bound = parameters_.bind(parameter("height"), "2 * t");
    if (!bound) {
        throw std::logic_error(bound.error().message);
    }
}

PropertyKey FeatureModel::parameter(std::string name) const { return {parameterId_, std::move(name)}; }

FeatureId FeatureModel::add(Feature feature) {
    if (nextId_ == 0) {
        throw std::overflow_error("feature IDs exhausted");
    }
    const FeatureId id{nextId_++};
    KeySet dependencies;
    if (feature.base) {
        dependencies.insert(graph_key(*feature.base));
    }
    graph_.replace_dependencies(graph_key(id), dependencies);
    features_.emplace(id, std::move(feature));
    return id;
}

FeatureId FeatureModel::add_extrusion(RectangleProfile profile, PropertyKey height) { return add({profile, std::move(height)}); }

FeatureId FeatureModel::add_cut(RectangleProfile profile, PropertyKey depth, FeatureId base, CutSupport support) {
    if (!features_.contains(base)) {
        throw std::invalid_argument("unknown base feature");
    }
    return add({profile, std::move(depth), base, support});
}

SetResult FeatureModel::set_parameters(const std::vector<Update> &changes) {
    report_.clear();
    const auto status = parameters_.set_many(parameterId_, changes);
    if (status == SetResult::Changed || status == SetResult::Unchanged) {
        rebuild();
    }
    return status;
}

std::expected<void, std::string> FeatureModel::bind_height(std::string_view formula) {
    report_.clear();
    const auto bound = parameters_.bind(parameter("height"), formula);
    if (!bound) {
        return std::unexpected(bound.error().message);
    }
    rebuild();
    return {};
}

FeatureModel::InputSnapshot FeatureModel::read_input(const PropertyKey &key) const {
    return {parameters_.status(key).state, numeric_value(parameters_.value(key))};
}

FeatureStep FeatureModel::execute(FeatureId id) {
    auto &feature = features_.at(id);
    auto finish = [&](FeatureState state, std::size_t executions, std::string message) {
        feature.state = state;
        return FeatureStep{id, state, executions, std::move(message)};
    };

    const auto status = parameters_.status(feature.dimension);
    if (status.state == ComputeState::MissingReference) {
        return finish(FeatureState::MissingReference, 0, status.reason);
    }
    if (status.state != ComputeState::Valid) {
        return finish(FeatureState::Blocked, 0, "parameter " + feature.dimension.name + ": " + status.reason);
    }
    const auto size = numeric_value(parameters_.value(feature.dimension));
    if (!size) {
        return finish(FeatureState::Failed, 0, "no valid numeric parameter: " + feature.dimension.name);
    }
    const Geometry *base = feature.base ? result(*feature.base) : nullptr;
    if (feature.base && !base) {
        return finish(FeatureState::Blocked, 0, "unavailable base feature " + std::to_string(feature.base->value));
    }

    auto built = base ? cut(*base, feature.profile, *size, feature.support) : extrude(feature.profile, *size);
    if (!built) {
        return finish(FeatureState::Failed, 1, std::move(built.error()));
    }
    feature.geometry = std::move(*built);
    return finish(FeatureState::Valid, 1, "shape rebuilt");
}

void FeatureModel::rebuild() {
    report_.clear();
    KeySet dirty;
    for (auto &[id, feature] : features_) {
        const auto input = read_input(feature.dimension);
        if (feature.state == FeatureState::Dirty || input != feature.lastInput) {
            dirty.insert(graph_key(id));
        }
        feature.lastInput = input;
    }
    dirty.merge(graph_.affected(dirty));
    const auto order = graph_.topological_order(dirty);
    if (!order) {
        throw std::logic_error(order.error().message);
    }
    for (const auto &key : *order) {
        features_.at(FeatureId{key.object}).state = FeatureState::Dirty;
    }
    for (const auto &key : *order) {
        report_.push_back(execute(FeatureId{key.object}));
    }
}

const Geometry *FeatureModel::result(FeatureId id) const {
    const auto found = features_.find(id);
    if (found == features_.end() || found->second.state != FeatureState::Valid) {
        return nullptr;
    }
    return &*found->second.geometry;
}

const Geometry *FeatureModel::cached(FeatureId id) const {
    const auto found = features_.find(id);
    if (found == features_.end() || !found->second.geometry) {
        return nullptr;
    }
    return &*found->second.geometry;
}

const std::vector<FeatureStep> &FeatureModel::last_report() const { return report_; }
}
