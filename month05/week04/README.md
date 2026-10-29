# 第五个月第 4 周：交互与整合

每周独立源码；控制台和 QML GUI 共享本周 `libcad_core.a`。界面按周开放功能，完整核心与统一 Qt 适配接口保留在本周，便于比较同一模型在不同阶段的显示。

## 本周训练

- **承接与前置**：[上周](../week03/README.md)已完成公式编辑，本周加入视图、拖动预览和文档切换；区分界面临时状态与正式模型。
- **目标**：能证明拖动只在松开后提交一次命令；能验证取消、删除与打开文档后不存在旧选择或旧订阅。
- **独立练习**：将 A 设为 length=144、width=`length/2`，先缩放和平移，再拖动并按 Escape；重新拖动并松开，记录前后位置及历史，撤销后保存、打开，最后删除选中 A。
- **验收**：视图变化和取消拖动都不改模型/历史，松开只增加一项历史且可恢复原位置；重开保留公式和 72/10368、清空历史与选择，删除后属性面板不继续引用 A。

## 本周源码阅读路线

承接前三周显示、选择和公式链，新增阅读 [CanvasView.qml](CanvasView.qml) 的缩放/平移/拖动 → [bridge.cpp](bridge.cpp) 的 beginDrag/previewDrag/finishDrag/cancelDrag → [Main.qml](Main.qml) 的删除与文件操作 → bridge 的 openFile/saveFile/sync。对照 [ui_tests.cpp](ui_tests.cpp) 中 `courseWeek >= 4` 的交互案例及下方整合验收。

## 月末综合验收与衔接

独立在 GUI 创建 A（120×60），绑定 width=`length/2`，将 length 改为 144，核对 72/10368；完成拖动、撤销/重做、保存和重新打开。提交坏公式后修复，再验证旧版本/坏文件打开失败时当前文档保留。跨对象映射使用核心案例验证，现有 UI 不提供映射编辑器。

解释预览与正式模型分离、Qt 适配层与纯 C++ 核心分离的理由。[第六月](../../month06/week01/README.md)沿用完整编辑器，补齐公式草稿、未保存判断、差分验证和性能测量，把“可以操作”推进为“可以证明行为可靠”。

## 96. 如何实现视图缩放和平移，同时保持模型坐标不变？

画布只修改 zoom、panX、panY。滚轮围绕光标缩放，按新旧 zoom 的比例修正平移，使光标下的模型点不变；中键拖动修改平移，zoom 限制在 0.05–20。适配视图根据可见有效形状计算边界。全部是视图操作，不调用核心命令。

代码位置：[缩放、平移与适配](CanvasView.qml)、[模型不变验证](ui_tests.cpp)。

## 97. 拖动矩形时，如何提供临时位置预览，并在松开鼠标后提交一次可撤销的命令？

按下时记录模型位置和画布坐标；移动时将像素差除以 zoom，Y 差取反，只更新适配层 previewX/Y。QML 显示临时位置，核心不变。释放时把两条 SetProperty 包装成一次 CompositeCommand，因此 X、Y 共同提交并只需一次撤销。无位移不增加历史，X/Y 已绑定公式或几何无效时拒绝拖动并提示。

代码位置：[拖动入口](bridge.cpp)、[鼠标坐标转换](CanvasView.qml)、[组合事务](document.cpp)。

## 98. 用户取消拖动时，如何丢弃预览并恢复原来的显示位置？

Escape、MouseArea 失去捕获、窗口失活或成功打开另一文档时调用 cancelDrag，清空 previewId，图形绑定立即回到核心角色。取消不改模型和历史；其他编辑、撤销、删除也先结束预览。打开文件失败保留当前模型、选择及预览，释放提交失败则回到核心位置。

代码位置：[取消路径](Main.qml)、[预览生命周期](bridge.cpp)。

## 99. 删除选中对象后，如何清理选择状态和界面引用？

