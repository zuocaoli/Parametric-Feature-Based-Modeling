module course.expression;
import std;
namespace units {
namespace {
using Node = std::shared_ptr<const ExpressionNode>;
bool ident_start(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 128;
}
bool ident_char(unsigned char c) {
    return ident_start(c) || (c >= '0' && c <= '9');
}
bool whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}
Error at(Error e, std::size_t p, std::size_t n) {
    e.position = p;
    e.length = n;
    e.stage = "expression";
    return e;
}
Result<Unit> binary_unit(char op, Unit left, Unit right) {
    if (op == '+' || op == '-') {
        if (left != right) {
            auto e = error("unit_mismatch", "incompatible operands");
            e.expected = describe(left);
            e.actual = describe(right);
            return std::unexpected(e);
        }
        return left;
    }
    return unit_product(op, left, right);
}
Result<Quantity> invoke(std::string_view name, std::span<const Quantity> a) {
    if (name == "sin" && a.size() == 1)
        return sine(a[0]);
    if (name == "cos" && a.size() == 1) {
        if (a[0].unit() != Unit::angle() && !a[0].unit().dimensionless())
            return std::unexpected(error("unit_mismatch", "cos requires Angle or Ratio"));
        return Quantity::make(std::cos(a[0].value() * std::numbers::pi / 180));
    }
    if (name == "sqrt" && a.size() == 1)
        return square_root(a[0]);
    if ((name == "asin" || name == "exp" || name == "log") && a.size() == 1) {
        if (!a[0].unit().dimensionless())
            return std::unexpected(error("unit_mismatch", "function requires dimensionless input"));
        auto v = a[0].value();
        if ((name == "asin" && (v < -1 || v > 1)) || (name == "log" && v <= 0))
            return std::unexpected(error("domain", "function outside domain"));
        return Quantity::make(name == "asin"  ? std::asin(v) * 180 / std::numbers::pi
                              : name == "log" ? std::log(v)
                                              : std::exp(v),
                              name == "asin" ? Unit::angle() : Unit{});
    }
    if (name == "pow" && a.size() == 2) {
        if (!a[1].unit().dimensionless())
            return std::unexpected(error("exponent", "exponent must be dimensionless"));
        return power(a[0], a[1].value());
    }
    if ((name == "atan2" || name == "min") && a.size() == 2) {
        if (a[0].unit() != a[1].unit())
            return std::unexpected(
                error("unit_mismatch", "function operands require equal dimensions"));
        if (name == "min")
            return a[0].value() < a[1].value() ? a[0] : a[1];
        if (a[0].value() == 0 && a[1].value() == 0)
            return std::unexpected(error("domain", "atan2(0,0)"));
        return Quantity::make(std::atan2(a[0].value(), a[1].value()) * 180 / std::numbers::pi,
                              Unit::angle());
    }
    return std::unexpected(error("function", "unknown function or wrong argument count"));
}
Result<Quantity> execute(std::string_view op, std::span<const Quantity> args) {
    if (op == "unary-" && args.size() == 1)
        return Quantity::make(-args[0].value(), args[0].unit());
    if (op.size() == 1 && args.size() == 2)
        return calculate(op[0], args[0], args[1]);
    return invoke(op, args);
}
class Parser {
    std::string_view source_;
    const UnitRegistry &registry_;
    const TypeResolver &resolver_;
    std::uint64_t object_;
    VariableMap aliases_;
    std::span<const UnitBinding> pinned_;
    bool require_pins_;
    std::size_t pos_ = 0, depth_ = 0, nodes_ = 0;
    std::vector<UnitBinding> units_;
    std::set<PropertyKey> dependencies_;
    void skip() {
        while (pos_ < source_.size() && whitespace(source_[pos_]))
            ++pos_;
    }
    Error fail(std::string code, std::string message, std::size_t p) {
        return at(error(std::move(code), std::move(message)), p, p < source_.size() ? 1 : 0);
    }
    Result<Node> make(std::string op, std::vector<Node> children, std::size_t start) {
        if (++nodes_ > 512)
            return std::unexpected(fail("complexity", "expression node limit", start));
        ExpressionNode node;
        node.operation = op;
        node.children = children;
        node.position = start;
        node.length = pos_ - start;
        std::vector<Quantity> samples;
        bool all = true;
        for (const auto &c : children) {
            node.height = std::max(node.height, c->height + 1);
            all = all && c->constant.has_value();
            samples.push_back(c->constant ? *c->constant : *Quantity::make(1, c->unit));
        }
        if (node.height > 64)
            return std::unexpected(fail("complexity", "expression height limit", start));
        if (op == "pow" && children.size() == 2 && !children[1]->constant)
            return std::unexpected(fail("exponent", "exponent must be constant", start));
        if (op == "/" && children[1]->constant && children[1]->constant->value() == 0)
            return std::unexpected(
                fail("division_by_zero", "constant zero divisor", children[1]->position));
        // 类型检查与数值定义域分开：非常量参数的样值不能决定合法公式的运行状态。
        Result<Quantity> sample = std::unexpected(error("type_mismatch", "invalid operation"));
        if (op.size() == 1 && children.size() == 2) {
            auto type = binary_unit(op[0], children[0]->unit, children[1]->unit);
            if (!type)
                return std::unexpected(at(type.error(), start, node.length));
            node.unit = *type;
            if (all)
                sample = execute(op, samples);
        } else {
            auto type_samples = samples;
            for (std::size_t i = 0; i < children.size(); ++i)
                if (!(op == "pow" && i == 1))
                    type_samples[i] = *Quantity::make(1, children[i]->unit);
            sample = execute(op, type_samples);
            if (!sample)
                return std::unexpected(at(sample.error(), start, node.length));
            node.unit = sample->unit();
            if (all)
                sample = execute(op, samples);
        }
        if (all) {
            if (!sample)
                return std::unexpected(at(sample.error(), start, node.length));
            node.constant = *sample;
        }
        return Node{std::make_shared<ExpressionNode>(std::move(node))};
    }
    Result<Node> primary() {
        skip();
        const auto start = pos_;
        if (++depth_ > 64)
            return std::unexpected(fail("complexity", "expression nesting limit", start));
        struct Guard {
            std::size_t &depth;
            ~Guard() {
                --depth;
            }
        } guard{depth_};
        if (pos_ >= source_.size())
            return std::unexpected(fail("syntax", "operand expected", pos_));
        bool signed_literal =
            (source_[pos_] == '+' || source_[pos_] == '-') && pos_ + 1 < source_.size() &&
            ((source_[pos_ + 1] >= '0' && source_[pos_ + 1] <= '9') || source_[pos_ + 1] == '.');
        if ((source_[pos_] == '+' || source_[pos_] == '-') && !signed_literal) {
            const auto op = source_[pos_++];
            auto value = primary();
            if (!value)
                return value;
            return op == '+' ? value : make("unary-", {*value}, start);
        }
        if (source_[pos_] == '(') {
            ++pos_;
            auto value = expression();
            if (!value)
                return value;
            skip();
            if (pos_ == source_.size() || source_[pos_] != ')')
                return std::unexpected(fail("syntax", "missing ')'", pos_));
            ++pos_;
            return value;
        }
        if (signed_literal || (source_[pos_] >= '0' && source_[pos_] <= '9') ||
            source_[pos_] == '.') {
            if (signed_literal)
                ++pos_;
            auto number = scan_number(source_, pos_);
            if (!number)
                return std::unexpected(number.error());
            pos_ = number->end;
            skip();
            if (pos_ < source_.size() && source_[pos_] == '[') {
                auto close = source_.find(']', pos_);
                if (close == std::string_view::npos)
                    return std::unexpected(fail("syntax", "missing ']'", pos_));
                pos_ = close + 1;
            } else if (pos_ < source_.size() &&
                       ident_start(static_cast<unsigned char>(source_[pos_]))) {
                while (pos_ < source_.size() &&
                       ident_char(static_cast<unsigned char>(source_[pos_])))
                    ++pos_;
            }
            auto quantity = parse_quantity(source_.substr(start, pos_ - start), registry_, {},
                                           start, pinned_, require_pins_);
            if (!quantity)
                return std::unexpected(quantity.error());
            units_.insert(units_.end(), quantity->bindings.begin(), quantity->bindings.end());
            ExpressionNode n;
            n.unit = quantity->value.unit();
            n.constant = quantity->value;
            n.position = start;
            n.length = pos_ - start;
            return Node{std::make_shared<ExpressionNode>(std::move(n))};
        }
        if (ident_start(static_cast<unsigned char>(source_[pos_]))) {
            while (pos_ < source_.size() && ident_char(static_cast<unsigned char>(source_[pos_])))
                ++pos_;
            auto name = std::string{source_.substr(start, pos_ - start)};
            skip();
            if (pos_ < source_.size() && source_[pos_] == '(') {
                ++pos_;
                std::vector<Node> args;
                skip();
                if (pos_ < source_.size() && source_[pos_] != ')')
                    while (true) {
                        auto arg = expression();
                        if (!arg)
                            return arg;
                        args.push_back(*arg);
                        skip();
                        if (pos_ >= source_.size() || source_[pos_] != ',')
                            break;
                        ++pos_;
                        if (args.size() > 8)
                            return std::unexpected(
                                fail("complexity", "too many function arguments", start));
                    }
                if (pos_ == source_.size() || source_[pos_] != ')')
                    return std::unexpected(fail("syntax", "missing function ')'", pos_));
                ++pos_;
                return make(name, std::move(args), start);
            }
            PropertyKey key =
                aliases_.contains(name) ? aliases_.at(name) : PropertyKey{object_, name};
            auto spec = resolver_(key);
            if (!spec)
                return std::unexpected(at(spec.error(), start, name.size()));
            auto checked_type = Quantity::make(1, spec->unit);
            if (!checked_type)
                return std::unexpected(at(checked_type.error(), start, name.size()));
            dependencies_.insert(key);
            ExpressionNode n;
            n.unit = spec->unit;
            n.property = key;
            n.position = start;
            n.length = name.size();
            return Node{std::make_shared<ExpressionNode>(std::move(n))};
        }
        return std::unexpected(fail("syntax", "unexpected character", pos_));
    }
    Result<Node> product() {
        auto value = primary();
        while (value) {
            skip();
            if (pos_ >= source_.size() || (source_[pos_] != '*' && source_[pos_] != '/'))
                break;
            auto op = source_[pos_++];
            auto rhs = primary();
            if (!rhs)
                return rhs;
            value = make(std::string(1, op), {*value, *rhs}, (*value)->position);
        }
        return value;
    }
    Result<Node> expression() {
        auto value = product();
        while (value) {
            skip();
            if (pos_ >= source_.size() || (source_[pos_] != '+' && source_[pos_] != '-'))
                break;
            auto op = source_[pos_++];
            auto rhs = product();
            if (!rhs)
                return rhs;
            value = make(std::string(1, op), {*value, *rhs}, (*value)->position);
        }
        return value;
    }

