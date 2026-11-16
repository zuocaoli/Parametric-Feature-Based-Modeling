# 表达式引擎与依赖图·第二月第 2 周：确定计算顺序

## 本周训练

- **承接与前置**：在[上周](../week01/README.md)的依赖图上加入合法顺序与循环拒绝；复习入度、队列/集合和 DFS 路径。
- **目标**：能检验每条边的前置先于目标；能报告真实闭环，并证明拒绝后原图保持。
- **独立练习**：先登记 A.area 再登记 A.width，关系仍为 length→width、length/width→area；用本周计算回调令 length=144、width=length/2、area=length*width。再尝试让 length 依赖 area，记录闭环并撤去该候选。
- **验收**：拓扑顺序保证 width 在 area 前，计算结果为 72/10368；新增循环被拒绝，旧图仍可排序。另在菱形图中核对汇聚节点只出现一次；完整引擎下周接入。

## 46. 为什么不能按创建顺序计算？

属性或公式的创建顺序不代表依赖顺序。先计算 area 再计算 width，会读到旧宽度；示例先得到 width=50、area=100。图按真实依赖安排 width 后 area，才能得到 50、5000。

代码：[顺序对照演示](main.cpp)。

## 47. 如何拓扑排序？

Kahn 算法记录每个节点的前置数量，零入度节点进入 ready；取出节点后减少各下游的计数，新零入度节点才可执行。ready 用有序集合使演示可重复。完整图包含输入节点，但计算器仅执行具有公式的目标；子集排序只统计子集内的边。

`topological_order(selected)` 开头的 `for (const auto &key : selected)` 负责初始化：遍历本次参与排序的所有节点，逐个统计它在 `selected` 内的直接前置数量，写入 `indegree`；计数为零的节点同时放入 `ready`，作为排序起点。后面的 `while` 才逐个取出就绪节点、更新下游计数并生成结果 `order`。矩形例子初始计数为 length=0、width=1、area=2，初始 ready 只有 length；取出 length 后变为 width=0、area=1，取出 width 后 area=0，最终得到 length → width → area。此函数只生成顺序，计算回调由调用方执行。

`main.cpp` 在确认排序成功后，遍历 `*order` 中的属性键，用 `calculations.find(key)` 查找对应的计算回调；找到才打印键并调用 `found->second()`，未找到就跳过。本例的 length 是已有值的输入，没有注册计算函数；width 和 area 则分别执行 `width=length/2`、`area=length*width`。因此完整拓扑顺序是 length → width → area，打印的计算顺序只有 `1.width 1.area`，最终得到 width=50、area=5000。图负责安排先后，回调表负责提供实际计算。

`indegree.emplace(key, count)` 保存当前节点的初始入度，供后续 `--indegree.at(child)` 更新剩余前置数量；每个选中节点都有记录。`if (count == 0) ready.insert(key)` 将无需等待任何选中前置的节点加入初始就绪集合。计数大于零的节点暂不进入 ready，等前置排入顺序、计数降至零时再加入；加入 ready 本身不执行属性计算。

`while` 每轮从 ready 取出并移除一个节点，再追加到 order，表示它已排入顺序。`dependents(key)` 返回直接依赖 key 的属性，这些属性就是这里所说的下游节点（child）；例如 width=length/2、area=length*width 时，length 的直接下游为 width 和 area。随后遍历这些属性，对选中的每个 child 将剩余入度减一：key 已排在前面，child 就少等待一个前置；减后为零才加入 ready，等待后续轮次处理。这个 for 只更新直接下游，间接影响由后续轮次逐层传递。`selected.contains(child) && ...` 保证只访问并更新选中节点的计数。这个过程只更新局部计数和结果，不修改依赖图；ready 为空时循环结束，是否排完全部节点由后续数量检查确认。

`dependencies(key)` 确实返回 `key` 依赖的属性，查询方向本身不决定图中箭头。为让拓扑顺序直接对应计算顺序，本实现约定边从前置指向目标（`parent → key`）。例如 width 依赖 length，记录为 `parents_[width] = {length}` 和 `children_[length] = {width}`，边方向是 length → width。因此这里统计的是 `key` 在选中子图中的**入度**；出度则是 `dependents(key)` 中选中下游的数量。若将箭头约定为依赖者 → 被依赖者，同一个计数就应称为出度。

