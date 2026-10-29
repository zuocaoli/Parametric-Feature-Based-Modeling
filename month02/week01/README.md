# 第二个月第 1 周：词法分析

## 本周训练

- **承接与前置**：[第一月末](../../month01/week04/README.md)已有安全属性入口，本周先把描述比例的文本拆成 Token；会使用字符串、游标和 expected 错误返回。
- **目标**：能手工列出 Token 的种类、文本与字节位置；能解释数字边界和非法输入的错误位置。
- **独立练习**：先标注 `length / 2 + .25` 的 Token，再运行 `./build/demo 'length / 2 + .25'`；把末尾数字换成 `2..5`，定位错误后改为 `2.5` 重试（先按下方命令构建）。
- **验收**：合法输入依次得到 Identifier、Slash、Number、Plus、Number、End，起点为 0、7、9、11、13、16；坏数字拒绝且位置可解释，修复后成功。此周只验证词法，不计算 width。

## 21. 为什么先拆 Token，保存什么信息？

Token 把文本分成可识别的单元，后续 Parser 只关心种类与顺序。保存种类、拥有所有权的文本、字节位置及数字值；End 表示输入结束。复制短文本避免输入销毁后的悬空引用，代价是少量分配。

代码：[Token 与错误类型](lexer.cppm)、[lex()](lexer.cpp)。

## 22. 如何识别整数、小数和边界？

先扫描数字，再允许一个小数点及其后数字，至少出现一位数字。支持 `12`、`12.5`、`.5`、`2.`；负号单独产生 Token。用 `from_chars` 转成有限 `double`，不依赖地区格式；紧邻的字母或第二个小数点报错，不支持科学计数法。

代码：[lex() 的数字分支](lexer.cpp)。

## 23. 如何区分名字、符号和空白？

名字遵循 `[A-Za-z_][A-Za-z0-9_]*`，保留大小写。六种 ASCII 空白直接跳过，八种运算符和括号各生成一个 Token。名字使用实际属性键，不将显示标签转换为变量名。

代码：[字符判断及符号 switch](lexer.cpp)。

## 24. 如何定位错误？

扫描游标就是从零开始的字节偏移。非法字符标出当前游标，非法数字后缀标出后缀起点，数字越界标出数字起点；以 `expected` 返回错误，不交出半份 Token。位置是字节数，本例名字仅支持 ASCII。

代码：[Error](lexer.cppm)、[错误分支](lexer.cpp)、[命令行错误输出](main.cpp)。

## 25. 如何验证 Lexer？

运行演示并手工核对 Token 的种类、文本、数值和位置。例如 `length / 2 + .25` 应得到六个 Token（含 End）；`2..5` 应在字节 2 报非法数字后缀。空输入在词法层合法，仅得到 End；是否缺少表达式由下一周判断。

## 单独打开与运行

用 IDE 打开本周目录，在该目录运行 `./run.sh`：构建、更新代码跳转信息并运行本周示例。依赖版本与 IDE 设置见[工具说明](../../docs/tooling.md)。

程序用法为 `demo [expression]`，最多接受一个表达式参数；本周 `run.sh` 会原样转交参数。在本周目录使用：

| 命令 | 行为 |
| --- | --- |
| `./run.sh` | 分析默认表达式 `length / 2 + 5` |
| `./run.sh 'length / 2 + .25'` | 分析传入的表达式，输出各 Token 的种类、文本、字节位置及数字值 |

表达式应整体加引号，作为一个参数传入，避免空格拆分参数或 Shell 解释 `*`、括号等字符。本周只做词法分析，不计算表达式结果。传入多个参数或词法分析失败时退出码为 1，成功时为 0。

## 代码与运行

本周独立命名模块 `month02.week01.lexer`，无第三方依赖。`lexer.cppm` 声明接口，`lexer.cpp` 实现扫描，`main.cpp` 演示。

环境：GCC 16.2.1（含 libstdc++ std 模块）、C++23、Python 3.14.7；格式与编辑器使用 clang-format / clangd / Clang 22.1.8。安装与公共环境说明见[工具文档](../../docs/tooling.md)。在本课程目录执行：

```bash
python3 tools/builder.py build
./run.sh month02/week01
python3 tools/builder.py check-format
./build/demo 'length @ 2'
```

默认演示输出六行，从 `Identifier 'length' @0` 到 `End '' @14`。最后的非法输入命令输出 `lexical at byte 7: illegal character`，退出码为 1。
