export module month03.week04.engine;

import std;
export import month03.week04.dependencies;
export import month03.week04.evaluator;
export import month03.week04.rectangle;

export namespace month03::week04 {
struct EngineError {
    std::string message;
    std::vector<PropertyKey> cycle;
};

enum class ComputeState { Valid, Dirty, Failed, Blocked, MissingReference };

struct PropertyStatus {
    ComputeState state = ComputeState::Valid;
    std::string reason;
};

std::string_view state_name(ComputeState state);

struct RecomputeStep {
    PropertyKey target;
    std::size_t executions;
    ComputeState state;
    std::string message;
};

class RecomputeEngine {
public:
    RecomputeEngine() = default;
    RecomputeEngine(const RecomputeEngine &) = delete;
    RecomputeEngine &operator=(const RecomputeEngine &) = delete;
    ObjectId create_rectangle(double length = 100, double width = 1);
    std::optional<Value> stored_value(const PropertyKey &key) const;
    // value 只交出有效结果；stored_value 用于诊断保留的旧数值。
    std::optional<Value> value(const PropertyKey &key) const;
    PropertyStatus status(const PropertyKey &key) const;
    SetResult set(PropertyKey key, const Value &value);
    SetResult set_many(ObjectId object, const std::vector<Update> &updates);
    std::expected<void, EngineError> bind(PropertyKey target, std::string_view source,
                                          const VariableMap &aliases = {});
    std::optional<std::string> formula(const PropertyKey &target) const;
    std::expected<bool, EngineError> unbind(PropertyKey target);
    std::expected<bool, EngineError> erase(ObjectId object);
    std::expected<void, EngineError> begin_batch();
    std::expected<void, EngineError> end_batch();
    ChangeSignal::Connection observe(ObjectId object,
                                     std::function<void(const ChangeEvent &)> callback);
    const DependencyGraph &graph() const;
    const KeySet &dirty() const;
    const std::vector<RecomputeStep> &last_report() const;

private:
    struct Managed {
        std::shared_ptr<Rectangle> object;
        ChangeSignal::Connection connection;
    };
    struct Binding {
        std::string source;
        Tree tree;
        VariableMap aliases;
    };

    ObjectId next_id_ = 1;
    std::map<ObjectId, Managed> objects_;
    DependencyGraph graph_;
    std::map<PropertyKey, Binding> bindings_;
    KeySet dirty_;
    std::map<PropertyKey, PropertyStatus> statuses_;
    std::vector<RecomputeStep> report_;
    std::size_t batch_depth_ = 0;
    bool writing_ = false;
    bool recomputing_ = false;

    Rectangle *find_object(ObjectId id);
    const Rectangle *find_object(ObjectId id) const;
    bool busy() const;
    void collect_changes(const ChangeEvent &event);
    void mark_downstream(const PropertyKey &source);
    void mark_dirty(const PropertyKey &target);
    void flush_if_ready();
};
} // namespace month03::week04
