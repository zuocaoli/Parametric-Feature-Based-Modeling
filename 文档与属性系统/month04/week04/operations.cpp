module month06.week04.operations;
import std;

namespace month06::week04 {
Operations::Operations(Document &document) : document_(document) {}
EngineSnapshot Operations::query() const {
    return document_.model().snapshot();
}
OperationError Operations::failure(std::string code, const EngineError &error) const {
    return {std::move(code), error.message, document_.revision(),
            error.operation, error.range,   error.cycle};
}
std::expected<std::unique_ptr<Command>, OperationError>
Operations::compile(const OperationRequest &request) const {
    if (request.expected_revision != document_.revision())
        return std::unexpected(failure("stale_revision", {"document changed; preview again", {}}));
    if (request.operations.empty() || request.operations.size() > 64)
        return std::unexpected(failure("invalid_request", {"expected 1..64 operations", {}}));
    std::vector<std::unique_ptr<Command>> commands;
    for (std::size_t i = 0; i < request.operations.size(); ++i) {
        const auto &op = request.operations[i];
        auto reject = [&](std::string message) {
            auto error = failure("invalid_request", {std::move(message), {}});
            error.operation = i;
            return std::unexpected(error);
        };
        if (op.formula.size() > 4096 || op.aliases.size() > 64)
            return reject("formula or mapping exceeds limit");
        switch (op.kind) {
        case OperationKind::SetValue:
            commands.push_back(std::make_unique<SetProperty>(op.target, op.value));
            break;
        case OperationKind::SetFormula:
            commands.push_back(std::make_unique<BindFormula>(op.target, op.formula, op.aliases));
            break;
        case OperationKind::ClearFormula:
            commands.push_back(std::make_unique<UnbindFormula>(op.target));
            break;
        case OperationKind::Create:
            commands.push_back(std::make_unique<CreateRectangle>(op.length, op.width));
            break;
        case OperationKind::Delete:
            commands.push_back(std::make_unique<DeleteRectangle>(op.target.object));
            break;
        default:
            return reject("unknown operation kind");
        }
        if (op.kind != OperationKind::Create) {
            if (op.target.object == 0 ||
                (op.kind != OperationKind::Delete && op.target.name.empty()))
                return reject("missing object or property identity");
            // Delete/unbind historically allow no-op; public API distinguishes absent targets.
            if ((op.kind == OperationKind::Delete &&
                 document_.model().properties(op.target.object).empty()) ||
                (op.kind == OperationKind::ClearFormula &&
                 !document_.model().stored_value(op.target)))
                return reject("unknown target");
        }
    }
    return std::make_unique<CompositeCommand>(std::move(commands));
}
std::expected<OperationPreview, OperationError>
Operations::preview(const OperationRequest &request) const {
    auto command = compile(request);
    if (!command)
        return std::unexpected(command.error());
    try {
        auto candidate = RecomputeEngine::restore(query(), false, 0, document_.model().ast_cache());
        if (!candidate)
            return std::unexpected(failure("rejected", candidate.error()));
        (*candidate)->begin_batch().value();
        auto result = (*command)->execute(**candidate);
        if (!result)
            return std::unexpected(failure("rejected", result.error()));
        (*candidate)->end_batch().value();
        // Include derived changes and status transitions, not just request text.
        auto flatten = [](const RecomputeEngine &model) {
            std::map<PropertyKey, std::string> fields;
            for (const auto id : model.object_ids())
                for (const auto &property : model.properties(id)) {
                    PropertyKey key{id, property.name};
                    std::string text = value_text(property.value);
                    if (const auto formula = model.formula(key))
                        text += " = " + *formula;
                    text += " [" + std::string{state_name(model.status(key).state)} + "]";
                    fields.emplace(key, std::move(text));
                }
            // Mappings are semantic even when the current value happens to be identical.
            for (const auto &formula : model.snapshot().formulas)
                for (const auto &[name, key] : formula.aliases)
                    fields[formula.target] += " {" + name + " -> " + key_text(key) + "}";
            return fields;
        };
        auto before = flatten(document_.model()), after = flatten(**candidate);
        OperationPreview preview{document_.revision(), {}, (*candidate)->last_report()};
        for (const auto &[key, text] : before) {
            auto it = after.find(key);
            if (it == after.end())
                preview.changes.push_back({key, text, "<deleted>"});
            else if (it->second != text)
                preview.changes.push_back({key, text, it->second});
        }
        for (const auto &[key, text] : after)
            if (!before.contains(key))
                preview.changes.push_back({key, "<new>", text});
        return preview;
    } catch (const std::exception &error) {
        return std::unexpected(failure("exception", {error.what(), {}}));
    }
}
std::expected<OperationOutcome, OperationError> Operations::apply(const OperationRequest &request) {
    auto command = compile(request);
    if (!command)
        return std::unexpected(command.error());
    auto result = document_.execute(std::move(*command));
    if (!result)
        return std::unexpected(failure("rejected", result.error()));
    return OperationOutcome{*result, document_.revision(),
                            *result ? document_.model().last_report()
                                    : std::vector<RecomputeStep>{}};
}
} // namespace month06::week04
