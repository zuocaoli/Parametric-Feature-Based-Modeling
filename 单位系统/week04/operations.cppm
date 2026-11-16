export module course.operations;
import std;
export import course.document;
export namespace units {
struct SetValue {
    PropertyKey target;
    Quantity value;
};
struct SetFormula {
    PropertyKey target;
    std::string source;
    VariableMap aliases;
};
struct SetConfig {
    UnitConfig config;
};
struct AddUnit {
    UnitDefinition definition;
};
struct RebindUnit {
    UnitRef definition;
}; // 显式迁移已有公式到此定义版本
using Operation = std::variant<SetValue, SetFormula, SetConfig, AddUnit, RebindUnit>;
struct OperationRequest {
    std::uint64_t expected_revision;
    std::vector<Operation> operations;
};
struct OperationOutcome {
    bool changed;
    std::uint64_t revision;
    std::vector<RecomputeStep> report;
};
struct OperationPreview {
    std::uint64_t revision;
    Model before;
    Model after;
    std::vector<RecomputeStep> report;
};
class Operations {
  public:
    explicit Operations(Document &document);
    Model query() const;
    Result<OperationPreview> preview(const OperationRequest &request) const;
    Result<OperationOutcome> apply(const OperationRequest &request);

  private:
    Document &document_;
    Result<void> validate_request(const OperationRequest &request) const;
    Result<void> execute(Model &model, const OperationRequest &request) const;
};
} // namespace units
