export module month04.week02.rectangle;

import std;
export import month04.week02.property;

export namespace month04::week02 {
class Rectangle : public PropertyObject {
public:
    Rectangle(double length = 5.0, double width = 3.0, ObjectId id = 0);
};
} // namespace month04::week02
