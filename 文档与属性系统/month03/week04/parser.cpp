module month05.week04.parser;

import std;

namespace month05::week04 {
namespace {
struct ParseFailure {
    Error error;
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

    // 各解析函数从 cursor_ 开始消费 Token，返回子树，游标停在第一个未消费的 Token 上。
    // 加减层 → 乘除层 → 一元层 → 基本元素层；低优先级层先取得高优先级的完整操作数。
    // 整条表达式的语法树（AST），由 Tree（即 std::unique_ptr<Node>）持有根节点.
    Tree parse_complete_expression() {
        auto result = parse_additive(); // 从当前最低优先级的加减层开始构建语法树

        if (current().kind != TokenKind::End) {
            fail("unexpected trailing token");
        }
        return result;
    }

private:
    static constexpr std::size_t maximum_depth = 128;

    std::vector<Token> tokens_;

    // 当前解析位置的索引，表示接下来要处理 tokens_ 中的哪个 Token
    std::size_t cursor_ = 0;

    // 记录的是解析过程的递归深度，不是树的高度，树的高度则由节点的 height 记录
    std::size_t nesting_ = 0;

    // 只查看当前 Token，不推进游标。
    const Token &current() const {
        return tokens_[cursor_];
    }

    // 消费并返回当前 Token；调用者已确认它不是输入末尾 End。
    Token advance() {
        return tokens_[cursor_++];
    }

    [[noreturn]] void fail(std::string message) const {
        // 界面使用当前 Token 的文本长度标出错误范围；End 的文本为空，长度为 0。
        throw ParseFailure{Error{ErrorStage::Syntax, current().position, std::move(message),
                                 current().text.size()}};
    }

    //
    // 创建语法树节点，并检查创建后的树是否超过深度限制.
    // 返回的是一个 Tree 类型的智能指针，指向新创建的语法树节点.
    // 返回的是以新创建的节点为根的子树, 如果左右子树都为空，它就是只有一个节点的子树，例如数字或变量节点。
    Tree make(NodeKind kind, Token token, Tree left = {}, Tree right = {}) {
        // 计算当前节点的高度（以当前节点为根的子树）：左右子树高度的最大值，再加上当前节点这一层.
        const auto height = 1 + std::max(left ? left->height : 0, right ? right->height : 0);

        // 检查语法树高度是否超过允许的上限 maximum_depth, 如果超过，就用 throw 抛出 ParseFailure 异常，中断正常解析.
        // 后续处理语法树通常需要递归遍历，树越深，递归调用层数就越多，过深可能导致调用栈溢出、程序崩溃.
        if (height > maximum_depth) {
            throw ParseFailure{Error{ErrorStage::Syntax, token.position, "AST is too deep"}};
        }

        return std::make_unique<Node>(kind, std::move(token), std::move(left), std::move(right),
                                      height);
    }

    // 将已解析的左右子树连接到二元运算节点；节点创建和树高检查仍由 make() 完成。
    Tree make_binary(Token op, Tree left, Tree right) {
        return make(NodeKind::Binary, std::move(op), std::move(left), std::move(right));
    }

    // 解析加减优先级层；当前表达式从此层开始，是否到达输入末尾由调用者检查。
    Tree parse_additive() {

        // 取出左操作数：
        // 解析当前表达式的第一项，并将返回的子树保存到 tree 中, 作为后续加减运算的左子树.
        // 一项可能是完整的乘除表达式，由于运算优先级的关系，所以要先处理乘除运算。
        // 没有乘除时，只返回一个操作数，例如 1 + 2 中的 1。
        auto tree = parse_multiplicative();

        // 只要当前 Token 是 + 或 -，就取出运算符，再调用 parse_multiplicative() 解析右操作数
        while (current().kind == TokenKind::Plus || current().kind == TokenKind::Minus) {

            // 取出运算符，推进游标
            auto op = advance();

            // 取出右操作数： 右操作数可能是一个乘除运算表达式，由于运算优先级的关系，所以要先处理乘除运算。
            auto right = parse_multiplicative();
            tree = make_binary(std::move(op), std::move(tree), std::move(right));
        }

        // 左操作数已经取出，但是运算符不是 + 或 - 那就返回。

        return tree;
    }

    // 乘除表达式解析，返回乘除表达式的 Tree
    Tree parse_multiplicative() {

        // 解析左操作数
        auto tree = parse_unary();

        // 只要当前 Token 是 * 或 /，就取出运算符，再调用 parse_unary() 解析右操作数
        while (current().kind == TokenKind::Star || current().kind == TokenKind::Slash) {
            // 就取出运算符
            auto op = advance();
            // 解析右操作数
            auto right = parse_unary();

            // 创建二元运算节点：
            // 左操作数作为左子树，右操作数作为右子树。
            tree = make_binary(std::move(op), std::move(tree), std::move(right));

            //      op
            //     /   ╲
            //    /     ╲
            //   /       ╲
            // 左操作数   右操作数
            //
            //
            // 2 / 3 * 4
            //          “*”
            //         /   ╲
            //        /     ╲
            //       /       ╲
            //      “/”      “4”
            //     /   ╲
            //    /     ╲
            //   /       ╲
            //  “2”      “3”
            //
        }

        return tree;
    }

