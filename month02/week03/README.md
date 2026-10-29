# 第二个月第 3 周：求值与属性读取

## 本周训练

- **承接与前置**：给[上周](../week02/README.md)的 AST 接上第一月的属性查询接口；会递归遍历并通过回调读取 variant 值。
- **目标**：能将解析与求值错误区分到具体阶段；能通过 Resolver 读取属性，并证明求值本身不修改模型。
- **独立练习**：通过本周接口将 A 设置为 120×60，计算 `length / 2 + 18 / 3 / 2`，记录 width；再计算 `length / (segments-1)`，将 segments 设为 1 后观察错误，再改为 3 修复。
- **验收**：首式为 63，segments=1 时返回 evaluation 除零错误、改为 3 后为 60；所有求值都不写回 width。解释为何 bool/string 不能直接参与数值求值。

## 31. 如何递归执行 AST？

`evaluate(const Node &node, const Resolver &resolve)` 的目的，是把以 node 为根的 AST 子树计算成一个数值。node 提供表达式结构，resolve 提供“变量名 → 属性值”的查询回调；返回的 `std::expected<double, Error>` 在成功时保存 double，失败时保存带阶段、字节位置和原因的 Error。

函数先记录当前 Token 的字节位置，供错误定位，再按节点类型处理：

| 节点类型 | 求值过程 |
|---|---|
| Number | 读取 `node.token.number`，检查是有限数后返回。 |
| Variable | 调用 `resolve(变量名)`；未提供回调或查不到属性时报错。double 直接使用，int64 转为 double，其他类型报非数值错误。 |
| Unary | 检查并递归计算 `left` 中的唯一操作数，再执行正号或负号。 |
| Binary | 检查两个子节点，先递归计算左侧，成功后再计算右侧，最后按当前运算符执行加减乘除；除法先检查分母是否为零。 |

递归调用返回的 `left`、`right` 是计算结果 `std::expected<double, Error>`，不是子节点指针：`if (!left) return left;` 表示子树失败就原样传回错误，成功后才通过 `*left` 取出数值。`finite()` 检查输入数值及各步运算结果，拒绝 NaN 和无穷。

例如 `length / 2 + 5` 的 AST 根节点是 `+`：先进入左子树 `/`，查询 length 得到 100、读取数字 2，返回 50；再读取右侧数字 5，根节点最终返回 55。

运算优先级已由 Parser 编入树结构，evaluate 按树执行，并检查未知变量、非数值属性、除零和非有限结果。它读取 AST，通过本周只读的 Resolver 读取属性，不把结果写回 width；从表达式文本开始的调用由 `calculate()` 串联 parse 与 evaluate。

代码：[evaluate()](evaluator.cpp)、[求值接口](evaluator.cppm)。

## 32. 如何读取属性，又不依赖矩形类？

Evaluator 接受 Resolver 回调，只要求按名字返回 optional<Value>。应用层用 `object.get(name)` 适配统一 PropertyObject 接口；求值器不知道 Rectangle，也不持有对象。本周沿用第一月属性模块的独立副本，矩形的 area 仍是即时计算属性。

代码：[Resolver](evaluator.cppm)、[应用层适配](main.cpp)、[get()](property.cpp)。

## 33. 未知属性和不同类型如何处理？

空 optional 是未知变量；double 直接读取，int64 转成 double；bool、string 明确报错，不隐式转换。名字区分大小写，length 是键，Length 是显示标签。统一浮点运算简化引擎，代价是超过 double 精确整数范围时可能舍入。

代码：[evaluate() 的变量分支](evaluator.cpp)。

## 34. 如何处理除零及非有限值？

除法检查精确零（包括负零），不以 epsilon 拒绝合法的小数。读取和每次运算后都检查 isfinite，拒绝 NaN、无穷与溢出。错误分别属于 lexical、syntax、evaluation 阶段；变量错误指向名字，除零和溢出指向运算符。

代码：[finite() 与除法分支](evaluator.cpp)、[错误结构](lexer.cppm)。

## 35. 如何串联并验证？

calculate 先 parse，成功才 evaluate，不丢失前一阶段的错误。用常量、矩形浮点属性、整数属性验证结果，并测试优先级、结合性、空白、错误类型、极值和零除。核对 width 保持 1，证明“求值”和“写回”分开。

`evaluator.cppm` 是求值模块的公开接口：定义属性查询回调类型 `Resolver`，声明计算已有 AST 的 `evaluate()` 和从表达式文本开始计算的 `calculate()`。`evaluator.cpp` 实现这些函数；`main.cpp` 通过 `import month02.week03.evaluator;` 使用它们，并提供矩形属性的查询回调。接口同时重新导出 Parser 和 Property 模块，供调用方使用相关的 `Node`、`Error`、`Value` 等类型。

代码：[calculate()](evaluator.cpp)。

## 单独打开与运行

用 IDE 打开本周目录，在 [main.cpp](main.cpp) 的 `input` 中填写表达式，然后在该目录运行 `./run.sh`：构建、更新代码跳转信息并运行本周示例。表达式直接写在代码中，不通过命令行输入；修改后需重新编译运行。依赖版本与 IDE 设置见[工具说明](../../docs/tooling.md)。

`run.sh` 成功时只显示程序输出；构建、跳转信息更新或验证失败时显示该步诊断并停止。模块接口变化后，已有 IDE 会话需重启 clangd。

## 代码与运行

Lexer/Parser 是前两周的本周副本，Property/Rectangle 是第一月第四周的本周副本。所有 import 指向 month02.week03 模块，无跨周源码依赖。新增 evaluator.cppm/evaluator.cpp 负责求值，main.cpp 演示；不新增第三方库。

环境：GCC 16.2.1（含 libstdc++ std 模块）、C++23、Python 3.14.7、clang-format / clangd / Clang 22.1.8；详见[工具文档](../../docs/tooling.md)。在本课程目录执行：

```bash
python3 tools/builder.py build
./run.sh month02/week03
python3 tools/builder.py check-format
```

默认输出 `length / 2 + 5 = 55`。验证除零错误时，将 main.cpp 中的 `input` 改为 `"length / 0"`，重新运行 `./run.sh month02/week03`；预期输出 `evaluation at byte 7: division by zero`，退出码为 1。
