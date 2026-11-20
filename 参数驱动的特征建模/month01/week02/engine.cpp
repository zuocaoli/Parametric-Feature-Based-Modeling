module month03.week04.engine;

import std;

namespace month03::week04 {
namespace {

//
// 用来自动管理状态标志：创建时设为 true，离开作用域时设回 false.
struct FlagGuard {
    bool &flag;

    // 成员初始化列表，在执行函数体之前初始化成员，
    // 此时 flag 已经引用 writing_，因此赋值实际修改的是 writing_
    explicit FlagGuard(bool &value) : flag(value) { flag = true; }
    ~FlagGuard() { flag = false; }
};

EngineError engine_error(std::string message) { return {std::move(message), {}}; }

std::string expression_error(const Error &error) { //
    return std::string{stage_name(error.stage)}    //
           + " at byte "                           //
           + std::to_string(error.position)        //
           + ": " + error.message;                 //
}
} // namespace

std::string_view state_name(ComputeState state) {
    switch (state) {
    case ComputeState::Valid:
        return "Valid";
    case ComputeState::Dirty:
        return "Dirty";
    case ComputeState::Failed:
        return "Failed";
    case ComputeState::Blocked:
        return "Blocked";
    case ComputeState::MissingReference:
        return "MissingReference";
    }
    return "Unknown";
}

//
// 引擎的重入状态查询：只要引擎正处于“内部写状态”，就返回true，公共入口据此拒绝调用。
// 它不是线程锁——这个引擎是单线程的，防的是同一调用栈内的重入（回调里再调引擎）。
bool RecomputeEngine::busy() const { return writing_ || recomputing_; }

PropertyObject *RecomputeEngine::find_object(ObjectId id) {
    const auto found = objects_.find(id);
    return found == objects_.end() ? nullptr : found->second.object.get();
}

const PropertyObject *RecomputeEngine::find_object(ObjectId id) const {
    const auto found = objects_.find(id);
    return found == objects_.end() ? nullptr : found->second.object.get();
}

namespace {
class ModelParameters : public PropertyObject {
  public:
    explicit ModelParameters(ObjectId id) : PropertyObject(id) {
        const auto finite = [](const Value &value) { return std::isfinite(std::get<double>(value)); };
        add("t", "Thickness input", "mm", 10.0, finite);
        add("height", "Extrusion height", "mm", 0.0, finite);
        add("depth", "Cut depth", "mm", 5.0, finite);
    }
};
}

ObjectId RecomputeEngine::create_rectangle(double length, double width) {
    return create_object([=](ObjectId id) { return std::make_shared<Rectangle>(length, width, id); });
}

ObjectId RecomputeEngine::create_model_parameters() {
    return create_object([](ObjectId id) { return std::make_shared<ModelParameters>(id); });
}

ObjectId RecomputeEngine::create_object(const std::function<std::shared_ptr<PropertyObject>(ObjectId)> &factory) {

    if (busy()) {
        throw std::logic_error("cannot create an object during modification or recomputation");
    }

    // ID 从 1 开始发，++next_id_ 递增。uint64_t 是无符号类型, 所以溢出后会变成 0，
    // 0 说明 ID 已用完。
    if (next_id_ == 0) {
        throw std::overflow_error("object IDs exhausted");
    }

    //
    const auto id = next_id_;

    auto object = factory(id);

    // 订阅属性变更，注册的回调函数是 collect_changes，该函数收集所有需要重算的属性(属性公式)，加入
    // dirty_，等待重算。
    auto connection = //
        object->observe([this](const ChangeEvent &event) { collect_changes(event); });

    // 以 id 为键，把新矩形和它的变化订阅存入引擎，交给引擎持续管理
    objects_.emplace(id, Managed{std::move(object), std::move(connection)});

    //
    ++next_id_;
    return id;
}

std::optional<Value> RecomputeEngine::stored_value(const PropertyKey &key) const {
    const auto *object = find_object(key.object);
    return object ? object->get(key.name) : std::nullopt;
}

PropertyStatus RecomputeEngine::status(const PropertyKey &key) const {
    if (!stored_value(key)) {
        return {ComputeState::MissingReference, "missing property: " + key_text(key)};
    }
    const auto found = statuses_.find(key);
    return found == statuses_.end() ? PropertyStatus{} : found->second;
}

std::optional<Value> RecomputeEngine::value(const PropertyKey &key) const {
    return status(key).state == ComputeState::Valid ? stored_value(key) : std::nullopt;
}

SetResult RecomputeEngine::set(PropertyKey key, const Value &value) {
    //
    return set_many(key.object, {{std::move(key.name), value}}); //
}

SetResult RecomputeEngine::set_many(ObjectId id, const std::vector<Update> &updates) {
    if (busy()) {
        // 正在修改
        return SetResult::Reentrant;
    }

    // 清除上次的执行记录，避免调用者把旧报告误认为本次调用的结果
    report_.clear();

    auto *object = find_object(id);
    if (!object) {
        return SetResult::NotFound;
    }

    for (const auto &update : updates) {
        if (bindings_.contains({id, update.name})) {
            return SetResult::ReadOnly;
        }

        // 遍历准备修改的属性，只要其中一个已经绑定公式，就拒绝整次修改，
        // 因为绑定公式后，这个属性的值就由公式决定了，直接赋值会与公式产生冲突。
    }

    SetResult result;
    {
        // 将 writing_ 标志设置为 true，表示引擎正在执行修改操作。
        FlagGuard guard{writing_};

        //
        // 修改属性:
        // 提交属性修改, 触发回调（回调中标记了需要重算的属性）。
        result = object->set_many(updates);
    } // 同步通知与底层 NotifyGuard 已结束，才允许公式写回。

    //
    // result == SetResult::Changed： 输入修改正常被接受时，就尝试执行重算。
    //
    // result == SetResult::Unchanged： 为什么 Unchanged 也调用重算：
    // 因为 dirty_中可能还留着之前尚未完成的公式，例如上轮求值失败留下的待计算目标。
    // 即使本次输入没变，也允许尝试处理这些目标。
    if (result == SetResult::Changed || result == SetResult::Unchanged) {
        flush_if_ready();
    }

    return result; // 输入修改结果；公式执行结果另见 last_report。
}

std::expected<void, EngineError> RecomputeEngine::bind(PropertyKey target, std::string_view source, const VariableMap &aliases) {
    if (busy()) {
        // 如果正在修改/重算是不能更改表达式的
        return std::unexpected(engine_error("cannot bind during modification or recomputation"));
    }

    // 绑定表达式会触发重算，重算前要清空上次的执行记录，避免调用者把旧报告误认为本次调用的结果。
    report_.clear();

    // 读取属性的当前值，检查它是否存在且是 double 类型；如果不是，就拒绝绑定。
    const auto value = stored_value(target);
    if (!value || !std::holds_alternative<double>(*value)) {
        // std::holds_alternative<double>， 在运行时检查 std::variant 对象当前是否持有 double
        // 类型的值。
        return std::unexpected(engine_error("target must be an existing double property"));
    }

    // 读取这个 object 的属性快照
    const auto properties = find_object(target.object)->properties();
    // 找到当前要绑定表达式的那个属性
    const auto view = std::find_if(                                //
        properties.begin(),                                        //
        properties.end(),                                          //
        [&](const auto &item) { return item.name == target.name; } //
    );
    if (view->read_only) {
        return std::unexpected(engine_error("target is read-only"));
    }

    //
    // ========================================================================================
    //

    // 生成表达式的语法树
    auto tree = parse(source);
    if (!tree) {
        return std::unexpected(engine_error(expression_error(tree.error())));
    }
    // 获取公式直接引用的属性集合
    const auto dependencies = resolve_dependencies(**tree, target.object, aliases);
    // 遍历依赖的属性
    for (const auto &key : dependencies) {
        // 检查该属性的值是否合法
        const auto input = stored_value(key);
        if (!input || (!std::holds_alternative<double>(*input) && !std::holds_alternative<std::int64_t>(*input))) {
            return std::unexpected(engine_error("invalid numeric reference: " + key_text(key)));
        }
    }

    //
    // =================================================================================
    //

    // 复制当前依赖图，先在副本上尝试修改，成功后再提交到引擎。
    auto candidate = graph_;

    // 建立该属性的依赖属性集合
    const auto valid = candidate.try_replace_dependencies(target, dependencies);
    if (!valid) {
        return std::unexpected(EngineError{valid.error().message, valid.error().cycle});
    }

    // 表达式绑定
    Binding binding{std::string{source}, std::move(*tree), aliases};
    bindings_.insert_or_assign(target, std::move(binding));
    // std::map::insert_or_assign() 是 C++17 引入的成员函数，用于向 std::map
    // 中插入键值对，如果键已存在则更新其值。

    // 检查通过的新依赖图 candidate，通过移动赋值提交给正式成员 graph_，替换原来的图。
    // graph_ 对象本身没有被销毁，它仍是引擎中的同一个成员，只是内部管理的数据换成了新图
    graph_ = std::move(candidate); // 允许赋值操作接管 candidate 的内部资源，避免再次完整复制图

    mark_dirty(target); // 常量公式也要计算，不能只从前置变化出发。

    // 将所有受影响的下游公式目标加入 dirty_，等待重算
    mark_downstream(target);

    // 进行重算，计算所有受影响的公式，更新它们的目标属性值。
    flush_if_ready();

    return {};
}

std::optional<std::string> RecomputeEngine::formula(const PropertyKey &target) const {
    const auto found = bindings_.find(target);
    return found == bindings_.end() ? std::nullopt : std::optional{found->second.source};
}

std::expected<bool, EngineError> RecomputeEngine::unbind(PropertyKey target) {
    if (busy()) {
        return std::unexpected(engine_error("cannot unbind during a callback"));
    }
    report_.clear();
    const auto found = bindings_.find(target);
    if (found == bindings_.end()) {
        return false;
    }
    graph_.remove_formula(target);
    bindings_.erase(found);
    dirty_.erase(target);
    statuses_[target] = {}; // 保留的数值成为普通输入，而非失败公式的结果。
    mark_downstream(target);
    flush_if_ready();
    return true;
}

std::expected<bool, EngineError> RecomputeEngine::erase(ObjectId id) {

    //
    if (busy()) {
        return std::unexpected(engine_error("cannot erase during a callback"));
    }

    report_.clear();

    const auto found = objects_.find(id);
    if (found == objects_.end()) {
        return false;
    }

    //
    // 在对象被拆除之前，把“删除对象”翻译成依赖图听得懂的语言：
    // “a 的全部属性一次性失效”，趁图还完整，把受影响的下游全部标脏、安排重算。
    //
    // 平时属性变化的路是：set 写值 → Rectangle 发 ChangeEvent → collect_changes → 对每个变化属性 mark_downstream。
    // 但是删除不走任何通知通道，对象销毁后订阅断开，不会再有任何事件发出来。
    for (const auto &property : found->second.object->properties()) {

        // 对每个属性 mark_downstream，语义上等价于“假设 a 的所有属性同时都变了一遍”。
        mark_downstream({id, property.name});

        //
        // 既然都要删除了，为啥还要重算下游：
        //
        // b.width 绑着公式 source_segments + 1，输入源是 {a, segments}。
        // 如果不重算，b.width 会是什么样：
        // - 删除前：b.width = 4，状态 Valid，值可信（基于 a.segments = 3）；
        // - 删除后不重算：b.width = 4，状态仍然 Valid——但这个 4 的依据（a.segments）已经不存在了。
        // 很显然，问题在于，值没变，可信度塌了，状态却没跟着塌。后果：
        // - value({b, "width"}) 照样交出 4、has_value() 是 true——引擎在为一个失去依据的值继续背书，闸门形同虚设；
        // - 下游 b.area = 400 也顶着 Valid，整条链都在“装没事”；
        // - 没有任何机制告诉 b 的消费者：你的数据源没了。
        //
        // 所以这次“重算”的产出不是值，是状态
    }

    for (auto binding = bindings_.begin(); binding != bindings_.end();) {
        if (binding->first.object != id) {
            ++binding;
            continue;
        }

        //
        // 把 a 作为“公式目标”的身份彻底拆除，它身上绑过的每条公式，登记处各清一遍。
        //

        // 通知依赖图：{1,width} 不再是一个公式目标——把它的出边（它对上游的依赖）从图上拆掉，而入边（别人对它的引用）一根不动。
        // 就是他不再依赖其他的，但是还有其他的依赖他。
        graph_.remove_formula(binding->first);

        // 把即将销毁的公式从“待算队列”里撤单
        dirty_.erase(binding->first);

        // 删除对象时，把它绑过的公式从绑定表里一并清除
        binding = bindings_.erase(binding);
    }

    // 把“状态登记表”里属于被删对象的每条记录，一次性批量删除。
    std::erase_if(statuses_, [&](const auto &item) { return item.first.object == id; });

    // 删除这个 Object
    objects_.erase(found);

    //
    // 把删除的后果立刻结算成状态(这次重算的产出不是值，是状态)
    flush_if_ready();

    return true;
}

std::expected<void, EngineError> RecomputeEngine::begin_batch() {
    if (busy()) {
        // 引擎正在重算，不允许在回调中打开新的批次，因为回调里可能会修改属性，导致递归重算。
        return std::unexpected(engine_error("cannot open a batch during a callback"));
    }

    // 取得 std::size_t 类型能表示的最大值，防止批次计数 batch_depth_ 溢出。
    if (batch_depth_ == std::numeric_limits<std::size_t>::max()) {
        return std::unexpected(engine_error("batch nesting exhausted"));
    }

    // 开始一个新的最外层批次时，清空之前的重算记录。
    if (batch_depth_ == 0) {
        // - batch_depth_ == 0 表示当前没有打开的批次，此次 begin_batch() 需要开启一组新的操作
        report_.clear();
    }

    ++batch_depth_;
    return {};
}

std::expected<void, EngineError> RecomputeEngine::end_batch() {
    if (busy() || batch_depth_ == 0) {
        return std::unexpected(engine_error("no closable batch"));
    }
    --batch_depth_;
    flush_if_ready();
    return {};
}

ChangeSignal::Connection RecomputeEngine::observe(ObjectId id, std::function<void(const ChangeEvent &)> callback) {
    auto *object = find_object(id);
    if (!object) {
        throw std::invalid_argument("unknown object");
    }
    return object->observe(std::move(callback));
}

const DependencyGraph &RecomputeEngine::graph() const { return graph_; }

const KeySet &RecomputeEngine::dirty() const { return dirty_; }

const std::vector<RecomputeStep> &RecomputeEngine::last_report() const { return report_; }

void RecomputeEngine::mark_downstream(const PropertyKey &source) {
    // 从指定属性出发，将所有受影响的下游公式目标加入 dirty_，等待重算：
    // 拿到所有直接或间接依赖 sources 的下游属性集合，逐一遍历：
    for (const auto &key : graph_.affected({source})) {
        mark_dirty(key);
    }
}

void RecomputeEngine::mark_dirty(const PropertyKey &target) {
    // 检查属性 target 是否绑定了公式，如果绑定了就添加到 dirty_，等待重算。
    if (bindings_.contains(target)) {
        dirty_.insert(target);
        statuses_[target] = {ComputeState::Dirty, "awaiting recomputation"};
    }
}

// 接收属性变化通知，将受影响的下游公式加入 dirty_，实际计算由 flush_if_ready() 执行。
void RecomputeEngine::collect_changes(const ChangeEvent &event) {
    if (recomputing_) {
        // 如果正在重算，就不要重复标记下游公式了，因为重算过程中已经在处理 dirty_。
        return;
    }

    // 遍历所有发生变化的属性，所有依赖这个属性的属性都要重算，
    // 然后通过 mark_downstream 将需要重算的属性的表达式加入 dirty_，等待重算。
    for (const auto &change : event.changes) {
        const PropertyKey key{event.source_id, change.name};
        statuses_[key] = {};
        mark_downstream(key);
    }
}

void RecomputeEngine::flush_if_ready() {

    // 判断现在能不能开始重算；时机不合适，就先返回
    if (batch_depth_ != 0 || busy()) {
        // batch_depth_ != 0 就是还有属性设置批次没结束，不能重算。
        // busy() 就是引擎正在修改属性或执行重算
        return;
    }

    // 新一轮重算开始前要清空旧记录
    report_.clear();

    // 拿到 dirty_ 中所有待计算目标的拓扑排序，按顺序执行公式计算。
    const auto order = graph_.topological_order(dirty_);
    if (!order) {
        throw std::logic_error("committed graph is cyclic");
    }

    // 设置引擎状态为正在重算，防止在回调中再次调用引擎。
    FlagGuard guard{recomputing_};

    //
    // 遍历拓扑排序的属性列表，按顺序执行公式计算：
    for (const auto &target : *order) {

        // 拿出 dirty_ 中的一个待计算目标，检查它是否绑定了公式，如果没有就跳过。
        const auto found = bindings_.find(target);
        if (found == bindings_.end()) {
            dirty_.erase(target);
            continue;
        }

        //
        // 检查依赖的属性是否存在
        PropertyStatus outcome;
        for (const auto &parent : graph_.dependencies(target)) {
            if (!stored_value(parent)) {
                // 依赖的属性不存在
                outcome = {ComputeState::MissingReference, "missing reference: " + key_text(parent)};
                break;
            }
        }

        if (outcome.state == ComputeState::Valid) {
            // outcome.state == ComputeState::Valid 表示“前面的检查都没发现问题，
            // 可以继续下一项检查

            // 检查依赖的属性状态是否正常
            for (const auto &parent : graph_.dependencies(target)) {

                // 查询该属性状态是否为 Valid
                if (status(parent).state != ComputeState::Valid) {

                    // 状态不是 Valid
                    outcome = {
                        ComputeState::Blocked,            // 修改状态为 Blocked
                        "blocked by: " + key_text(parent) //  记录是谁阻塞了重算操作
                    };

                    break;
                }
            }
        }

        //
        std::size_t executions = 0;

        //
        if (outcome.state == ComputeState::Valid) {

            executions = 1;

            // 求解表达式
            const auto result =                  //
                evaluate(                        //
                    *found->second.tree,         //
                    [&](std::string_view name) { //
                        return value(resolve_key(target.object, name, found->second.aliases));
                    } //
                );

            if (!result) {
                // 计算出错
                outcome = {ComputeState::Failed, expression_error(result.error())};
            } else {

                // 计算成功，将新值更新到 object
                const auto assigned = find_object(target.object)->set(target.name, *result);
                // 更新属性状态
                if (assigned == SetResult::Changed || assigned == SetResult::Unchanged) {
                    outcome.reason = std::string{result_name(assigned)};
                } else {
                    outcome = {ComputeState::Failed, "write rejected: " + std::string{result_name(assigned)}};
                }
            }
        }

        //
        statuses_[target] = outcome;

        //
        dirty_.erase(target);

        //
        report_.push_back({target, executions, outcome.state, std::move(outcome.reason)});
    }
}
} // namespace month03::week04
