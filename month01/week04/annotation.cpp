module week04.annotation;

import std;

namespace week04 {
Annotation::Annotation(std::string text, ObjectId id) : PropertyObject(id) {
    add("name", "Name", "", std::string{"Annotation"});
    add("text", "Text", "", std::move(text));
    add("visible", "Visible", "", true);
}
} // namespace week04
