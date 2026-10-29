module month06.week04.rectangle;

import std;

namespace month06::week04 {
namespace {
bool positive_size(const Value &value) {
    const double number = std::get<double>(value);
    return std::isfinite(number) && number > 0.0;
}
} // namespace

Rectangle::Rectangle(double length, double width, ObjectId id) : PropertyObject(id) {
    add("x", "X", "mm", 0.0,
        [](const Value &value) { return std::isfinite(std::get<double>(value)); });
    add("y", "Y", "mm", 0.0,
        [](const Value &value) { return std::isfinite(std::get<double>(value)); });
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
} // namespace month06::week04
