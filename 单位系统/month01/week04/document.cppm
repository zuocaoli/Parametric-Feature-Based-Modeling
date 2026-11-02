export module course.document;
import std;
export import course.expression;
export import course.property;
export namespace units {
using MetadataValue = std::variant<bool, std::int64_t, std::string>;
enum class State { Valid, Failed, Blocked };
std::string_view state_name(State state);
struct Property {
    QuantitySpec spec;
    Quantity value;
    State state = State::Valid;
    std::optional<Expression> formula;
    std::optional<Error> failure;
};
struct RecomputeStep {
    PropertyKey key;
    State state;
};
class Model {
  public:
    explicit Model(bool initialize = true);
    const std::map<PropertyKey, Property> &properties() const;
    const UnitRegistry &registry() const;
    const std::map<PropertyKey, MetadataValue> &metadata() const;
    Result<void> set_metadata(PropertyKey key, MetadataValue value);
    std::uint64_t high_water() const;
    Result<void> set_high_water(std::uint64_t value);
    const UnitConfig &config() const;
    Result<QuantitySpec> spec(const PropertyKey &key) const;
    Result<Quantity> value(const PropertyKey &key) const;
    Result<void> add_property(PropertyKey key, Quantity value, QuantitySpec spec);
    Result<void> set_value(const PropertyKey &key, Quantity value);
    Result<void> bind(const PropertyKey &key, std::string source, VariableMap aliases = {},
                      std::span<const UnitBinding> pinned = {}, bool require_pins = false);
    Result<void> register_unit(UnitDefinition definition);
    Result<void> configure(UnitConfig config);
    Result<std::string> display(const PropertyKey &key) const;
    Result<void> check_graph() const;
    std::vector<RecomputeStep> recompute(bool all = false);
    // 权威内容，不包含缓存、派生值、计算状态或通知计数。
    std::string content() const;
    const std::vector<RecomputeStep> &last_report() const;

  private:
    std::map<PropertyKey, Property> properties_;
    UnitRegistry registry_;
    std::map<PropertyKey, MetadataValue> metadata_;
    std::uint64_t high_water_ = 1;
    UnitConfig config_;
    std::set<PropertyKey> dirty_;
    std::vector<RecomputeStep> last_report_;
};
struct DocumentEvent {
    std::uint64_t revision;
    std::vector<RecomputeStep> report;
    bool format_only;
};
using Edit = std::function<Result<void>(Model &)>;
class Document {
  public:
    Document();
    Document(const Document &) = delete;
    Document &operator=(const Document &) = delete;
    const Model &model() const;
    Result<Model> preview(const Edit &edit) const;
    Result<bool> transact(const Edit &edit);
    Result<bool> undo();
    Result<bool> redo();
    Result<void> replace(Model model);
    bool modified() const;
    void mark_saved();
    std::uint64_t revision() const;
    std::uint64_t generation() const;
    std::uint64_t identity() const;
    std::size_t history_size() const;
    std::size_t position() const;
    const std::vector<RecomputeStep> &last_report() const;
    void observe(std::function<void(const DocumentEvent &)> callback);

  private:
    struct History {
        Model before;
        Model after;
        bool model_change;
    };
    Model model_;
    std::string saved_;
    std::vector<History> history_;
    std::size_t position_ = 0;
    std::uint64_t revision_ = 0, generation_ = 1;
    std::uint64_t identity_;
    bool busy_ = false;
    std::vector<RecomputeStep> report_;
    std::vector<std::function<void(const DocumentEvent &)>> observers_;
    void publish(bool format_only);
    Result<bool> move_history(bool forward);
};
} // namespace units
