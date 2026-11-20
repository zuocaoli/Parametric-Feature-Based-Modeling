module;

#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

module geometry;

import std;

namespace cad {
namespace {
bool positive_size(double size) { return std::isfinite(size) && size > 0; }

std::expected<TopoDS_Shape, std::string> checked_solid(const TopoDS_Shape &shape) {
    if (shape.IsNull()) {
        return std::unexpected("empty geometry");
    }
    TopExp_Explorer solids(shape, TopAbs_SOLID);
    if (!solids.More()) {
        return std::unexpected("expected a solid");
    }
    const TopoDS_Shape solid = solids.Current();
    solids.Next();
    if (solids.More() || !BRepCheck_Analyzer(solid).IsValid()) {
        return std::unexpected("expected one valid solid");
    }
    return solid;
}

std::expected<Geometry, std::string> prism(RectangleProfile profile, double startZ, double distance) {

    const auto [x, y, width, length] = profile;

    if (!positive_size(width)                 //
        || !positive_size(length)             //
        || !positive_size(std::abs(distance)) //
        || !std::isfinite(x)                  //
        || !std::isfinite(y)                  //
        || !std::isfinite(startZ)             //
        || !std::isfinite(x + width)          //
        || !std::isfinite(y + length)         //
        || !std::isfinite(startZ + distance)) {

        return std::unexpected("invalid prism dimensions");
    }

    BRepBuilderAPI_MakePolygon wire;

    wire.Add(gp_Pnt(x, y, startZ));

    wire.Add(gp_Pnt(x + width, y, startZ));

    wire.Add(gp_Pnt(x + width, y + length, startZ));

    wire.Add(gp_Pnt(x, y + length, startZ));

    wire.Close();

    if (!wire.IsDone()) {
        return std::unexpected("could not construct rectangular wire");
    }

    BRepBuilderAPI_MakeFace face(wire.Wire());

    if (!face.IsDone()) {
        return std::unexpected("could not construct planar profile");
    }

    BRepPrimAPI_MakePrism builder(face.Face(), gp_Vec(0, 0, distance));

    if (!builder.IsDone()) {
        return std::unexpected("could not extrude profile");
    }

    auto solid = checked_solid(builder.Shape());

    if (!solid) {
        return std::unexpected(solid.error());
    }

    Geometry result{*solid};

    return result;
}

std::string kernel_error(const Standard_Failure &failure) {
    if (const char *message = failure.what(); message && *message) {
        return message;
    }
    return "OCCT operation failed";
}

}

std::expected<Geometry, std::string> extrude(RectangleProfile profile, double height) {

    if (!positive_size(height)) {
        return std::unexpected("extrusion height must be positive and finite");
    }

    //
    try {
        return prism(profile, 0, height);

    } catch (const Standard_Failure &failure) {

        return std::unexpected(kernel_error(failure));
    }
}

std::array<double, 6> bounds(const Geometry &geometry) {
    Bnd_Box box;
    BRepBndLib::AddOptimal(geometry.solid, box, false, false);
    std::array<double, 6> limits;
    box.Get(limits[0], limits[1], limits[2], limits[3], limits[4], limits[5]);
    return limits;
}

double volume(const Geometry &geometry) {
    GProp_GProps mass;
    BRepGProp::VolumeProperties(geometry.solid, mass);
    return mass.Mass();
}
}
