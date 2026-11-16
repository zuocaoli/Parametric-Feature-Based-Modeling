module week03.demo;

import std;
import week03.rectangle;

namespace week03 {
void demo() {
    Rectangle rectangle;

    auto log = rectangle.observe( //

        // 属性发生变化的时候调用
        [](const ChangeEvent &event) {
            std::cout << "changed:";
            for (const auto &change : event.changes) {
                std::cout << ' ' << change.name << '=' << value_text(change.before) << "->"
                          << value_text(change.after);
            }
            std::cout << '\n';
        } //
    );

    {
        auto view = rectangle.observe( //
            [&](const ChangeEvent &) {
                std::cout << "view area=" << value_text(*rectangle.get("area")) << '\n';
            } //
        );

        std::cout << " 01 " << std::endl;
        rectangle.set("length", 10.0);
        std::cout << " 02 " << std::endl;

        std::cout << " 03 " << std::endl;
        rectangle.set("length", 10.0); // 相同值不通知。
        std::cout << " 04 " << std::endl;

        std::cout << " 05 " << std::endl;
        rectangle.set("width", -1.0); // 非法值不通知。
        std::cout << " 06 " << std::endl;

        std::cout << " 07 " << std::endl;
        rectangle.set_many({{"length", 20.0}, {"width", 4.0}});
        std::cout << " 08 " << std::endl;

    } // 结束内部作用域，自动销毁 view，其析构函数调用 disconnect() 退订。
      // 外层的 log 仍然存在，所以随后修改宽度时只通知 log

    std::cout << " 09 " << std::endl;
    rectangle.set("width", 5.0);
    std::cout << " 10 " << std::endl;
    std::cout << "final area=" << value_text(*rectangle.get("area")) << '\n';
}
} // namespace week03
