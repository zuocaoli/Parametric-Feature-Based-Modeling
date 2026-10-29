export module week04.annotation;

import std;
export import week04.property;

export namespace week04 {
class Annotation : public PropertyObject {
public:
    explicit Annotation(std::string text = "CAD note", ObjectId id = 0);
};
} // namespace week04
