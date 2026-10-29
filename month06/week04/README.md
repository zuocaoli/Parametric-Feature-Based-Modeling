# 第六个月第 4 周：交付与操作接口

完成本月后，继续学习[第七个月：单位系统](../../month07/week01/README.md)。现有矩形使用毫米裸数与单位标签；下一阶段以同一案例解释工程量校验，以及单位与表达式、重算和参数化文档的协作。

承接第五个月完整 QML 编辑器，本周源码独立构建；C++23 标准库核心与 Qt6 适配层分离。本周保留前三周成果，界面与进程内结构化调用共用操作接口。

## 本周训练

- **承接与前置**：保留[上周](../week03/README.md)的可靠性与性能结果，把 UI 和进程内请求统一到操作入口；理解强类型请求、结构化错误与修订号。
- **目标**：能展示实际修改差异再确认提交；能证明失败批次、取消和过期请求都不会覆盖正式内容。
- **独立练习**：在核心 operation_tests 中以 A（120×60、width=`length/2`）为起点，请求 length=144、name="A2"；先预览并核对差异，再提交和撤销。另在预览后修改文档再提交旧请求，最后重新预览提交。
- **验收**：预览不改内容/历史，成功为 72/10368并只新增一条历史；旧请求返回 stale_revision，重新预览可成功。把第二项改为 segments=0 时报告零起始下标 1，第一项也不提交。

## 月末综合验收与后续方向

按[交付验收](acceptance.md)独立完成“创建 A/B → 公式与修改 → 异常修复 → 撤销/重做 → 保存加载 → 请求预览/确认”，再运行核心与真实 QML 测试；贯穿 A 的 120×60→144×72 流程应得到 7200→10368。将本周构建、运行、验证命令和预期结果交给首次阅读项目的人复现，记录环境差异与失败恢复步骤。

解释为何结构化请求共用命令事务、预览需要修订校验，以及快照历史/候选恢复的规模代价。至此完成参数化编辑器基础架构；后续围绕几何内核、约束求解、单位/容差与大模型增量架构继续学习。

## 116. 如何通过统一接口提供对象查询、属性修改和公式设置？

Operations 提供 query、preview、apply。OperationRequest 包含 expected_revision 和一组强类型操作，支持属性赋值、公式绑定/解除、创建和删除；query 返回带 ID、属性、公式、映射与状态的快照。输入校验复用核心命令；每批最多 64 项、公式最多 4096 字节、映射最多 64 项。

这是进程内 C++ 接口，没有网络、JSON 解析或模型依赖；未来传输层需先严格解码为这些类型。新建对象返回后的快照提供实际 ID，同批操作暂不提供临时 ID 引用。

代码：[接口定义](operations.cppm)、[实现](operations.cpp)。

## 117. 如何让接口返回结构化的成功结果和错误信息？

使用 expected<OperationOutcome, OperationError>。成功包含 changed、revision、逐属性 diagnostics；changed=false 不增加历史，且不返回上一轮诊断。错误包含稳定 code、message、current_revision、可选零起始操作下标、源范围和闭环路径，调用者不用解析人类消息。

区分 invalid_request、stale_revision、rejected 和预览异常 exception。合法编辑后计算失败仍是成功，diagnostics 给出 Failed/Blocked，用户可修复或撤销。Qt 显示消息，C++ 调用可根据 code 分支。

代码：[OperationOutcome / OperationError](operations.cppm)。

## 118. 如何在执行一组修改前，向用户展示将要修改的对象与内容？

preview 在隔离候选上执行同样的校验和重算，输出对象 ID、属性键、前后值/公式/映射、新增删除和计算状态，连派生结果一并显示。正式文档、历史、选择和草稿不变；仅解析缓存可增长。QML 的“预览批量修改”准备选中矩形 length=200、width=length/2，用户确认才 apply，取消不提交。

请求绑定预览时的修订号，期间编辑、撤销或打开文档都会使其过期；返回 stale_revision，要求重新预览。确认前展示的是实际差异，不能只回显自然语言。

代码：[preview](operations.cpp)、[预览与确认](bridge.cpp)、[确认框](Main.qml)。

## 119. 如何让界面操作与外部调用使用相同的校验、事务和撤销机制？

属性控件、公式控件、新建、删除和拖动在适配层转换为 OperationRequest，然后走 Operations::apply → CompositeCommand → Document；外部进程内调用也是这条路径。undo/redo 仍由 Document 管理；保存/加载由原有存储入口处理。

事务中第二项失败时第一项也不会发布，失败不动历史或发模型通知；全部成功只提交一个历史项。可通过操作接口核对预览无副作用、失败操作下标、修订冲突、计算失败修复、无变化提交和一次撤销；Qt Test 验证普通编辑会使旧预览过期。

代码：[全部 UI 修改入口](bridge.cpp)、[界面测试](ui_tests.cpp)。

## 120. 如何让一个没看过项目的人根据文档完成构建、运行和案例验证？

从仓库根目录检查工具版本与 Qt 模块，构建本周，先运行控制台案例和自动测试，再启动 QML。完整命令、预期结果、截图及异常案例见下方和[交付验收](acceptance.md)；数据流及边界见[架构说明](architecture.md)。

本周仅依赖标准库核心与 Qt6 UI，没有隐藏服务、API Key 或其他周产物。保留每周源码副本以便独立学习，代价是后续修复需有意识地同步。AI 为选做且未连接：将来只生成受限请求，先预览、用户确认，再通过本接口执行。

## 单独打开与运行

用 IDE 打开本周目录，在该目录运行 `./run.sh`：构建、更新代码跳转信息并运行本周示例。依赖版本与 IDE 设置见[工具说明](../../docs/tooling.md)。

## 代码与运行

已用 Linux、GCC 16.2.1（libstdc++ std 模块）、Clang/clangd/clang-format 22.1.8、Python 3.14.8、Qt 6.11.2 验证。需要 Qt Quick、QML、Controls、Layouts、Dialogs、Qt Test、moc/rcc、pkg-config 和 ar；安装与检查见[公共工具说明](../../docs/tooling.md)。以下命令均在本课程目录执行：

```bash
./build.sh month06/week04
./build/console-demo
./run.sh month06/week04
python3 tools/builder.py test
python3 tools/builder.py check-format
```

GUI 需要图形会话；test 自动使用 offscreen 软件渲染，运行 [ui_tests.cpp](ui_tests.cpp) 中的 Qt 信号与真实 QML 输入测试，成功输出 `month06/week04 UI tests passed`，失败返回非零。源码修改后先重新构建；`test` 自带构建。控制台与 GUI 共享本周 `libcad_core.a`。

[控制台案例](console.cpp)预期：A 从 200/100/20000 撤销到 80/40/3200，重做恢复；成功保存和加载后 modified=0，坏文件不替换当前文档。案例文件位于 `month06/week04/build/demo.cad`。GUI 用 QSaveFile 原子提交；控制台只写演示产物并检查 flush/close，不承诺原子替换用户文件。

[Main.qml](Main.qml)、[CanvasView.qml](CanvasView.qml)、[Inspector.qml](Inspector.qml) 构成界面；[bridge.hpp](bridge.hpp) / [bridge.cpp](bridge.cpp) 管理 Qt 适配、草稿与选择。核心代码只 import std 和本周命名模块。

```bash
./build/console-demo --benchmark
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./build/ui-tests --benchmark
```

性能数据来自实测，不作为跨机器通过阈值。

方法与第三周[性能记录](../week03/performance.md)相同；第四周普通 UI 命令额外经过 Operations。
