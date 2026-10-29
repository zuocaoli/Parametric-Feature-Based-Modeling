module week03.property;

import std;

namespace week03 {
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

// 注册一个接收 ChangeEvent 的回调，返回管理该订阅的 Connection；此处尚不执行回调。
ChangeSignal::Connection ChangeSignal::connect(std::function<void(const ChangeEvent &)> callback) {
    // std::function 可以为空；没有可调用的函数时拒绝订阅。
    if (!callback) {
        throw std::invalid_argument("empty callback");
    }

    // 清理此前已退订的记录，保留仍然有效的订阅。
    // std::erase_if 是 C++20 引入的，用来删除容器中所有满足指定条件的元素。
    // 调试表达式返回 true 就删除这个元素。
    std::erase_if(slots_, [](const auto &slot) { return !slot->active; });

    // 创建一个订阅并设置订阅状态： active 为 true，并把回调移动到 Slot 中保存。
    auto slot = std::make_shared<Slot>(true, std::move(callback));

    // 通知管理器持有该状态，后续 emit() 会调用其中的回调。
    slots_.push_back(slot);

    // 返回弱引用同一 Slot 的退订凭证，调用方须保存它以维持订阅。
    return Connection{slot};
}

std::size_t ChangeSignal::emit(const ChangeEvent &event) {

    // 快照稳定迭代：回调中新订阅者从下次生效，已断开的订阅立即跳过。
    const auto snapshot = slots_;
    std::size_t errors = 0;

    //
    for (const auto &slot : snapshot) {
        if (!slot->active) {
            // 该订阅无效，可能是已退订或被销毁；跳过它。
            continue;
        }

        try {
            // 调用回调，传入事件；回调可能抛异常，不能阻断其他订阅者。
            slot->callback(event);
        } catch (...) {
            // 一个观察者失败不能阻止其他观察者，也不回滚已提交的数据。
            ++errors;
        }
    }

    //
    std::erase_if(slots_, [](const auto &slot) { return !slot->active; });
    return errors;
}
} // namespace week03
