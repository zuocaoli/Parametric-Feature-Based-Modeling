export module week03.rectangle;

export import week03.property;

export namespace week03 {
class Rectangle : public PropertyObject {
public:
    Rectangle(double length = 5.0, double width = 3.0);
};
} // namespace week03
