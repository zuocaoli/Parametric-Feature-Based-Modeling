# 单位系统: Base层单位系统架构

> 日期: 2026-08-31
> 来源: understand-anything 知识图谱问答(图谱提交 `dc2ed9d586`，为原记录版本), 结论经磁盘源码逐行核实(行号随文); 原文的源码版本与行号保留供历史阅读。
> 2026-10-30 课程所需关键结论已对照 `9a274695f3` 校对，范围和更正见[参考入口与校对](../README.md)。

## 背景

问题: Base 层的单位系统由哪些模块组成、各自提供什么功能、分别对应哪些源文件?

图谱将所有相关文件归入「基础库 (Base)」层——即 `FreeCADBase` 库, 无 Qt/OCCT 依赖。这是全部模块的底盘: 单位系统只依赖本层的异常与字符串设施。

## 结论: 6 个模块的概念分工

```
Unit (量纲/单位定义)                 ← 最底层, 不依赖 Quantity
  ↑
Quantity (物理量: 值+单位、解析、格式化、换算)
  ↑          ↖
UnitsSchema (单套单位制的换算+格式化策略)  ← 数据来自三张表(见模块 6)
  ↑
UnitsSchemas (单位制集合, 当前单位制选择)
  ↑
UnitsApi (门面 Facade + 全局状态 + Python 模块 API)
```

上图描述概念分工，不表示所有实现调用都单向；例如 Quantity 的用户显示路径会调用 UnitsApi，再由 schema 读取 Quantity。Unit 的量纲代数不依赖 Quantity，是需要保持的底层边界。

## 模块明细

### 1. Unit — 量纲与单位定义
- 源文件: `src/Base/Unit.h` / `Unit.cpp`
- 核心: `UnitExponents = std::array<int8_t, unitNumExponents>`(`Unit.h:42-43`)——8 个基本量纲的指数数组(长度/质量/时间/电流/热力学温度/物质的量/发光强度/角度, 见 `Unit.h:57` 起的 8 参构造)。
- `class Unit final`(`Unit.h:47`): 提供量纲代数 `*` `/` `pow` `root`/`sqrt`/`cbrt`, 指数越界抛 `OverflowError`/`UnderflowError`（当前校对版本范围为−8至7）; 输出 `getString()`(如 `mm^2`)、`getTypeString()`(如 `"Area"`)、`representation()`。
- 61 个类型常量(`Unit.h:117` 起: `Length`、`Pressure`、`Stress`、`ThermalConductivity`…)——每个物理量类型对应一个基准单位, 也是表驱动: `Unit.cpp:43` 的 `unitSpecs` 表(`UnitSpec{name, exps}`) + `make(name)`(`Unit.cpp:308`)在 `Unit.cpp:318` 起定义全部常量。

### 2. Quantity — 物理量(数值 + 单位)
- 源文件: `src/Base/Quantity.h` / `Quantity.cpp`; 字符串解析器 `Quantity.l`(flex 词法)/ `Quantity.y`(bison 语法)→ 预生成 `Quantity.lex.c` / `Quantity.tab.c`, 由 `Quantity.cpp:572` 直接 `#include "Quantity.tab.c"` 内嵌。
- `class Quantity`(`Quantity.h:111`): `myValue + myUnit`; 算术运算带单位一致性检查(不匹配抛 `UnitsMismatchError`, `Quantity.cpp:132`); `getValueAs(other)` 单位换算(`Quantity.h:190`); `static parse(const std::string&)`(`Quantity.h:165`)把用户输入的 `"1.5 mm"` 解析为 Quantity; 119 个预定义常量(`Metre`、`MilliMetre`、`Newton`…, `Quantity.h:204` 起声明、`Quantity.cpp:348` 起定义)。
- 格式化: `toString` / `toNumber` / `getUserString` 按当前单位制翻译, 由 `QuantityFormat`(`Quantity.h:36`)控制精度、分数分母、千位分隔符等选项。
- 旧版的 `UnitSignature` 概念已移除, 签名解析并入 `Quantity` 解析器。

### 3. UnitsSchema — 单套单位制的翻译策略
- 源文件: `src/Base/UnitsSchema.h` / `UnitsSchema.cpp`
- `class UnitsSchema`(`UnitsSchema.h:40`): 围绕一个 `UnitsSchemaSpec` 工作, `translate(quant[, factor, unitString])`(`UnitsSchema.h:53-54`)把一个 Quantity 按该单位制换算成显示值与单位字符串。自身不含数据, 规则来自规格表(模块 6)。

### 4. UnitsSchemas — 单位制集合与切换
- 源文件: `src/Base/UnitsSchemas.h` / `UnitsSchemas.cpp`
- `class UnitsSchemas`(`UnitsSchemas.h:40`): 持有一整套 `UnitsSchemaSpec`(由 `UnitsSchemasDataPack` 注入), 负责 `select(name/num)`、`names()`、`descriptions()`、`currentSchema()`、`setdefFractDenominator()`。

