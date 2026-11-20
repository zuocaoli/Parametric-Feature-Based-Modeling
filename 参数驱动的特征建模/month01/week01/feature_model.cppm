export module feature_model;

import std;
export import geometry;
export import month03.week04.engine;

export namespace cad {
using month03::week04::ObjectId;
using month03::week04::PropertyKey;
using month03::week04::SetResult;
using month03::week04::Update;

/**
 * FeatureId 是一个特征在所属 FeatureModel 中的编号，
 * 把 std::uint64_t 编号包装成独立的类型，编号存在 value 里。
 * 模型从 1 开始分配编号，并用 FeatureId 作为 features_ 的查找键。
 */
struct FeatureId {

    std::uint64_t value;

    auto operator<=>(const FeatureId &) const = default;
};

/**
 * 单个特征当前的构建状态
 */
enum class FeatureState {
    Dirty,  // 特征刚登记，等待构建.
    Valid,  // 构建成功，可以通过 model.result(id) 取得几何结果.
    Failed, // 构建失败，例如读不到有效的高度值，或拉伸操作失败。具体原因记录在 FeatureStep::message 中.
};

std::string_view state_name(FeatureState state);

/**
 * 一个特征在本次重建中的执行记录:
 * rebuild() 每处理一个特征，就把一条 FeatureStep 加入 report_ 。
 */
struct FeatureStep {
    FeatureId id;           // 被处理的特征编号，
    FeatureState state;     // 处理后的状态，例如 Valid 或 Failed，
    std::size_t executions; // 本次调用几何构建函数的次数。读不到有效高度时就是 0，调用了 extrude() 就是 1，无论拉伸是否成功。
    std::string message;    // 结果说明，例如 "shape rebuilt"，失败时则记录原因。
};

//
// 一个完整的参数驱动特征模型。
// 它管理模型参数、特征定义和生成的几何，并负责在参数更新后重新构建几何。
// 一个 FeatureModel 可以管理多个特征，每个特征用 FeatureId 标识。
class FeatureModel {
  public:
    FeatureModel();

    /**
     * 根据参数名构造一个 PropertyKey，也就是参数的查找键。这个键由模型的参数对象 ID 和参数名组成。
     * name 模型参数名称
     */
    PropertyKey parameter(std::string name) const;

    /**
     * 向模型登记一个矩形拉伸特征，并返回它的 FeatureId。
     * - profile 是拉伸用的二维矩形轮廓，包含位置和尺寸。
     * - height 是高度属性的查找键，由参数对象 ID 和属性名组成。模型在每次重建时通过这个键读取当前高度。
     */
    FeatureId add_extrusion(RectangleProfile profile, PropertyKey height);

    SetResult set_parameters(const std::vector<Update> &changes);

    ///
    void rebuild();

    const Geometry *result(FeatureId id) const;

    const std::vector<FeatureStep> &last_report() const;

  private:
    /// 保存单个矩形拉伸特征的数据结构，它保存拉伸的输入、构建状态和几何结果。
    struct Feature {
        RectangleProfile profile;                        // 草图。
        PropertyKey dimension;                           // 特征(文档的一个属性)，本节课是长度。
        FeatureState state = FeatureState::Dirty;        // 构建状态： 构建是从矩形轮廓和当前高度生成三维实体，state 记录的是这件事进行到了哪一步。
        std::optional<Geometry> geometry = std::nullopt; // 构建生成的三维实体，初始为 std::nullopt，拉伸成功后存入生成的实体。

        //
        // std::optional<T> 表示一个可能不存在的 T 值。
        // 它内部存储一个 bool（是否含值）加一块对齐的存储区（存 T）。
        // 语义上等价于"要么有一个 T，要么什么都没有"。
        //
    };

    FeatureId add(Feature feature);

    FeatureStep execute(FeatureId id);

    /// FeatureModel 内置的参数重算引擎
    month03::week04::RecomputeEngine parameters_;

    ///
    ObjectId parameterId_;

    std::uint64_t nextId_ = 1;

    /// 草图和特征以及其ID
    std::map<FeatureId, Feature> features_;

    /// 保存 FeatureModel 最近一次重建的执行报告。每次调用 rebuild()，模型先清空旧报告，再为每个执行过的特征追加一条 FeatureStep
    std::vector<FeatureStep> report_;
};
}
