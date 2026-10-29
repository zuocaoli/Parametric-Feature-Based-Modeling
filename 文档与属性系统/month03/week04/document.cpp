module month05.week04.document;

import std;

namespace month05::week04 {
namespace {
struct BusyGuard {
    bool &flag;
    explicit BusyGuard(bool &value) : flag(value) {
        flag = true;
    }
    ~BusyGuard() {
        flag = false;
    }
};
EngineError error(std::string message) {
    return {std::move(message), {}};
}
} // namespace

std::expected<std::unique_ptr<RecomputeEngine>, EngineError> Command::undo(ObjectId floor) const {
    if (!before_) {
        return std::unexpected(error("command has not been committed"));
    }
    return RecomputeEngine::restore(*before_, true, floor);
}
std::expected<std::unique_ptr<RecomputeEngine>, EngineError> Command::redo(ObjectId floor) const {
    if (!after_) {
        return std::unexpected(error("command has not been committed"));
    }
    return RecomputeEngine::restore(*after_, true, floor);
}

SetProperty::SetProperty(PropertyKey key, Value value)
    : key_(std::move(key)), value_(std::move(value)) {}
std::expected<bool, EngineError> SetProperty::execute(RecomputeEngine &model) const {
    const auto result = model.set(key_, value_);
    if (result != SetResult::Changed && result != SetResult::Unchanged) {
        return std::unexpected(
            error("set " + key_text(key_) + ": " + std::string{result_name(result)}));
    }
    return result == SetResult::Changed;
}

BindFormula::BindFormula(PropertyKey target, std::string source, VariableMap aliases)
    : target_(std::move(target)), source_(std::move(source)), aliases_(std::move(aliases)) {}
std::expected<bool, EngineError> BindFormula::execute(RecomputeEngine &model) const {
    for (const auto &binding : model.snapshot().formulas) {
        if (binding.target == target_ && binding.source == source_ && binding.aliases == aliases_) {
            return false;
        }
    }
    auto result = model.bind(target_, source_, aliases_);
    if (!result) {
        return std::unexpected(result.error());
    }
    return true;
}
UnbindFormula::UnbindFormula(PropertyKey target) : target_(std::move(target)) {}
std::expected<bool, EngineError> UnbindFormula::execute(RecomputeEngine &model) const {
    return model.unbind(target_);
}
CreateRectangle::CreateRectangle(double length, double width) : length_(length), width_(width) {}
std::expected<bool, EngineError> CreateRectangle::execute(RecomputeEngine &model) const {
    if (!std::isfinite(length_) || !std::isfinite(width_) || length_ <= 0 || width_ <= 0) {
        return std::unexpected(error("rectangle dimensions must be finite and positive"));
    }
    const auto id = model.create_rectangle(length_, width_);
    auto result = model.bind({id, "area"}, "length * width");
    if (!result) {
        return std::unexpected(result.error());
    }
    return true;
}
DeleteRectangle::DeleteRectangle(ObjectId id) : id_(id) {}
std::expected<bool, EngineError> DeleteRectangle::execute(RecomputeEngine &model) const {
    return model.erase(id_);
}

CompositeCommand::CompositeCommand(std::vector<std::unique_ptr<Command>> commands)
    : commands_(std::move(commands)) {}
std::expected<bool, EngineError> CompositeCommand::execute(RecomputeEngine &model) const {
    // 先拒绝空子命令/嵌套，避免悄悄扩大本课事务约定。
    for (const auto &command : commands_) {
        if (!command || dynamic_cast<const CompositeCommand *>(command.get())) {
            return std::unexpected(error("null or nested composite command"));
        }
    }
    bool changed = false;
    for (std::size_t index = 0; index < commands_.size(); ++index) {
        const auto prefix = "command " + std::to_string(index + 1) + ": ";
        try {
            auto result = commands_[index]->execute(model);
            if (!result) {
                auto failure = result.error();
                failure.message = prefix + failure.message;
                return std::unexpected(std::move(failure));
            }
            changed = changed || *result;
        } catch (const std::exception &failure) {
            return std::unexpected(error(prefix + "exception: " + failure.what()));
        } catch (...) {
            return std::unexpected(error(prefix + "non-standard exception"));
        }
    }
    return changed;
}

DocumentSignal::Connection::Connection(const std::shared_ptr<Slot> &slot) : slot_(slot) {}
DocumentSignal::Connection::Connection(Connection &&other) noexcept
    : slot_(std::move(other.slot_)) {
    other.slot_.reset();
}
DocumentSignal::Connection &DocumentSignal::Connection::operator=(Connection &&other) noexcept {
    if (this != &other) {
        disconnect();
        slot_ = std::move(other.slot_);
        other.slot_.reset();
    }
    return *this;
}
DocumentSignal::Connection::~Connection() {
    disconnect();
}
void DocumentSignal::Connection::disconnect() noexcept {
    if (auto slot = slot_.lock()) {
        slot->active = false;
    }
    slot_.reset();
}
DocumentSignal::Connection
DocumentSignal::connect(std::function<void(const DocumentEvent &)> callback) {
    auto slot = std::make_shared<Slot>();
    slot->callback = std::move(callback);
    slots_.push_back(slot);
    return Connection{slot};
}
void DocumentSignal::emit(const DocumentEvent &event) {
    // 回调可断开连接；本轮不访问可能因新连接而重分配的 slots_。
    try {
        const auto slots = slots_;
        for (const auto &slot : slots) {
            if (slot->active) {
                try {
                    slot->callback(event);
                } catch (...) { /* 已提交，隔离观察者异常。 */
                }
            }
        }
        std::erase_if(slots_, [](const auto &slot) { return !slot->active; });
    } catch (...) { /* 分配失败也不能将已提交操作报告为回滚。 */
    }
}

Document::Document() : model_(std::make_unique<RecomputeEngine>()) {
    const auto id = model_->create_rectangle();
    if (!model_->begin_batch() || !model_->bind({id, "width"}, "length / 2") ||
        !model_->bind({id, "area"}, "length * width") || !model_->end_batch()) {
        throw std::logic_error("initial document setup failed");
    }
}
const RecomputeEngine &Document::model() const {
    return *model_;
}
std::size_t Document::history_size() const {
    return history_.size();
}
std::size_t Document::position() const {
    return position_;
}
DocumentSignal::Connection Document::observe(std::function<void(const DocumentEvent &)> callback) {
    return changed_.connect(std::move(callback));
}

std::expected<bool, EngineError> Document::execute(std::unique_ptr<Command> command) {
    if (busy_ || !command) {
        return std::unexpected(error("busy document or null command"));
    }
    BusyGuard guard{busy_};
    try {
        auto before = model_->snapshot();
        auto candidate = RecomputeEngine::restore(before, false);
        if (!candidate) {
            return std::unexpected(candidate.error());
        }
        auto started = (*candidate)->begin_batch();
        if (!started) {
            return std::unexpected(started.error());
        }
        auto result = command->execute(**candidate);
        if (!result) {
            return std::unexpected(result.error());
        }
        if (!*result) {
            return false;
        }
        auto ended = (*candidate)->end_batch();
        if (!ended) {
            return std::unexpected(ended.error());
        }
        auto after = (*candidate)->snapshot();
        // 抵消的组合操作也不切断重做分支；分配高水位在候选模型里尚未公开。
        if (before.objects == after.objects && before.formulas == after.formulas) {
            return false;
        }
        DocumentEvent event{before, after, (*candidate)->last_report(), false};
        command->before_ = std::move(before);
        command->after_ = std::move(after);
        history_.reserve(history_.size() + 1); // 提交前完成可能失败的分配。
        model_.swap(*candidate);
        history_.erase(history_.begin() + static_cast<std::ptrdiff_t>(position_), history_.end());
        history_.push_back(std::move(command));
        ++position_;
        changed_.emit(event);
        return true;
    } catch (const std::exception &failure) {
        return std::unexpected(error(failure.what()));
    } catch (...) {
        return std::unexpected(error("command threw a non-standard exception"));
    }
}
std::expected<bool, EngineError> Document::undo() {
    return move_history(false);
}
std::expected<bool, EngineError> Document::redo() {
    return move_history(true);
}
std::expected<bool, EngineError> Document::move_history(bool forward) {
    if (busy_) {
        return std::unexpected(error("busy document"));
    }
    if ((!forward && position_ == 0) || (forward && position_ == history_.size())) {
        return false;
    }
    BusyGuard guard{busy_};
    try {
        const auto &command = history_[forward ? position_ : position_ - 1];
        auto candidate =
            forward ? command->redo(model_->high_water()) : command->undo(model_->high_water());
        if (!candidate) {
            return std::unexpected(candidate.error());
        }
        DocumentEvent event{model_->snapshot(), (*candidate)->snapshot(),
                            (*candidate)->last_report(), false};
        model_.swap(*candidate);
        if (forward) {
            ++position_;
        } else {
            --position_;
        }
        changed_.emit(event);
        return true;
    } catch (const std::exception &failure) {
        return std::unexpected(error(failure.what()));
    }
}
std::expected<void, EngineError>
Document::replace_model(std::unique_ptr<RecomputeEngine> candidate) {
    if (busy_ || !candidate) {
        return std::unexpected(error("busy document or null model"));
    }
    BusyGuard guard{busy_};
    try {
        DocumentEvent event{model_->snapshot(), candidate->snapshot(), candidate->last_report(),
                            true};
        model_.swap(candidate);
        history_.clear();
        position_ = 0;
        changed_.emit(event);
        return {};
    } catch (const std::exception &failure) {
        return std::unexpected(error(failure.what()));
    }
}
} // namespace month05::week04
