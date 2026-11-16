# 表达式引擎与依赖图·第二月第 4 周：维护与错误恢复

## 本周训练

- **承接与前置**：[上周](../week03/README.md)已有增量重算，本周为失败与删除加入有效状态和恢复；区分正式输入、最近缓存与安全读取。
- **目标**：能推导 Failed、Blocked、MissingReference 的传播；能在保留无关分支计算的同时修复失败链。
- **独立练习**：将 A.length 设为 144、width 绑定为 `length/(segments-1)`、area 绑定为 `length*width`，保留独立 B；先令 segments=1 再改为 3，逐步记录状态、安全读取和执行次数。
- **验收**：除零时 width Failed、area Blocked，安全读取都不可用，B 仍有效；修复后 A 为 72/10368，各目标本轮最多执行一次，缓存碰巧相同也恢复 Valid。

## 月末综合验收与衔接

独立完成 A 的自动重算与上述错误恢复，再把 B.width 的 `source_length/2` 显式映射到 A.length；B.length=30 时 area 为 2160。删除 A 后核对 B.width MissingReference、B.area Blocked；新建同名 A 使用新 ID，必须显式重绑才能恢复。另尝试循环公式，验证旧绑定和图保持。

解释单向依赖图、脏标记和批次各自职责。模型已有自动计算，但删除后仍需手工修复、合法编辑也无法整体撤回；[文档与属性系统·第二月](../../../文档与属性系统/month02/week01/README.md)将通过命令、事务和历史恢复用户操作。

## 56. 如何替换或解除公式依赖？

新绑定先解析、确认数值引用，再在候选图检查循环，成功才提交文本、AST、映射与图。随后把目标及下游标脏并自动计算；常量也标记目标。解绑移除旧前置边，保留下游引用和当前数值，将该属性恢复为可编辑的普通输入。此设计复制图，换取明确的拒绝边界，不提供整轮事务回滚。

代码：[bind()/unbind()](engine.cpp)、[候选图](dependencies.cpp)。

## 57. 循环或无效引用如何保留原状态？

解析错误、未知对象/属性、非数值引用、直接或跨对象循环都在提交前拒绝；旧公式、映射、依赖、数值与计算状态保持。图与求值使用同一份 VariableMap，未映射名称按本对象实际属性键查找。语法正确但除零或违反尺寸校验的公式可以绑定，计算时进入 Failed，不把运行失败误认为绑定回滚。

代码：[bind() 校验](engine.cpp)、[EngineError](engine.cppm)、[GraphError](dependencies.cppm)。

## 58. 对象删除后如何标识引用失效？

先沿旧图收集下游，再删除对象、它自己的绑定与订阅。其他公式保留指向旧 ID 的引用，直接使用者为 MissingReference，其下游为 Blocked；不静默丢边或改绑同名对象。对象 ID 单调分配且不复用，新对象必须显式重新绑定。订阅连接弱引用状态，发布者删除后断开仍安全。

代码：[erase()/create_rectangle()](engine.cpp)。

## 59. 如何阻止使用失败旧值，又继续无关分支？

状态分 Valid、Dirty、Failed、Blocked、MissingReference。stored_value 只用于查看缓存，value 仅返回 Valid 的值。重算前先检查前置：缺失引用或无效前置直接记录阻塞而不执行 AST；执行失败保留旧缓存并标 Failed，不终止整个排序循环。独立分支仍然执行，失败计数 1，阻塞与预检引用失效计数 0。

代码：[状态和报告](engine.cppm)、[value()/status()/flush_if_ready()](engine.cpp)。

## 60. 修复后如何恢复正常计算？

相关输入改变或绑定修复时，目标及下游重新进入 Dirty，按拓扑顺序计算；成功写回或 Unchanged 都恢复 Valid。下游仍执行一次，不能因新数值恰好等于旧缓存而保持阻塞。不会因无关属性改变或重复赋相同输入就重新尝试失败链。

代码：[mark_dirty()/collect_changes()/flush_if_ready()](engine.cpp)。

## 单独打开与运行

用 IDE 打开本周目录，以 **Clang + CMake + clangd** 独立构建、运行和导航。在本周目录执行：

```bash
cmake --preset default
cmake --build --preset default
./build/demo
```

也可执行 `./run.sh` 完成上述三步。依赖版本、Qt 配置及测试入口见[本周工具说明](tools/README.md)；[CMakeLists.txt](CMakeLists.txt) 和 [CMakePresets.json](CMakePresets.json) 均在本周目录，全部产物位于本周 `build/`。

## 代码与运行

独立复制前三周的前置模块，engine 增加状态、安全读取、解绑与删除。引擎拥有对象，不暴露可绕过协调层写入的指针；外部只能改未绑定输入，回调只读，重入修改、绑定、解绑和删除均被拒绝。单线程，批次需显式配对；状态与报告在协调操作或最外层批次结束后查看，底层修改通知期间可用 stored_value 诊断已提交数值，但不代表整个重算已完成。

环境：Linux、CMake 4.3.x、Ninja ≥ 1.11、Clang/clang-scan-deps/clangd 22.1.8、C++23；标准库为含 std 模块开发文件的 libstdc++ 16.2.1。依赖与 IDE 设置见[本周工具说明](tools/README.md)。以下命令均从本周目录运行：

```bash
cmake --preset default
cmake --build --preset default
./run.sh
clang-format --dry-run --Werror *.cpp *.cppm
```

演示使用两个独立场景，第二个场景的对象 ID 重新从 1 开始：

```text
rectangle: width=50, area=5000
1.width Valid count=1 (Changed)
1.area Valid count=1 (Changed)
width=100, area=20000
name change executions=0
rejected cycle: 1.length 1.width 1.length
failed round:
1.width Failed count=1 (evaluation at byte 7: division by zero)
1.area Blocked count=0 (blocked by: 1.width)
2.width Valid count=1 (Changed)
cached: width=100, area=10000, independent width=2
valid reads: width=false, area=false
repaired round:
1.width Valid count=1 (Changed)
1.area Valid count=1 (Changed)
2.width Valid count=1 (Changed)
repaired: width=50, area=5000, independent width=4
deleted source:
2.width MissingReference count=0 (missing reference: 1.segments)
2.area Blocked count=0 (blocked by: 2.width)
replacement id=3, target state=MissingReference
rebound round:
2.width Valid count=1 (Unchanged)
2.area Valid count=1 (Unchanged)
target width=4, area=400
```

本课不添加引用语法、单位运算、函数、双向约束、并行或新的保存格式；当前重算图和状态为内存数据，前两个月课程保持原实现。
