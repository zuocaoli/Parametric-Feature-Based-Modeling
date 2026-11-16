module month05.week04.property;

import std;

namespace month05::week04 {
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
            if constexpr (std::is_same_v<std::decay_t<decltype(item)>, double>) {
                return std::format("{}", item); // 最短往返文本，不让“应用”隐式舍入。
            } else {
                std::ostringstream stream;
                stream << std::boolalpha << item;
                return stream.str();
            }
        },
        value);
}

PropertyObject::PropertyObject(ObjectId id) : id_(id) {}

ObjectId PropertyObject::id() const {
    return id_;
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
    if (notifying_) {
        return SetResult::Reentrant;
    }
    struct Pending {
        Property *property;
        Value next;
    };
    std::vector<Pending> pending;
    ChangeEvent event{this, id_, {}};
    std::set<std::string> names;
    for (const auto &update : updates) {
        if (!names.insert(update.name).second) {
            return SetResult::InvalidValue; // 同一批中不接受重复名称。
        }
        auto found = std::find_if(properties_.begin(), properties_.end(),
                                  [&](const auto &item) { return item.name == update.name; });
        if (found == properties_.end()) {
            return SetResult::NotFound;
        }
        if (found->read_only) {
            return SetResult::ReadOnly;
        }
        if (found->value.index() != update.value.index()) {
            return SetResult::TypeMismatch;
        }
        if (found->validate && !found->validate(update.value)) {
            return SetResult::InvalidValue;
        }
        if (found->value != update.value) {
            pending.push_back({&*found, update.value});
            event.changes.push_back({found->name, found->value, update.value});
        }
    }
    if (pending.empty()) {
        return SetResult::Unchanged;
    }
    // 校验、复制、内存分配均在提交前完成；这四种值的 swap 不抛异常。
    static_assert(std::is_nothrow_swappable_v<Value>);
    for (auto &item : pending) {
        item.property->value.swap(item.next);
    }
    struct NotifyGuard {
        bool &flag;
        explicit NotifyGuard(bool &value) : flag(value) {
            flag = true;
        }
        ~NotifyGuard() {
            flag = false;
        }
    } guard{notifying_};
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
} // namespace month05::week04
