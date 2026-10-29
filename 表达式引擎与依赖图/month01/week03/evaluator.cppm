export module month02.week03.evaluator;

import std;
export import month02.week03.parser;
export import month02.week03.property;

//
// 表达式求值模块的公开接口.
//

export namespace month02::week03 {

// 属性查询回调类型，根据属性名称获取属性值，
// 例如输入 "length"，返回矩形的长度属性值
using Resolver = std::function<std::optional<Value>(std::string_view)>;

// 计算已经解析好的 AST，返回数值结果或错误
std::expected<double, Error> evaluate(const Node &node, const Resolver &resolve = {});

// 接收表达式字符串, 先解析, 再求值, 内部调用了 evaluate()，返回数值结果或错误
std::expected<double, Error> calculate(std::string_view source, const Resolver &resolve = {});
} // namespace month02::week03
