export module month05.week03.evaluator;

import std;
export import month05.week03.parser;
export import month05.week03.property;

export namespace month05::week03 {
using Resolver = std::function<std::optional<Value>(std::string_view)>;

std::expected<double, Error> evaluate(const Node &node, const Resolver &resolve = {});
std::expected<double, Error> calculate(std::string_view source, const Resolver &resolve = {});
} // namespace month05::week03
