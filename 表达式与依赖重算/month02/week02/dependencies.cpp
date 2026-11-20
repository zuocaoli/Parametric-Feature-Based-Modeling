module month03.week02.dependencies;

import std;

namespace month03::week02 {
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

std::string key_text(const PropertyKey &key) { return std::to_string(key.object) + "." + key.name; }

// 遍历一棵表达式 AST，返回其中所有不重复的变量名
std::set<std::string> collect_variables(const Node &node) {
    std::set<std::string> names;
    collect(node, names);
    return names;
}

//
// 把一个变量名解析为具体的属性标识 PropertyKey
//
PropertyKey resolve_key(std::uint64_t owner, std::string_view variable, const VariableMap &aliases) {
    // 在 aliases 中查找 variable 项
    const auto found = aliases.find(variable);

    // 返回一个 PropertyKey，
    // 如果 aliases 中有 variable 就返回 variable 对应的 PropertyKey，
    // 否则返回 <owner, variable>。
    return found == aliases.end() ? PropertyKey{owner, std::string{variable}} : found->second;
}

//
// 把表达式中引用的变量名，转换成实际依赖的属性集合，供后续构建依赖图使用.
//
KeySet resolve_dependencies(const Node &node, std::uint64_t owner, const VariableMap &aliases) {
    KeySet result;
    for (const auto &name : collect_variables(node)) {
        // 收集语法树中的所有不重复变量名称，遍历这些变量名称（name），
        // 去 aliases 中查找 name 对应的 PropertyKey，
        // 找不到的时候就是使用 <owner, name>。
        result.insert(resolve_key(owner, name, aliases));
    }
    return result;
}

//
// 把 target 原来的依赖整体替换为新依赖，并同步维护两个方向的查询关系.
// - target：要修改这个的依赖属性。
// - dependencies：这个属性现在依赖的完整属性集合。
//
// 建立或更新一张 “有向图”，
// PropertyKey 是节点，source → target 是边，
// parents_ 和 children_ 保存同一张图的两个查询方向。
//
void DependencyGraph::replace_dependencies(const PropertyKey &target, const KeySet &dependencies) {

    //
    // 1. 清除旧的反向关系：从原来各个前置属性的 children_ 集合中删除 area
    //
    for (const auto &old : this->dependencies(target)) {
        //
        children_[old].erase(target);
        // 之前，target 是依赖 old 的，所以在 old 的 children_ 中有 target 这个记录。
        // 现在 target 不再依赖 old，所以要从 old 的 children_ 中删除 target。
    }

    //
    // 2. 把 target 的依赖集合，设置为 dependencies
    //
    parents_[target] = dependencies;

    //
    // std::map::try_emplace() 是 C++17 引入的成员函数，用于向 std::map中插入键值对，
    // 如果 Key 不存在，就插入；
    // 如果 Key已存在，就不执行插入，也不会覆盖原来的 Value，
    // 这里默认构造了 Value。
    children_.try_emplace(target); // <target, 谁依赖了target>, 先高一个空的放在这里，后面再填充

    // 为新的依赖建立反向记录，让每个依赖来源知道 “target 正在使用我”
    for (const auto &source : dependencies) {
        // 在 parents_ 中增加新的记录（如果还没有这条记录），但是这条记录的 Value 先空着。
        parents_.try_emplace(source); // <source, source依赖的属性>

        // target (PropertyKey) 依赖 source (PropertyKey)，
        // 所以把 target 放到这个 source 对应的 KeySet 中。
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
    // 取出 target 对应的 <PropertyKey, KeySet>
    const auto found = parents_.find(target);
    // 取出 KeySet
    return found == parents_.end() ? empty : found->second;
}

//
// 谁依赖 source
//
const KeySet &DependencyGraph::dependents(const PropertyKey &source) const {
    const auto found = children_.find(source);
    return found == children_.end() ? empty : found->second;
}

//
// 收集依赖图中的全部节点，返回一个去重后的集合.
//
KeySet DependencyGraph::nodes() const {
    //
    KeySet result;
    //
    for (const auto &[key, parents] : parents_) {
        result.insert(key);
        result.insert(parents.begin(), parents.end());
    }
    return result;
}

//
// 对整张依赖图的所有节点进行拓扑排序，返回计算顺序或错误.
//
std::expected<std::vector<PropertyKey>, GraphError> DependencyGraph::topological_order() const {
    //
    return topological_order(nodes());
}

std::expected<std::vector<PropertyKey>, GraphError> DependencyGraph::topological_order(const KeySet &selected) const {
    //
    std::map<PropertyKey, std::size_t> indegree;
    //
    KeySet ready;

    //
    // 初始化每个节点的入度，并找出拓扑排序的起点.
    // selected 表示本次参与排序的属性集合, 对于其中每个 key，循环做三件事:
    //    1. 统计前置数量： 遍历 dependencies(key)，只把同样位于 selected 中的依赖计入 count。
    //    2. 保存入度：  indegree.emplace(key, count) 记录这个节点需要等待多少个前置节点。
    //    3. 收集起点：  如果 count == 0，就把它放入 ready，表示它可以首先排入结果。
    //
    for (const auto &key : selected) {
        std::size_t count = 0;

        // 遍历 key 依赖的属性，统计key节点的入度
        for (const auto &parent : dependencies(key)) {
            // std::set::contains 是 C++20 引入的成员函数，用于检查集合中是否存在特定键,
            // 它返回 bool 值，存在为 true，否则为 false,
            // true 转换为 1, false 转换为 0.
            count += selected.contains(parent);
        }

        // 记录当前节点的入度： <节点，该节点的入度>
        indegree.emplace(key, count);

        // 找出可以首先处理的节点：
        // 入度为零，说明它无需等待选中的前置节点（不依赖其他属性），就把它加入就绪集合 ready，供后面的 while 取出。
        if (count == 0) {
            ready.insert(key);
        }
    }

    //
    // 拓扑排序的主体：不断取出就绪节点，放入 order，并让它的下游节点逐步变为就绪。
    //
    std::vector<PropertyKey> order;
    while (!ready.empty()) {

        // 取集合中的第一个元素，复制到局部变量 key
        const auto key = *ready.begin();
        // 移除
        ready.erase(ready.begin());
        // 添加到排序
        order.push_back(key);

        //
        // key 已排入顺序后，更新依赖它的下游节点，并找出新就绪的节点.
        for (const auto &child : dependents(key)) {
            // 遍历所有直接依赖 key 的属性，每次取出一个，叫作 child
            //
            // 1. 只处理本次参与排序的节点;
            // 2. key 已排在前面，child 就少等待一个前置，因此剩余入度减一，入度为0即可添加到 ready.
            if (selected.contains(child) && --indegree.at(child) == 0) {
                ready.insert(child);
            }
        }
    }

    //
    // 如果 while 已经结束，但数量仍不相等，说明：ready 已空，却还有节点在等待前置节点，排序卡住了.
    // 在这个算法中，这意味着存在循环依赖.
    if (order.size() != selected.size()) {
        // 还有选中的节点没有排入排序结果
        return std::unexpected(GraphError{"dependency cycle", find_cycle(selected)});
    }
    return order;
}

std::vector<PropertyKey> DependencyGraph::find_cycle(const KeySet &selected) const {

    //
    std::map<PropertyKey, int> color; // 0 未访问，1 在当前路径上，2 已完成

    //
    std::vector<PropertyKey> path;
    std::vector<PropertyKey> cycle;

    //
    // 定义了递归函数 visit(key)，作用是沿依赖图搜索一条循环路径，找到后存入 cycle，并返回 true，
    // [&] 让函数按引用使用外部的 color、path、cycle 等变量，各层递归共用这些记录，
    std::function<bool(const PropertyKey &)> visit = [&](const PropertyKey &key) {
        //
        // 从 key 开始搜索
        color[key] = 1;
        path.push_back(key);

        // 遍历所有依赖 key 属性,
        // 从当前 key 出发，在 selected 范围内沿下游搜索，寻找一条循环依赖路径
        for (const auto &next : dependents(key)) {

            // 如果 next 不在选中的节点集合中，则跳过。
            if (!selected.contains(next)) {
                continue;
            }

            // color[next] == 1 表示 next 已经在路径中了
            //
            // color 的用途：
            //
            // 例如搜索 width → area → width：
            // 1. 调用 visit(width)，此时 key 是 width：
            //    color[key] = 1;    实际就是 color[width] = 1
            //
            // 2. 发现下游 area，递归调用 visit(area)。新一层的 key 是 area：
            //    color[key] = 1;    实际就是 color[area] = 1
            //    上一层还没结束，color[width] 仍然为 1。
            //
            // 3. 在 visit(area) 中，又遇到下游 width，此时 next 是 width：
            //    color[next] == 1   实际检查 color[width] == 1
            //
            if (color[next] == 1) {

                // 在 path 中查找 next，并把它所在位置的迭代器保存到 begin
                const auto begin = std::find(path.begin(), path.end(), next);

                // 把 path 中从 begin 开始的位置到末尾的元素，复制到 cycle，替换cycle原来的内容
                cycle.assign(begin, path.end());
                // 此时 cycle 的开头就是 next.

                // 把 next 追加到 cycle 的末尾，让闭环首尾相同
                // cycle 用来保存找到的具体闭环，例如 [width, area, width]，随错误返回，帮助定位哪些属性形成了循环
                cycle.push_back(next);

                // 已经找到了循环依赖，具体闭环已保存到 cycle
                return true;
            }

            // 如果 next 还没有被访问过，并且递归访问 next 找到了环，则返回 true。
            if (color[next] == 0 && visit(next)) {
                // 把递归深处“找到环”的结果向上传递
                return true;
            }
        }

        // 移除 path 中的最后一个节点，也就是当前的 key,
        // 例如，当前正在搜索 area：
        // 执行前：path = [length, width, area]
        // 执行后：path = [length, width]
        // 当 area 的所有下游都检查完、没有找到环时，就将它移出当前路径，准备返回上一层继续搜索。
        // 这一步称为回溯，让 path 始终只记录当前仍在探索的递归路径。
        path.pop_back();

        // 将当前 key 标记为已访问过，表示该节点的所有下游都已检查完毕，且未找到环。
        color[key] = 2;

        // 返回 false，表示从当前 key 出发没有找到环。
        return false;
    };

    // 为深度优先搜索选择起点，直到找到第一条环，或者检查完所有选中节点
    for (const auto &key : selected) {
        if (color[key] == 0 && visit(key)) {
            break; // 找到第一条环
        }
    }

    // 返回找到的循环依赖路径，如果没有找到环，则返回空向量。
    return cycle;

    //
    // 深度优先搜索（DFS）：
    // 先沿一条分支不断深入，探索完这条分支后，再退回去搜索其他分支。
    // 本函数用 DFS 寻找具体的循环依赖。
    //
    // 示例依赖图，每个字母代表一个属性，箭头从前置指向依赖它的属性：
    //
    //        [A]
    //         |
    //     +---+---+
    //     |       |
    //     v       v
    //    [B]     [C]
    //     |
    //     v
    //    [D]
    //
    // 边：A -> B，A -> C，B -> D。
    //
    // 假设四个节点都在 selected 中，从 A 开始，先选择 B，搜索过程如下：
    // 1. 进入 A：将 A 标记为 1，path = [A]。
    // 2. 深入 B：调用 visit(B)，将 B 标记为 1，path = [A, B]。
    // 3. 继续深入 D：调用 visit(D)，将 D 标记为 1，path = [A, B, D]。
    // 4. D 没有下游：执行 path.pop_back()，将 D 标记为 2，返回 B。
    //    此时 path = [A, B]。
    // 5. B 的下游已经搜完：移除路径中的 B，将 B 标记为 2，返回 A。
    //    此时 path = [A]。
    // 6. A 继续检查 C：调用 visit(C)，探索另一条分支。
    //
    // 首次访问节点的顺序：A → B → D → C。
    // “深度优先”体现在：进入 B 后，先把 B 下面的 D 搜完，才返回 A 检查 C。
    //
    // 递归调用 visit(next) 时，当前这一层暂停，进入下一层。
    // 下一层完成后，才回到当前层，继续执行 for 循环。
    //
}

//
// 把 length 的依赖集合替换为 {width}，让长度依赖宽度, 这改变了依赖关系，改变可能导致依赖图中有环
// target: 要修改哪个属性，
// dependencies: 这个属性现在依赖哪些属性
//
std::expected<void, GraphError> DependencyGraph::try_replace_dependencies(const PropertyKey &target, const KeySet &dependencies) {

    // 取得当前图对象，得到独立的一个副本，用于尝试修改依赖关系。
    // 先在副本上试改，确认没有循环后，再修改原图。
    auto candidate = *this;

    // 更新属性依赖
    candidate.replace_dependencies(target, dependencies);
    // 重建属性拓扑谱图
    const auto order = candidate.topological_order();
    if (!order) {
        // 如果拓扑排序失败，说明存在循环依赖，返回错误。
        return std::unexpected(order.error());
    }

    // 把已经校验通过的新依赖关系从副本提交到当前图
    parents_.swap(candidate.parents_);
    children_.swap(candidate.children_);
    return {};
}
} // namespace month03::week02