这个方向要结合索引和遍历代码确认：`replace_dependencies()` 用 `children_[source].insert(target)` 登记下游，`dependents(source)` 读取该集合；`topological_order()` 处理 key 后沿 `dependents(key)` 减少 child 的入度，`find_cycle()` 也沿 `dependents(key)` 递归到 next。因此算法将 source → target 作为图的正向，`dependencies(key)` 则提供前置查询。

`selected.contains(parent)` 返回布尔值，加到计数中相当于加 1 或 0；子集外的前置不计数，调用方须保证其值已就绪。每取出一个节点，仅递减选中下游的计数。

`order.size()` 是已排入结果的节点数，`selected.size()` 是本次要排序的节点总数。`order.size() != selected.size()` 表示没有排完全部节点，在本算法中就是前者小于后者。此时 ready 已空，剩余节点仍有未排入的前置，无法继续；无环图总能继续找到零入度节点，因此这种阻塞说明选中子图存在循环。随后用 `find_cycle(selected)` 找出具体闭环，返回包含该路径的 `GraphError`；未排入结果的节点也可能只是受循环阻塞的下游，不一定全在环上。

代码：[topological_order()](dependencies.cpp)、[图接口](dependencies.cppm)。

## 48. 汇聚节点如何只计算一次？

菱形图 a → b/c → d 中，d 的计数必须经过 b 和 c 两次递减才能归零；集合不存重复边，节点只进入结果一次。验证时应核对每条前置边的先后关系，不能只核对一个碰巧正确的输出列表。

代码：[拓扑计数](dependencies.cpp)。

## 49. 如何识别直接自引用？

width → width 是长度为一条边的循环，拓扑无法完成，DFS 返回闭合路径 width → width。try_replace_dependencies 在候选图校验，失败不触碰正式图；不把表达式引擎与依赖图·第一月读取旧值的自引用规则带进自动依赖图。

代码：[try_replace_dependencies()/find_cycle()](dependencies.cpp)。

## 50. 如何报告多个属性的循环？

拓扑发现无法完成后，用 DFS 的当前路径和颜色寻找回边，返回一条具体闭环。`visit(key)` 是递归搜索函数，返回 true 表示已找到环，结果写入 cycle。color 的 0 表示未访问、1 表示仍在当前递归路径上、2 表示已完成探索；path 保存当前路径，不是所有已访问节点的列表。

进入 `visit(key)` 时先设置 `color[key] = 1` 并将 key 压入 path，然后沿 `dependents(key)` 搜索选中的直接下游。遇到 `color[next] == 1`，说明边 key → next 回到当前路径，形成闭环：从 path 中的 next 开始复制到末尾，再追加 next 使首尾相同。遇到未访问的 next 则递归 `visit(next)`，找到环后逐层返回 true；颜色为 2 的节点无需再次搜索，不能仅因节点曾被访问就判环。

每层递归都有自己的 key 参数，上层节点在递归返回前仍保持状态 1，因此多个节点可以同时为 1，`color[next] == 1` 不要求 `next == key`。例如当前 `path = {length, width, area}`、`key = area`，又遇到 `next = width`，虽然 next 与 key 不同，仍会提取出 `cycle = {width, area, width}`，前面的 length 不属于这个闭环。`next == key` 则是直接自引用的特殊情况。每一步都对应前置 → 目标的真实边，错误携带对象 ID 与属性键。

如果所有下游都未发现环，就用 `path.pop_back()` 退出当前路径，将 key 标为 2 并返回 false。这只表示从当前节点沿下游搜索未发现环；外层循环还会从其他未访问的选中节点继续搜索，找到第一条环后停止。

`try_replace_dependencies()` 的目的是在修改依赖关系时保护正式图：调用方把新公式引用的属性集合交给它，它先检查替换后的整张图是否无环；无环才提交，有环就拒绝并保留原图，同时返回具体闭环。这样，错误的公式修改不会破坏已有的计算顺序，调用方可以报告原因，让用户修正公式。

