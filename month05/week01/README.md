# 第五个月第 1 周：模型与 QML 显示

每周独立源码；控制台和 QML GUI 共享本周 `libcad_core.a`。界面按周开放功能，完整核心与统一 Qt 适配接口保留在本周，便于比较同一模型在不同阶段的显示。

## 本周训练

- **承接与前置**：[第四月末](../../month04/week04/README.md)已有完整文档，本周增加显示适配；先完成 Qt6 环境检查，复习 QObject、列表模型角色与 QML 绑定。
- **目标**：能跟踪核心提交到 QML 图形更新的路径；能独立完成模型坐标到屏幕坐标的转换。
- **独立练习**：把本周初始 A 配置为 120×60、x=20、y=30，width=`length/2`；用 zoom=2、panX=100、panY=300 推算屏幕矩形，再通过核心把 length 改为 144，核对控制台数值与 GUI 显示。
- **验收**：初始屏幕左上角为 (140,120)、尺寸为 240×120；修改后模型为 72/10368，屏幕左上角为 (140,96)、尺寸为 288×144。解释位置和尺寸为何不保存像素。

## 本周源码阅读路线

本月四周保留完整实现，通过 `courseWeek` 按周开放功能。第一周只沿 [console.cpp](console.cpp) 与核心接口 → [bridge.hpp](bridge.hpp) 的角色 → [bridge.cpp](bridge.cpp) 的 data/roleNames/sync → [CanvasView.qml](CanvasView.qml) 的坐标转换阅读，再对照 [ui_tests.cpp](ui_tests.cpp) 的显示更新验证。核心命令与保存作为第四月已学背景，选择编辑、公式控件和拖动实现随后三周阅读。

## 81. 如何让参数化核心作为独立库，被控制台程序和 QML 界面同时使用？

核心仍使用 C++23 命名模块，仅依赖标准库。构建工具将核心接口与实现归档为 `libcad_core.a`，控制台与 GUI 链接同一个库；Qt 适配层不进入核心库。每周各自保存源码，不依赖前一周目录。选择静态库便于学习和部署，代价是每周有核心副本。

代码位置：[核心接口](engine.cppm)、[文档命令](document.cppm)、[构建工具](tools/builder.py)。

## 82. 如何根据矩形的位置、长度和宽度生成二维显示图形？

矩形新增有限双精度 `x`、`y`，尺寸仍要求正数。适配层把有效位置与尺寸放进列表模型，QML `Repeater` 生成 `Rectangle`；Length 对应水平尺寸，Width 对应竖直尺寸，Visible 控制可见性。图形只读模型角色，不负责计算面积或修改核心。

代码位置：[位置校验](rectangle.cpp)、[显示适配](bridge.cpp)、[画布](CanvasView.qml)。

## 83. 如何将 QML 图形项与模型对象 ID 建立对应关系？

列表模型提供 `objectId` 角色，列表项与矩形代理保存同一个稳定 ID，不保存核心对象地址。ID 在 QML 中以十进制字符串传递，避免 JavaScript 数字超过 2^53 后失去精度；C++ 将它还原为 uint64。每次命令提交可能替换核心实例，因此地址不能作为身份。

代码位置：[角色接口](bridge.hpp)、[角色数据](bridge.cpp)。

## 84. 如何把模型中的坐标和长度映射到界面坐标及屏幕像素？

模型用毫米，X 向右、Y 向上，位置是左下角。显示转换为 `screenX = panX + x * zoom`、`screenY = panY - (y + width) * zoom`，显示尺寸分别为 `length * zoom` 与 `width * zoom`。zoom 表示每毫米的 Qt 逻辑像素，高 DPI 的物理像素交给 Qt；模型不保存像素。

代码位置：[坐标转换](CanvasView.qml)。

## 85. 重算完成后，如何仅更新发生变化的对象图形？

订阅稳定 Document 的提交事件，在重算完成后读取最终状态。适配层比较新旧显示行，仅对改变的行发 `dataChanged`；新增、删除分别发列表模型的插入和移除通知。小型教学模型每轮遍历对象，避免增加复杂差量协议；通知和实际图形更新仍是增量的。

代码位置：[文档事件](document.cppm)、[sync 实现](bridge.cpp)、[增量验证](ui_tests.cpp)。

## 单独打开与运行

用 IDE 打开本周目录，在该目录运行 `./run.sh`：构建、更新代码跳转信息并运行本周示例。依赖版本与 IDE 设置见[工具说明](../../docs/tooling.md)。

## 代码与运行

核心使用 C++23 命名模块和 `import std;`；Qt 只在 bridge、main 和 UI 测试中使用。GUI 由 [Main.qml](Main.qml)、[CanvasView.qml](CanvasView.qml)、[Inspector.qml](Inspector.qml) 组成，[bridge.hpp](bridge.hpp) 是 Qt 元对象接口。[console.cpp](console.cpp) 演示同一核心的保存加载，[ui_tests.cpp](ui_tests.cpp) 验证适配层及 QML 交互。

已验证：Linux、GCC 16.2.1、Qt 6.11.2（Quick、QML、Quick Controls、Dialogs、Qt Test）、Clang/clangd/clang-format 22.1.8、Python 3.14.7、pkg-config 与 ar。依赖检查和模块工具链见[公共工具说明](../../docs/tooling.md)。在本课程目录运行：

```bash
python3 tools/builder.py build
./run.sh month05/week01                 # 启动本周 QML 窗口
./build/console-demo  # 控制台，共享同一核心库
python3 tools/builder.py test
python3 tools/builder.py check-format
```

test 运行 Qt Test 交互测试；UI 自动采用 offscreen 与软件渲染，无需桌面。成功打印 `month05/weekXX UI tests passed`，失败返回非零。交互运行需要图形会话；源码修改后重新 build。核心行为通过控制台演示与本周练习核对。

预期演示：点击右侧 Length 按钮，矩形从 100×50 更新为 140×70 mm；再次恢复，显示随核心更新。

本月仅读写 `CAD_DOCUMENT 2`；相对第四个月新增 x/y 属性，记录结构沿用对象、输入、公式与显式映射。要求八个属性被输入与公式完整覆盖；不保存视图、选择、拖动预览、撤销历史或派生缓存。旧格式和未知版本明确拒绝，不做迁移。
