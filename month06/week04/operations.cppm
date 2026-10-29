export module month06.week04.operations;
import std;
export import month06.week04.document;

export namespace month06::week04 {
enum class OperationKind { SetValue, SetFormula, ClearFormula, Create, Delete };
struct Operation {
    OperationKind kind;
    PropertyKey target{};
    Value value = 0.0;
    std::string formula;
    VariableMap aliases;
    double length = 80, width = 40;
};
struct OperationRequest {
    std::uint64_t expected_revision;
    std::vector<Operation> operations;
};
struct OperationError {
    std::string code;
    std::string message;
    std::uint64_t current_revision;
    std::optional<std::size_t> operation;
    std::optional<SourceRange> range;
    std::vector<PropertyKey> cycle;
};
struct OperationOutcome {
    bool changed;
    std::uint64_t revision;
    std::vector<RecomputeStep> diagnostics; // Accepted edits may still have Failed/Blocked results.
};
struct ProposedChange {
    PropertyKey target;
    std::string before;
    std::string after;
};
struct OperationPreview {
    std::uint64_t revision;
    std::vector<ProposedChange> changes;
    std::vector<RecomputeStep> diagnostics;
};
// In-process, typed interface. A future transport must decode into these types first.
class Operations {
public:
    explicit Operations(Document &document);
    EngineSnapshot query() const;
    std::expected<OperationPreview, OperationError> preview(const OperationRequest &request) const;
    std::expected<OperationOutcome, OperationError> apply(const OperationRequest &request);

private:
    Document &document_;
    std::expected<std::unique_ptr<Command>, OperationError>
    compile(const OperationRequest &request) const;
    OperationError failure(std::string code, const EngineError &error) const;
};
} // namespace month06::week04
