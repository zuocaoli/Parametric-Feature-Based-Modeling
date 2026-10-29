# 第二个月第 2 周：语法分析

## 本周训练

- **承接与前置**：将[上周](../week01/README.md)的 Token 组织成计算结构；复习递归、树、unique_ptr 与运算优先级。
- **目标**：能为表达式画出或写出 AST；能用树形证明优先级、左结合和一元符号的处理顺序。
- **独立练习**：手工推导 `length / 2 + 18 / 3 / 2` 的树，将 `main.cpp` 中的 `expression` 改为该表达式，再在本周目录运行 `./run.sh`；增加括号改变结合顺序，并测试缺右括号后的修复。每次修改后重新运行脚本以编译并查看结果。
- **验收**：原树为 `(+ (/ length 2) (/ (/ 18 3) 2))`，能指出 `18/(3/2)` 的不同树形；缺括号返回 syntax 错误，补齐后成功。此周只验证结构，求值留到下周。

## 26. 为什么需要 AST，有哪些节点？

Token 只记录顺序，AST 保存计算结构。节点分数字、变量、一元、二元四种，保留原 Token 以定位错误；unique_ptr 独占子节点，树销毁时自动释放。括号只影响树形，不需要单独节点。代价是每个节点一次分配。

`NodeKind` 是 AST 节点的分类标签，存放在 `Node::kind` 中：

| 枚举值 | 含义与例子 | 子节点 |
| --- | --- | --- |
| `Number` | 数字字面量，如 `2` | 无 |
| `Variable` | 变量引用，如 `length` | 无 |
| `Unary` | 一个操作数的运算，如 `-length`、`+2` | `left` 保存唯一操作数，`right` 为空 |
| `Binary` | 两个操作数的运算，如 `length + 2`、`3 * 4` | `left`、`right` 保存左右操作数 |

例如 `-length + 2` 的根节点是 `Binary`（`+`），左边是 `Unary`（`-`，其子节点是 `Variable`），右边是 `Number`。`kind` 表示节点类别，`token` 则记录具体的数字、变量名或运算符；所以 `+` 和 `-` 都可能对应一元或二元节点，取决于上下文。

`Node` 表示 AST 中的一个实际节点，把分类、内容和子节点连接放在一起：

| 成员 | 作用 |
| --- | --- |
| `kind` | 节点类别，即上面的 `NodeKind` |
| `token` | 当前节点对应的 Token，包含文本、源代码字节位置和数字值等信息 |
| `left` | 独占左子节点；一元运算的唯一操作数也存在这里 |
| `right` | 独占右子节点；仅二元运算使用 |
| `height` | 以当前节点为根的子树高度，叶子为 1；用于限制树高，最多 128 层 |

例如 `length + 2` 由三个 `Node` 组成：根节点保存 `Binary` 和 `+` 的 Token，`left` 指向保存 `length` 的变量节点，`right` 指向保存 `2` 的数字节点。两个叶子的子指针都为空，高度为 1；根的高度为 2。一般有 `height = 1 + max(左子树高度, 右子树高度)`，空子树高度按 0 计算，由 `Parser::make()` 设置。

子节点也使用 `Node` 类型，因此可以逐层组成整棵树。`std::unique_ptr<Node>` 管理子节点的生命周期，父节点销毁时会自动释放其子树。本周的 `Node` 只保存表达式结构，不执行求值。

`using Tree = std::unique_ptr<Node>;` 为智能指针类型起了别名：`Tree tree;` 与 `std::unique_ptr<Node> tree;` 完全等价，默认都为空。`Node` 是一个节点，`Tree` 则通过独占拥有根节点来拥有整棵 AST 或一棵子树。因此 `tree->kind` 访问根节点类型，`*tree` 得到根节点对象（都要求指针非空）；左右子树从根节点的 `left`、`right` 继续访问。`Tree` 不能复制，只能移动来转移所有权；销毁时自动释放根节点及其子树，无需手动 `delete`。`parse()` 成功时返回的就是装在 `std::expected` 中的 `Tree`。