`main.cpp` 的 `graph.try_replace_dependencies(length, {width})` 尝试将 length 的完整依赖集合替换为仅含 width 的集合，即让 length 依赖 width，对应新增边 width → length。原图已有 length → width，候选图因此出现 length → width → length。函数先复制图，在副本上替换依赖并尝试拓扑排序；发现循环就返回错误，正式图保持原样。只有校验通过才交换两个索引、提交新图；此操作不执行计算回调，也不修改属性数值。

返回类型为 `std::expected<void, GraphError>`，成功时没有附带值，失败时携带错误。变量名 cyclic 只是本例的命名：`if (cyclic)` 判断的是“修改成功”，并不表示“存在循环”。本例故意制造循环，预期修改失败，因此若成功反而抛出 `cycle was accepted`；失败后读取 `cyclic.error().cycle`，输出 `1.length 1.width 1.length`。

`rejected cycle:` 意为“已拒绝的循环”，是 main.cpp 主动打印的拒绝原因。后面的路径来自失败结果中保存的候选图闭环，不是正式图当前存在的环。防止循环是阻止有环的修改提交到正式图；候选图仍需先检测出环，才能说明为什么拒绝。本例输出这行，正是拦截成功的预期结果；拒绝后再次对 graph 调用 `topological_order()`，仍能得到 length → width → area。

`auto candidate = *this` 创建当前图的独立副本，包含 parents_ 和 children_ 两个索引。后续依赖替换和校验都在 candidate 上进行，失败返回时局部副本自动销毁，正式图无需撤销操作；校验通过后才交换两个索引、提交修改。如果直接修改正式图，发现循环时就必须额外恢复原来的关系。因此这里用复制整张图的开销换取清晰的“先验证、后提交”流程；大型图的优化留到后续。

`parents_.swap(candidate.parents_)` 和 `children_.swap(candidate.children_)` 分别交换当前图与候选图的两个索引。交换前当前图保存旧关系、candidate 保存已验证的新关系；交换后当前图持有新关系、candidate 持有旧关系，函数结束时旧关系随局部副本一起销毁。parents_ 回答“当前属性依赖谁”，children_ 回答“谁依赖当前属性”，两者记录同一组边，必须都更新才能保持查询一致。swap 本身只交换数据，不检查循环；是否调用它由前面的校验分支决定。有环时 try_replace_dependencies 仍会被调用并执行检查，但 `if (!order)` 中的 return 会立即结束整个函数，不会继续执行后面的两行 swap；返回 main.cpp 后才打印拒绝原因。

代码：[GraphError](dependencies.cppm)、[find_cycle()](dependencies.cpp)。

## 单独打开与运行

用 IDE 打开本周目录，以 **Clang + CMake + clangd** 独立构建、运行和导航。在本周目录执行：

```bash
cmake --preset default
cmake --build --preset default
./build/demo
```

也可执行 `./run.sh` 完成上述三步。依赖版本、Qt 配置及测试入口见[本周工具说明](tools/README.md)；[CMakeLists.txt](CMakeLists.txt) 和 [CMakePresets.json](CMakePresets.json) 均在本周目录，全部产物位于本周 `build/`。

## 代码与运行

前置 Lexer/Parser 与依赖图是本周副本，无跨周源码依赖。main.cpp 用两条简短计算回调比较错误顺序与拓扑顺序；第三周才接入完整表达式求值器和属性写回。

环境：Linux、CMake 4.3.x、Ninja ≥ 1.11、Clang/clang-scan-deps/clangd 22.1.8、C++23；标准库为含 std 模块开发文件的 libstdc++ 16.2.1。依赖与 IDE 设置见[本周工具说明](tools/README.md)。以下命令均从本周目录运行：

```bash
cmake --preset default
cmake --build --preset default
./run.sh
clang-format --dry-run --Werror *.cpp *.cppm
```

默认输出：

```text
creation order: width=50, area=100
calculation order: 1.width 1.area
width=50, area=5000
rejected cycle: 1.length 1.width 1.length
```

