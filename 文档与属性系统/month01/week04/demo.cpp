module week04.demo;

import std;
import week04.document;
import week04.printer;

namespace week04 {

void demo() {
    Document document;

    // 用默认长 5、宽 3 创建第一个矩形，保存文档分配的 ID。
    const auto first_id = document.create_rectangle();
    // 创建长 2.5、宽 4.0 的第二个矩形，保存其 ID。
    const auto second_id = document.create_rectangle(2.5, 4.0);
    // 创建包含指定文本的注释对象，保存其 ID。
    const auto note_id = document.create_annotation("Use properties, not concrete types");

    std::cout << "created ids=" << first_id << ',' << second_id << ',' << note_id << '\n';

    auto handle = document.find(first_id);
    int logged = 0;
    int refreshed = 0;
    ChangeSignal::Connection log;

    // lock() 返回一个 shared_ptr
    if (auto object = handle.lock()) {

        // 注册一个订阅
        log = object->observe([&](const ChangeEvent &event) {
            ++logged;
            std::cout << "[id=" << event.source_id << ']';
            for (const auto &change : event.changes) {
                std::cout << ' ' << change.name << '=' << value_text(change.before) << "->"
                          << value_text(change.after);
            }
        });

        {
            // 局部作用于的订阅，离开作用域后自动退订。
            auto view = object->observe([&](const ChangeEvent &event) {
                ++refreshed;
                std::cout << "view area=" << value_text(*event.source->get("area")) << '\n';
            });

            object->set("length", 10.0);
            const auto invalid = object->set("width", -1.0);
            const auto repeated = object->set("length", 10.0);
            std::cout << "invalid=" << result_name(invalid)
                      << ", repeated=" << result_name(repeated) << '\n';
            object->set_many({{"length", 20.0}, {"width", 4.0}});
        }

        object->set("width", 5.0); // 此时 view 已退订，只有 log 收到通知。
    }

    std::cout << "log calls=" << logged << ", view calls=" << refreshed << '\n';

    for (const auto id : document.ids()) {
        if (auto object = document.find(id).lock()) {
            print_properties(*object); // 此处不判断对象的具体类型。
        }
    }
    
    document.erase(first_id);
    std::cout << "deleted id=" << first_id << ", expired=" << std::boolalpha << handle.expired()
              << '\n';
}
} // namespace week04
