module week04.document;

import std;
import week04.rectangle;
import week04.annotation;

namespace week04 {
// template <class T, class... Args> 声明了两个模板参数：
//
// - T：一个类型，例如 Rectangle。
// - Args：类型参数包，可以包含零个或多个类型。... 是表示参数包的语法；这里的 class 也可以写成typename。
//         class、...、Args 三个记号之间可以有空格，也可以没有空格，书写习惯的问题。
// 例如调用 create<Rectangle>(length, width) 时，T 明确是 Rectangle，编译器再根据 length 和 width。
// 推导 Args 中的两个类型。这里的 Args 是类型的集合，函数括号里的 args 才是实际传入的值。
template <class T, class... Args> ObjectId Document::create(Args &&...args) {

    if (next_id_ == 0) {
        throw std::overflow_error("document object IDs exhausted");
    }
    const ObjectId id = next_id_;

    // std::forward std::forward 是 C++11 引入的一个模板函数，主要用于完美转发。
    // 在模板编程中，当一个函数模板的参数被传递给另一个函数时，我们希望保持参数原有的值类别（左值还是右值）,std::forward 就是用来实现这一点的。
    // ... 则对整个表达式逐项展开.
    //
    // 这里的 Args 和 args 来自函数声明：
    // template <class T, class... Args>
    // ObjectId Document::create(Args&&... args)
    // Args 是一串类型，args 是与它们一一对应的形参。假设有两个参数，用 A0、A1 表示类型，用 a0、a1 表示形参，那么：
    // std::forward<Args>(args)...
    // 展开后相当于： std::forward<A0>(a0), std::forward<A1>(a1)
    // 因此整行代码可以理解为：
    // std::make_shared<T>(
    //     std::forward<A0>(a0),
    //     std::forward<A1>(a1),
    //     id
    // );
    //
    auto object = std::make_shared<T>(std::forward<Args>(args)..., id);
    
    objects_.emplace(id, std::move(object));
    ++next_id_; // 无符号溢出后变为 0，下次创建明确拒绝，永不复用已有 ID。
    return id;
}

ObjectId Document::create_rectangle(double length, double width) {
    return create<Rectangle>(length, width);
}

ObjectId Document::create_annotation(std::string text) {
    return create<Annotation>(std::move(text));
}

std::weak_ptr<PropertyObject> Document::find(ObjectId id) const {
    const auto found = objects_.find(id);
    return found == objects_.end() ? std::weak_ptr<PropertyObject>{} : found->second;
}

bool Document::erase(ObjectId id) {
    return objects_.erase(id) != 0;
}

std::vector<ObjectId> Document::ids() const {
    std::vector<ObjectId> result;
    for (const auto &entry : objects_) {
        result.push_back(entry.first);
    }
    return result;
}
} // namespace week04
