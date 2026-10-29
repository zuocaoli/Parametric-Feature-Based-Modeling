export module month02.week04.formulas;

import std;
export import month02.week04.evaluator;

export namespace month02::week04 {

//
// 属性公式计算引擎，用于管理和执行属性上的公式
//
// 它负责三个步骤：
// 1. 绑定公式：记录哪个属性使用哪个公式，并解析出语法树。
// 2. 计算公式：读取当前属性值，例如 length = 100，计算得到 width = 50。
// 3. 写回结果：通过属性对象的 set() 保存结果，由属性系统校验并发送变化通知。
//
class FormulaEngine {
public:
    // 对象须比引擎存活更久；单线程，公式计算由调用者显式触发。
    explicit FormulaEngine(PropertyObject &object);

    // 删除拷贝构造函数，不能用已有引擎创建另一个引擎
    FormulaEngine(const FormulaEngine &) = delete;
    // 删除拷贝赋值运算符，不能把一个引擎赋值给另一个已有引擎
    FormulaEngine &operator=(const FormulaEngine &) = delete;

    std::expected<void, Error> bind(std::string_view target, std::string_view source);

    bool unbind(std::string_view target);

    std::optional<std::string> formula(std::string_view target) const;

    std::vector<std::pair<std::string, std::string>> formulas() const;

    std::expected<SetResult, Error> compute(std::string_view target);

private:
    struct Binding {
        std::string source; // 原表达式
        Tree tree;          // 表达式对应的语法树
    };

    PropertyObject &object_;

    //
    // <Key, Value, 比较器, 内存分配器>
    std::map<std::string, Binding, std::less<>> bindings_;
    // std::less<> 就是 std::less<void>。
    // std::less<> 本质上是一个泛化的“小于”函数对象，你几乎可以在任何需要比较器的地方用它替代 std::less<T>，
    // 尤其在泛型编程和需要异构查找的关联容器场景下，它是更现代、更高效的选择。
    //
};
} // namespace month02::week04
