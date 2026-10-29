export module month04.week04.dependencies;

import std;
export import month04.week04.parser;

export namespace month04::week04 {
struct PropertyKey {
    std::uint64_t object;
    std::string name;
    auto operator<=>(const PropertyKey &) const = default;
};

using KeySet = std::set<PropertyKey>;
using VariableMap = std::map<std::string, PropertyKey, std::less<>>;

std::string key_text(const PropertyKey &key);
std::set<std::string> collect_variables(const Node &node);
PropertyKey resolve_key(std::uint64_t owner, std::string_view variable,
                        const VariableMap &aliases = {});
KeySet resolve_dependencies(const Node &node, std::uint64_t owner, const VariableMap &aliases = {});

struct GraphError {
    std::string message;
    std::vector<PropertyKey> cycle; // 首尾为同一节点，沿图的边方向形成闭合路径。
};

class DependencyGraph {
public:
    // replace 只维护结构；正式提交应使用 try_replace_dependencies 校验无环。
    void replace_dependencies(const PropertyKey &target, const KeySet &dependencies);
    void remove_formula(const PropertyKey &target);
    const KeySet &dependencies(const PropertyKey &target) const;
    const KeySet &dependents(const PropertyKey &source) const;
    KeySet nodes() const;
    KeySet affected(const KeySet &sources) const;
    std::expected<std::vector<PropertyKey>, GraphError> topological_order() const;
    std::expected<std::vector<PropertyKey>, GraphError>
    topological_order(const KeySet &selected) const;
    std::expected<void, GraphError> try_replace_dependencies(const PropertyKey &target,
                                                             const KeySet &dependencies);

private:
    std::map<PropertyKey, KeySet> parents_;
    std::map<PropertyKey, KeySet> children_;
    std::vector<PropertyKey> find_cycle(const KeySet &selected) const;
};
} // namespace month04::week04
