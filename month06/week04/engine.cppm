export module month06.week04.engine;

import std;
export import month06.week04.dependencies;
export import month06.week04.evaluator;
export import month06.week04.rectangle;

export namespace month06::week04 {
struct SourceRange {
    std::size_t position;
    std::size_t length;
};

struct EngineError {
    std::string message;
    std::vector<PropertyKey> cycle;
    std::optional<SourceRange> range = std::nullopt;
    std::optional<std::size_t> operation = std::nullopt;
};

enum class ComputeState { Valid, Dirty, Failed, Blocked, MissingReference };

struct PropertyStatus {
    ComputeState state = ComputeState::Valid;
    std::string reason;
    bool operator==(const PropertyStatus &) const = default;
};

std::string_view state_name(ComputeState state);

struct RecomputeStep {
    PropertyKey target;
    std::size_t executions;
    ComputeState state;
    std::string message;
};

// 内存快照含缓存/状态；磁盘格式在第四周另行定义，只保存权威数据。
struct ObjectSnapshot {
    ObjectId id;
    std::vector<Update> values;
    bool operator==(const ObjectSnapshot &) const = default;
};
struct FormulaSnapshot {
    PropertyKey target;
    std::string source;
    VariableMap aliases;
    bool operator==(const FormulaSnapshot &) const = default;
};
struct EngineSnapshot {
    ObjectId high_water = 0;
    std::vector<ObjectSnapshot> objects;
    std::vector<FormulaSnapshot> formulas;
    std::map<PropertyKey, PropertyStatus> statuses;
};

// Authoritative content: IDs, inputs, formula source and mappings; no caches/allocator.
EngineSnapshot semantic_snapshot(EngineSnapshot snapshot);
bool same_content(const EngineSnapshot &a, const EngineSnapshot &b);

// Per-document immutable AST cache; values/references are resolved on each evaluation.
struct ParseMetrics {
    std::size_t parses = 0, hits = 0;
    std::int64_t nanoseconds = 0;
};
class AstCache {
public:
    std::expected<std::shared_ptr<const Node>, Error> get(std::string_view source);
    ParseMetrics metrics() const;

private:
    std::map<std::string, std::shared_ptr<const Node>, std::less<>> entries_;
    ParseMetrics metrics_;
};
class RecomputeEngine {
public:
    RecomputeEngine() = default;
    RecomputeEngine(const RecomputeEngine &) = delete;
    RecomputeEngine &operator=(const RecomputeEngine &) = delete;
    EngineSnapshot snapshot() const;
    static std::expected<std::unique_ptr<RecomputeEngine>, EngineError>
    restore(const EngineSnapshot &snapshot, bool recompute, ObjectId high_water_floor = 0,
            std::shared_ptr<AstCache> cache = {});
    std::shared_ptr<AstCache> ast_cache() const;
    std::int64_t recompute_nanoseconds() const;
    ObjectId high_water() const;
    std::vector<ObjectId> object_ids() const;
    std::vector<PropertyView> properties(ObjectId id) const;
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
        std::shared_ptr<const Node> tree;
        VariableMap aliases;
    };

    std::shared_ptr<AstCache> cache_ = std::make_shared<AstCache>();
    std::int64_t recompute_ns_ = 0;
    ObjectId high_water_ = 0;
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
} // namespace month06::week04
