# 文档与属性系统·第三月第 3 周：公式与错误反馈

每周独立源码；控制台和 QML GUI 共享本周 `libcad_core.a`。界面按周开放功能，完整核心与统一 Qt 适配接口保留在本周，便于比较同一模型在不同阶段的显示。

## 本周训练

- **承接与前置**：给[上周](../week02/README.md)的编辑面板增加公式、错误与历史反馈；复习 Failed/Blocked、解析范围与命令成功的含义。
- **目标**：能区分公式字段、只读结果和失败草稿；能通过修复或撤销同步恢复模型、面板与图形。
- **独立练习**：新建 A、设为 120×60，为 width 绑定 `length/2`，把 length 改为 144；输入 `length +` 验证拒绝，再绑定 `length/(segments-1)`，令 segments=1 后改为 3。
- **验收**：首轮结果为 72/10368；语法失败保留旧公式且不进历史；除零显示 Failed/Blocked 与旧几何警告，修复后恢复 72/10368。用撤销/重做核对公式、数值与错误状态同步。

## 本周源码阅读路线

承接第二周属性提交，新增阅读 [Inspector.qml](Inspector.qml) 的公式/结果双字段 → [bridge.cpp](bridge.cpp) 的 setFormula/clearFormula 与错误范围转换 → [engine.cpp](engine.cpp) 的绑定和状态 → [CanvasView.qml](CanvasView.qml) 的旧几何警告。对照 [ui_tests.cpp](ui_tests.cpp) 中 `courseWeek >= 3` 的公式与历史案例；草稿跨选择保留在文档与属性系统·第四月补齐。

## 91. 如何在属性面板中分别展示公式文本和计算结果？

为 double 属性显示两个独立字段：数值字段读最近缓存与有效状态，公式字段读绑定文本。公式通过 BindFormula 提交，计算结果由核心重算。Valid、Failed、Blocked、MissingReference 及原因另行展示，不用数值本身表示有效性。

代码位置：[属性行数据](bridge.cpp)、[双字段面板](Inspector.qml)。

## 92. 公式驱动的属性应如何处理用户直接输入数值的操作？

选择显式解除：结果字段只读，点击“解除公式，保留数值”执行 UnbindFormula，再编辑普通数值。解除本身可撤销，恢复公式和依赖边。失败公式解除后，保留的缓存成为普通输入；界面明确展示当前数值，用户可继续改值。代价是多一步交互，收益是不因输入数值意外丢失公式。

代码位置：[解除入口](bridge.cpp)、[核心解除规则](engine.cpp)。

## 93. 公式解析失败时，如何在编辑位置显示错误及对应文本范围？

Lexer/Parser 返回错误位置，Parser 使用当前 Token 文本长度；末尾错误长度为零。EngineError 携带可选 SourceRange，UI 不解析错误字符串。适配层将 UTF-8 字节范围转为 QString 的 UTF-16 索引，面板保留失败草稿并选中错误 Token，末尾错误定位光标。绑定校验或循环错误无文本范围时显示原因。

代码位置：[解析范围](parser.cpp)、[结构化错误](engine.cppm)、[字段定位](Inspector.qml)。

## 94. 属性重算失败时，如何让用户识别图形当前展示的是上一次有效结果？

适配层只在位置、长度和宽度全部有效时更新几何；失败时保留本会话上一次有效形状，用橙色边框和文字标记。面积独立失败不影响已有效的尺寸图形，但属性面板仍报告面积错误。加载后从未获得有效尺寸的对象不绘制占位形状，列表仍可选择它进行修复。

代码位置：[显示缓存与状态](bridge.cpp)、[旧图形标记](CanvasView.qml)。

## 95. 撤销和重做后，如何同步更新图形、属性值、公式及错误状态？

所有更新来自 Document 的同一种提交事件。历史恢复数值、公式映射与计算状态，适配层刷新显示角色和属性行。合法命令导致计算失败仍增加历史并可撤销；解析失败属于命令失败，不进入历史。界面错误草稿在成功编辑或切换选择后清除。

代码位置：[历史与事件](document.cpp)、[界面同步测试](ui_tests.cpp)。

## 单独打开与运行

用 IDE 打开本周目录，以 **Clang + CMake + clangd** 独立构建、运行和导航。在本周目录执行：

```bash
cmake --preset default
cmake --build --preset default
./build/demo
```

也可执行 `./run.sh` 完成上述三步。依赖版本、Qt 配置及测试入口见[本周工具说明](tools/README.md)；[CMakeLists.txt](CMakeLists.txt) 和 [CMakePresets.json](CMakePresets.json) 均在本周目录，全部产物位于本周 `build/`。

## 代码与运行

核心使用 C++23 命名模块和 `import std;`；Qt 只在 bridge、main 和 UI 测试中使用。GUI 由 [Main.qml](Main.qml)、[CanvasView.qml](CanvasView.qml)、[Inspector.qml](Inspector.qml) 组成，[bridge.hpp](bridge.hpp) 是 Qt 元对象接口。[console.cpp](console.cpp) 演示同一核心的保存加载，[ui_tests.cpp](ui_tests.cpp) 验证适配层及 QML 交互。

环境：Linux、CMake 4.3.x、Ninja ≥ 1.11、Clang/clang-scan-deps/clangd 22.1.8、C++23；标准库为含 std 模块开发文件的 libstdc++ 16.2.1。界面与 UI 测试另需 Qt 6.11.2。依赖与 IDE 设置见[本周工具说明](tools/README.md)。以下命令均从本周目录运行：

```bash
cmake --preset default
cmake --build --preset default
./run.sh                 # 启动本周 QML 窗口
./build/console-demo  # 控制台，共享同一核心库
ctest --preset default --no-tests=error
clang-format --dry-run --Werror *.cpp *.cppm *.hpp
```

CTest 运行 Qt Test 交互测试；UI 自动采用 offscreen 与软件渲染，无需桌面。成功打印 `文档与属性系统/month03/weekXX UI tests passed`，失败返回非零。交互运行需要图形会话；源码修改后重新执行 CMake 构建。核心行为通过控制台演示与本周练习核对。

预期演示：选中 #1，Width 已绑定 `length / 2`；输入 `length width` 选中错误 Token，输入 `length +` 定位末尾；绑定 `1 / 0` 显示 Failed，原形状标橙色；撤销恢复公式及图形。

本月仅读写 `CAD_DOCUMENT 2`；相对文档与属性系统·第二月新增 x/y 属性，记录结构沿用对象、输入、公式与显式映射。要求八个属性被输入与公式完整覆盖；不保存视图、选择、拖动预览、撤销历史或派生缓存。旧格式和未知版本明确拒绝，不做迁移。
