export module month04.week02.evaluator;

import std;
export import month04.week02.parser;
export import month04.week02.property;

export namespace month04::week02 {
using Resolver = std::function<std::optional<Value>(std::string_view)>;

std::expected<double, Error> evaluate(const Node &node, const Resolver &resolve = {});
std::expected<double, Error> calculate(std::string_view source, const Resolver &resolve = {});
} // namespace month04::week02
