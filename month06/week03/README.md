# 第六个月第 3 周：性能与架构整理

承接第五个月完整 QML 编辑器，本周源码独立构建；C++23 标准库核心与 Qt6 适配层分离。本周保留第二周验证，增加 AST 缓存、恢复建图优化和可重复性能测量。

## 本周训练

- **承接与前置**：在[上周](../week02/README.md)的正确性验证基础上测量性能；理解预热、中位数、包含关系和 AST 复用。
- **目标**：能分开测量解析、重算、完整命令和 UI 更新；能用数据解释增量求值为何不保证整次操作为常数时间。
- **独立练习**：按下方命令顺序运行核心与 UI benchmark，记录自己的环境；比较 20/100/300 对象的独立与链式场景，预测并核对执行公式数、AST 新解析次数和通知次数，再选出当前最值得优化的环节。
- **验收**：核心独立场景每次执行 2 条公式，链式为 2×对象数，数值编辑新解析为 0；每例 8 次有变化命令产生 8 次文档通知。优化建议引用实测数据，并按前两周的操作步骤核对错误恢复；不套用跨机器耗时阈值。

## 111. 如何分别测量表达式解析、重算和界面更新的耗时？

使用 steady_clock，解析只测 `parse`；重算计时从受影响子图排序到 AST 求值、写回和诊断结束；同时测包含脏传播的引擎 set、全量恢复和完整 Document 命令。GUI 单独测适配层 sync（含同步 Qt 信号/QML 响应）以及事件处理＋软件渲染截图耗时。后者包含截图读回，不代表真实桌面帧延迟；这些时间存在包含关系，不能相加。

固定 20/100/300 对象、独立和跨对象链两种拓扑，预热一次后取 7 次中位数；构造场景不计时，循环内核对重算结果。编译使用现有教学构建默认 -O0，环境、原始结果及限制见[性能记录](performance.md)。

代码：[核心测量](benchmark.cpp)、[QML 测量](ui_tests.cpp)。

## 112. 对象数量和依赖关系增加时，哪个环节最先成为瓶颈？

优化前的实测显示，300 对象仅有 2 个公式受影响时，重算约 28 微秒，整个命令却约 0.95 秒。原因是快照恢复时为每个公式复制整图并检测循环，建立候选的成本接近平方增长。

本周在隔离候选中批量建立边，最后统一检测循环；坏候选仍不能发布。AST 缓存去掉重复解析，但不能替代这项结构优化。优化后的数据和当前瓶颈见[性能记录](performance.md)；普通用户新绑定仍走原子图校验路径。

代码：[restore 的批量建图](engine.cpp)。

## 113. 如何复用已经解析的 AST，避免只修改数值时重复解析公式？

每个文档共享一个 AstCache，按完整公式文本缓存 `shared_ptr<const Node>`，绑定只读使用语法树；变量取值和别名映射在每次求值时重新解析到对象属性，不缓存计算值。数值修改、候选恢复及 undo/redo 复用 AST；新公式解析一次，语法错误不缓存。

最多保留 256 个源文本，达到上限按键序淘汰一个条目；这不是 LRU，已被绑定引用的树仍存活。缓存及计数不参与持久化/撤销，也不进入全局状态。恢复时重新验证引用与图结构，缓存命中不会绕过校验。

代码：[AstCache 与 Binding](engine.cppm)。

## 114. 如何减少一次批量修改产生的重复通知和界面刷新？

多条输入命令放入 CompositeCommand，候选模型只打开一次批次，最后 end_batch 重算；Document 只发布一条提交事件，增加一条历史。Qt 先比较对象显示行，只对实际变化的行发 dataChanged；属性面板仍整体刷新，但草稿独立保存。

同一批次多次改 length 和位置应得到：一次通知、两条公式执行、一次历史；性能用例的单对象编辑只发一次 dataChanged。当前同步单线程下不需要计时器防抖，整图快照和全表扫描仍有线性成本。

代码：[组合事务](document.cpp)、[行差异同步](bridge.cpp)。

## 115. 如何让重算诊断信息帮助定位问题，同时避免核心库依赖界面？

核心诊断使用标准库值类型：属性键、执行次数、ComputeState、错误消息、解析源范围或循环路径。UI 把它们转换为中文提示、错误选择范围和图形警告；控制台、测试和接口可直接读取同一数据。重算失败与命令拒绝分开表达，Blocked 不再次执行 AST。

核心没有 QObject、QString 或 QML 依赖。诊断是本轮报告，界面长期状态从模型读取；性能计数和计时是观测数据，不改变文档内容。

代码：[EngineError / RecomputeStep](engine.cppm)、[Qt 转换](bridge.cpp)。

## 单独打开与运行

用 IDE 打开本周目录，在该目录运行 `./run.sh`：构建、更新代码跳转信息并运行本周示例。依赖版本与 IDE 设置见[工具说明](../../docs/tooling.md)。

## 代码与运行

已用 Linux、GCC 16.2.1（libstdc++ std 模块）、Clang/clangd/clang-format 22.1.8、Python 3.14.8、Qt 6.11.2 验证。需要 Qt Quick、QML、Controls、Layouts、Dialogs、Qt Test、moc/rcc、pkg-config 和 ar；安装与检查见[公共工具说明](../../docs/tooling.md)。以下命令均在本课程目录执行：

```bash
./build.sh month06/week03
./build/console-demo
./run.sh month06/week03
python3 tools/builder.py test
python3 tools/builder.py check-format
```

GUI 需要图形会话；test 自动使用 offscreen 软件渲染，运行 [ui_tests.cpp](ui_tests.cpp) 中的 Qt 信号与真实 QML 输入测试，成功输出 `month06/week03 UI tests passed`，失败返回非零。源码修改后先重新构建；`test` 自带构建。控制台与 GUI 共享本周 `libcad_core.a`。

[控制台案例](console.cpp)预期：A 从 200/100/20000 撤销到 80/40/3200，重做恢复；成功保存和加载后 modified=0，坏文件不替换当前文档。案例文件位于 `month06/week03/build/demo.cad`。GUI 用 QSaveFile 原子提交；控制台只写演示产物并检查 flush/close，不承诺原子替换用户文件。

[Main.qml](Main.qml)、[CanvasView.qml](CanvasView.qml)、[Inspector.qml](Inspector.qml) 构成界面；[bridge.hpp](bridge.hpp) / [bridge.cpp](bridge.cpp) 管理 Qt 适配、草稿与选择。核心代码只 import std 和本周命名模块。

```bash
./build/console-demo --benchmark
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./build/ui-tests --benchmark
```

性能数据来自实测，不作为跨机器通过阈值。

详见[性能记录](performance.md)。
