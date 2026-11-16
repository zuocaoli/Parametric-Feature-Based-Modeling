export module month02.week03.rectangle;

import std;
export import month02.week03.property;

export namespace month02::week03 {
class Rectangle : public PropertyObject {
public:
    Rectangle(double length = 5.0, double width = 3.0, ObjectId id = 0);
};
} // namespace month02::week03
