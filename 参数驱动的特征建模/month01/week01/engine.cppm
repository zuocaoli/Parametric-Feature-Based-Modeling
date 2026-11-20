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

//
//
//
enum class ComputeState {
    Valid,           //  本轮算成功并写回，或该属性本来就没问题。
    Dirty,           //  已排队待算，还没轮到（批次内最常见）。
    Failed,          //  真正执行了 AST 但失败：求值出错（如除零）或写回被拒。
    Blocked,         //  直接依赖的上游这轮没能产出 Valid 结果，所以干脆不算了。
    MissingReference //  直接引用的属性已不存在（对象被删除），或查询一个不存在的属性。
};

//
// 每个属性记录的 “最新计算状态” 的值类型
struct PropertyStatus {
    ComputeState state = ComputeState::Valid; // 属性当前处于五种 ComputeState 中的哪一种
    std::string reason;                       // 人类可读的说明： 成功时是 "Changed" / "Unchanged"；失败/阻塞时是具体原因
};

std::string_view state_name(ComputeState state);

// 一次重算执行记录：
// 目的是让调用者观察和检查重算过程。
// 引擎把这些记录按执行顺序存入 report_，
// 通过 last_report()提供查询，便于确认执行顺序、次数和失败原因
struct RecomputeStep {
    PropertyKey target;     // 计算哪个属性，包含对象 ID 和属性名
    std::size_t executions; // 执行次数；失败也算一次尝试，阻塞或引用失效时不执行为 0
    ComputeState state;
    std::string message; // 结果说明，如 Changed、Unchanged 或错误原因
};

//
// 增量重算的协调器：输入属性变化时，找到受影响的公式，按正确顺序计算，并把结果写回属性。
//
// 它把三个部分连接起来：
//  - Rectangle/PropertyObject：保存属性、校验修改、发送变化通知。
//  - DependencyGraph：确定哪些公式受影响，以及计算顺序。
//  - Evaluator：执行公式，算出结果。
//
class RecomputeEngine {
  public:
    RecomputeEngine() = default;
    RecomputeEngine(const RecomputeEngine &) = delete;
    RecomputeEngine &operator=(const RecomputeEngine &) = delete;

    //
    ObjectId create_model_parameters();
    ObjectId create_rectangle(double length = 100, double width = 1);

    // 根据对象 ID 和属性名，读取该属性当前保存的值
    std::optional<Value> stored_value(const PropertyKey &key) const;

    // value 只交出有效结果；stored_value 用于诊断保留的旧数值。
    std::optional<Value> value(const PropertyKey &key) const;
    
    PropertyStatus status(const PropertyKey &key) const;

    // 修改单个属性，触发受影响的公式重算
    SetResult set(PropertyKey key, const Value &value);

    //
    // 一次修改同一个对象的多个输入属性，再统一重算受影响的公式，减少重复计算:
    //
    // 1.
    // 检查修改是否合法：对象存在、属性没有绑定公式，底层再检查类型和取值；任一项不合法，整次不提交。
    // 2. 统一修改并通知：长、宽全部更新后，发送一次变化通知，将受影响的公式加入 dirty_。
    // 3. 统一重算：通知结束后，area 只计算一次，得到 7200。如果处于
    // begin_batch()批次中，则延迟到最外层 end_batch() 才计算。
    SetResult set_many(ObjectId object, const std::vector<Update> &updates);

    //
    // 给某个属性绑定计算公式，让引擎在相关输入变化时自动重算该属性.
    // 必须参数： target, source.
    std::expected<void, EngineError> bind(PropertyKey target, std::string_view source, const VariableMap &aliases = {});
    std::optional<std::string> formula(const PropertyKey &target) const;
    std::expected<bool, EngineError> unbind(PropertyKey target);
    std::expected<bool, EngineError> erase(ObjectId object);

    //
    // 开启一组批量修改，将公式重算延迟到最外层 end_batch() 时统一执行。
    // 从 begin_batch() 到 end_batch() 期间输入属性立即更新，受影响的公式加入
    // dirty_，批次结束后，统一重算。
    std::expected<void, EngineError> begin_batch();
    std::expected<void, EngineError> end_batch();

    //
    ChangeSignal::Connection observe(ObjectId object, std::function<void(const ChangeEvent &)> callback);
    const DependencyGraph &graph() const;
    const KeySet &dirty() const;
    const std::vector<RecomputeStep> &last_report() const;

