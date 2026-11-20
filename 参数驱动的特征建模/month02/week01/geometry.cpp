module;

#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Pln.hxx>
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
    if (!positive_size(width) || !positive_size(length) || !positive_size(std::abs(distance)) || !std::isfinite(x) || !std::isfinite(y) ||
        !std::isfinite(startZ) || !std::isfinite(x + width) || !std::isfinite(y + length) || !std::isfinite(startZ + distance)) {
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
    const auto end = builder.LastShape();
    for (TopExp_Explorer faces(end, TopAbs_FACE); faces.More(); faces.Next()) {
        result.endFaces.push_back(TopoDS::Face(faces.Current()));
    }
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
    try {
        return prism(profile, 0, height);
    } catch (const Standard_Failure &failure) {
        return std::unexpected(kernel_error(failure));
    }
}

std::expected<TopoDS_Face, FaceReferenceError> resolve_end_face(std::span<const TopoDS_Face> candidates) {
    if (candidates.empty()) {
        return std::unexpected(FaceReferenceError::Missing);
    }
    if (candidates.size() > 1) {
        return std::unexpected(FaceReferenceError::Ambiguous);
    }
    if (candidates.front().IsNull()) {
        return std::unexpected(FaceReferenceError::Missing);
    }
    return candidates.front();
}

std::expected<Geometry, std::string> cut(const Geometry &base, RectangleProfile profile, double depth, CutSupport support) {
    if (!positive_size(depth)) {
        return std::unexpected("cut depth must be positive and finite");
    }
    try {
        double startZ = 0;
        double distance = depth;
        if (support == CutSupport::ExtrusionEnd) {
            const auto face = resolve_end_face(base.endFaces);
            if (!face) {
                return std::unexpected(face.error() == FaceReferenceError::Missing ? "extrusion end face is missing"
                                                                                   : "extrusion end face is ambiguous");
            }
            BRepAdaptor_Surface surface(*face);
            if (surface.GetType() != GeomAbs_Plane || std::abs(surface.Plane().Axis().Direction().Z()) < 1 - 1e-9) {
                return std::unexpected("extrusion end face must be horizontal");
            }
            startZ = surface.Plane().Location().Z();
            distance = -depth;
        }
        auto tool = prism(profile, startZ, distance);
        if (!tool) {
            return std::unexpected(tool.error());
        }
        NCollection_List<TopoDS_Shape> arguments;
        arguments.Append(base.solid);
        NCollection_List<TopoDS_Shape> tools;
        tools.Append(tool->solid);

        BRepAlgoAPI_Cut builder;
        builder.SetArguments(arguments);
        builder.SetTools(tools);
        builder.SetNonDestructive(true);
        builder.Build();
        if (!builder.IsDone() || builder.HasErrors()) {
            return std::unexpected("boolean cut failed");
        }
        auto solid = checked_solid(builder.Shape());
        if (!solid) {
            return std::unexpected(solid.error());
        }
        Geometry result{*solid};
        if (volume(base) - volume(result) <= 1e-7) {
            return std::unexpected("cut removed no material");
        }
        return result;
    } catch (const Standard_Failure &failure) {
        return std::unexpected(kernel_error(failure));
    }
}

bool contains_material(const Geometry &geometry, double x, double y, double z) {
    BRepClass3d_SolidClassifier classifier(geometry.solid, gp_Pnt(x, y, z), 1e-7);
    return classifier.State() == TopAbs_IN;
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
