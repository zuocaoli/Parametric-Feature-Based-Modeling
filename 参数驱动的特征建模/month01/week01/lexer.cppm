export module month03.week04.lexer;

import std;

export namespace month03::week04 {

//
enum class ErrorStage {
    Lexical,    // 词法错误：字符或数字字面量无法拆成合法 Token。
    Syntax,     // 语法错误：Token 顺序不符合表达式语法。
    Evaluation, // 求值错误：变量无法读取、除零或结果不是有限数。
    Binding,    // 绑定错误：目标属性或计算结果无法写入属性。
    Storage     // 存储错误：模型快照保存或加载失败。
};

// 表示一次处理失败的错误信息
struct Error {
    ErrorStage stage;     // 错误发生在哪个阶段，例如词法分析.
    std::size_t position; // 错误在输入文本中的字节位置，从 0 开始.
    std::string message;  // 具体原因，例如 illegal character.
};

std::string_view stage_name(ErrorStage stage);

// TokenKind 表示 Token 的类别，让后续解析器知道读到的是数字、名字、运算符、括号，还是输入结束
enum class TokenKind {
    Number,     // 数字字面量，如 12、.5 或 2.。
    Identifier, // 名字，如 length 或 width。
    Plus,       // 加号 +。
    Minus,      // 减号 -；负号也先作为独立 Token。
    Star,       // 乘号 *。
    Slash,      // 除号 /。
    LeftParen,  // 左括号 (。
    RightParen, // 右括号 )。
    End         // 输入结束标记，不对应实际字符。
};

// 表示 Lexer 从输入中识别出的一个词法单元.
struct Token {
    TokenKind kind;       // Token 的类别，如数字、名字或加号
    std::string text;     // 从输入中识别出的原始文本；Token 自己保存一份
    std::size_t position; // 这段文本在输入中的起始字节位置，从 0 开始。End 的位置是输入长度
    double number = 0.0;  // 数字 Token 转换后的 double 值；其他类别不使用它，默认是 0.0
};

//
std::string_view token_name(TokenKind kind);

//
std::expected<std::vector<Token>, Error> lex(std::string_view source);
} // namespace month03::week04
