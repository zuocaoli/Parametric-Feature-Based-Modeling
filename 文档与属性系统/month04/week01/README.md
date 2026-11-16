# 文档与属性系统·第四月第 1 周：完整流程与状态一致性

承接文档与属性系统·第三月完整 QML 编辑器，本周源码独立构建；C++23 标准库核心与 Qt6 适配层分离。本周补齐保存基准、草稿和完整案例。

## 本周训练

- **承接与前置**：沿用[文档与属性系统·第三月末](../../month03/week04/README.md)的完整编辑器，补齐正式内容、草稿与保存基准的区分；复习语义快照与事务提交。
- **目标**：能预测保存、撤销和新分支后的未保存状态；能验证公式草稿不会被模型刷新覆盖或偷偷保存。
- **独立练习**：新建 A、设为 120×60并绑定 width=`length/2`，保存；输入未提交草稿 `length/3`，切换选择再回来；把 length 改为 144 后撤销，再在同一历史位置改为 160，最后放弃草稿并保存。
- **验收**：草稿跨选择保留且正式公式仍为 `length/2`；修改后为 72/10368且 modified=true，撤销回保存内容后为 false，新分支为 80/12800且 true，成功保存后 false。解释草稿提示与文档 modified 的区别。

## 101. 如何用一个案例贯穿创建对象、设置公式、修改参数、撤销和保存加载？

创建矩形 A（80 × 40），绑定 `width = length / 2`，把 length 改成 200；width/area 应为 100/20000。撤销回到 80/40/3200，重做后保存再加载仍为 200/100/20000。`console.cpp` 从创建一路执行到损坏文件拒绝；QML 用同一核心完成对应操作。初始模板还有对象 #1，A 的 ID 是 #2。

代码：[完整案例](console.cpp)、[命令提交](document.cpp)。

## 102. 如何让 QML 准确显示属性的正常、待重算和计算失败状态？

适配层读取 `ComputeState`，显示 Valid（有效）、Dirty（待重算）、Failed（本节点失败）、Blocked（前置失败）、MissingReference（引用失效）。非 Valid 的结果不能被后续公式消费；画布可保留上一次有效几何并显示警告，首次加载就失败则隐藏无效几何。

本课程同步提交，Dirty 只存在于候选模型批次内部，界面收到的始终是提交后的状态，不制造“正在计算”的闪烁。如果以后异步计算，必须同时提交版本号和 Dirty 状态，并拒绝旧版本回写。可在批次内观察 Dirty 状态与读取隔离。

代码：[状态机](engine.cpp)、[界面映射](bridge.cpp)、[状态与错误显示](Inspector.qml)。

## 103. 编辑公式尚未提交时，如何区分界面草稿与模型中的正式公式？

正式公式属于模型并参与保存和撤销；草稿按“对象 ID＋属性键”保存在 Qt 适配层，只在显式提交时进入命令。切换选择和重算刷新不会丢掉草稿；解析失败保留文本与错误范围，放弃草稿恢复正式公式。外部修改或撤销改变正式公式时标记冲突，显示正式文本，用户可明确选择覆盖或放弃；打开文档或删除对象会清理对应草稿。

代价是必须管理两份文本，但输入每个字符不会产生历史记录。保存只保存正式模型，标题另行提示仍有草稿。

代码：[Draft 生命周期](bridge.cpp)、[草稿编辑](Inspector.qml)、[验证](ui_tests.cpp)。

## 104. 操作失败后，如何保证画布、属性面板和核心模型状态一致？

在候选模型执行命令、校验和重算，成功才替换正式模型，并发送一次最终事件。输入/解析/循环错误丢弃候选，历史、画布和正式属性不变；数值输入失败时控件恢复正式值，公式失败文本留在明确标注的草稿里。合法编辑导致除零则保留命令和历史，显示 Failed/Blocked，修复输入或撤销即可恢复。

文件加载也先验证候选；失败不清空当前选择、预览或订阅。GUI 保存用 QSaveFile，成功提交文件后才更新保存基准。

代码：[事务](document.cpp)、[文件与显示同步](bridge.cpp)、[QML 输入](Inspector.qml)。

## 105. 文档发生修改后，如何准确判断是否存在尚未保存的内容？

比较当前内容与最近成功保存/加载时的语义快照：稳定对象 ID、未绑定输入、公式原文和变量映射。排除派生缓存、错误状态、视图、选择、草稿、历史和分配器高水位。撤销回到已保存内容即为干净；相同历史位置的新分支仍可能未保存，不能只比较撤销栈下标。公式文本按原文比较，空白变化也算编辑。

初始化模板作为干净基准；保存失败不更新基准。打开/关闭前对文档修改或草稿提供放弃/取消提示。保持高水位递增可避免 ID 复用，因此“撤销恢复”采用内容语义而非内存逐字节相等。当前比较为 O(N)，适用于本月小文档；大型模型可缓存规范摘要。

代码：[semantic_snapshot](engine.cpp)、[modified/mark_saved](document.cpp)。

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

GUI 需要图形会话；CTest 自动使用 offscreen 软件渲染，运行 [ui_tests.cpp](ui_tests.cpp) 中的 Qt 信号与真实 QML 输入测试，成功输出 `文档与属性系统/month04/week01 UI tests passed`，失败返回非零。源码修改后先重新构建；运行 CTest 前需先构建。控制台与 GUI 共享本周 `libcad_core.a`。

[控制台案例](console.cpp)预期：A 从 200/100/20000 撤销到 80/40/3200，重做恢复；成功保存和加载后 modified=0，坏文件不替换当前文档。案例文件位于 `文档与属性系统/month04/week01/build/demo.cad`。GUI 用 QSaveFile 原子提交；控制台只写演示产物并检查 flush/close，不承诺原子替换用户文件。

[Main.qml](Main.qml)、[CanvasView.qml](CanvasView.qml)、[Inspector.qml](Inspector.qml) 构成界面；[bridge.hpp](bridge.hpp) / [bridge.cpp](bridge.cpp) 管理 Qt 适配、草稿与选择。核心代码只 import std 和本周命名模块。
