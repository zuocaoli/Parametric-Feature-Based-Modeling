export module course_scenarios;

import std;

export namespace cad {

// 一个可运行的课程用例，把用例名称和执行函数绑在一起
struct Scenario {

    // 用例名
    std::string_view name;

    // 函数指针：
    // 它指向一个接收 std::ostream&、返回 void 的函数。
    // 调用 scenario.run(std::cout) 就会执行该用例，并把输出写到终端。
    void (*run)(std::ostream &);
};

std::span<const Scenario> scenarios();

//
// std::span 是 C++20 引入的一个非拥有的连续序列视图。
// 可以把它理解为针对任意连续内存（如数组、std::vector）的“轻量级包装器”，
// 它既提供了类似容器的安全访问接口（如 .size()、迭代器、operator[]），又不会拷贝或管理底层数据。
//
// 非拥有（Non-owning）：它只引用数据，不负责生命周期。
// 底层数据销毁后，span 即失效，继续使用是未定义行为。
//
// 连续内存：只能用于 std::vector、std::array、C 风格数组等内存连续的场景。
//
// 低开销：通常只包含一个指针和一个大小（动态长度时），拷贝成本极低，且是平凡可拷贝类型。
}
