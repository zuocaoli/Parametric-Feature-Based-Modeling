module month02.week04.formulas;

import std;

namespace month02::week04 {
FormulaEngine::FormulaEngine(PropertyObject &object) : object_(object) {}

// 检查目标属性，将公式解析为语法树，再保存两者的绑定关系。它不会计算公式
// target: 属性名称
// source: 属性关联的表达式
std::expected<void, Error> FormulaEngine::bind(std::string_view target, std::string_view source) {

    //
    // 1. 属性检查
    //

    // 获取属性快照
    const auto properties = object_.properties();

    // 查找这个属性
    const auto found = std::find_if(properties.begin(), properties.end(),
                                    [&](const auto &item) { return item.name == target; });
    if (found == properties.end()) {
        return std::unexpected(Error{ErrorStage::Binding, 0, "target property does not exist"});
    }
    if (found->read_only) {
        return std::unexpected(Error{ErrorStage::Binding, 0, "target property is read-only"});
    }

    // 目标属性当前保存的值是不是 double，如果不是，就拒绝绑定公式，这是仅仅是本周课程的逻辑。
    // 检查 found->value 这个 std::variant 对象当前是否持有 double 类型，
    // found->value 是 std::variant 类型的。
    if (!std::holds_alternative<double>(found->value)) {
        return std::unexpected(Error{ErrorStage::Binding, 0, "target must be a double property"});
    }

    //
    // 2. 解析语法树
    //

    // 解析获取语法树
    auto tree = parse(source);
    if (!tree) {
        return std::unexpected(tree.error());
    }

    //
    // 3. 绑定
    //

    // 表达式，语法树
    Binding binding{std::string{source}, std::move(*tree)};
    // insert_or_assign 存在则赋值，不存在则插入
    bindings_.insert_or_assign(std::string{target}, std::move(binding));
    return {};
}

bool FormulaEngine::unbind(std::string_view target) {
    const auto found = bindings_.find(target);
    if (found == bindings_.end()) {
        return false;
    }
    bindings_.erase(found); // 解除后保留最近一次成功写回的数值。
    return true;
}

std::optional<std::string> FormulaEngine::formula(std::string_view target) const {
    const auto found = bindings_.find(target);
    return found == bindings_.end() ? std::nullopt : std::optional{found->second.source};
}

//
// 公式批量导出接口——把当前所有绑定关系以 (目标属性, 表达式文本) 的形式拷出来。
//
//
std::vector<std::pair<std::string, std::string>> FormulaEngine::formulas() const {
    std::vector<std::pair<std::string, std::string>> result;

    // [属性名称，表达式绑定]
    for (const auto &[target, binding] : bindings_) {
        // 属性名称，原表达式
        result.emplace_back(target, binding.source);
    }
    return result;
}

//
// 计算并且更新属性值
//
std::expected<SetResult, Error> FormulaEngine::compute(std::string_view target) {

    // 根据属性名称查找绑定关系
    const auto found = bindings_.find(target);
    if (found == bindings_.end()) {
        return std::unexpected(Error{ErrorStage::Binding, 0, "target has no formula"});
    }

    // 计算属性值
    const auto value =
        evaluate(*found->second.tree, [&](std::string_view name) { return object_.get(name); });
    if (!value) {
        return std::unexpected(value.error());
    }

    const std::string name{target};
    // 更新目标属性值
    const auto result = object_.set(name, *value);

    if (result != SetResult::Changed && result != SetResult::Unchanged) {
        // 如果写入结果既不是“已修改”，也不是“值未变”，就返回写入失败的错误
        return std::unexpected(
            Error{ErrorStage::Binding, 0, "write rejected: " + std::string{result_name(result)}});
    }
    return result;
}
} // namespace month02::week04