    // 解析一元正负号（如 -width、+3），并限制递归嵌套深度
    Tree parse_unary() {

        // 检查 nesting_ 是否达到上限，达到就报错
        if (nesting_ >= maximum_depth) {
            fail("expression nesting is too deep");
        }

        // 构造时将 nesting_ 加 1，退出函数时自动减 1
        // 因为 parse_unary() 会递归调用自身，需要用 nesting_ 记录当前递归层数，防止嵌套过深导致栈溢出,
        // 内层调用退出只减去自己这一层，外层仍然保留计数，因此 nesting_ 始终表示当前还未退出的 parse_unary() 调用层数.
        struct DepthGuard {
            std::size_t &depth;
            explicit DepthGuard(std::size_t &value) : depth(value) {
                ++depth; // 引用 nesting_
            }
            ~DepthGuard() {
                --depth;
            }
        } guard{nesting_};

        // 只有一元运算才会走这个分支
        if (current().kind == TokenKind::Plus || current().kind == TokenKind::Minus) {

            // 取出运算符号
            auto op = advance();

            // 递归：一元运算符的操作数还可能带有一元运算符，例如 --3，含义是 -(-3)，
            // “一元”表示每个运算符只有一个操作数，并不表示只能出现一个运算符，
            // 即使是 -3，也需要解析符号后面的 3；递归调用发现它没有正负号，就转到 parse_primary()，不会继续递归。
            auto operand = parse_unary();

            // operand 存放在一元运算节点 (op) 的左子树位置.
            return make(NodeKind::Unary, std::move(op), std::move(operand));

            //
            // 一元表达式只有左操作数
            //
        }

        // 不是一元运算符
        return parse_primary();
    }

    // 从当前位置解析一个数字、变量或括号表达式，返回对应的语法树并推进游标，遇到不符合语法的输入则报错。
    Tree parse_primary() {

        // 判断当前 Token 是否为数字或标识符，如果是，就取出它并推进游标，然后创建并返回对应的数字节点（Number）或变量节点（Variable）
        if (current().kind == TokenKind::Number || current().kind == TokenKind::Identifier) {
            auto token = advance();
            const auto kind =
                token.kind == TokenKind::Number ? NodeKind::Number : NodeKind::Variable;

            // 创建的是一个叶子节点，例如数字节点或变量节点
            return make(kind, std::move(token));

            //
            // NodeKind::Variable 和 TokenKind::Identifier 是什么关系？
            // NodeKind::Variable 表示语法树中的变量节点，例如表达式 width + 10 中的 width。
            // 它与 TokenKind::Identifier 属于不同阶段：
            // - TokenKind::Identifier：词法分析识别出的标识符 Token，表示这段文本是一个名字。
            // - NodeKind::Variable：语法分析将这个名字解释为变量后，创建的节点类型。
            // 在当前解析器中，标识符都作为变量处理，因此 parse_primary() 将 Identifier Token 转换为 Variable 节点。
            //
        }

        // 解析括号表达式，并返回括号内表达式的语法树
        if (current().kind == TokenKind::LeftParen) {
            advance(); // 跳过左括号( ，开始解析括号内的表达式

            // 括号内从加减层重新解析，允许停在右括号；此处不要求到达输入末尾。
            auto result = parse_additive();

            if (current().kind != TokenKind::RightParen) {
                fail("expected ')'");
            }
            advance(); // 跳过已经确认存在的右括号 )
            return result;
        }
        fail("expected a number, variable or '('");
    }
};
} // namespace

std::expected<Tree, Error> parse(std::string_view source) {

    // 词法分析：将表达式文本拆分为 Token 序列
    auto tokens = lex(source);

    if (!tokens) {
        return std::unexpected(tokens.error());
    }

    try {
        // std::expected 提供了 operator*()，用于访问内部保存的成功值。
        //
        // tokens 的类型是：std::expected<std::vector<Token>, Error>
        // *tokens 得到内部 vector 的左值引用，而 Parser 构造函数按值接收：
        // explicit Parser(std::vector<Token> tokens): tokens_(std::move(tokens)) {}
        //
        // 因此：
        // Parser{*tokens}               // 不使用 std::move 就是复制 vector 到构造函数参数
        // Parser{std::move(*tokens)}    // 这就是移动 vector 到构造函数参数了
        // 移动构造会接管原 vector 的缓冲区，无需逐个复制 Token 和其中的字符串，构造函数里再用一次 std::move，把参数持有的缓冲区交给成员 tokens_
        return Parser{std::move(*tokens)}
            .parse_complete_expression(); // 从 tokens 到 Parser 发生了两次移动

    } catch (const ParseFailure &failure) {

        return std::unexpected(failure.error);
    }
}

std::string describe(const Node &node) {
    if (node.kind == NodeKind::Number || node.kind == NodeKind::Variable) {
        return node.token.text;
    }
    const auto left = describe(*node.left);
    if (node.kind == NodeKind::Unary) {
        return "(" + node.token.text + " " + left + ")";
    }
    return "(" + node.token.text + " " + left + " " + describe(*node.right) + ")";
}
} // namespace month05::week04
