# 几何引擎第一月课程已实现

2026-10-29，依据第一月目标完成四周代码与讲义，任务 55～60 按 workflow.md 执行。

- week01 默认 transform-order：坐标架、点/向量、T/R 两种顺序、实际 BREP 顶点与保存重载。
- week02 默认 geometry-sampling：四类几何、参数/导数/方向、周期与 Location 应用一次。
- week03 默认 topology-sharing：身份/使用次数/共享、邻接；显式零平移 Location 的几何效果相同不代表与空 Location 身份相同。
- week04 默认 face-boundaries：给定带孔面、方向、pcurve/三维对照、周期接缝与给定无效 Wire 的检查。

每周包含完整框架、CMake/presets/run.sh、tools、IDE 配置、五题答案和测试；不共享课程源码或构建产物。只共同引用分类下 occt 第三方安装。源模板保留不变，后续月份尚未实施。

构建与验收：C++23/import std、Clang+CMake+Ninja+clangd；每周 default CTest 6 项、headless CTest 5 项，真实视窗 1×/2×均通过，逐阶段图像保存本周 build。GUI 测试串行运行，避免 xvfb-run 自动选择同一显示时相互干扰。
