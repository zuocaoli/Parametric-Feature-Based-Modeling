module month04.week01.rectangle;

import std;

namespace month04::week01 {
namespace {
bool positive_size(const Value &value) {
    const double number = std::get<double>(value);
    return std::isfinite(number) && number > 0.0;
}
} // namespace

Rectangle::Rectangle(double length, double width, ObjectId id) : PropertyObject(id) {
    add("length", "Length", "mm", length, positive_size);
    add("width", "Width", "mm", width, positive_size);
    // 本周用存储型面积演示公式写回；绑定和计算前允许 0 占位。
    add("area", "Area", "mm^2", 0.0, [](const Value &value) {
        const double number = std::get<double>(value);
        return std::isfinite(number) && number >= 0.0;
    });
    add("segments", "Segments", "", std::int64_t{1},
        [](const Value &value) { return std::get<std::int64_t>(value) > 0; });
    add("visible", "Visible", "", true);
    add("name", "Name", "", std::string{"Rectangle"});
}
} // namespace month04::week01
