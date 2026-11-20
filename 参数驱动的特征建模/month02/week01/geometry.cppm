module;

#include <TopoDS_Face.hxx>
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
    std::vector<TopoDS_Face> endFaces = {};
};

std::expected<Geometry, std::string> extrude(RectangleProfile profile, double height);
enum class CutSupport { FixedPlane, ExtrusionEnd };
enum class FaceReferenceError { Missing, Ambiguous };

std::expected<TopoDS_Face, FaceReferenceError> resolve_end_face(std::span<const TopoDS_Face> candidates);
std::expected<Geometry, std::string> cut(const Geometry &base, RectangleProfile profile, double depth, CutSupport support);
bool contains_material(const Geometry &geometry, double x, double y, double z);
std::array<double, 6> bounds(const Geometry &geometry);
double volume(const Geometry &geometry);
}
