# 第二个月第 4 周：绑定与整合

本周的核心目标是：**把表达式求值接入属性系统，形成可保存、恢复并继续计算的参数化矩形。**

需要掌握四点：

1. **公式与数值分离**：区分公式文本、AST 和属性当前值，理解绑定不等于立即计算。
2. **显式计算与写回**：按 `width → area` 顺序计算，通过统一属性接口校验、更新并通知，理解逆序为何读到旧值。
3. **失败保护**：绑定失败保留旧公式，计算失败保留最近成功值。
4. **保存与恢复**：保存数值和公式，加载时重建 AST，之后仍能继续计算。

## 本周训练

- **承接与前置**：把[上周](../week03/README.md)的数值结果通过属性接口写回，并保存公式；会使用流与对象生命周期规则。本周 area 转为存储值，由公式维护。
- **独立练习**：创建 A（120×60），绑定 width=`length/2`、area=`length*width` 并手动计算；length 改为 144 后先算 area 再算 width，记录旧结果，然后重新按 width→area 计算；尝试坏公式并修复。
- **验收**：逆序第一次 area 为 8640，重排后 width/area 为 72/10368；语法错误不替换原公式，计算失败保留最近成功值。

## 月末综合验收与衔接

独立完成“绑定两式 → 指定顺序计算 → 修改 length → 重算 → 保存加载 → 继续计算”，将练习文件放在本周 `build/`；加载后核对公式和 72/10368，解释加载不自动计算。另把 width 绑定为 `length/0`，证明失败保留旧值，再恢复 `length/2` 并计算。

说明公式文本与数值分开保存的理由，并展示逆序计算的旧值问题。[第三月](../../month03/week01/README.md)将从 AST 提取关系，使调用者无需手工安排顺序；第二月文件仍作为本阶段的独立快照保留。

## 36. 如何分别保存公式和数值？

PropertyObject 保存当前值，FormulaEngine 的名字到 Binding 映射另存源文本与独占 AST。绑定不计算，计算失败时当前值仍表示最近一次成功结果；直接手动赋值也允许，不会解除公式。本周独立矩形将 area 改为存储型 double，第一月实现保留。

`formulas.cppm` 声明的 `FormulaEngine` 负责管理一个属性对象的公式绑定，并在显式调用时求值、写回。接口实现在 `formulas.cpp`：

| 接口 | 功能 |
| --- | --- |
| `FormulaEngine(object)` | 借用属性对象，作为读取变量和写回结果的入口；对象必须比引擎存活更久。 |
| `bind(target, source)` | 校验目标并解析公式，成功后新增或替换绑定；不立即计算。返回成功或 `Error`。 |
| `unbind(target)` | 删除绑定，保留属性当前值；返回是否删除了绑定。 |
| `formula(target)` | 返回指定目标的公式文本副本；未绑定时返回 `std::nullopt`。 |
| `formulas()` | 返回全部“目标名、公式文本”副本，供保存等操作使用。 |
| `compute(target)` | 用当前属性值求值并写回目标，返回 `Changed`、`Unchanged` 或 `Error`。 |

内部 `object_` 是属性对象的引用；`bindings_` 按目标属性名保存 `Binding`，其中 `source` 保留原始公式文本，`tree` 独占解析后的 AST，计算时可直接复用。引擎禁止复制构造和复制赋值。

例如 `bind("width", "length / 2")` 只建立绑定；当 length 为 100 时，调用 `compute("width")` 才会把 width 写为 50。之后 length 改为 200，width 仍为 50，直到再次调用 compute 才变为 100。本周引擎不自动重算或安排依赖顺序，文件读写由 storage 模块负责。

代码：[Binding 与引擎接口](formulas.cppm)、[存储型面积](rectangle.cpp)、[PropertyObject](property.cppm)。

## 37. 如何校验、替换和解除？

bind 先检查目标存在、可写且为 double，再解析文本，全部成功才替换旧绑定。语法失败保留旧公式；变量存在性和取值留到 compute 时判断，因而绑定 `missing / 2` 可以成功。unbind 删除公式与 AST，保留当前值，重复解除返回 false。

目标必须是 double 是本周的类型约定：evaluate 始终返回 double，compute 将结果直接交给 set，而属性写入要求新旧值的 variant 类型一致，不做隐式转换。提前限制目标类型可避免绑定成功后必然写入失败，也无需引入小数转整数时的舍入与溢出规则。公式引用的输入属性可以是 int64（求值时转为 double），但输出目标仍须是 double；支持整数、布尔或字符串目标需要另行设计结果类型和转换规则。