选择 `unique_ptr` 是因为本周 AST 的每个子节点只归一个父节点所有，根节点则归调用者持有的 `Tree` 所有：

- **用指针连接递归结构**：不能在 `Node` 内直接声明 `Node left;`，否则对象需要无限嵌套自身，无法确定大小。指针大小固定，也能用空指针表示没有子节点。
- **自动释放**：正常退出或解析失败抛异常时，已构建的子树都会随拥有它的 `unique_ptr` 销毁。若用裸指针管理，就需要另行保证所有路径都正确释放。
- **表达唯一所有权**：不允许复制，组装新父节点时用 `std::move(left)`、`std::move(right)` 转交子树；成功移动后原指针为空，节点本身没有被复制。只读遍历可借用 `const Node &`，不需要转交所有权。
- **避免不需要的共享成本**：本周没有多个所有者共享同一节点的需求，因此无需 `shared_ptr` 的控制块和引用计数。代价仍是每个节点单独分配；复制整棵树需要显式深拷贝。

代码：[Node 与 Tree](parser.cppm)、[Parser::make()](parser.cpp)。

## 27. 如何表达优先级？

递归下降分四层：`parse_additive()` 处理加减，`parse_multiplicative()` 处理乘除，`parse_unary()` 处理正负号，`parse_primary()` 处理数字、变量及括号。上层先调用下层，乘除自然先形成子树。

`parse_additive()` 按语法层命名是合适的：它负责连接加减运算，操作数交给更高优先级的解析函数，因此也能解析没有加减号的 `3 * 4` 或 `42`。当前加减是最低优先级，所以表达式从这一层开始解析；完整输入由 `parse_complete_expression()` 解析并检查末尾，对外文本入口则是 `parse(source)`。若以后加入比较、赋值等更低优先级运算，可增加 `parse_expression()` 作为统一的内部表达式入口，让完整输入和括号内表达式都调用它，再由它转入最低优先级层；保留 `parse_additive()` 专门表示加减层。

`Tree parse_additive()` 返回从当前游标开始解析出的表达式子树，`Tree` 独占其根节点，不执行数值计算。它先用 `parse_multiplicative()` 得到第一项的树并保存到 `tree`；每遇到一个 `+` 或 `-`，就读取运算符、用 `parse_multiplicative()` 解析右项，再创建新的二元节点，以旧 `tree` 和右项为左右子树，最后返回更新后的 `tree`。例如 `2 + 3 * 4 - 5` 中，`tree` 依次表示 `2`、`(+ 2 (* 3 4))`、`(- (+ 2 (* 3 4)) 5)`；若没有加减号，就直接返回第一项的树，例如 `3 * 4` 返回 `(* 3 4)`。

`parse_additive()` 遇到当前层不再是加减号时就返回，不要求到达输入末尾；例如在 `(2 + 3) * 4` 中，括号内的调用返回 `(+ 2 3)`，并把右括号留给 `parse_primary()` 检查和消费。输入 `1 2` 时，它也会先返回 `1` 的树，随后由 `parse_complete_expression()` 检查并拒绝多余的 `2`。因此只有通过 `parse_complete_expression()` 的末尾检查，才能确认整条输入已完整解析。

`Tree parse_multiplicative()` 返回当前项的子树：先用 `parse_unary()` 解析第一个操作数，再循环读取 `*` 或 `/`，每次用 `parse_unary()` 解析右操作数，并把旧树和右操作数组成新的二元节点。例如 `8 / 4 * 2 + 1` 中，`tree` 依次表示 `8`、`(/ 8 4)`、`(* (/ 8 4) 2)`；遇到 `+` 后返回最后这棵树，把加号留给 `parse_additive()` 处理。若没有乘除号，则直接返回第一个操作数的树。操作数也可以是一元表达式或括号表达式，例如 `-2 * (3 + 4)` 返回 `(* (- 2) (+ 3 4))`。`parse_multiplicative()` 只构建结构，不计算数值；乘除先在这里组成子树，外层 `parse_additive()` 再连接加减，从而体现优先级。