  private:
    // 把一个矩形对象和引擎对它的变化订阅放在一起管理.
    struct Managed {
        std::shared_ptr<PropertyObject> object;
        // 保存订阅凭证。矩形属性变化时，回调调用 collect_changes()，标记需要重算的公式
        ChangeSignal::Connection connection;
    };
    // 用于保存某个属性的公式定义，供后续重算使用
    struct Binding {
        std::string source;  // 公式原文，如 "length * width".
        Tree tree;           // 解析后的语法树，供求值器直接执行.
        VariableMap aliases; // 变量名到具体对象属性的映射.
    };

    ObjectId next_id_ = 1;

    //
    std::map<ObjectId, Managed> objects_;

    DependencyGraph graph_;

    //
    // 公式绑定表：目标属性（对象 ID + 属性名）-> 公式原文、语法树及别名映射
    // 每个目标最多一份公式；计算结果保存在目标对象的属性中
    //
    // bindings_ 是引擎的公式绑定表，记录“哪个属性由什么公式计算”
    // 同一目标最多对应一份公式，再次成功绑定会替换原公式。
    // 执行顺序由 graph_ 的拓扑排序决定；bindings_ 负责提供具体的公式。
    std::map<PropertyKey, Binding> bindings_;

    //
    // 脏属性： 就是需要通过公式重算的
    KeySet dirty_;

    //
    // 属性状态登记表:
    // 以 PropertyKey 为键，为引擎经手的每个属性保存一份最新的PropertyStatus
    std::map<PropertyKey, PropertyStatus> statuses_;

    //
    // 每个 RecomputeStep 保存一次公式计算的情况，包括目标属性、执行次数、是否成功和结果说明，
    // 调用者通过 last_report() 读取这些记录，用于检查以下情况。
    //   - 计算顺序是否正确，例如先算 width，再算依赖它的 area。
    //   - 受影响的公式是否各执行一次。
    //   - 求值或写回失败时，具体哪个属性失败，以及失败原因。
    // 新一轮重算开始前会清空旧记录，再按实际执行顺序追加本轮记录。
    std::vector<RecomputeStep> report_;

    //
    // 当前尚未结束的批次嵌套层数：
    // 批次用于合并修改、减少重复计算；这个计数器用于支持批次嵌套。
    // 每次 begin_batch() 将它加一；每次 end_batch() 将它减一。只有最外层批次结束、深度回到 0
    // 时，才允许统一重算： engine.begin_batch();  // 0 → 1 engine.set(...);       //
    // 更新输入，暂缓重算
    //
    // engine.begin_batch();  // 1 → 2
    // engine.set(...);       // 更新输入，暂缓重算
    //
    // engine.end_batch();    // 2 → 1，仍不重算
    // engine.end_batch();    // 1 → 0，统一重算
    //
    // “批次”： 告诉引擎“接下来我要修改一些属性，先别重算，等我改完再统一算”。
    // “层数”： 调用了多少次 begin_batch()，还没有用 end_batch() 配对结束。
    // 调用多少次 set() 都不会增加这个计数。如果只调用一次 begin_batch()，期间即使修改了 100
    // 个属性，batch_depth_ 也一直是 1。
    std::size_t batch_depth_ = 0;

    //
    // 引擎当前是否正在执行输入属性的修改操作，包括修改触发的同步通知回调
    bool writing_ = false;

    //
    // 引擎当前是否正在执行一轮公式求值和结果写回.
    bool recomputing_ = false;

    ObjectId create_object(const std::function<std::shared_ptr<PropertyObject>(ObjectId)> &factory);
    PropertyObject *find_object(ObjectId id);
    const PropertyObject *find_object(ObjectId id) const;
    bool busy() const;

    //
    // 收到属性变化通知后，把受影响的公式标记为“需要重新计算”
    void collect_changes(const ChangeEvent &event);

    //
    // 拿到所有直接或间接依赖 sources 的下游属性集合，添加到 dirty_，等待重算。
    void mark_downstream(const PropertyKey &source);
    void mark_dirty(const PropertyKey &target);

    //
    // 条件满足时，处理积压的重算任务：
    // 在允许重算时，处理 dirty_ 中待计算的公式，将结果写回属性。
    // 标脏负责记下“哪些公式需要算”，这个函数负责在合适的时机真正执行计算。
    void flush_if_ready();
};
} // namespace month03::week04
