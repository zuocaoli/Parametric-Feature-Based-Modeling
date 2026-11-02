# FreeCAD 单位系统参考与校对

这组资料用于[单位系统一个月课程的新规划](../README.md)，阅读顺序是量纲与工程量核心、单位制策略、App 属性与表达式、应用及文档装配。课程采用八维量纲与 mm、kg、s、A、K、mol、cd、度的内部约定。

## 阅读入口

| 顺序 | 资料 | 对应课程 |
|---|---|---|
| 1 | [笔记索引](单位系统Base层设计/00-索引与阅读路线.md) | 阅读路线与历史来源 |
| 2 | [Base 层架构](单位系统Base层设计/01-Base层单位系统架构.md) | 第 1 周 Unit/Quantity，第 2 周单位制与格式 |
| 3 | [App 层组成](单位系统App层设计/单位系统App层组成.md) | 第 3 周属性/表达式，第 4 周应用/文档装配 |
| 4 | [封装与装配机制](单位系统App层设计/单位系统封装与装配.md) | 三条写值通道、约束和配置消费 |

## 来源与证据范围

原笔记日期为 **2026-08-31**，记录的源码版本为 `dc2ed9d586`。本次于 **2026-10-30** 对课程所需关键结论进行源码校对，参照本机 FreeCAD 学习仓库提交 **`9a274695f3c490f418e321a964a0b5b8b9ad8570`**。该仓库 origin 为 [zuoco/FreeCAD-learn](https://github.com/zuoco/FreeCAD-learn/tree/9a274695f3c490f418e321a964a0b5b8b9ad8570)，所查文件在本地没有未提交修改。

下列链接固定到该提交；路径均相对 FreeCAD 源码根目录。旧笔记保留原日期与历史行号，其行号、模块数量和外围依赖不能视为已在新版本全面复核。没有重新构建或运行 FreeCAD，下面的结论是静态源码核对结果。

| 核验点 | 源码证据 | 本次确认 |
|---|---|---|
| 量纲结构与相等 | [Unit.h](https://github.com/zuoco/FreeCAD-learn/blob/9a274695f3c490f418e321a964a0b5b8b9ad8570/src/Base/Unit.h)、[Unit.cpp](https://github.com/zuoco/FreeCAD-learn/blob/9a274695f3c490f418e321a964a0b5b8b9ad8570/src/Base/Unit.cpp) | 八个指数含角度；相等比较指数，不比较输入符号或工程量名称；当前指数范围为 −8 至 7 |
| 内部单位与换算常量 | [Quantity.cpp](https://github.com/zuoco/FreeCAD-learn/blob/9a274695f3c490f418e321a964a0b5b8b9ad8570/src/Base/Quantity.cpp) | `Metre=1000`、`MilliMetre=1`、`Newton=1000`、`Pascal=0.001`；`Degree=1`、`Radian=180/π` |
| 数量输入语法 | [Quantity.l](https://github.com/zuoco/FreeCAD-learn/blob/9a274695f3c490f418e321a964a0b5b8b9ad8570/src/Base/Quantity.l)、[Quantity.y](https://github.com/zuoco/FreeCAD-learn/blob/9a274695f3c490f418e321a964a0b5b8b9ad8570/src/Base/Quantity.y) | 数字与单位归一化后形成 Quantity；复合单位示例用 `2 N/mm^2`；方括号内容是注释 |
| 显示规格与策略 | [UnitsSchemasSpecs.h](https://github.com/zuoco/FreeCAD-learn/blob/9a274695f3c490f418e321a964a0b5b8b9ad8570/src/Base/UnitsSchemasSpecs.h)、[UnitsSchema.cpp](https://github.com/zuoco/FreeCAD-learn/blob/9a274695f3c490f418e321a964a0b5b8b9ad8570/src/Base/UnitsSchema.cpp) | 规格行含 threshold、unitString、factor；按绝对值选择规则，普通显示数值为内部值除以 factor，格式化不回写模型 |
| 单位制与统一入口 | [UnitsSchemas.h](https://github.com/zuoco/FreeCAD-learn/blob/9a274695f3c490f418e321a964a0b5b8b9ad8570/src/Base/UnitsSchemas.h)、[UnitsApi.h](https://github.com/zuoco/FreeCAD-learn/blob/9a274695f3c490f418e321a964a0b5b8b9ad8570/src/Base/UnitsApi.h) | 集合提供选择、枚举和当前 schema；门面集中暴露单位服务与显示配置 |
| 属性写值通道 | [PropertyUnits.cpp](https://github.com/zuoco/FreeCAD-learn/blob/9a274695f3c490f418e321a964a0b5b8b9ad8570/src/App/PropertyUnits.cpp) | 脚本数字按属性当前量纲构造，字符串先解析；表达式通道可更新量纲；约束类脚本入口有裁剪逻辑 |
| 表达式角度边界 | [Expression.cpp](https://github.com/zuoco/FreeCAD-learn/blob/9a274695f3c490f418e321a964a0b5b8b9ad8570/src/App/Expression.cpp) | App 表达式三角函数接受角度或无量纲参数，调用数学函数前按度转弧度；不要与 Base 数量解析器的函数入口混为一谈 |
| 配置装配与消费 | [Application.cpp](https://github.com/zuoco/FreeCAD-learn/blob/9a274695f3c490f418e321a964a0b5b8b9ad8570/src/App/Application.cpp)、[Document.cpp](https://github.com/zuoco/FreeCAD-learn/blob/9a274695f3c490f418e321a964a0b5b8b9ad8570/src/App/Document.cpp)、[Gui/MainWindow.cpp](https://github.com/zuoco/FreeCAD-learn/blob/9a274695f3c490f418e321a964a0b5b8b9ad8570/src/Gui/MainWindow.cpp)、[Gui/Document.cpp](https://github.com/zuoco/FreeCAD-learn/blob/9a274695f3c490f418e321a964a0b5b8b9ad8570/src/Gui/Document.cpp) | Application 读取用户偏好，Document 存 UnitSystem；Gui 激活文档配置，IgnoreProjectSchema 影响其选择 |

## 已校正的关键解释

1. **英寸与毫米量纲相同。** `1.5 in` 先转换为内部值 `38.1` 与长度量纲，写入长度属性不会仅因符号不同触发 UnitsMismatchError；输入质量等不同量纲才会触发该检查。
2. **脚本浮点数并非一律无量纲。** `createQuantityFromPy` 对 float/int 使用属性当前 `_Unit`；另有对真正无量纲 Quantity 的旁路，二者是不同路径。
3. **属性的 `_Unit` 表示量纲。** 表达式通道更新 `_Unit` 不能解释为“从米显示改成毫米显示”；输入前缀已在解析时归一化，显示由 schema/format 决定。
4. **源码行为与采用建议分开。** 三条通道的检查、量纲更新和裁剪行为存在差异；本次只确认实现事实，不能据此断言其历史动机，也不要求教学实现复刻全部行为。
5. **语法契约有区别。** 课程参照FreeCAD数量语法，用 `2 N/mm^2`表示复合数量；方括号表示注释，`2 [N/mm^2]`的值为无量纲2。

## 课程边界

四周教学课程现已按八维量纲、毫米／度内部约定重新实现，采用标准库Unit/Quantity、数据驱动schema、属性/表达式及应用/文档装配。第四周只讲解当前模型的保存、加载和单位交换。上述源码校对的版本与事实范围保持不变；本项目的固定属性校验、输入上下文、事务及存储接口仍是教学设计，不宣称与FreeCAD接口或文件格式相同。
