# 单位系统: App层单位系统组成

> 日期: 2026-08-31
> 来源: understand-anything 知识图谱问答(图谱提交 `dc2ed9d586`，为原记录版本), 结论经磁盘源码逐行核实(行号随文); 原文的源码版本与行号保留供历史阅读。
> 2026-10-30 课程所需关键结论已对照 `9a274695f3` 校对，范围和更正见[参考入口与校对](../README.md)。

## 背景

问题: 聚焦于单位系统的 App 层——单位系统在 App 层由哪些模块组成、各自提供什么功能、分别对应哪些源文件?

前提校正: 单位系统的引擎本体全部在 Base 层(见 01)。图谱中 `layer:app` 的单位相关文件节点只有一个 `src/App/PropertyUnits.cpp/.h`, 其余是散落在 Application / Document / Expression 的装配与集成点。因此"App 层的单位系统"的准确定义是:**文档模型侧对单位系统的封装与装配**, 不包含任何换算/解析引擎。

## 结论: 4 个组成, 1 个中心接口

```
PropertyUnits (带单位属性体系)      ← App 层中心接口, 被几乎所有工作台与 Gui 导入
  ↑ 依赖 Base/Quantity.h + Base/Unit.h
Expression 引擎单位支持             ← UnitExpression 家族, 带单位表达式求值
Application 装配与生命周期          ← FreeCAD.Units 模块注册 / 偏好读取 / 清理
Document 文档级单位系统             ← UnitSystem 属性(项目级单位制选择)
```

## 模块明细

### 1. PropertyUnits — 带单位属性体系(中心接口)

- 源文件: `src/App/PropertyUnits.h` / `PropertyUnits.cpp`; 继承链挂在 `PropertyStandard.h` 的 `PropertyFloat` 之下(值仍存 double, 即**内部单位**)。
- 三层结构:

| 层 | 类 | 行号 | 功能 |
| --- | --- | --- | --- |
| 基类 | `PropertyQuantity : PropertyFloat` | `PropertyUnits.h:44` | 带单位浮点属性: 持 `_Unit` + `_Format` 两个成员(`:101-102`), 值是 PropertyFloat 的 double(内部单位)。<br>API: `getQuantityValue()`(`:51`)、`setUnit/getUnit`(`:58/:62`)、`setFormat/getFormat`(`:80/:76`)、`setPyObject`(`:56`, 经 `createQuantityFromPy`(`:100`)接受带单位字符串或 Quantity)、`setPathValue/getPathValue`(供表达式绑定路径写值)、`isSame` 同时比较值与单位(`:91-97`) |
| 约束 | `PropertyQuantityConstraint : PropertyQuantity` | `PropertyUnits.h:108` | 加 `Constraints{LowerBound, UpperBound, StepSize}` 边界(`:114-117`), 供属性编辑器与有界的量(如角度)使用 |
| 具体属性 | 59 个 `Property*` 类 | `PropertyUnits.h:155-917` | 每个类构造时 `setUnit()` 固定该物理量的内部单位——**类型即单位语义**, 类间唯一差异是默认单位 |

- 59 个具体属性类清单(`PropertyUnits.h:155` 起, 按行序): Acceleration / AmountOfSubstance / Angle / Area / CompressiveStrength / CurrentDensity / Density / DissipationRate / Distance / DynamicViscosity / ElectricalCapacitance / ElectricalConductance / ElectricalConductivity / ElectricalInductance / ElectricalResistance / ElectricCharge / SurfaceChargeDensity / VolumeChargeDensity / ElectricCurrent / ElectricPotential / Frequency / Force / HeatFlux / InverseArea / InverseLength / InverseVolume / KinematicViscosity / Length / LuminousIntensity / MagneticFieldStrength / MagneticFlux / MagneticFluxDensity / Magnetization / ElectromagneticPotential / Mass / Moment / Pressure / Power / ShearModulus / SpecificEnergy / SpecificHeat / Speed / Stiffness / StiffnessDensity / Stress / Temperature / ThermalConductivity / ThermalExpansionCoefficient / ThermalTransferCoefficient / Time / UltimateTensileStrength / VacuumPermittivity / Velocity / Volume / VolumeFlowRate / VolumetricThermalExpansionCoefficient / Work / YieldStrength / YoungsModulus。其中 Angle / Area / Length / Volume 四个派生自 `PropertyQuantityConstraint`(带边界)。
- 消费侧(图谱导入边): Gui 的 `DlgAddProperty`、`PropertyItem`、`InputField`、`SpinBox`, 以及 Part / PartDesign / TechDraw / FEM / Measure / Spreadsheet / Surface / Robot / CAM / Mesh 等工作台全部导入此头文件——它是整个代码库消费单位系统的 App 侧公共出口。

