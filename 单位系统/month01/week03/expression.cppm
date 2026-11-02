export module course.expression;
import std;
export import course.schema;
export namespace units {
struct PropertyKey {
    std::uint64_t object = 1;
    std::string name;
    auto operator<=>(const PropertyKey &) const = default;
};
using VariableMap = std::map<std::string, PropertyKey>;
using TypeResolver = std::function<Result<QuantitySpec>(const PropertyKey &)>;
using ValueResolver = std::function<Result<Quantity>(const PropertyKey &)>;
struct ExpressionNode {
    std::string operation;
    Unit unit;
    std::optional<Quantity> constant;
    std::optional<PropertyKey> property;
    std::vector<std::shared_ptr<const ExpressionNode>> children;
    std::size_t position = 0, length = 0, height = 1;
};
struct Expression {
    std::string source;
    VariableMap aliases;
    std::vector<UnitBinding> units;
    std::set<PropertyKey> dependencies;
    std::shared_ptr<const ExpressionNode> root;
};
Result<Expression> bind_expression(std::string_view source, const UnitRegistry &registry,
                                   const TypeResolver &resolver, std::uint64_t object = 1,
                                   VariableMap aliases = {},
                                   std::span<const UnitBinding> pinned = {},
                                   bool require_pins = false);
Result<Quantity> evaluate(const Expression &expression, const ValueResolver &resolver);
std::string describe_ast(const Expression &expression);
} // namespace units
