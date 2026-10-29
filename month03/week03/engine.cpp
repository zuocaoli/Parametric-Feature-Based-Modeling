module month03.week03.engine;

import std;

namespace month03::week03 {
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

ObjectId RecomputeEngine::create_rectangle(double length, double width) {
    if (busy()) {
        throw std::logic_error("cannot create an object during modification or recomputation");
    }
    if (next_id_ == 0) {
        throw std::overflow_error("object IDs exhausted");
    }
    const auto id = next_id_;
    auto object = std::make_shared<Rectangle>(length, width, id);
    auto connection = object->observe([this](const ChangeEvent &event) { collect_changes(event); });
    objects_.emplace(id, Managed{std::move(object), std::move(connection)});
    ++next_id_;
    return id;
}

std::optional<Value> RecomputeEngine::stored_value(const PropertyKey &key) const {
    const auto *object = find_object(key.object);
    return object ? object->get(key.name) : std::nullopt;
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
        return std::unexpected(engine_error(expression_error(tree.error())));
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
    dirty_.insert(target); // 常量公式也要计算，不能只从前置变化出发。
    mark_downstream(target);
    flush_if_ready();
    return {};
}

std::optional<std::string> RecomputeEngine::formula(const PropertyKey &target) const {
    const auto found = bindings_.find(target);
    return found == bindings_.end() ? std::nullopt : std::optional{found->second.source};
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
        if (bindings_.contains(key)) {
            dirty_.insert(key);
        }
    }
}

void RecomputeEngine::collect_changes(const ChangeEvent &event) {
    if (recomputing_) {
        return; // 当前轮的下游已安排，不因公式写回再次入队。
    }
    for (const auto &change : event.changes) {
        mark_downstream({event.source_id, change.name});
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
        const auto value = evaluate(*found->second.tree, [&](std::string_view name) {
            return stored_value(resolve_key(target.object, name, found->second.aliases));
        });
        if (!value) {
            report_.push_back({target, 1, false, expression_error(value.error())});
            break; // 本周先停止失败轮次；第四周加入状态、分支隔离及恢复。
        }
        const auto result = find_object(target.object)->set(target.name, *value);
        if (result != SetResult::Changed && result != SetResult::Unchanged) {
            report_.push_back(
                {target, 1, false, "write rejected: " + std::string{result_name(result)}});
            break;
        }
        dirty_.erase(target);
        report_.push_back({target, 1, true, std::string{result_name(result)}});
    }
}
} // namespace month03::week03
