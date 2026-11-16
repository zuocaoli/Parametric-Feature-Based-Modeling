export module week02.rectangle;

export import week02.property;

export namespace week02 {
class Rectangle : public PropertyObject {
public:
    Rectangle(double length = 5.0, double width = 3.0);
};
} // namespace week02
