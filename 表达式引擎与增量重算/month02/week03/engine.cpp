module month03.week03.engine;

import std;

namespace month03::week03 {
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

//
// 引擎的重入状态查询：只要引擎正处于“内部写状态”，就返回true，公共入口据此拒绝调用。
// 它不是线程锁——这个引擎是单线程的，防的是同一调用栈内的重入（回调里再调引擎）。
bool RecomputeEngine::busy() const { return writing_ || recomputing_; }

Rectangle *RecomputeEngine::find_object(ObjectId id) {
    const auto found = objects_.find(id);
    return found == objects_.end() ? nullptr : found->second.object.get();
}

const Rectangle *RecomputeEngine::find_object(ObjectId id) const {
    const auto found = objects_.find(id);
    return found == objects_.end() ? nullptr : found->second.object.get();
}

ObjectId RecomputeEngine::create_rectangle(double length, double width) {

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

    // 创建一个新的 Rectangle，把它纳入引擎的生命周期管理，并接入依赖图的变更通知体系。
    auto object = std::make_shared<Rectangle>(length, width, id);

    // 订阅属性变更，注册的回调函数是 collect_changes，该函数收集所有需要重算的属性(属性公式)，加入 dirty_，等待重算。
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
    }

    //
    // result == SetResult::Changed： 输入修改正常被接受时，就尝试执行重算。
    //
    // result == SetResult::Unchanged： 为什么 Unchanged 也调用重算：
    // 因为 dirty_中可能还留着之前尚未完成的公式，例如上轮求值失败留下的待计算目标。
    // 即使本次输入没变，也允许尝试处理这些目标。
    if (result == SetResult::Changed || result == SetResult::Unchanged) {
        flush_if_ready();
    }

    return result;
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
        // std::holds_alternative<double>， 在运行时检查 std::variant 对象当前是否持有 double 类型的值。
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
    // std::map::insert_or_assign() 是 C++17 引入的成员函数，用于向 std::map 中插入键值对，如果键已存在则更新其值。

    // 检查通过的新依赖图 candidate，通过移动赋值提交给正式成员 graph_，替换原来的图。
    // graph_ 对象本身没有被销毁，它仍是引擎中的同一个成员，只是内部管理的数据换成了新图
    graph_ = std::move(candidate); // 允许赋值操作接管 candidate 的内部资源，避免再次完整复制图

    // 绑定表达式后，立即标记这个属性为 dirty_，等待重算。
    dirty_.insert(target);

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

        if (bindings_.contains(key)) {
            // 检查属性 key 是否绑定了公式，如果绑定了就添加到 dirty_，等待重算。
            dirty_.insert(key);
        }
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
        mark_downstream({event.source_id, change.name});
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

        // 拿到计算结果或者报错信息
        const auto value = //
            evaluate(
                //
                *found->second.tree,
                //
                [&](std::string_view name) {                                                      //
                    return stored_value(resolve_key(target.object, name, found->second.aliases)); //
                } //

            );

        if (!value) {
            // 添加一条失败记录，其中 target 是目标属性，1 表示尝试执行一次，false 表示失败，最后一项是转换成字符串的错误原因。
            // 某一属性计算失败后就不能继续了，因为后续的公式依赖这个属性，继续计算只会产生更多错误。
            report_.push_back({target, 1, false, expression_error(value.error())});
            break;
        }

        // 重算成功后将结果写回目标属性，写回失败也会记录在 report_ 中。
        const auto result = find_object(target.object)->set(target.name, *value);

        if (result != SetResult::Changed && result != SetResult::Unchanged) {
            // Unchanged 表示写入被接受，只是新值与旧值相同。
            // 新属性写入失败也要记录在 report_ 中。
            report_.push_back({target, 1, false, "write rejected: " + std::string{result_name(result)}});
            break;
        }

        // 该属性已经成功完成求值和结果写回，本次重算任务完成。
        dirty_.erase(target);

        // 向 report_ 追加一条当前属性重算成功的记录。
        report_.push_back({target, 1, true, std::string{result_name(result)}});
    }
}
} // namespace month03::week03
