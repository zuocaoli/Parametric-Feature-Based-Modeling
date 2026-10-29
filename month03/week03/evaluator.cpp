module month03.week03.evaluator;

import std;

namespace month03::week03 {
namespace {
// 检查数值是否为有限数，并返回数值或错误
std::expected<double, Error> finite(double value, std::size_t position) {
    if (!std::isfinite(value)) {
        return std::unexpected(
            Error{ErrorStage::Evaluation, position, "non-finite numeric result"});
    }
    return value;
}
} // namespace

//
// 递归执行已经解析好的表达式语法树（AST），得到数值结果或错误
//
// 它根据节点类型决定如何计算：
// - 数字：直接返回数值。
// - 变量：调用 resolve 查询，取出数值。
// - 一元运算：先计算操作数，再执行正号或负号。
// - 二元运算：先计算左右子树，再执行 + - * /。
//
// 例如，计算 length / 2 + 5，当 resolve("length") 返回 100 时：
// 计算 length / 2 → 100 / 2 → 50
// 计算 50 + 5    → 55
//
// 返回类型 std::expected<double, Error> 表示两种结果：
// - 成功：保存 double，例如 55.0。
// - 失败：保存 Error，例如未知变量、非数值属性、除零；子树的错误会直接向上传递
//
std::expected<double, Error> evaluate(const Node &node, const Resolver &resolve) {
    //
    // 1. 处理 AST 中的数字和变量节点
    //
    // 取出当前节点对应的 Token 在表达式中的起始字节位置
    const auto position = node.token.position;

    // 处理 AST 中的数字节点
    // 如果是数字节点，直接返回数值；
    // 如果不是有限数，就返回错误。
    // 如果不是数字节点，就继续处理其他类型的节点。
    if (node.kind == NodeKind::Number) {
        // 检查数值是否为有限数，并返回数值或错误
        return finite(node.token.number, position);
    }

    // 处理 AST 中的变量节点
    // 如果是变量节点，调用 resolve 查询属性值；
    if (node.kind == NodeKind::Variable) {
        // 取出变量的值
        const auto value = resolve ? resolve(node.token.text) : std::nullopt;
        if (!value) {
            return std::unexpected(Error{ErrorStage::Evaluation, position,
                                         "unknown variable '" + node.token.text + "'"});
        }
        // 如果属性值保存的是 double，就取出这个数值
        if (const auto *number = std::get_if<double>(&*value)) {
            return finite(*number, position);
        }
        // 如果属性值保存的是 std::int64_t 整数，就取出它，转换成 double，检查后返回
        if (const auto *integer = std::get_if<std::int64_t>(&*value)) {
            return finite(static_cast<double>(*integer), position);
        }
        return std::unexpected(Error{ErrorStage::Evaluation, position,
                                     "variable '" + node.token.text + "' is not numeric"});
    }

    //
    // 2. 不是变量和数值，那就是运算符
    //
    // 检查运算节点是否缺少必要的子节点：
    // 执行到这里时，数字和变量节点已经在前面的分支返回，剩下的是一元或二元运算节点。
    // 左子节点必须存在；一元运算只有左子节点，二元运算节点还需要右子节点。
    if (!node.left || (node.kind == NodeKind::Binary && !node.right)) {
        return std::unexpected(Error{ErrorStage::Evaluation, position, "malformed AST"});
    }
    // 递归计算左子树；如果计算失败，就立即返回错误
    auto left = evaluate(*node.left, resolve);
    if (!left) {
        return left;
    }

    // 处理一元运算：
    // 判断当前节点是否为一元运算。执行到这里时，前面的递归已经计算出操作数，成功结果保存在 left 中，*left 就是它的数值
    if (node.kind == NodeKind::Unary) {
        if (node.token.kind == TokenKind::Plus) {
            // 保持数值不变。例如 length = 100，则 +length 得到 100
            return finite(*left, position);
        }
        if (node.token.kind == TokenKind::Minus) {
            // -*left 等价于 -(*left)：先取出数值，再取相反数，例如 -length 得到 -100
            return finite(-*left, position);
        }
        // 如果是一元节点，但运算符既不是 + 也不是 -。
        return std::unexpected(Error{ErrorStage::Evaluation, position, "invalid unary operator"});
    }

    // 递归计算右子树；如果失败，就把错误返回给上一层
    auto right = evaluate(*node.right, resolve);
    if (!right) {
        return right;
    }

    // 当前节点是二元运算，并且左右子树都计算成功时执行
    switch (node.token.kind) {
    case TokenKind::Plus:
        return finite(*left + *right, position);
    case TokenKind::Minus:
        return finite(*left - *right, position);
    case TokenKind::Star:
        return finite(*left * *right, position);
    case TokenKind::Slash:
        if (*right == 0.0) { // 包括 -0.0；不使用任意 epsilon 拒绝小数。
            return std::unexpected(Error{ErrorStage::Evaluation, position, "division by zero"});
        }
        return finite(*left / *right, position);
    default:
        return std::unexpected(Error{ErrorStage::Evaluation, position, "invalid binary operator"});
    }
}

std::expected<double, Error> calculate(std::string_view source, const Resolver &resolve) {
    // 解析表达式字符串，得到 AST
    auto tree = parse(source);
    if (!tree) {
        return std::unexpected(tree.error());
    }
    //
    // 计算 AST，得到数值结果或错误
    //
    // 由于 evaluate(const Node &node, const Resolver &resolve)， 第一个参数的类型是 const Node &node 所以：
    // *tree       // 从 expected 中取出 unique_ptr<Node>
    // *(*tree)    // 再从 unique_ptr 中取出 Node
    return evaluate(**tree, resolve);
}
} // namespace month03::week03
