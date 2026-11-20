module feature_model;

import std;

namespace cad {
using month03::week04::ComputeState;

namespace {

//
// 从重算引擎中提取属性值
std::optional<double> numeric_value(const std::optional<month03::week04::Value> &value) {

    if (!value) {
        return std::nullopt;
    }

    //
    if (const auto *number = std::get_if<double>(&*value)) {
        return *number;

        //
        // &*value 是两个一元运算符连用，等价于 &(*value)。
        //  1. *value 取出 std::optional 里保存的 Value，得到一个引用。
        //  2. & 对这个 Value 取地址，得到 const Value*。
        //

        //
        // std::get_if 是 C++17 引入的、用于 std::variant 的非抛出式访问器。
        // 它不抛异常，而是通过返回指针（失败时为 nullptr）来安全地查询 variant 当前是否持有目标类型。
        // std::get_if 的参数是指向 std::variant 的指针。
        //
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

    features_.emplace(id, std::move(feature));

    return id;
}

FeatureId FeatureModel::add_extrusion(RectangleProfile profile, PropertyKey height) {
    return add({profile, std::move(height)}); //
}

SetResult FeatureModel::set_parameters(const std::vector<Update> &changes) {
    report_.clear();
    const auto status = parameters_.set_many(parameterId_, changes);
    if (status == SetResult::Changed || status == SetResult::Unchanged) {
        rebuild();
    }
    return status;
}

FeatureStep FeatureModel::execute(FeatureId id) {

    // 根据ID取出 Feature
    auto &feature = features_.at(id);

    // 让 execute() 的成功和失败分支共用同一段收尾代码
    auto finish = [&](FeatureState state, std::size_t executions, std::string message) {
        feature.state = state;
        return FeatureStep{id, state, executions, std::move(message)};
    };

    // 拿到特征值
    const auto size = numeric_value(parameters_.value(feature.dimension));
    if (!size) {
        return finish(FeatureState::Failed, 0, "no valid numeric parameter: " + feature.dimension.name);
    }

    // 
    auto built = extrude(feature.profile, *size);
    if (!built) {
        return finish(FeatureState::Failed, 1, std::move(built.error()));
    }

    
    feature.geometry = std::move(*built);

    return finish(FeatureState::Valid, 1, "shape rebuilt");
}

void FeatureModel::rebuild() {

    // 每次调用 rebuild()，模型先清空旧报告，再为每个执行过的特征追加一条 FeatureStep
    report_.clear();

    //
    for (const auto &[id, feature] : features_) {
        report_.push_back(execute(id));
    }
}

const Geometry *FeatureModel::result(FeatureId id) const {
    const auto found = features_.find(id);
    if (found == features_.end() || found->second.state != FeatureState::Valid) {
        return nullptr;
    }
    return &*found->second.geometry;
}

const std::vector<FeatureStep> &FeatureModel::last_report() const { return report_; }
}
