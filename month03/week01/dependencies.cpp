module month03.week01.dependencies;

import std;

namespace month03::week01 {
namespace {
void collect(const Node &node, std::set<std::string> &names) {
    if (node.kind == NodeKind::Variable) {
        names.insert(node.token.text);
    }
    if (node.left) {
        collect(*node.left, names);
    }
    if (node.right) {
        collect(*node.right, names);
    }
}

const KeySet empty;
} // namespace

std::string key_text(const PropertyKey &key) {
    return std::to_string(key.object) + "." + key.name;
}

// 遍历一棵表达式 AST，返回其中所有不重复的变量名
std::set<std::string> collect_variables(const Node &node) {
    std::set<std::string> names;
    collect(node, names);
    return names;
}

//
// 把一个变量名解析为具体的属性标识 PropertyKey
//
PropertyKey resolve_key(std::uint64_t owner, std::string_view variable,
                        const VariableMap &aliases) {
                            
    const auto found = aliases.find(variable);
    return found == aliases.end() ? PropertyKey{owner, std::string{variable}} : found->second;
}

//
// 把表达式中引用的变量名，转换成实际依赖的属性集合，供后续构建依赖图使用.
//
KeySet resolve_dependencies(const Node &node, std::uint64_t owner, const VariableMap &aliases) {

    //
    KeySet result;

    for (const auto &name : collect_variables(node)) {
        // 收集语法树中的所有变量
        result.insert(resolve_key(owner, name, aliases));
    }
    return result;
}

//
// 把 target 原来的依赖，替换为传入的新依赖，并同步更新反向查询关系
//
// target： 要修改谁的依赖。
// dependencies ： 它现在要依赖哪些属性。
//
void DependencyGraph::replace_dependencies(const PropertyKey &target, const KeySet &dependencies) {

    for (const auto &old : this->dependencies(target)) {

        // 通过 dependencies(target) 取出这个 target 依赖的所有属性（KeySet），
        // 把这个 KeySet 中的每一个（PropertyKey）都取出来。
        // 目的： 检查 KeySet 中有没有依赖这个 target 的，如果有就清除这个依赖关系。
        //
        // children_[old]： 拿到依赖这个 PropertyKey （old）的所有属性（KeySet），
        // 从这个 KeySet 中找到 target 并删除它。
        children_[old].erase(target);
    }

    // 把 target 的依赖集合，设置为 dependencies
    parents_[target] = dependencies;

    // std::map::try_emplace() 是 C++17 引入的成员函数，用于向 std::map 中插入键值对，
    // 如果 Key 不存在，就插入；如果 Key 已存在，就不执行插入，也不会覆盖原来的 Value，这里默认构造了 Value。
    children_.try_emplace(target);

    // 为新的依赖建立反向记录，让每个依赖来源知道“target 正在使用我”
    for (const auto &source : dependencies) {

        // 在 parents_ 增加新的记录（如果还没有这条记录），但是这条记录的Value先空着。
        parents_.try_emplace(source);

        // 这个 source (PropertyKey) 依赖 target (PropertyKey), 所以把 target 放到这个 source 对应的 KeySet 中的
        children_[source].insert(target);
    }
}

void DependencyGraph::remove_formula(const PropertyKey &target) {
    replace_dependencies(target, {}); // 保留别人对 target 的引用。
}

//
// target 这个属性依赖哪些属性
//
const KeySet &DependencyGraph::dependencies(const PropertyKey &target) const {

    // 返回一个 <PropertyKey, KeySet>
    const auto found = parents_.find(target);
    // 取出 KeySet
    return found == parents_.end() ? empty : found->second;
}

const KeySet &DependencyGraph::dependents(const PropertyKey &source) const {
    const auto found = children_.find(source);
    return found == children_.end() ? empty : found->second;
}

KeySet DependencyGraph::nodes() const {
    KeySet result;
    for (const auto &[key, parents] : parents_) {
        result.insert(key);
        result.insert(parents.begin(), parents.end());
    }
    return result;
}
} // namespace month03::week01
