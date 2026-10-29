export module month03.week01.dependencies;

import std;
export import month03.week01.parser;

export namespace month03::week01 {

//
// 表示 “某个对象的某个属性”的唯一标识，是对属性身份的抽象，在依赖图中充当节点标识.
//
// PropertyKey{1, "width"}  // A 的宽度
// PropertyKey{2, "width"}  // B 的宽度
// PropertyKey{1, "length"} // A 的长度
//
struct PropertyKey {
    std::uint64_t object; // 属性所属对象的 ID
    std::string name;     // 属性名称

    // 让编译器自动生成 PropertyKey 的比较规则，
    // 可以使用 ==、 !=、 <、 <=、 >、 >= 运算符。
    auto operator<=>(const PropertyKey &) const = default;

    //
    // - operator<=>：C++20 引入的三路比较运算符，判断小于、等于或大于。
    // - const PropertyKey &：接收另一个 PropertyKey，不复制、不修改它；默认实现不需要参数名。
    // - 后面的 const：比较时不修改当前对象。
    // - = default：由编译器按成员声明顺序逐个比较。
    // - auto：自动推导比较结果类型；这里是 std::strong_ordering。
    //

    //
    // 因为成员声明顺序是：
    // std::uint64_t object;
    // std::string name;
    // 所以规则是：先比较 object，相同时再比较 name。
    //

    //
    // 根据 operator<=> 返回值来判断大小
    //    result < 0	a 小于 b
    //    result == 0	a 等于 b
    //    result > 0	a 大于 b
    //
    // 也就是日常使用的比较运算符内部会调用 operator<=> 来判断大小，但不是所有比较运算符都会调用 operator<=>，
    //
    // 对于自定义类型：
    //    - <、>、<=、>=：如果没有更合适的比较运算符，可以通过 operator<=> 实现。
    //    - ==、!=：依赖 operator==，不会直接调用 operator<=>
};

// 存放属性标识的集合，特点是自动排序、不允许重复
using KeySet = std::set<PropertyKey>;

//  <表达式中的变量名, 属性所属对象的 ID 和属性名称>
using VariableMap = std::map<std::string, PropertyKey, std::less<>>;

std::string key_text(const PropertyKey &key);
std::set<std::string> collect_variables(const Node &node);
PropertyKey resolve_key(std::uint64_t owner, std::string_view variable,
                        const VariableMap &aliases = {});
KeySet resolve_dependencies(const Node &node, std::uint64_t owner, const VariableMap &aliases = {});

//
// 记录属性之间“谁依赖谁”，并在公式变化时更新这些关系。
//
class DependencyGraph {
public:
    void replace_dependencies(const PropertyKey &target, const KeySet &dependencies);

    void remove_formula(const PropertyKey &target);

    const KeySet &dependencies(const PropertyKey &target) const;

    const KeySet &dependents(const PropertyKey &source) const;

    KeySet nodes() const;

private:
    //
    // 这两个 map 保存的是同一组依赖关系的两个查询方向
    //

    // <哪个属性, 它依赖哪些属性>， 这个 PropertyKey 依赖谁
    std::map<PropertyKey, KeySet> parents_;

    // <哪个属性, 依赖这个属性的有哪些>， 谁依赖这个 PropertyKey
    std::map<PropertyKey, KeySet> children_;
};
} // namespace month03::week01