### 5. UnitsApi — 门面 + 全局状态 + Python 模块
- 源文件: `src/Base/UnitsApi.h` / `UnitsApi.cpp`; Python 侧 `UnitsApiPy.cpp`; 模块契约 `FreeCAD.Units.module.pyi`。
- `class UnitsApi`(`UnitsApi.h:36`): 静态持有 `schemas`(`UnitsApi.h:74`)与全局显示状态 `decimals` / `denominator`(`UnitsApi.h:75-76`)。
- C++ API: `setSchema` / `schemaTranslate` / `toDouble`(按当前单位制解析用户输入)/ `getDescriptions` / `getBasicLengthUnit` / `isMultiUnitAngle` 等。
- Python 模块 `FreeCAD.Units` 的 `parseQuantity`、`listSchemas`、`getSchema`、`setSchema`、`schemaTranslate`、`toNumber` 由 `s*` 静态函数(`UnitsApi.h:79-84`)实现(如 `UnitsApiPy.cpp:37` 的 `sParseQuantity`)。

### 6. 数据/规格表(data-driven 设计)
- `src/Base/UnitsSchemasSpecs.h` — 结构定义: `UnitTranslationSpec{threshold, unitString, factor, fn}`(`:35`)、`UnitsSchemaSpec{num, name, basicLengthUnitStr, isMultUnitLen, isMultUnitAngle, translationSpecs}`(`:43`)、`UnitsSchemasDataPack`(`:61`)。
- `src/Base/UnitsSchemasData.h` — 10 套内置单位制的具体数据(`s0`~`s9`, `:53` 起), 末尾打包成 `unitSchemasDataPack`:

| 序号 | 标识 | 基准长度 | 说明 |
| --- | --- | --- | --- |
| s0 | MmMin | mm | Metric small parts & CNC |
| s1 | MeterDecimal | m | 米制十进制 |
| s2 | ImperialDecimal | in | 英制十进制 |
| s3 | Internal | mm | Standard(mm, kg, s, °), `isDefault=true` |
| s4 | MKS | m | MKS 制 |
| s5 | Centimeter | cm | Building Euro(cm, m², m³) |
| s6 | FEM | mm | FEM(mm, N, s) |
| s7 | Imperial | in | US customary(in, lb) |
| s8 | ImperialBuilding | ft | Building US, `isMultUnitLen=true`(ft-in 多单位长度) |
| s9 | ImperialCivil | ft | Civil Eng, `isMultUnitAngle=true`(mph 多单位角度) |

- `src/Base/UnitsConvData.h` — 英制换算常量(`in=25.4`、`lb=0.45359237`、`psi`…), 供各 schema 数据复用。

### 7. Python 绑定(贯穿模块 1/2/5)
- 存根 `.pyi` 是契约: `Unit.pyi`、`Quantity.pyi`、`FreeCAD.Units.module.pyi`; 手写实现 `UnitPyImp.cpp`、`QuantityPyImp.cpp`、`UnitsApiPy.cpp`; 绑定代码由 `.pyi` 经 `generate_from_py` 构建期生成(生成文件勿手改)。

## 一次换算的调用路径

`Quantity::getUserString()` → `UnitsApi::schemaTranslate()` → `UnitsSchemas::currentSchema()` → `UnitsSchema::translate()` → 按 `Unit::getTypeString()`(如 `"Length"`)查 `UnitsSchemaSpec.translationSpecs` → 返回 `(factor, unitString)` → `Quantity` 组装显示字符串。

## 上层消费者

- App 层: `src/App/PropertyUnits.h`(Quantity 属性)、`src/App/ExpressionParser.h`(表达式带单位运算)。
- Gui 层: `src/Gui/QuantitySpinBox.cpp`(参数编辑框, 详见姊妹册《参数编辑》)、`src/Gui/Dialogs/DlgUnitsCalculator`(单位换算对话框)。
- 各工作台: Sketcher 约束、Part 对话框、FEM、Spreadsheet(`DisplayUnit`)等, 经 C++ 或 `FreeCAD.Units` 使用。

## 设计要点

- 量纲代数(Unit)与数值/格式(Quantity)分离; Unit 不依赖 Quantity, 是纯量纲模块。
- 单位制完全数据驱动: 新增单位制只改 `UnitsSchemasData.h`(加一个 `UnitsSchemaSpec` + 编号)。
- 全局「当前单位制 + 小数位 + 分数分母」状态收敛在 `UnitsApi` 一处, 解析与格式化都从这里取策略。

## 阅读路径

- 本册索引: [00-索引与阅读路线](00-索引与阅读路线.md)
- 后篇: [单位系统App层组成](../单位系统App层设计/单位系统App层组成.md)(上层消费与装配)
- 姊妹册《参数编辑》为原资料库的外部笔记，本目录未收录；消费侧问题先见[参考入口](../README.md)。
