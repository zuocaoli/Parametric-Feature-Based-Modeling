module month02.week02.lexer;

import std;

namespace month02::week02 {
namespace {
bool digit(char c) {
    return c >= '0' && c <= '9';
}

//
// 字符 c 能否作为标识符（变量名）的第一个字符，也就是大小写字母或下划线
//
bool identifier_start(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
} // namespace

std::string_view stage_name(ErrorStage stage) {
    switch (stage) {
    case ErrorStage::Lexical:
        return "lexical";
    case ErrorStage::Syntax:
        return "syntax";
    case ErrorStage::Evaluation:
        return "evaluation";
    case ErrorStage::Binding:
        return "binding";
    case ErrorStage::Storage:
        return "storage";
    }
    return "unknown";
}

std::string_view token_name(TokenKind kind) {
    switch (kind) {
    case TokenKind::Number:
        return "Number";
    case TokenKind::Identifier:
        return "Identifier";
    case TokenKind::Plus:
        return "Plus";
    case TokenKind::Minus:
        return "Minus";
    case TokenKind::Star:
        return "Star";
    case TokenKind::Slash:
        return "Slash";
    case TokenKind::LeftParen:
        return "LeftParen";
    case TokenKind::RightParen:
        return "RightParen";
    case TokenKind::End:
        return "End";
    }
    return "unknown";
}

//
// std::expected 是 C++23 引入的标准库类型，用于表示一个操作要么成功（持有一个类型为 T 的值），要么失败（持有一个类型为 E 的错误）.
// std::unexpected 是 C++23 中引入的一个类模板，定义在头文件 <expected> 中。它的核心作用是包装一个错误值，以便将其存入 std::expected 对象，表示操作失败。
// std::expected 要么持有成功值，要么持有错误值。你不能直接把一个错误对象塞进去，因为编译器无法区分它是“成功值”还是“错误值”。
// std::unexpected 就是一个明确的标记，告诉 std::expected 的构造函数：“请把这个值当作错误来存储”。
//
//
//
std::expected<std::vector<Token>, Error> lex(std::string_view source) {

    //
    std::vector<Token> tokens;
    std::size_t cursor = 0;

    //
    while (cursor < source.size()) {

        // 从表达式中取出一个字符
        const char c = source[cursor];

        // 判断字符 c 是否为空白字符
        if (whitespace(c)) {
            ++cursor;
            continue;
        }

        // 标记表达式开始位置
        const auto start = cursor;

        //
        // 处理分支： 当前字符为 “数字” 或 “小数点”。
        // 如果是数值，要把整个数值作为一个整体提取出来
        if (digit(c) || c == '.') {
            bool has_digit = false;

            // 跳过连续数字字符，让 cursor 停在第一个非数字字符或输入末尾.
            while (cursor < source.size() && digit(source[cursor])) {
                has_digit = true;
                ++cursor;
            }

            // 遇到小数点后，继续扫描小数部分
            if (cursor < source.size() && source[cursor] == '.') {
                ++cursor; // 跳过小数点

                // 扫描小数部分的数字
                while (cursor < source.size() && digit(source[cursor])) {
                    has_digit = true;
                    ++cursor;
                }
            }

            // 扫描到的内容是否至少包含一位数字，否则报错
            if (!has_digit) {
                return std::unexpected(
                    Error{ErrorStage::Lexical, start, "number requires a digit"});
            }

            // 数字扫描结束后，紧接着的字符是否为非法后缀
            //
            // | 输入   | 已扫描的数字 | 当前字符 |         结果        |
            // |------ |------------|---------|--------------------|
            // | 2..5  | 2.         | .       | 报错                |
            // | 12abc | 12         | a       | 报错                |
            // | 1e2   | 1          | e       | 报错，不支持科学计数法 |
            // | 12+3  | 12         | +       | 继续扫描             |
            //
            if (cursor < source.size() &&
                (source[cursor] == '.' || identifier_start(source[cursor]))) {
                return std::unexpected(Error{ErrorStage::Lexical, cursor,
                                             "invalid number suffix (exponents are unsupported)"});
            }

            //
            double number = 0.0;
            //
            // 扫描到的 “数值文本” 转换为 double 数值，存入 number
            //
            // std::from_chars 是 C++17 引入的底层数值解析函数，核心用途是以最高性能将字符序列解析为整数或浮点数，特别适合 JSON、CSV 等文本交换格式中高频数字解析的场景。
            // std::from_chars 返回一个 std::from_chars_result 类型的结构体，该结构体包含两个公开成员：ptr 和 ec。
            // 解析成功：ec 等于 std::errc{}（即无错误）。
            // ptr 指向第一个未匹配模式的字符；如果整个 [first, last) 范围都被成功解析，则 ptr 等于 last。解析出的数值会被写入你传入的 value 参数中。
            //
            // 函数输入参数：
            // first, last：要解析的字符范围 [first, last)。不要求以空字符结尾，且不忽略前导空格。
            // value：输出参数。解析成功时，数值存入此处；失败时（如无匹配或越界），该值保持不变。
            // base：整数基数，范围 2 到 36（含），默认为 10。注意：即使基数为 16，也不识别 0x 前缀。
            //
            const auto [end, error] = std::from_chars(source.data() + start, source.data() + cursor,
                                                      number, std::chars_format::fixed);

            // 数字转换是否成功、完整，且结果是有限值
            // 1. std::from_chars 是否报错。
            // 2. 实际转换停止的位置，是否不同于预期结束的位置
            // 3. 判断 number 是否为有限数值（不是无穷大或 NaN）。
            if (error != std::errc{} || end != source.data() + cursor || !std::isfinite(number)) {
                return std::unexpected(Error{ErrorStage::Lexical, start, "number is out of range"});
            }
            tokens.push_back({TokenKind::Number, std::string{source.substr(start, cursor - start)},
                              start, number});
            continue;
        }

        // 如果不是数值，就检查是否为标识符（以字母或下划线开头）
        if (identifier_start(c)) {
            ++cursor;
            while (cursor < source.size() &&
                   (identifier_start(source[cursor]) || digit(source[cursor]))) {
                ++cursor; // 读取连续字符
            }

            tokens.push_back(
                {TokenKind::Identifier, std::string{source.substr(start, cursor - start)}, start});
            continue;
        }

        //
        TokenKind kind;

        // 处理运行符号
        switch (c) {
        case '+':
            kind = TokenKind::Plus;
            break;
        case '-':
            kind = TokenKind::Minus;
            break;
        case '*':
            kind = TokenKind::Star;
            break;
        case '/':
            kind = TokenKind::Slash;
            break;
        case '(':
            kind = TokenKind::LeftParen;
            break;
        case ')':
            kind = TokenKind::RightParen;
            break;
        default:
            return std::unexpected(Error{ErrorStage::Lexical, start, "illegal character"});
        }
        tokens.push_back({kind, std::string(1, c), cursor++});
    }
    tokens.push_back({TokenKind::End, "", source.size()});
    return tokens;
}
} // namespace month02::week02
