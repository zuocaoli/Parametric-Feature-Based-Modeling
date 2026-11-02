module course.operations;
import std;
namespace units {
Operations::Operations(Document &d) : document_(d) {}
Model Operations::query() const {
    return document_.model();
}
Result<void> Operations::validate_request(const OperationRequest &r) const {
    if (r.expected_revision != document_.revision())
        return std::unexpected(error("stale_revision", "document changed; preview again"));
    if (r.operations.empty() || r.operations.size() > 64)
        return std::unexpected(error("request", "batch must contain 1..64 operations"));
    return {};
}
Result<void> Operations::execute(Model &m, const OperationRequest &r) const {
    for (std::size_t index = 0; index < r.operations.size(); ++index) {
        auto result = std::visit(
            [&](const auto &op) -> Result<void> {
                using T = std::decay_t<decltype(op)>;
                if constexpr (std::is_same_v<T, SetValue>)
                    return m.set_value(op.target, op.value);
                else if constexpr (std::is_same_v<T, SetFormula>)
                    return m.bind(op.target, op.source, op.aliases);
                else if constexpr (std::is_same_v<T, SetConfig>)
                    return m.configure(op.config);
                else if constexpr (std::is_same_v<T, AddUnit>) {
                    auto d = op.definition;
                    d.builtin = false;
                    return m.register_unit(std::move(d));
                } else {
                    auto definition = m.registry().find_id(op.definition.id, op.definition.version);
                    if (!definition)
                        return std::unexpected(definition.error());
                    const auto properties = m.properties();
                    for (const auto &[key, p] : properties)
                        if (p.formula) {
                            auto pins = p.formula->units;
                            bool changed = false;
                            for (auto &pin : pins)
                                if (pin.unit.id == op.definition.id && pin.unit != op.definition) {
                                    pin.unit = op.definition;
                                    changed = true;
                                }
                            if (changed) {
                                auto v =
                                    m.bind(key, p.formula->source, p.formula->aliases, pins, true);
                                if (!v)
                                    return v;
                            }
                        }
                    return {};
                }
            },
            r.operations[index]);
        if (!result) {
            auto e = result.error();
            e.message = "operation " + std::to_string(index) + ": " + e.message;
            return std::unexpected(e);
        }
    }
    return {};
}
Result<OperationPreview> Operations::preview(const OperationRequest &r) const {
    auto valid = validate_request(r);
    if (!valid)
        return std::unexpected(valid.error());
    auto after = document_.preview([&](Model &m) { return execute(m, r); });
    if (!after)
        return std::unexpected(after.error());
    return OperationPreview{document_.revision(), document_.model(), *after, after->last_report()};
}
Result<OperationOutcome> Operations::apply(const OperationRequest &r) {
    auto valid = validate_request(r);
    if (!valid)
        return std::unexpected(valid.error());
    auto changed = document_.transact([&](Model &m) { return execute(m, r); });
    if (!changed)
        return std::unexpected(changed.error());
    return OperationOutcome{*changed, document_.revision(),
                            *changed ? document_.last_report() : std::vector<RecomputeStep>{}};
}
} // namespace units
