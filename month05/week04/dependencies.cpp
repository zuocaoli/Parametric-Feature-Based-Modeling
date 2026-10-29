module month05.week04.dependencies;

import std;

namespace month05::week04 {
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

std::set<std::string> collect_variables(const Node &node) {
    std::set<std::string> names;
    collect(node, names);
    return names;
}

PropertyKey resolve_key(std::uint64_t owner, std::string_view variable,
                        const VariableMap &aliases) {
    const auto found = aliases.find(variable);
    return found == aliases.end() ? PropertyKey{owner, std::string{variable}} : found->second;
}

KeySet resolve_dependencies(const Node &node, std::uint64_t owner, const VariableMap &aliases) {
    KeySet result;
    for (const auto &name : collect_variables(node)) {
        result.insert(resolve_key(owner, name, aliases));
    }
    return result;
}

void DependencyGraph::replace_dependencies(const PropertyKey &target, const KeySet &dependencies) {
    for (const auto &old : this->dependencies(target)) {
        children_[old].erase(target);
    }
    parents_[target] = dependencies;
    children_.try_emplace(target);
    for (const auto &source : dependencies) {
        parents_.try_emplace(source);
        children_[source].insert(target);
    }
}

void DependencyGraph::remove_formula(const PropertyKey &target) {
    replace_dependencies(target, {}); // 保留别人对 target 的引用。
}

const KeySet &DependencyGraph::dependencies(const PropertyKey &target) const {
    const auto found = parents_.find(target);
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

std::expected<std::vector<PropertyKey>, GraphError> DependencyGraph::topological_order() const {
    return topological_order(nodes());
}

KeySet DependencyGraph::affected(const KeySet &sources) const {
    KeySet seen = sources, result;
    std::vector<PropertyKey> pending(sources.begin(), sources.end());
    for (std::size_t cursor = 0; cursor < pending.size(); ++cursor) {
        for (const auto &child : dependents(pending[cursor])) {
            if (seen.insert(child).second) {
                result.insert(child);
                pending.push_back(child);
            }
        }
    }
    return result; // 下游闭包不包含源；公式绑定会另行标记目标自身。
}

std::expected<std::vector<PropertyKey>, GraphError>
DependencyGraph::topological_order(const KeySet &selected) const {
    std::map<PropertyKey, std::size_t> indegree;
    KeySet ready;
    for (const auto &key : selected) {
        std::size_t count = 0;
        for (const auto &parent : dependencies(key)) {
            count += selected.contains(parent);
        }
        indegree.emplace(key, count);
        if (count == 0) {
            ready.insert(key);
        }
    }
    std::vector<PropertyKey> order;
    while (!ready.empty()) {
        const auto key = *ready.begin();
        ready.erase(ready.begin());
        order.push_back(key);
        for (const auto &child : dependents(key)) {
            if (selected.contains(child) && --indegree.at(child) == 0) {
                ready.insert(child);
            }
        }
    }
    if (order.size() != selected.size()) {
        return std::unexpected(GraphError{"dependency cycle", find_cycle(selected)});
    }
    return order;
}

std::vector<PropertyKey> DependencyGraph::find_cycle(const KeySet &selected) const {
    std::map<PropertyKey, int> color; // 0 未访问，1 在当前路径上，2 已完成。
    std::vector<PropertyKey> path, cycle;
    std::function<bool(const PropertyKey &)> visit = [&](const PropertyKey &key) {
        color[key] = 1;
        path.push_back(key);
        for (const auto &next : dependents(key)) {
            if (!selected.contains(next)) {
                continue;
            }
            if (color[next] == 1) {
                const auto begin = std::find(path.begin(), path.end(), next);
                cycle.assign(begin, path.end());
                cycle.push_back(next);
                return true;
            }
            if (color[next] == 0 && visit(next)) {
                return true;
            }
        }
        path.pop_back();
        color[key] = 2;
        return false;
    };
    for (const auto &key : selected) {
        if (color[key] == 0 && visit(key)) {
            break;
        }
    }
    return cycle;
}

std::expected<void, GraphError>
DependencyGraph::try_replace_dependencies(const PropertyKey &target, const KeySet &dependencies) {
    auto candidate = *this;
    candidate.replace_dependencies(target, dependencies);
    const auto order = candidate.topological_order();
    if (!order) {
        return std::unexpected(order.error());
    }
    parents_.swap(candidate.parents_);
    children_.swap(candidate.children_);
    return {};
}
} // namespace month05::week04
