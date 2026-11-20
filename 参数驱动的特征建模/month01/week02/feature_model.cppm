export module feature_model;

import std;
export import geometry;
export import month03.week04.engine;

export namespace cad {
using month03::week04::ObjectId;
using month03::week04::PropertyKey;
using month03::week04::SetResult;
using month03::week04::Update;

struct FeatureId {
    std::uint64_t value;
    auto operator<=>(const FeatureId &) const = default;
};

enum class FeatureState {
    Dirty,
    Valid,
    Failed,
};

std::string_view state_name(FeatureState state);

struct FeatureStep {
    FeatureId id;
    FeatureState state;
    std::size_t executions;
    std::string message;
};

class FeatureModel {
  public:
    FeatureModel();

    PropertyKey parameter(std::string name) const;
    FeatureId add_extrusion(RectangleProfile profile, PropertyKey height);
    FeatureId add_cut(RectangleProfile profile, PropertyKey depth, FeatureId base);
    SetResult set_parameters(const std::vector<Update> &changes);
    void rebuild();
    const Geometry *result(FeatureId id) const;
    const std::vector<FeatureStep> &last_report() const;

  private:
    struct Feature {
        RectangleProfile profile;
        PropertyKey dimension;
        std::optional<FeatureId> base = std::nullopt;
        FeatureState state = FeatureState::Dirty;
        std::optional<Geometry> geometry = std::nullopt;
    };

    FeatureId add(Feature feature);
    FeatureStep execute(FeatureId id);

    month03::week04::RecomputeEngine parameters_;
    ObjectId parameterId_;
    std::uint64_t nextId_ = 1;
    std::map<FeatureId, Feature> features_;
    month03::week04::DependencyGraph graph_;
    std::vector<FeatureStep> report_;
};
}
