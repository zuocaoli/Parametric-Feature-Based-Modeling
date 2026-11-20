module;

#include <TopoDS_Shape.hxx>

export module geometry;

import std;

export namespace cad {

/**
 * 表示拉伸前的二维矩形轮廓。它用四个数描述矩形的位置和尺寸。
 */
struct RectangleProfile {
    // 矩形起始角点的坐标
    double x;
    double y;

    // 沿 X，Y 轴的尺寸
    double width;
    double length;
};

struct Geometry {
    TopoDS_Shape solid;
};

std::expected<Geometry, std::string> extrude(RectangleProfile profile, double height);
std::array<double, 6> bounds(const Geometry &geometry);
double volume(const Geometry &geometry);
}
