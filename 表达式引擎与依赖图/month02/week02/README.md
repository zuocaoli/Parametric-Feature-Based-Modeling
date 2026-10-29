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

代码：[topological_order()](dependencies.cpp)、[图接口](dependencies.cppm)。

## 48. 汇聚节点如何只计算一次？

菱形图 a → b/c → d 中，d 的计数必须经过 b 和 c 两次递减才能归零；集合不存重复边，节点只进入结果一次。验证时应核对每条前置边的先后关系，不能只核对一个碰巧正确的输出列表。

代码：[拓扑计数](dependencies.cpp)。

## 49. 如何识别直接自引用？

width → width 是长度为一条边的循环，拓扑无法完成，DFS 返回闭合路径 width → width。try_replace_dependencies 在候选图校验，失败不触碰正式图；不把表达式引擎与依赖图·第一月读取旧值的自引用规则带进自动依赖图。

代码：[try_replace_dependencies()/find_cycle()](dependencies.cpp)。

## 50. 如何报告多个属性的循环？

拓扑发现无法完成后，用 DFS 的当前路径和颜色寻找回边，将路径中被再次遇到的节点至末尾复制，再追加该节点形成闭环。每一步都对应前置 → 目标的真实边，错误携带对象 ID 与属性键。候选图用副本验证、交换提交，代价是每次维护复制图；大型图的优化留到后续。

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