### 2. Expression 引擎中的单位支持

- 源文件: `src/App/ExpressionParser.h` + `src/App/Expression.cpp`(均属 App 层表达式引擎)。
- `class UnitExpression : Expression`(`ExpressionParser.h:79`, 实现 `Expression.cpp:1232`): 持有 `Base::Quantity` 的表达式节点基类; 四个派生家族全部带单位——`NumberExpression`(`:138`)、`OperatorExpression`(`:190`)、`FunctionExpression`(`:297`)、`VariableExpression`(`:457`)。表达式如 `"1 mm + 2 cm"` 经此求值，算术运算检查兼容量纲；赋值行为由具体写值入口决定，不能把脚本入口的检查推广到表达式入口，详见姊妹篇三通道分析。
- `parseUnit()`(`ExpressionParser.h:635`): 解析单位串; `anyToQuantity()`(`Expression.h:70`): 把 `App::any` 值转 `Base::Quantity`(类型不符抛 `Base::TypeError`)。
- 初始化: `App::UnitExpression::init()` 在 `Application::initApplication` 中调用(`Application.cpp:2353`)。

### 3. Application — 单位系统的装配与生命周期

- 源文件: `src/App/Application.cpp`(接口声明 `Application.h:980`)。
- **注册 Python 模块 `FreeCAD.Units`**(`Application.cpp:528-542`): 创建 `UnitsModuleDef` 挂到主模块, 并用 `Base::InterpreterSingleton::addType` 把 `QuantityPy`、`UnitPy` 两个类型注册进去——Base 层单位 API 暴露给 Python 的装配点(引擎本体在 Base, 组装在 App)。
- **`cleanupUnits()`**(定义 `Application.cpp:1984`, 析构流程 `Application.cpp:2040` 调用): 清空 `FreeCAD.Units` 模块的全部属性, 规避 Python 解释器销毁顺序问题。
- **读取用户偏好**(`Application.cpp:3034-3039`): 从 `User parameter:BaseApp/Preferences/Units` 读 `UserSchema` / `Decimals` / `FracInch`, 调 `Base::UnitsApi::setSchema / setDecimals / setDenominator`——全局单位制状态的初始化点。
- **异常类型注册**: `UnitsMismatchError` 注册进 Python(`Application.cpp:2109`、`:2402`)。

### 4. Document — 文档级单位系统属性

- 源文件: `src/App/Document.cpp`。
- `UnitSystem` 属性(`Document.cpp:992`): 枚举由 `Base::UnitsApi::getDescriptions()` 填充(`:995`), 默认值取用户参数 `UserSchema`(`:999`)——项目/文档级的单位制选择, 与全局偏好独立。

## 一次装配后的调用路径

App 启动: Application 读偏好 → `UnitsApi::setSchema`(全局 schema) → Document 的 `UnitSystem` 属性按 `getDescriptions` 列枚举 → 用户编辑某 `PropertyLength` 时, Gui 按 schema 显示、写入经 `Quantity` 解析回内部单位存 double → 表达式 `"1 mm + 2 cm"` 经 `UnitExpression` 家族求值后按目标属性的单位做一致性检查。

## 设计要点

- 值存储与显示单位分离的收口在 App 层: 属性只存内部单位 double, 换算归 Base 的 `UnitsApi`; `PropertyQuantity` 自身不做换算。
- App 层不复制引擎, 只做三类事: 类型性代码在 PropertyUnits(类型即单位语义)、计算性代码在 Expression(带单位求值)、装配性代码在 Application / Document(模块注册 / 偏好 / 项目单位制)。
- `FreeCAD.Units` 的 Python 模块定义在 Base(`UnitsModulePy`), 但注册动作在 App 的 Application——分层边界清晰: Base 提供、App 装配。

## 阅读路径

- 本册索引(单位系统系列): [00-索引与阅读路线](../单位系统Base层设计/00-索引与阅读路线.md)
- 前篇: [01-Base层单位系统架构](../单位系统Base层设计/01-Base层单位系统架构.md)(Base 层引擎本体)
- 姊妹篇(机制深挖): [单位系统封装与装配](单位系统封装与装配.md)(三通道写值语义/约束裁剪/表达式绑定)
- 姊妹册《参数编辑》为原资料库的外部笔记，本目录未收录；消费侧问题先见[参考入口](../README.md)。