  public:
    Parser(std::string_view source, const UnitRegistry &r, const TypeResolver &resolver,
           std::uint64_t object, VariableMap aliases, std::span<const UnitBinding> pins,
           bool require)
        : source_(source), registry_(r), resolver_(resolver), object_(object),
          aliases_(std::move(aliases)), pinned_(pins), require_pins_(require) {}
    Result<Expression> run() {
        if (source_.size() > 4096)
            return std::unexpected(fail("complexity", "expression text limit", 0));
        auto root = expression();
        if (!root)
            return std::unexpected(root.error());
        skip();
        if (pos_ != source_.size())
            return std::unexpected(fail("syntax", "unexpected trailing expression text", pos_));
        if (require_pins_ && units_.size() != pinned_.size())
            return std::unexpected(fail("binding", "unused unit bindings", 0));
        return Expression{std::string{source_}, std::move(aliases_), std::move(units_),
                          std::move(dependencies_), *root};
    }
};
Result<Quantity> eval_node(const ExpressionNode &n, const ValueResolver &resolver) {
    if (n.constant)
        return *n.constant;
    if (n.property) {
        auto q = resolver(*n.property);
        if (!q)
            return std::unexpected(at(q.error(), n.position, n.length));
        if (q->unit() != n.unit)
            return std::unexpected(at(error("type_mismatch", "property type changed since binding"),
                                      n.position, n.length));
        return q;
    }
    std::vector<Quantity> args;
    for (const auto &child : n.children) {
        auto q = eval_node(*child, resolver);
        if (!q)
            return q;
        args.push_back(*q);
    }
    auto q = execute(n.operation, args);
    if (!q)
        return std::unexpected(at(q.error(), n.position, n.length));
    return q;
}
} // namespace
Result<Expression> bind_expression(std::string_view source, const UnitRegistry &registry,
                                   const TypeResolver &resolver, std::uint64_t object,
                                   VariableMap aliases, std::span<const UnitBinding> pinned,
                                   bool require) {
    auto clean = mask_comments(source);
    if (!clean)
        return std::unexpected(clean.error());
    auto bound =
        Parser{*clean, registry, resolver, object, std::move(aliases), pinned, require}.run();
    if (bound)
        bound->source = std::string{source};
    return bound;
}
Result<Quantity> evaluate(const Expression &expression, const ValueResolver &resolver) {
    return eval_node(*expression.root, resolver);
}
std::string describe_ast(const Expression &e) {
    std::string result;
    std::function<void(const ExpressionNode &, int)> visit = [&](const ExpressionNode &n,
                                                                 int depth) {
        result += std::string(static_cast<std::size_t>(depth) * 2, ' ');
        result += n.property ? "PropertyRef(" + std::to_string(n.property->object) + "," +
                                   n.property->name + ")"
                  : n.operation.empty() ? "Literal"
                                        : n.operation;
        result += " [" + describe(n.unit) + "]\n";
        for (const auto &c : n.children)
            visit(*c, depth + 1);
    };
    visit(*e.root, 0);
    return result;
}
} // namespace units
