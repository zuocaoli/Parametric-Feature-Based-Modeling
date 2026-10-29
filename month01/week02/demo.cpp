module week02.demo;

import std;
import week02.rectangle;

namespace week02 {
// 通过统一属性接口打印对象，无需知道它是否为矩形；const 引用避免复制且不允许修改对象。
void print_properties(const PropertyObject &object) {
    // 获取属性快照，逐项只读遍历；const 引用避免再次复制每个属性。
    for (const auto &property : object.properties()) {
        // 输出名称、实际值类型和文本形式的值，例如 length (double) = 5。
        std::cout << property.name << " (" << type_name(property.value)
                  << ") = " << value_text(property.value);
        // 有单位时追加单位，例如 mm。
        if (!property.unit.empty()) {
            std::cout << ' ' << property.unit;
        }
        // 标记只读属性，例如由长宽计算出的面积。
        if (property.read_only) {
            std::cout << " [read-only]";
        }
        std::cout << '\n';
    }
}

// 演示统一属性接口：枚举属性、拒绝非法修改，以及修改尺寸后读取派生面积。
void demo() {
    // 默认长为 5、宽为 3；通过基类引用操作同一个对象，无需使用矩形专有接口。
    Rectangle rectangle;
    PropertyObject &object = rectangle;
    // 打印全部属性的名称、类型、值、单位及只读标记。
    print_properties(object);
    
    // 属性不存在，返回 NotFound，不创建新属性。
    std::cout << "missing: " << result_name(object.set("missing", 1.0)) << '\n';
    // length 要求 double，传入整数返回 TypeMismatch，不做隐式类型转换。
    std::cout << "wrong type: " << result_name(object.set("length", std::int64_t{10})) << '\n';
    // 类型正确但尺寸为负，返回 InvalidValue，保留原长度。
    std::cout << "invalid size: " << result_name(object.set("length", -1.0)) << '\n';
    // area 是只读的派生属性，直接赋值返回 ReadOnly。
    std::cout << "read-only: " << result_name(object.set("area", 100.0)) << '\n';
    // 合法修改：长度从 5 变为 10，返回 Changed。
    std::cout << "set length: " << result_name(object.set("length", 10.0)) << '\n';
    // 已知 area 存在，解引用 get() 返回的 optional；读取时计算面积 10 × 3 = 30。
    std::cout << "new area=" << value_text(*object.get("area")) << '\n';
}
} // namespace week02
