module month05.week04.engine;

import std;

namespace month05::week04 {
namespace {
struct FlagGuard {
    bool &flag;
    explicit FlagGuard(bool &value) : flag(value) {
        flag = true;
    }
    ~FlagGuard() {
        flag = false;
    }
};

EngineError engine_error(std::string message) {
    return {std::move(message), {}};
}

std::string expression_error(const Error &error) {
    return std::string{stage_name(error.stage)} + " at byte " + std::to_string(error.position) +
           ": " + error.message;
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

bool RecomputeEngine::busy() const {
    return writing_ || recomputing_;
}

Rectangle *RecomputeEngine::find_object(ObjectId id) {
    const auto found = objects_.find(id);
    return found == objects_.end() ? nullptr : found->second.object.get();
}

const Rectangle *RecomputeEngine::find_object(ObjectId id) const {
    const auto found = objects_.find(id);
    return found == objects_.end() ? nullptr : found->second.object.get();
}

std::vector<ObjectId> RecomputeEngine::object_ids() const {
    std::vector<ObjectId> ids;
    for (const auto &[id, _] : objects_) {
        ids.push_back(id);
    }
    return ids;
}

std::vector<PropertyView> RecomputeEngine::properties(ObjectId id) const {
    const auto *object = find_object(id);
    return object ? object->properties() : std::vector<PropertyView>{};
}

ObjectId RecomputeEngine::high_water() const {
    return high_water_;
}

EngineSnapshot RecomputeEngine::snapshot() const {
    EngineSnapshot result;
    result.high_water = high_water_;
    for (const auto &[id, managed] : objects_) {
        ObjectSnapshot item{id, {}};
        for (const auto &property : managed.object->properties()) {
            item.values.push_back({property.name, property.value});
        }
        result.objects.push_back(std::move(item));
    }
    for (const auto &[target, binding] : bindings_) {
        result.formulas.push_back({target, binding.source, binding.aliases});
    }
    result.statuses = statuses_;
    return result;
}

std::expected<std::unique_ptr<RecomputeEngine>, EngineError>
RecomputeEngine::restore(const EngineSnapshot &snapshot, bool recompute, ObjectId floor) {
    auto engine = std::make_unique<RecomputeEngine>();
    engine->high_water_ = std::max(snapshot.high_water, floor);
    for (const auto &item : snapshot.objects) {
        if (item.id == 0 || item.id > snapshot.high_water || engine->objects_.contains(item.id)) {
            return std::unexpected(engine_error("invalid or duplicate object ID"));
        }
        auto object = std::make_shared<Rectangle>(100, 1, item.id);
        std::set<std::string> names;
        for (const auto &update : item.values) {
            if (!names.insert(update.name).second) {
                return std::unexpected(engine_error("duplicate property"));
            }
        }
        const auto result = object->set_many(item.values);
        if (result != SetResult::Changed && result != SetResult::Unchanged) {
            return std::unexpected(
                engine_error("invalid property snapshot: " + std::string{result_name(result)}));
        }
        auto connection = object->observe(
            [ptr = engine.get()](const ChangeEvent &event) { ptr->collect_changes(event); });
        engine->objects_.emplace(item.id, Managed{std::move(object), std::move(connection)});
    }
    // 与用户新绑定不同：恢复允许引用已删除的 ID，保留依赖边及错误状态。
    for (const auto &item : snapshot.formulas) {
        const auto target = engine->stored_value(item.target);
        if (!target || !std::holds_alternative<double>(*target) ||
            engine->bindings_.contains(item.target)) {
            return std::unexpected(engine_error("invalid or duplicate formula target"));
        }
        auto tree = parse(item.source);
        if (!tree) {
            return std::unexpected(
                EngineError{expression_error(tree.error()),
                            {},
                            SourceRange{tree.error().position, tree.error().length}});
        }
        const auto dependencies = resolve_dependencies(**tree, item.target.object, item.aliases);
        for (const auto &key : dependencies) {
            if (key.object == 0 || key.object > snapshot.high_water) {
                return std::unexpected(engine_error("reference outside allocated ID range"));
            }
            const auto value = engine->stored_value(key);
            if (engine->find_object(key.object) &&
                (!value || (!std::holds_alternative<double>(*value) &&
                            !std::holds_alternative<std::int64_t>(*value)))) {
                return std::unexpected(engine_error("invalid numeric reference: " + key_text(key)));
            }
        }
        auto valid = engine->graph_.try_replace_dependencies(item.target, dependencies);
        if (!valid) {
            return std::unexpected(EngineError{valid.error().message, valid.error().cycle});
        }
        engine->bindings_.emplace(item.target,
                                  Binding{item.source, std::move(*tree), item.aliases});
    }
    if (recompute) {
        for (const auto &[key, _] : engine->bindings_) {
            engine->mark_dirty(key);
        }
        engine->flush_if_ready();
    } else {
        engine->statuses_ = snapshot.statuses;
    }
    return engine;
}

ObjectId RecomputeEngine::create_rectangle(double length, double width) {
    if (!std::isfinite(length) || !std::isfinite(width) || length <= 0 || width <= 0) {
        throw std::invalid_argument("rectangle dimensions must be finite and positive");
    }
    if (busy()) {
        throw std::logic_error("cannot create an object during modification or recomputation");
    }
    if (high_water_ == std::numeric_limits<ObjectId>::max()) {
        throw std::overflow_error("object IDs exhausted");
    }
    const auto id = high_water_ + 1;
    auto object = std::make_shared<Rectangle>(length, width, id);
    auto connection = object->observe([this](const ChangeEvent &event) { collect_changes(event); });
    objects_.emplace(id, Managed{std::move(object), std::move(connection)});
    high_water_ = id;
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
    return set_many(key.object, {{std::move(key.name), value}});
}

SetResult RecomputeEngine::set_many(ObjectId id, const std::vector<Update> &updates) {
    if (busy()) {
        return SetResult::Reentrant;
    }
    report_.clear();
    auto *object = find_object(id);
    if (!object) {
        return SetResult::NotFound;
    }
    for (const auto &update : updates) {
        if (bindings_.contains({id, update.name})) {
            return SetResult::ReadOnly; // 绑定目标由计算器写，外部只修改输入。
        }
    }
    SetResult result;
    {
        FlagGuard guard{writing_};
        result = object->set_many(updates);
    } // 同步通知与底层 NotifyGuard 已结束，才允许公式写回。
    if (result == SetResult::Changed || result == SetResult::Unchanged) {
        flush_if_ready();
    }
    return result; // 输入修改结果；公式执行结果另见 last_report。
}

std::expected<void, EngineError> RecomputeEngine::bind(PropertyKey target, std::string_view source,
                                                       const VariableMap &aliases) {
    if (busy()) {
        return std::unexpected(engine_error("cannot bind during modification or recomputation"));
    }
    report_.clear();
    const auto value = stored_value(target);
    if (!value || !std::holds_alternative<double>(*value)) {
        return std::unexpected(engine_error("target must be an existing double property"));
    }
    const auto properties = find_object(target.object)->properties();
    const auto view = std::find_if(properties.begin(), properties.end(),
                                   [&](const auto &item) { return item.name == target.name; });
    if (view->read_only) {
        return std::unexpected(engine_error("target is read-only"));
    }
    auto tree = parse(source);
    if (!tree) {
        return std::unexpected(
            EngineError{expression_error(tree.error()),
                        {},
                        SourceRange{tree.error().position, tree.error().length}});
    }
    const auto dependencies = resolve_dependencies(**tree, target.object, aliases);
    for (const auto &key : dependencies) {
        const auto input = stored_value(key);
        if (!input || (!std::holds_alternative<double>(*input) &&
                       !std::holds_alternative<std::int64_t>(*input))) {
            return std::unexpected(engine_error("invalid numeric reference: " + key_text(key)));
        }
    }
    auto candidate = graph_;
    const auto valid = candidate.try_replace_dependencies(target, dependencies);
    if (!valid) {
        return std::unexpected(EngineError{valid.error().message, valid.error().cycle});
    }
    Binding binding{std::string{source}, std::move(*tree), aliases};
    bindings_.insert_or_assign(target, std::move(binding));
    graph_ = std::move(candidate);
    mark_dirty(target); // 常量公式也要计算，不能只从前置变化出发。
    mark_downstream(target);
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
    if (busy()) {
        return std::unexpected(engine_error("cannot erase during a callback"));
    }
    report_.clear();
    const auto found = objects_.find(id);
    if (found == objects_.end()) {
        return false;
    }
    // 先沿旧关系收集影响，再清除被删除对象自己的公式和订阅。
    for (const auto &property : found->second.object->properties()) {
        mark_downstream({id, property.name});
    }
    for (auto binding = bindings_.begin(); binding != bindings_.end();) {
        if (binding->first.object != id) {
            ++binding;
            continue;
        }
        graph_.remove_formula(binding->first); // 保留下游对这个已失效标识的引用。
        dirty_.erase(binding->first);
        binding = bindings_.erase(binding);
    }
    std::erase_if(statuses_, [&](const auto &item) { return item.first.object == id; });
    objects_.erase(found);
    flush_if_ready();
    return true;
}

std::expected<void, EngineError> RecomputeEngine::begin_batch() {
    if (busy()) {
        return std::unexpected(engine_error("cannot open a batch during a callback"));
    }
    if (batch_depth_ == std::numeric_limits<std::size_t>::max()) {
        return std::unexpected(engine_error("batch nesting exhausted"));
    }
    if (batch_depth_ == 0) {
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

ChangeSignal::Connection
RecomputeEngine::observe(ObjectId id, std::function<void(const ChangeEvent &)> callback) {
    auto *object = find_object(id);
    if (!object) {
        throw std::invalid_argument("unknown object");
    }
    return object->observe(std::move(callback));
}

const DependencyGraph &RecomputeEngine::graph() const {
    return graph_;
}

const KeySet &RecomputeEngine::dirty() const {
    return dirty_;
}

const std::vector<RecomputeStep> &RecomputeEngine::last_report() const {
    return report_;
}

void RecomputeEngine::mark_downstream(const PropertyKey &source) {
    for (const auto &key : graph_.affected({source})) {
        mark_dirty(key);
    }
}

void RecomputeEngine::mark_dirty(const PropertyKey &target) {
    if (bindings_.contains(target)) {
        dirty_.insert(target);
        statuses_[target] = {ComputeState::Dirty, "awaiting recomputation"};
    }
}

void RecomputeEngine::collect_changes(const ChangeEvent &event) {
    if (recomputing_) {
        return; // 当前轮的下游已安排，不因公式写回再次入队。
    }
    for (const auto &change : event.changes) {
        const PropertyKey key{event.source_id, change.name};
        statuses_[key] = {};
        mark_downstream(key);
    }
}

void RecomputeEngine::flush_if_ready() {
    if (batch_depth_ != 0 || busy()) {
        return;
    }
    report_.clear();
    const auto order = graph_.topological_order(dirty_);
    if (!order) {
        throw std::logic_error("committed graph is cyclic");
    }
    FlagGuard guard{recomputing_};
    for (const auto &target : *order) {
        const auto found = bindings_.find(target);
        if (found == bindings_.end()) {
            dirty_.erase(target);
            continue;
        }
        PropertyStatus outcome;
        // 直接缺失优先报告为引用失效；其他失败前置使目标阻塞，不执行 AST。
        for (const auto &parent : graph_.dependencies(target)) {
            if (!stored_value(parent)) {
                outcome = {ComputeState::MissingReference,
                           "missing reference: " + key_text(parent)};
                break;
            }
        }
        if (outcome.state == ComputeState::Valid) {
            for (const auto &parent : graph_.dependencies(target)) {
                if (status(parent).state != ComputeState::Valid) {
                    outcome = {ComputeState::Blocked, "blocked by: " + key_text(parent)};
                    break;
                }
            }
        }
        std::size_t executions = 0;
        if (outcome.state == ComputeState::Valid) {
            executions = 1;
            const auto result = evaluate(*found->second.tree, [&](std::string_view name) {
                return value(resolve_key(target.object, name, found->second.aliases));
            });
            if (!result) {
                outcome = {ComputeState::Failed, expression_error(result.error())};
            } else {
                const auto assigned = find_object(target.object)->set(target.name, *result);
                if (assigned == SetResult::Changed || assigned == SetResult::Unchanged) {
                    outcome.reason = std::string{result_name(assigned)};
                } else {
                    outcome = {ComputeState::Failed,
                               "write rejected: " + std::string{result_name(assigned)}};
                }
            }
        }
        statuses_[target] = outcome;
        dirty_.erase(target);
        report_.push_back({target, executions, outcome.state, std::move(outcome.reason)});
        // 不因失败结束循环：有依赖的下游被阻塞，无关分支继续。
    }
}
} // namespace month05::week04
