# 按FreeCAD设计思想重新实现单位系统四周课程

目标：四周独立C++23/import std控制台课程及新版五题答案，Unit八维（含角度）、mm/kg/s/A/K/mol/cd/度内部值，数据驱动schema、App属性/表达式与文档装配。仅标准库，不引入FreeCAD/Qt/Python依赖。源码参考9a274695f3；不照搬不一致的写值政策，矩形模型统一校验。

| 任务 | 依赖 | 设计与验收 | 状态 |
|---|---|---|---|
| 79 | 无 | 备份旧课程、真实SI_RAD样本、建立任务与保护基线 | 完成 |
| 80 | 79 | 独立Unit八维代数、Quantity与内部常量；121–125讲义及测试 | 完成 |
| 81 | 80 | 完整复制核心，数量语法、QuantityFormat/Schema/集合/UnitsApi；126–130及测试 | 完成 |
| 82 | 81 | 完整复制源码，PropertyQuantity通道演示、公式、重算与事务；131–135及测试 | 完成 |
| 83 | 82 | 完整复制源码，应用偏好/文档激活/输入上下文、V4 MM_DEG存储与显式旧文件迁移；136–140及测试 | 完成 |
| 84 | 80–83 | 四周隔离构建、CTest/clangd/格式、文档状态和记忆同步、归档清空 | 完成 |

实现取舍：Unit相等只比较八指数，不保留额外Kind强类型；mm/deg内部值。Quantity保持有限值与量纲，范围由属性负责。通用属性演示raw/script/expression差异，文档模型采用固定量纲校验。数量输入支持2 N/mm^2及[]注释；App公式使用显式数量乘除。新文件CAD_DOCUMENT 4/MM_DEG，旧V3/SI_RAD须显式迁移，保留源文件；不声称兼容FreeCAD原生文档。

每成功一项写task_archive/编号.md，再更新状态；后续先读依赖结果。全部通过后归档并清空task.md和task_status.md。workflow不变，不提交Git。
