module week02.rectangle;

import std;

namespace week02 {
namespace {
bool positive_size(const Value &value) {
    const double number = std::get<double>(value);
    return std::isfinite(number) && number > 0.0;
}
} // namespace

Rectangle::Rectangle(double length, double width) {
    add("length", "Length", "mm", length, positive_size);
    add("width", "Width", "mm", width, positive_size);
    // 初始 0 只是类型占位，实际面积由读取函数计算，不维护面积缓存。
    add("area", "Area", "mm^2", 0.0, {}, [this] {
        return Value{std::get<double>(*get("length")) * std::get<double>(*get("width"))};
    });
    add("segments", "Segments", "", std::int64_t{1},
        [](const Value &value) { return std::get<std::int64_t>(value) > 0; });
    add("visible", "Visible", "", true);
    add("name", "Name", "", std::string{"Rectangle"});
}
} // namespace week02
