export module week02.property;

import std;

export namespace week02 {

//
// `Value` 是一个**类型别名**，表示“属性值类型”。
// 一个 `Value` 对象可以保存 `double`、`std::int64_t`、`bool`、`std::string` 中的一种值。
// 一次只保存一种类型的值。
//
// std::variant 是 C++17 引入的类型安全联合体：
// 一个变量可以保存多种指定类型中的一种，但同一时刻只保存其中一种。
using Value = std::variant<double, std::int64_t, bool, std::string>;

// set() 的修改结果；只有 Changed 会改变属性值，其余结果均保持原状态。
enum class SetResult {
    Changed,      // 新值合法且与旧值不同，已完成修改。
    Unchanged,    // 新值合法但与旧值相同，无需修改。
    NotFound,     // 指定名称的属性不存在。
    TypeMismatch, // 新值类型与属性类型不一致，例如给 double 属性传入整数。
    InvalidValue, // 类型正确，但取值未通过校验，例如尺寸小于或等于零。
    ReadOnly      // 属性只读，不允许通过 set() 修改。
};

std::string_view result_name(SetResult result);
std::string_view type_name(const Value &value);
std::string value_text(const Value &value);

// 用于枚举和展示的属性快照；修改快照不会改变原对象，修改属性须调用 set()。
struct PropertyView {
    std::string name;  // 稳定名称，用于 get()/set() 查找，例如 "length"。
    std::string label; // 展示给用户的标签，例如 "Length"。
    std::string unit;  // 单位，例如 "mm"；空字符串表示无单位。
    bool read_only;    // 原属性是否只读；为 true 时 set() 拒绝修改。
    Value value;       // 获取快照时的值副本；派生属性保存当时计算出的结果。
};

class PropertyObject {
public:
    PropertyObject() = default;
    virtual ~PropertyObject() = default;
    // 派生属性的读取函数捕获 this，因此对象不可复制或移动。
    PropertyObject(const PropertyObject &) = delete;
    PropertyObject &operator=(const PropertyObject &) = delete;

    // 获取属性快照
    std::vector<PropertyView> properties() const;

    std::optional<Value> get(std::string_view name) const;
    SetResult set(std::string_view name, const Value &value);

protected:
    using Validator = std::function<bool(const Value &)>;
    using Reader = std::function<Value()>;

    void add(std::string name, std::string label, std::string unit, Value value,
             Validator validate = {}, Reader compute = {});

private:
    // 对象内部保存的属性，包含元数据、存储值及校验和计算规则。
    // 一个 Property 对象就是一个属性
    struct Property {
        std::string name;   // 稳定名称，用于查找属性，例如 "length"。
        std::string label;  // 展示给用户的标签，例如 "Length"。
        std::string unit;   // 单位，例如 "mm"；空字符串表示无单位。
        Value value;        // 普通属性的存储值；派生属性读取时改用 compute() 的结果。
        bool read_only;     // 是否只读；为 true 时 set() 拒绝修改。
        Validator validate; // 可选的取值校验函数，返回 true 表示合法；为空则不做取值校验。
        Reader compute;     // 可选的派生值计算函数，每次读取时调用；设置后属性为只读。
    };

    static Value read(const Property &property);

    // =================================
    std::vector<Property> properties_;
};
} // namespace week02