DeleteRectangle 成功后，文档事件触发模型 removeRows，QML 销毁对应代理。适配层验证 selectedId 不再存在后清空它，属性面板与预览一并清理。历史只保存值快照和 ID；撤销删除恢复相同 ID，界面不自动恢复选择。

代码位置：[删除同步](bridge.cpp)、[对象身份恢复](document.cpp)、[删除验证](ui_tests.cpp)。

## 100. 打开另一个文档后，如何解除旧文档订阅并重新建立图形与属性面板？

load_document 在候选模型中完成结构校验和重算，成功才调用 replace_model。replacement 事件清空显示缓存、选择和预览，重建列表，断开并重建 UI 的文档级订阅，画布随后适配视图。Document 协调器保持稳定，核心对象实例全部重新创建。损坏或旧格式不产生 replacement，当前模型和历史不变。

代码位置：[候选加载](storage.cpp)、[订阅与重建](bridge.cpp)。

## 单独打开与运行

用 IDE 打开本周目录，在该目录运行 `./run.sh`：构建、更新代码跳转信息并运行本周示例。依赖版本与 IDE 设置见[工具说明](../../docs/tooling.md)。

## 代码与运行

核心使用 C++23 命名模块和 `import std;`；Qt 只在 bridge、main 和 UI 测试中使用。GUI 由 [Main.qml](Main.qml)、[CanvasView.qml](CanvasView.qml)、[Inspector.qml](Inspector.qml) 组成，[bridge.hpp](bridge.hpp) 是 Qt 元对象接口。[console.cpp](console.cpp) 演示同一核心的保存加载，[ui_tests.cpp](ui_tests.cpp) 验证适配层及 QML 交互。

已验证：Linux、GCC 16.2.1、Qt 6.11.2（Quick、QML、Quick Controls、Dialogs、Qt Test）、Clang/clangd/clang-format 22.1.8、Python 3.14.7、pkg-config 与 ar。依赖检查和模块工具链见[公共工具说明](../../docs/tooling.md)。在本课程目录运行：

```bash
python3 tools/builder.py build
./run.sh month05/week04                 # 启动本周 QML 窗口
./build/console-demo  # 控制台，共享同一核心库
python3 tools/builder.py test
python3 tools/builder.py check-format
```

test 运行 Qt Test 交互测试；UI 自动采用 offscreen 与软件渲染，无需桌面。成功打印 `month05/weekXX UI tests passed`，失败返回非零。交互运行需要图形会话；源码修改后重新 build。核心行为通过控制台演示与本周练习核对。

预期演示：滚轮缩放、中键平移；左键拖动后一次撤销恢复位置，Escape 取消不增加历史。删除再撤销恢复对象。保存 .cad，重新打开恢复位置与公式；旧版 CAD_DOCUMENT 1 被拒绝。

本月仅读写 `CAD_DOCUMENT 2`；相对第四个月新增 x/y 属性，记录结构沿用对象、输入、公式与显式映射。要求八个属性被输入与公式完整覆盖；不保存视图、选择、拖动预览、撤销历史或派生缓存。旧格式和未知版本明确拒绝，不做迁移。

## 整合验收

| 操作 | 预期 |
|---|---|
| 创建、选择多个矩形 | 列表、画布、属性面板一致 |
| 修改 Length / Width 公式 | 依赖重算，尺寸更新，错误范围明确 |
| 拖动后撤销 / Escape 取消 | 一次撤销恢复 XY；取消不改模型与历史 |
| 删除后撤销 | 同 ID 对象与图形恢复，选择仍为空 |
| 缩放、平移 | 只修改视图，无新增命令 |
| 保存再打开 | 输入、位置、公式恢复，历史清空 |
| 旧格式或损坏文件 | 当前模型、历史、选择及预览保留 |

可生成实际 QML 截图（输出属于 build，不提交生成文件）：

```bash
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./build/demo --screenshot build/month05/week04/editor.png
```

[生成的界面截图](../../build/editor.png)。单文档、单选、单线程、矩形；暂不做约束、三维、单位运算与跨对象公式映射编辑器。已有映射保存加载并在修改公式时保留。
