//
// 表达式语法树
//

export module month03.week01.parser;

import std;
export import month03.week01.lexer;

export namespace month03::week01 {

// 表示抽象语法树（AST）中节点的类型，用于判断这个节点代表什么
enum class NodeKind {
    Number,   // 数字字面量，如 2；无子节点
    Variable, // 变量引用，如 length；无子节点
    Unary,    // 一元运算，如 -length；left 存放唯一操作数
    Binary    // 二元运算，如 length + 2；left、right 分别存放左右操作数
};

// 表示抽象语法树（AST）中的一个节点。它保存“这个节点是什么、具体内容是什么、连接哪些子节点”
struct Node {
    NodeKind kind;               // 节点的类型
    Token token;                 // 当前节点对应的 Token，保留文本、位置和数字值等信息
    std::unique_ptr<Node> left;  // 左子节点；一元运算的唯一操作数也放在这里
    std::unique_ptr<Node> right; // 右子节点；仅二元运算使用
    std::size_t height = 1;      // 以当前节点为根的子树高度；叶子为 1
};

// 表示拥有一棵语法树
// 每个子节点只属于一个父节点，unique_ptr 正好表达这种“唯一所有权”，并自动管理内存。
//   - 自动释放：父节点销毁时，子节点及其子树自动释放；解析中途失败也能清理。
//   - 归属明确：一个子节点只有一个所有者，不能复制指针来重复拥有它。
//   - 开销较小：这里不需要共享节点，因此无需 shared_ptr 的引用计数。
using Tree = std::unique_ptr<Node>;

// 将表达式文本解析为 AST；成功返回 Tree，词法或语法错误返回 Error，不执行求值。
std::expected<Tree, Error> parse(std::string_view source);

std::string describe(const Node &node);
} // namespace month03::week01
