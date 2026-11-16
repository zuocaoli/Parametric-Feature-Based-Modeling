export module month06.week03.rectangle;

import std;
export import month06.week03.property;

export namespace month06::week03 {
class Rectangle : public PropertyObject {
public:
    Rectangle(double length = 5.0, double width = 3.0, ObjectId id = 0);
};
} // namespace month06::week03
