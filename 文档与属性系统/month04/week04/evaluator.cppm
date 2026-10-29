export module month06.week04.evaluator;

import std;
export import month06.week04.parser;
export import month06.week04.property;

export namespace month06::week04 {
using Resolver = std::function<std::optional<Value>(std::string_view)>;

std::expected<double, Error> evaluate(const Node &node, const Resolver &resolve = {});
std::expected<double, Error> calculate(std::string_view source, const Resolver &resolve = {});
} // namespace month06::week04