代码：[parse_additive()/parse_multiplicative()](parser.cpp)。语法如下：

```text
expression = term { ('+' | '-') term }
term       = unary { ('*' | '/') unary }
unary      = ('+' | '-') unary | primary
primary    = number | identifier | '(' expression ')'
```

## 28. 如何保证左结合？

同级运算用循环，每次把旧树作为新节点的左孩子。因此 `8 / 4 / 2` 生成 `(/ (/ 8 4) 2)`；若对整个右侧递归，则容易变成右结合。加减与乘除采用相同结构。

`parse_multiplicative()` 中的 `tree` 保存的是目前已解析的整棵乘除子树，不只是最初的操作数。关键语句 `tree = make_binary(std::move(op), std::move(tree), std::move(right));` 将旧 `tree` 和本次 `right` 的所有权交给新节点，再让 `tree` 接管新根；节点没有被复制，也不会丢失旧子树。例如解析 `8 / 4 * 2` 时，第一次循环构建以 `/` 为根的树，第二次构建以 `*` 为根的树，并将整棵 `/` 子树作为其左孩子，因此结构对应 `(8 / 4) * 2`。右侧调用 `parse_unary()` 只解析当前操作数，后续同级乘除仍由本层循环处理，这正是左结合的关键。

代码：[parse_additive()/parse_multiplicative() 的循环](parser.cpp)。

## 29. 如何处理括号和一元符号？

一元符号递归解析一元操作数，允许 `+--length`；括号内重新进入 `parse_additive()`，并要求闭括号。括号和一元递归最多 128 层，生成的树高也限制为 128，避免极深输入耗尽栈。

代码：[parse_unary()/parse_primary()/make()](parser.cpp)。

## 30. 如何发现不完整或多余输入？

`parse_primary()` 必须读到数字、变量或左括号，否则在当前 Token 位置失败；缺右括号报告当前位置，End 使用输入长度。`parse_complete_expression()` 只接受根节点后为 End，因此 `1 2`、`length(2)` 不会只解析前半段。内部 ParseFailure 用来退出递归，公共接口统一返回 expected 错误。

`Tree parse_complete_expression()` 成功返回整条表达式的 AST，由 `Tree`（即 `std::unique_ptr<Node>`）独占根节点，不返回计算出的数值。`auto result = parse_additive();` 先构建树，随后检查当前 Token 是否为 `End`，确认没有多余输入才 `return result;`。例如 `2 + 3 * 4` 返回以 `+` 为根、左子树为 `2`、右子树为 `3 * 4` 的树，`describe(*tree)` 可将它显示为 `(+ 2 (* 3 4))`，并未计算出 `14`。返回局部变量 `result` 时，编译器可直接在返回位置构造它（NRVO）；未进行 NRVO 时会隐式移动，均不会复制整棵树。若表达式不完整或有多余 Token，函数抛出 `ParseFailure`，由 `parse()` 转换为错误结果，不返回部分树。

`std::expected<Tree, Error> parse(std::string_view source)` 是公开的解析入口：接收表达式文本的只读视图（不复制输入字符串），先调用 `lex()` 分词，再按优先级、结合性和括号构建完整 AST。成功时返回持有根节点的 `Tree`；词法或语法失败时返回 `Error`，包含阶段、从 0 开始的字节位置和错误说明。`expected` 每次只保存成功值或错误之一。

例如 `parse("length + 2 * 3")` 得到 `(+ length (* 2 3))` 对应的树；`parse("length +")` 返回语法错误。此函数只检查表达式结构，不查询变量值，也不计算结果，因此 `parse("1 / 0")` 仍可成功生成树。

