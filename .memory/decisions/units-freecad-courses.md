# CONTEXT：FreeCAD思想的单位系统四周已重新实现

更新时间：2026-10-30T13:28:24.147Z。项目：Parametric-Feature-Based-Modeling；承接课程设计决策units-freecad-curriculum-20261030。

四周独立C++23/import std标准库控制台课程现已按八维Unit和mm/kg/s/A/K/mol/cd/度内部约定实现，题号121–140五题答案均在各周《课程讲义.md》；分类及各周 README 只说明核心目标和学习成果，分类《课程安排.md》保留逐周问题。第1周Unit/Quantity，第2周数据驱动schema与UnitsApi，第3周属性通道及固定量纲模型/表达式，第4周Application激活、偏好、冻结编辑、事务与保存加载。

课程只提供当前八维、毫米／度模型的保存、加载与单位交换。用户明确不需要人为的新旧版本、历史兼容或迁移；这些额外功能已删除。参考笔记的FreeCAD源码提交仍为9a274695f3。

Unit相等只比较指数，通用PropertyQuantity演示三通道差异，文档模型固定Unit并统一校验；UnitsApi使用实例而非全局单例，无FreeCAD/Qt/Python运行依赖。5个教学schema及数值格式并不复刻全部FreeCAD方案。文件/编辑协议是本项目设计。

验证及工作记录见task_archive/79.md至84.md；四周CTest分别2/4/7/11项，另行隔离构建、原生clangd和文档检查。旧设计-only决策保留为历史，此记录更新当前交付状态。

用户范围纠正与清理记录见task_archive/85–87；当前CLI仅--save/--load/--help，不以课程草稿前后变化建立文件兼容系统。
