module month05.week03.evaluator;

import std;

namespace month05::week03 {
namespace {
std::expected<double, Error> finite(double value, std::size_t position) {
    if (!std::isfinite(value)) {
        return std::unexpected(
            Error{ErrorStage::Evaluation, position, "non-finite numeric result"});
    }
    return value;
}
} // namespace

std::expected<double, Error> evaluate(const Node &node, const Resolver &resolve) {
    const auto position = node.token.position;
    if (node.kind == NodeKind::Number) {
        return finite(node.token.number, position);
    }
    if (node.kind == NodeKind::Variable) {
        const auto value = resolve ? resolve(node.token.text) : std::nullopt;
        if (!value) {
            return std::unexpected(Error{ErrorStage::Evaluation, position,
                                         "unknown variable '" + node.token.text + "'"});
        }
        if (const auto *number = std::get_if<double>(&*value)) {
            return finite(*number, position);
        }
        if (const auto *integer = std::get_if<std::int64_t>(&*value)) {
            return finite(static_cast<double>(*integer), position);
        }
        return std::unexpected(Error{ErrorStage::Evaluation, position,
                                     "variable '" + node.token.text + "' is not numeric"});
    }
    if (!node.left || (node.kind == NodeKind::Binary && !node.right)) {
        return std::unexpected(Error{ErrorStage::Evaluation, position, "malformed AST"});
    }
    auto left = evaluate(*node.left, resolve);
    if (!left) {
        return left;
    }
    if (node.kind == NodeKind::Unary) {
        if (node.token.kind == TokenKind::Plus) {
            return finite(*left, position);
        }
        if (node.token.kind == TokenKind::Minus) {
            return finite(-*left, position);
        }
        return std::unexpected(Error{ErrorStage::Evaluation, position, "invalid unary operator"});
    }
    auto right = evaluate(*node.right, resolve);
    if (!right) {
        return right;
    }
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
    auto tree = parse(source);
    if (!tree) {
        return std::unexpected(tree.error());
    }
    return evaluate(**tree, resolve);
}
} // namespace month05::week03
