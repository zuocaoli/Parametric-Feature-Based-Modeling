export module month03.week04.dependencies;

import std;
export import month03.week04.parser;

export namespace month03::week04 {
struct PropertyKey {
    std::uint64_t object; // 文档对象
    std::string name;     // 对象的那个属性
    auto operator<=>(const PropertyKey &) const = default;
};

using KeySet = std::set<PropertyKey>;
using VariableMap = std::map<std::string, PropertyKey, std::less<>>;

std::string key_text(const PropertyKey &key);
std::set<std::string> collect_variables(const Node &node);
PropertyKey resolve_key(std::uint64_t owner, std::string_view variable,
                        const VariableMap &aliases = {});

// 公式的语法树和属性定位信息，返回公式直接引用的属性集合
KeySet resolve_dependencies(const Node &node, std::uint64_t owner, const VariableMap &aliases = {});

struct GraphError {
    std::string message;
    std::vector<PropertyKey> cycle; // 首尾为同一节点，沿图的边方向形成闭合路径。
};

//
// 维护属性之间的依赖关系，为增量重算确定“哪些属性需要重算、按什么顺序重算”，并检查循环依赖。
// 图中的每个节点代表一个属性，由 PropertyKey{对象ID, 属性名} 唯一标识，因此也能表达不同对象之间的依赖。
//
// 在整体分工中，
// DependencyGraph 类提供依赖关系和排序结果；
// RecomputeEngine 管理待重算集合、批次和执行时机；
// 求值器执行具体公式。
//
// 将依赖管理独立成类，让这些职责可以分别理解和维护。
class DependencyGraph {
public:
    //
    // 用于将目标属性的直接依赖替换为指定集合，同时更新“谁依赖谁”的查询记录。
    // replace 只维护结构；正式提交应使用 try_replace_dependencies 校验无环。
    void replace_dependencies(const PropertyKey &target, const KeySet &dependencies);

    //
    void remove_formula(const PropertyKey &target);
    // 查询 target 依赖哪些属性
    const KeySet &dependencies(const PropertyKey &target) const;

    // 直接依赖当前属性的所有属性组成的集合
    const KeySet &dependents(const PropertyKey &source) const;

    //
    KeySet nodes() const;

    //
    // 【确定重算范围】
    // 返回所有直接或间接受影响的下游属性:
    // graph.affected({length}); 就是 {width, area}，
    // 返回结果不包含输入的 length 本身。这样引擎就能把受影响的公式加入 dirty_，无关属性无需参与本次重算。
    KeySet affected(const KeySet &sources) const;

    //
    // 【确定计算顺序】
    //  对于依赖图中的节点进行拓扑排序，返回计算顺序或错误。
    std::expected<std::vector<PropertyKey>, GraphError> topological_order() const; // 调用 topological_order(const KeySet &selected) const
    std::expected<std::vector<PropertyKey>, GraphError>
    topological_order(const KeySet &selected) const;

    //
    // 【防止提交循环依赖】
    // 例如已有绑定关系： area  = length * width，然后再绑定 length = area / 2，由于 area 已经依赖 length，就会形成循环，
    // try_replace_dependencies() 会在图的副本上修改并检查；发现环时返回 GraphError，保留原图。
    // 注意，普通的 replace_dependencies() 只维护关系，不保证修改后无环。
    //
    std::expected<void, GraphError> try_replace_dependencies(const PropertyKey &target,
                                                             const KeySet &dependencies);

private:
    //
    // 【类内部维护两张表】：
    // |     成员    |    表达的关系    |               当前例子               |
    // |------------|-----------------|------------------------------------|
    // | parents_   | 某个属性直接依赖谁 | parents_[area] = {length, width}   |
    // | children_  | 谁直接依赖某个属性 | children_[length] = {width, area}  |
    //
    // 保存两个方向，是为了方便回答两类问题：
    // - 计算 area 前，需要哪些输入？
    // - length 改变后，需要更新哪些属性？
    // replace_dependencies() 会同步维护这两张表。公式修改后，它先移除旧关系，再建立新关系。
    std::map<PropertyKey, KeySet> parents_;
    std::map<PropertyKey, KeySet> children_;

    //
    std::vector<PropertyKey> find_cycle(const KeySet &selected) const;
};
} // namespace month03::week04
