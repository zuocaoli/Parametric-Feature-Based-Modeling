# 第五个月第 2 周：选择与属性编辑

每周独立源码；控制台和 QML GUI 共享本周 `libcad_core.a`。界面按周开放功能，完整核心与统一 Qt 适配接口保留在本周，便于比较同一模型在不同阶段的显示。

## 本周训练

- **承接与前置**：[上周](../week01/README.md)已将模型显示出来，本周把选择和属性输入接到命令入口；理解元数据、控件提交与绑定刷新。
- **目标**：能让列表和画布共享稳定 ID 选择；能证明模型刷新不会再次提交命令。
- **独立练习**：新建并选择 A，设 length=120、width=60（新建对象的 width 未绑定），再把 length 改为 144、name 改为 "A2"，切换列表/画布选择；提交 length=-1，随后改为 160。
- **验收**：两处选择与属性面板一致；合法值面积依次为 7200、8640、9600，非法输入恢复正式值且不增加历史；每次合法有变化的提交只增加一个历史项。

## 本周源码阅读路线

承接第一周显示链，新增阅读 [Main.qml](Main.qml) 的对象列表 → [CanvasView.qml](CanvasView.qml) 的点击 → [bridge.cpp](bridge.cpp) 的 select/propertyRows/setValue → [Inspector.qml](Inspector.qml) 的类型控件与提交。对照 [ui_tests.cpp](ui_tests.cpp) 中 `courseWeek >= 2` 的输入案例；公式与拖动留到后两周。

## 86. 如何通过鼠标点击确定当前选中的模型对象？

QML 代理收到左键点击后把 objectId 交给适配层。适配层验证对象仍存在，再设置唯一的 selectedId；点击空白清空选择。选择属于界面状态，不执行命令，不增加历史。第二周只启用选择，拖动在第四周开放。

代码位置：[画布点击](CanvasView.qml)、[选择入口](bridge.cpp)。

## 87. 如何让对象列表与画布共享同一份选择状态？

适配层保存一份稳定 ID，列表高亮与矩形边框都绑定 selectedId。两处点击都调用同一个 select 入口，属性面板也从该 ID 读取数据。这样无需列表和画布相互调用；对象销毁时只需清理这一份选择。

代码位置：[共享选择](bridge.hpp)、[对象列表](Main.qml)。

## 88. 如何根据属性元数据生成适合不同类型的编辑控件？

核心 properties(id) 返回标签、单位、类型、只读标志和值的只读快照。QML 用重复器生成字段：double 与 integer 用文本输入并在 C++ 严格解析，bool 用复选框，string 用文本输入。整数保持文本直至 C++ 转成 int64，避免精度丢失。绑定属性结果只读，公式编辑在第三周开放。

代码位置：[属性元数据](property.cppm)、[只读枚举](engine.cppm)、[属性面板](Inspector.qml)。

## 89. 如何让属性面板通过命令系统提交修改？

用户按 Enter 或点“应用”时调用 setValue，适配层按当前属性类型解析，构造 SetProperty 再由 Document 执行。类型错误、非有限数、非法尺寸和绑定目标写入被拒绝，保留原模型和历史。输入验证在 C++ 完成，QML 只负责交互和提示。

代码位置：[输入与命令](bridge.cpp)、[命令提交](document.cpp)。

## 90. 模型变化后，如何刷新属性面板而不再次触发修改命令？

控件读取 propertyRows；文档提交、撤销和选择变化通过 uiChanged 更新数据。TextField 只在用户 accepted 或按钮 clicked 时提交，CheckBox 只在 clicked 时提交，不监听 textChanged/checkedChanged 写回。刷新可以重建字段，但不会产生修改命令。

代码位置：[提交信号](Inspector.qml)、[刷新零新增命令测试](ui_tests.cpp)。

## 单独打开与运行

用 IDE 打开本周目录，在该目录运行 `./run.sh`：构建、更新代码跳转信息并运行本周示例。依赖版本与 IDE 设置见[工具说明](../../docs/tooling.md)。

## 代码与运行

核心使用 C++23 命名模块和 `import std;`；Qt 只在 bridge、main 和 UI 测试中使用。GUI 由 [Main.qml](Main.qml)、[CanvasView.qml](CanvasView.qml)、[Inspector.qml](Inspector.qml) 组成，[bridge.hpp](bridge.hpp) 是 Qt 元对象接口。[console.cpp](console.cpp) 演示同一核心的保存加载，[ui_tests.cpp](ui_tests.cpp) 验证适配层及 QML 交互。

已验证：Linux、GCC 16.2.1、Qt 6.11.2（Quick、QML、Quick Controls、Dialogs、Qt Test）、Clang/clangd/clang-format 22.1.8、Python 3.14.7、pkg-config 与 ar。依赖检查和模块工具链见[公共工具说明](../../docs/tooling.md)。在本课程目录运行：

```bash
python3 tools/builder.py build
./run.sh month05/week02                 # 启动本周 QML 窗口
./build/console-demo  # 控制台，共享同一核心库
python3 tools/builder.py test
python3 tools/builder.py check-format
```

test 运行 Qt Test 交互测试；UI 自动采用 offscreen 与软件渲染，无需桌面。成功打印 `month05/weekXX UI tests passed`，失败返回非零。交互运行需要图形会话；源码修改后重新 build。核心行为通过控制台演示与本周练习核对。

预期演示：点击对象列表或画布，选择保持一致；创建第二个矩形，修改 Length、Name、Visible、Segments，属性面板刷新不重复提交。

本月仅读写 `CAD_DOCUMENT 2`；相对第四个月新增 x/y 属性，记录结构沿用对象、输入、公式与显式映射。要求八个属性被输入与公式完整覆盖；不保存视图、选择、拖动预览、撤销历史或派生缓存。旧格式和未知版本明确拒绝，不做迁移。
