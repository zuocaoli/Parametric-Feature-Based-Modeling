module week02.property;

import std;

namespace week02 {
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

SetResult PropertyObject::set(std::string_view name, const Value &value) {
    for (auto &property : properties_) {
        if (property.name != name) {
            continue;
        }
        if (property.read_only) {
            return SetResult::ReadOnly;
        }
        if (property.value.index() != value.index()) {
            return SetResult::TypeMismatch;
        }
        if (property.validate && !property.validate(value)) {
            return SetResult::InvalidValue;
        }
        if (property.value == value) {
            return SetResult::Unchanged;
        }
        property.value = value;
        return SetResult::Changed;
    }
    return SetResult::NotFound;
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
} // namespace week02
