# 文档与属性系统·第四月第 2 周：正确性与恢复验证

承接文档与属性系统·第三月完整 QML 编辑器，本周源码独立构建；C++23 标准库核心与 Qt6 适配层分离。本周在第一周基础上讲解顺序、差分和连续历史的验证方法，代码保留完整编辑器与控制台演示。

## 本周训练

- **承接与前置**：把[上周](../week01/README.md)的完整操作链转为可重复验证；理解固定种子、语义比较和浮点比较容差。
- **目标**：能设计增量/全量对照并检查无关分支；能分别验证历史往返、保存往返与异常修复。
- **独立练习**：先手工写出 A 从 120×60 经 length=144 后的 72/10368与撤销结果；通过界面完成修改、撤销、重做与保存加载，记录数值、状态与恢复结果；再设计两种公式记录顺序的对照步骤。
- **验收**：修改后 width/area 为 72/10368，撤销回到 60/7200；历史与保存往返保持语义一致，失败分支可修复；说明两条计算路径共享求值器时为何仍需核对已知结果。

## 106. 如何验证不同属性创建顺序不会影响最终计算结果？

可用固定种子 `20261012` 构造 12 个矩形、两条独立依赖链及 area 汇合节点，24 次打乱对象记录、属性输入记录和公式绑定顺序，再分阶段恢复。逐属性比较结果与状态；稳定 ID 保持不变。属性元数据本身由 Rectangle 固定定义，这里验证的是输入/公式创建和恢复顺序，不随机改变类定义。

代码：[分阶段恢复](engine.cpp)。

## 107. 如何验证增量重算与全量重算得到相同结果？

每次随机修改第一条链的输入后，保留增量结果，再从相同权威数据全量恢复计算，比较全部有效值与状态。浮点采用 `1e-10 * max(1, |a|, |b|)` 容差；其他类型精确相等。不比较错误节点的旧缓存；额外断言无关链未执行、受影响公式每轮只执行一次。

两种调度共享求值器，差分可发现漏算/错序，不能证明求值器本身正确，所以还应核对已知数值、优先级和异常场景。

代码：[全量恢复与增量入口](engine.cpp)。

## 108. 如何验证一连串操作全部撤销后能恢复初始模型状态？

记录初始语义快照，连续执行 40 次不同长度修改，穿插 5 次创建、公式修改和删除；逐条撤销到起点，再全部重做到终点，比较内容与计算结果。单独覆盖中途失败、无变化操作、撤销后分叉和组合事务一次撤销。

不要求分配器高水位或文档修订号回退，它们分别用于避免身份重用和过期操作。按操作前后的语义快照核对恢复结果。

代码：[历史与事务](document.cpp)。

## 109. 如何验证保存再加载后的模型与保存前语义一致？

先比较对象 ID、所有权威输入、公式文本及映射，再比较重建的有效结果和状态；加载后历史为空、文档干净。文件不保存 AST、依赖图、错误缓存、草稿、选择和视图，因此不以完整 EngineSnapshot 或文件字节相同为标准。

验证场景应包含多对象、跨对象引用、失败模型、特殊字符串、数字精度、缺失引用及坏格式。磁盘仍为 CAD_DOCUMENT 2，不引入迁移功能。

代码：[存储实现](storage.cpp)。

## 110. 如何构造循环依赖、除零、删除引用对象等异常案例，并验证修复后的恢复行为？

循环：新公式 length=area 形成闭环，拒绝并保留旧公式/图，错误给出闭合路径。除零：width=length/(segments-1)，segments=1 时 width Failed、area Blocked，无关链 Valid；改成 2 后整链恢复。删除：下游引用的 ID 消失时直接节点 MissingReference、后继 Blocked；撤销删除以相同 ID 恢复。

每例都验证失败时的状态、旧内容保护、无关分支和修复后的有效值；再用全量恢复对照增量状态。运行控制台演示还可观察坏文件不会替换当前模型。

代码：[重算与恢复](engine.cpp)、[加载保护演示](console.cpp)。

## 单独打开与运行

用 IDE 打开本周目录，以 **Clang + CMake + clangd** 独立构建、运行和导航。在本周目录执行：

```bash
cmake --preset default
cmake --build --preset default
./build/demo
```

也可执行 `./run.sh` 完成上述三步。依赖版本、Qt 配置及测试入口见[本周工具说明](tools/README.md)；[CMakeLists.txt](CMakeLists.txt) 和 [CMakePresets.json](CMakePresets.json) 均在本周目录，全部产物位于本周 `build/`。

## 代码与运行

环境：Linux、CMake 4.3.x、Ninja ≥ 1.11、Clang/clang-scan-deps/clangd 22.1.8、C++23；标准库为含 std 模块开发文件的 libstdc++ 16.2.1。界面与 UI 测试另需 Qt 6.11.2。依赖与 IDE 设置见[本周工具说明](tools/README.md)。以下命令均从本周目录运行：

```bash
cmake --preset default
cmake --build --preset default
./build/console-demo
./run.sh
ctest --preset default --no-tests=error
clang-format --dry-run --Werror *.cpp *.cppm *.hpp
```

GUI 需要图形会话；CTest 自动使用 offscreen 软件渲染，运行 [ui_tests.cpp](ui_tests.cpp) 中的 Qt 信号与真实 QML 输入测试，成功输出 `文档与属性系统/month04/week02 UI tests passed`，失败返回非零。源码修改后先重新构建；运行 CTest 前需先构建。控制台与 GUI 共享本周 `libcad_core.a`。

[控制台案例](console.cpp)预期：A 从 200/100/20000 撤销到 80/40/3200，重做恢复；成功保存和加载后 modified=0，坏文件不替换当前文档。案例文件位于 `文档与属性系统/month04/week02/build/demo.cad`。GUI 用 QSaveFile 原子提交；控制台只写演示产物并检查 flush/close，不承诺原子替换用户文件。

[Main.qml](Main.qml)、[CanvasView.qml](CanvasView.qml)、[Inspector.qml](Inspector.qml) 构成界面；[bridge.hpp](bridge.hpp) / [bridge.cpp](bridge.cpp) 管理 Qt 适配、草稿与选择。核心代码只 import std 和本周命名模块。
