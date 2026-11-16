export module week04.rectangle;

import std;
export import week04.property;

export namespace week04 {
class Rectangle : public PropertyObject {
public:
    Rectangle(double length = 5.0, double width = 3.0, ObjectId id = 0);
};
} // namespace week04
