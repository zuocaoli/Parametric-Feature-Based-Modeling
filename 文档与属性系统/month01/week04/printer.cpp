module week04.printer;

import std;
import week04.property;

namespace week04 {
void print_properties(const PropertyObject &object, std::ostream &out) {
    out << "object id=" << object.id() << '\n';
    for (const auto &property : object.properties()) {
        out << property.name << " (" << type_name(property.value)
            << ") = " << value_text(property.value);
        if (!property.unit.empty()) {
            out << ' ' << property.unit;
        }
        if (property.read_only) {
            out << " [read-only]";
        }
        out << '\n';
    }
}
} // namespace week04
