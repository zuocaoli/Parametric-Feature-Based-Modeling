# 第 3 月第 4 周：交互验证端

## 本周训练

- **承接与前置**：承接全部核心能力；观察画布已从第一周随周自包含提供。
- **唯一新增功能**：完成最小 Qt Widgets 验证端，将已有能力接入交互。
- **学习方式**：先推导本周必要公式，再阅读模块实现，最后改变输入并核对结果。

## 1. 屏幕坐标怎样转换为模型坐标？

画布使用 4 像素/mm，模型原点位于左下方留白 50 像素处。转换时反转 y 轴；鼠标命中距离用像素判断，几何求解始终使用 mm/rad。

## 2. 点选取与拖拽生命周期怎样连接库 API？

按下时拾取 12 像素内最近点，带左键移动调用 drag()，释放调用 endDrag()。画布只转发目标和显示模型，数值求解仍在无 Qt 的核心库中。

## 3. 尺寸编辑怎样触发求解和状态刷新？

编辑宽度后点击 Apply width 更新目标并求解；Remove width 制造欠约束。Reset 重新构建本周样例并重建 Solver，不保留上一个场景的成功快照。

## 4. 如何显示可动参数、冗余和冲突来源？

状态栏显示状态、自由度、残差和约束 ID；可动点使用橙色，诊断涉及的直线或圆使用红色。Duplicate width 与 Conflicting width 产生可复现的诊断场景。

## 5. 怎样验证交互结果与无头测试一致？

核心 CTest 覆盖数学与状态，Qt Test 发送实际按钮及鼠标事件验证适配。界面新建点、线、圆只调用同一模型 API；创建的独立几何会增加自由度，不自动吸附或补约束。

## 独立练习与验收

依次完成尺寸驱动、重复尺寸、矛盾尺寸、删宽度拖拽；再用 Point 单击、Line 两次单击和 Circle 两次单击创建几何。

ui 测试验证实际绘图、按钮、尺寸、诊断、鼠标拖拽与创建模式；生成 build/workbench.png 供人工检查。Release 测量包含诊断和拖拽的完整预算。

实现入口：[ui/workbench.cpp](ui/workbench.cpp)；验证入口：[核心测试](tests/core_tests.cpp)。

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default --no-tests=error
ctest --preset default -R "^(ui|drag|rounded)$" --no-tests=error
./build/console-demo --headless
./build/demo
```

依赖版本、Qt 可选配置、运行参数与 clangd 设置见 [tools/README.md](tools/README.md)。

**下一步**：完成 mini-gcs 验收；图分解、增量求解、解析求导和全局多解搜索保持范围外。

[课程目录](../../README.md) · [上一周](../../month03/week03/README.md)