`parse()` 的实现按以下顺序衔接两个阶段：

1. `auto tokens = lex(source);` 得到 `std::expected<std::vector<Token>, Error>`。成功保存 Token 序列，失败保存词法错误。
2. `if (!tokens)` 表示词法失败；`tokens.error()` 取出错误，`std::unexpected(...)` 将它包装为返回结果的错误分支。这里是普通返回，不是抛异常。
3. `Parser{std::move(*tokens)}.parse_complete_expression()` 中，`*tokens` 取出 Token 序列，`std::move` 允许构造函数移动它，交给临时 `Parser` 保存，避免复制整个序列。`parse_complete_expression()` 构建表达式树并检查末尾是否为 `End`；成功返回的 `Tree` 被移动进 `expected` 的成功分支。
4. 如果语法分析抛出 `ParseFailure`，`catch` 捕获它，再通过 `std::unexpected(failure.error)` 返回错误分支。内部异常帮助退出多层递归，对外调用者用同一种 `expected` 接口处理词法和语法错误；这里的 `catch` 只捕获 `ParseFailure`。

这里转交 Token 序列，是因为分词结果接下来只供 `Parser` 使用。`tokens` 是一个局部 `expected`，`*tokens` 是其内部 `std::vector<Token>` 的左值引用；直接写 `Parser{*tokens}` 会把整个 vector 复制到构造函数的按值参数中。`std::move(*tokens)` 将它转换为右值引用，使参数通过移动构造接管原 vector 的缓冲区，避免逐个复制 Token 及其字符串。构造函数中的 `tokens_(std::move(tokens))` 再将参数的缓冲区移交给成员；这两处 `std::move` 本身都只是转换，实际转移发生在 vector 的移动构造中。移动后，外层 `expected` 仍保存成功分支，但内部 vector 已处于有效但状态未指定的状态，本函数不再读取它。去掉调用处的 `std::move` 仍能正常解析，只是会多一次完整复制。

调用方式与本周 `main.cpp` 一致：

```cpp
auto result = parse("length + 2 * 3");
if (result) {
    std::cout << describe(**result) << '\n'; // 输出 (+ length (* 2 3))
} else {
    const auto &error = result.error();
    std::cerr << stage_name(error.stage) << " at byte " << error.position << ": "
              << error.message << '\n';
}
```

`if (result)` 检查是否保存成功值；第一次 `*result` 取出 `Tree`，第二次解引用取得根节点 `Node`，所以 `describe()` 接收 `**result`。

代码：[parse_complete_expression()/fail()/parse()](parser.cpp)。

## 单独打开与运行

用 IDE 打开本周目录，直接在 `main.cpp` 的 `expression` 中填写表达式，在该目录运行 `./run.sh`：构建、更新代码跳转信息并运行本周示例。表达式不通过命令行输入，修改后需要重新编译。依赖版本与 IDE 设置见[工具说明](../../docs/tooling.md)。

## 代码与运行

Lexer 是第一周实现的本周副本，Parser 只 import 本周模块，独立构建；`parser.cppm` 定义 AST，`parser.cpp` 实现语法，`main.cpp` 用前缀形式显示 AST。本周不求值。

环境：GCC 16.2.1（含 libstdc++ std 模块）、C++23、Python 3.14.7、clang-format / clangd / Clang 22.1.8；无第三方库。详情见[工具文档](../../docs/tooling.md)。在本课程目录执行：

```bash
python3 tools/builder.py build
./run.sh month02/week02
python3 tools/builder.py check-format
```

`main.cpp` 中的输入为：

```cpp
constexpr std::string_view expression = "-(2 + 3) * 4";
```

默认输出 `(* (- (+ 2 3)) 4)`。手工验证错误处理时，将 `expression` 改为 `"1 2"`，重新构建并运行，预期输出 `syntax at byte 2: unexpected trailing token`，退出码为 1；验证后恢复原表达式并重新构建。
