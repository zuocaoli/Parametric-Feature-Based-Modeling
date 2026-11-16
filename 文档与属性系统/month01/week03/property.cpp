module week03.property;

import std;

namespace week03 {
std::string_view result_name(SetResult result) {
    switch (result) {
    case SetResult::Changed:
        return "Changed";
    case SetResult::Unchanged:
        return "Unchanged";
    case SetResult::NotFound:
        return "NotFound";
    case SetResult::TypeMismatch:
        return "TypeMismatch";
    case SetResult::InvalidValue:
        return "InvalidValue";
    case SetResult::ReadOnly:
        return "ReadOnly";
    case SetResult::Reentrant:
        return "Reentrant";
    }
    return "Unknown";
}

std::string_view type_name(const Value &value) {
    static constexpr std::string_view names[]{"double", "int64", "bool", "string"};
    return names[value.index()];
}

std::string value_text(const Value &value) {
    return std::visit(
        [](const auto &item) {
            std::ostringstream stream;
            stream << std::boolalpha << item;
            return stream.str();
        },
        value);
}

std::vector<PropertyView> PropertyObject::properties() const {
    std::vector<PropertyView> result;
    for (const auto &property : properties_) {
        result.push_back(
            {property.name, property.label, property.unit, property.read_only, read(property)});
    }
    return result;
}

std::optional<Value> PropertyObject::get(std::string_view name) const {
    for (const auto &property : properties_) {
        if (property.name == name) {
            return read(property);
        }
    }
    return std::nullopt;
}

ChangeSignal::Connection
PropertyObject::observe(std::function<void(const ChangeEvent &)> callback) {
    return changed_.connect(std::move(callback));
}

std::size_t PropertyObject::notification_errors() const {
    return notification_errors_;
}

SetResult PropertyObject::set(std::string_view name, const Value &value) {
    return set_many({{std::string{name}, value}});
}

SetResult PropertyObject::set_many(const std::vector<Update> &updates) {
    // 回调不能在本对象发送通知期间再次发起修改，避免递归通知。
    if (notifying_) {
        return SetResult::Reentrant;
    }

    // 第一阶段只收集待修改项和事件内容；此时不写入任何属性。
    struct Pending {
        Property *property;
        Value next;
    };

    std::vector<Pending> pending;
    ChangeEvent event{this, {}};
    std::set<std::string> names;

    for (const auto &update : updates) {
        // 同一批中每个属性只能出现一次；任何错误都在提交前返回。
        if (!names.insert(update.name).second) {
            return SetResult::InvalidValue; // 同一批中不接受重复名称。
        }

        // 读取当前属性
        auto found = std::find_if(properties_.begin(), properties_.end(),
                                  [&](const auto &item) { return item.name == update.name; });

        if (found == properties_.end()) {
            return SetResult::NotFound;
        }

        if (found->read_only) {
            return SetResult::ReadOnly;
        }

        // std::variant 提供成员函数 index()，从 C++17 开始支持.
        // index() 返回当前存储类型在模板参数列表中的索引,
        // 返回的是类型索引，不是数据索引.
        if (found->value.index() != update.value.index()) {
            return SetResult::TypeMismatch; // 类型不对
        }

        // 属性有校验函数，而且新值没有通过校验，就返回“取值非法”
        if (found->validate && !found->validate(update.value)) {
            return SetResult::InvalidValue;
        }

        // 相同值无需写入，也不会出现在变化通知中。
        if (found->value != update.value) {
            pending.push_back({&*found, update.value});
            event.changes.push_back({found->name, found->value, update.value});
        }
    }

    if (pending.empty()) {
        return SetResult::Unchanged;
    }

    // static_assert(...)：要求条件为 true，否则编译失败；运行时不执行检查
    // std::is_nothrow_swappable_v 用于验证该类型的两个对象是否可以使用 swap()
    static_assert(std::is_nothrow_swappable_v<Value>);

    // 批量修改数值
    for (auto &item : pending) {
        // 更改数值
        item.property->value.swap(item.next);
    }

    // RAII
    struct NotifyGuard {
        bool &flag;

        // 初始化列表确定 flag 引用哪个变量.
        // 构造函数中将 flag 引用的变量设置为 true，表示正在通知中。
        explicit NotifyGuard(bool &value) : flag(value) {
            flag = true;
        }

        ~NotifyGuard() {
            flag = false;
        }

    } guard{notifying_}; // 创建一个 NotifyGuard 对象，传入 notifying_ 的引用

    // 触发修改事件
    notification_errors_ += changed_.emit(event);
    return SetResult::Changed;
}

void PropertyObject::add(std::string name, std::string label, std::string unit, Value value,
                         Validator validate, Reader compute) {
    if (get(name)) {
        throw std::invalid_argument("duplicate property name");
    }
    if (validate && !validate(value)) {
        throw std::invalid_argument("invalid initial property value");
    }
    const bool read_only = static_cast<bool>(compute);
    properties_.push_back({std::move(name), std::move(label), std::move(unit), std::move(value),
                           read_only, std::move(validate), std::move(compute)});
}

Value PropertyObject::read(const Property &property) {
    return property.compute ? property.compute() : property.value;
}
} // namespace week03
