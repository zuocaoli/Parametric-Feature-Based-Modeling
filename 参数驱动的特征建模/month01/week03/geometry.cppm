module;

#include <TopoDS_Shape.hxx>

export module geometry;

import std;

export namespace cad {
struct RectangleProfile {
    double x;
    double y;
    double width;
    double length;
};

struct Geometry {
    TopoDS_Shape solid;
};

std::expected<Geometry, std::string> extrude(RectangleProfile profile, double height);
std::expected<Geometry, std::string> cut(const Geometry &base, RectangleProfile profile, double depth);
std::array<double, 6> bounds(const Geometry &geometry);
double volume(const Geometry &geometry);
}
