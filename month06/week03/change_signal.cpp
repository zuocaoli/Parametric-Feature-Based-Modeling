module month06.week03.property;

import std;

namespace month06::week03 {
ChangeSignal::Connection::Connection(Connection &&other) noexcept : slot_(std::move(other.slot_)) {}

ChangeSignal::Connection &ChangeSignal::Connection::operator=(Connection &&other) noexcept {
    if (this != &other) {
        disconnect();
        slot_ = std::move(other.slot_);
    }
    return *this;
}

ChangeSignal::Connection::~Connection() {
    disconnect();
}

void ChangeSignal::Connection::disconnect() noexcept {
    if (auto slot = slot_.lock()) {
        slot->active = false;
    }
    slot_.reset();
}

ChangeSignal::Connection::Connection(const std::shared_ptr<Slot> &slot) : slot_(slot) {}

ChangeSignal::Connection ChangeSignal::connect(std::function<void(const ChangeEvent &)> callback) {
    if (!callback) {
        throw std::invalid_argument("empty callback");
    }
    std::erase_if(slots_, [](const auto &slot) { return !slot->active; });
    auto slot = std::make_shared<Slot>(true, std::move(callback));
    slots_.push_back(slot);
    return Connection{slot};
}

std::size_t ChangeSignal::emit(const ChangeEvent &event) {
    // 快照稳定迭代：回调中新订阅者从下次生效，已断开的订阅立即跳过。
    const auto snapshot = slots_;
    std::size_t errors = 0;
    for (const auto &slot : snapshot) {
        if (!slot->active) {
            continue;
        }
        try {
            slot->callback(event);
        } catch (...) {
            ++errors; // 一个观察者失败不能阻止其他观察者，也不回滚已提交的数据。
        }
    }
    std::erase_if(slots_, [](const auto &slot) { return !slot->active; });
    return errors;
}
} // namespace month06::week03