例如 `bind("width", "length / 2")`：先从属性描述列表中找到 width，检查只读标记和当前值类型，再把公式解析为 AST。成功后，Binding 拥有公式文本副本和转移进来的 AST，最后通过 `insert_or_assign` 新增或替换 width 的绑定。把修改绑定表放在校验与解析之后，可保证这些检查失败时旧绑定仍可使用；整个过程不修改 width 的数值。

代码：[bind()/unbind()](formulas.cpp)。

## 38. 如何指定顺序并写回？

调用者先 compute(width)，再 compute(area)。求值通过 object.get 读取当前值，成功后只用 object.set 写回，不访问属性内部存储。引擎不会递归运行其他公式，也不订阅变化进行自动重算；错误顺序会读取旧 width，调用者必须负责顺序。

代码：[compute()](formulas.cpp)、[compute_in_order()](main.cpp)。

## 39. 如何保留原值及通知规则？

解析失败不替换绑定，求值失败不调用 set，set 拒绝非法尺寸或面积时不提交。只有 Changed/Unchanged 视为成功；通知沿用统一入口的提交后发送、相同值不通知、异常隔离与拒绝重入规则。width 必须有限且大于零，area 有限且非负；`-1` 能绑定但不能写入这两种属性。

代码：[compute() 错误处理](formulas.cpp)、[set_many()](property.cpp)、[通知](change_signal.cpp)。

## 40. 如何保存恢复并验证完整流程？

版本化文本保存单个矩形的六个属性（带类型）和公式源文本，不保存 AST、订阅者或对象身份。double 用 max_digits10，字符串与公式用 quoted 转义。load_model 在新模型上验证属性、重新解析公式，成功才返回；失败不会部分覆盖已有模型，加载也不自动计算。

加载公式时，局部集合 `targets` 记录文件中已出现的目标属性名；同一目标再次出现就报错，避免 `bind` 的替换行为掩盖文件中的重复记录。它只用于本次加载校验，正式绑定由 `engine` 保存。

`RectangleModel` 把矩形属性和维护这些属性的公式引擎组合成一个完整的参数化矩形，作为保存、加载和使用的统一单位：

- `rectangle` 保存属性当前值，`engine` 保存公式绑定并负责显式计算。
- 构造函数 `: rectangle(length, width), engine(rectangle)` 让引擎引用本模型的矩形。成员按声明顺序构造、逆序销毁，因此必须先声明 rectangle，再声明 engine，保证矩形在引擎存活期间始终有效。
- `save_model` 同时读取这两个成员，保存数值和公式；`load_model` 在新模型上恢复二者，全部校验成功后才交给调用者。构造模型本身不会绑定公式或自动计算。
- `load_model` 返回 `unique_ptr<RectangleModel>`，转移指针所有权时模型本体地址保持不变，引擎引用的矩形也保持原位。当前模型受 FormulaEngine 的限制，不能直接复制或移动。

最小格式适合教学，代价是仅支持这一种模型、无迁移或完整文档持久化；写文件失败可能留下部分文件，本例不提供原子文件替换。

代码：[RectangleModel 与持久化接口](storage.cppm)、[save_model()/load_model()](storage.cpp)、[文件演示](main.cpp)。

## 单独打开与运行

用 IDE 打开本周目录，在该目录运行 `./run.sh`：构建、更新代码跳转信息并运行本周示例。依赖版本与 IDE 设置见[工具说明](../../docs/tooling.md)。

## 代码与运行

Lexer、Parser、Evaluator 和 Property 均是本周独立副本。新增 formulas 模块负责绑定与手动计算，storage 模块负责矩形快照；所有 import 仅依赖本周。单线程，FormulaEngine 借用对象；独立使用引擎时须保证对象活得更久。

环境：GCC 16.2.1（含 libstdc++ std 模块）、C++23、Python 3.14.7、clang-format / clangd / Clang 22.1.8；仅用标准库，详见[工具文档](../../docs/tooling.md)。在本课程目录执行：

```bash
python3 tools/builder.py build
./run.sh month02/week04
python3 tools/builder.py check-format
```

演示输出两轮计算、保持旧值、通知次数、非法替换及真实文件恢复：

```text
width=50, area=5000
after length=200, before compute: width=50, area=5000
width=100, area=20000
repeat=Unchanged, notifications=5
syntax at byte 8: expected a number, variable or '('; old formula preserved
saved and restored month02/week04/build/rectangle.txt
loaded: width=100, area=20000
```

程序参数直接写在代码中，不从命令行读取。公式、修改后的 length 和快照路径均在 `main.cpp` 中设置；需要调整时修改代码并重新编译运行。快照固定写入已忽略的 `build/rectangle.txt`（本周目录内，路径由源码位置推导，与运行目录无关）。运行后按上述输出核对往返结果，并检查生成快照中保存的数值和公式文本。
