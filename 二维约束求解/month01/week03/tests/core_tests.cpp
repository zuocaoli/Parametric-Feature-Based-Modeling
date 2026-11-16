import std;
import mini_gcs.lesson;
namespace {
using namespace gcs;
void check(bool condition, std::string_view message) {
    if (!condition)
        throw std::runtime_error(std::string(message));
}
void near(double actual, double expected, double tolerance = 1e-6) {
    if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance)
        throw std::runtime_error("expected " + std::to_string(expected) + ", got " +
                                 std::to_string(actual));
}
template <class F> void rejects(F operation) {
    bool rejected = false;
    try {
        operation();
    } catch (const std::exception &) {
        rejected = true;
    }
    check(rejected, "invalid input must be rejected");
}
void modelTests() {
    Sketch s;
    auto a = s.addPoint(0, 0), b = s.addPoint(10, 0), center = s.addPoint(0, 10);
    auto line = s.addLine(a, b);
    auto circle = s.addCircle(center, 10);
    auto arc = s.addArc(center, 10, a, s.addPoint(10, 10));
    check(s.parameters().size() == 10, "shared points do not duplicate parameters");
    check(s.entity(line).parameters.empty(), "line owns no parameters");
    auto parameters = s.parameters();
    parameters[0] = 2;
    s.setParameters(parameters);
    near(s.getEntityParams(line)[0], 2);
    near(s.getEntityParams(arc)[2], 2);
    near(s.radius(circle), 10);
    check(s.parameterIndices(arc).size() == 7, "arc parameter dependency count");
    rejects([&] { s.entity(0); });
    rejects([&] { s.addLine(a, 999); });
    rejects([&] { s.addCircle(a, -1); });
    rejects([&] { s.addArc(a, 1, b, b); });
    rejects([&] { s.addPoint(std::numeric_limits<double>::quiet_NaN(), 0); });
    rejects([&] { s.setParameters({1}); });
}
Sketch primitive(ConstraintType type, bool internal = false) {
    Sketch s;
    auto a = s.addPoint(0, 0), b = s.addPoint(10, 0);
    auto c = s.addPoint(0, 5), d = s.addPoint(10, 5);
    auto horizontal = s.addLine(a, b);
    auto parallel = s.addLine(c, d);
    auto vertical = s.addLine(a, c);
    auto circle = s.addCircle(c, 5);
    switch (type) {
    case ConstraintType::Coincident:
        s.addConstraint(type, {a, s.addPoint(0, 0)});
        break;
    case ConstraintType::PointOnObject:
        s.addConstraint(type, {a, circle});
        break;
    case ConstraintType::Horizontal:
        s.addConstraint(type, {horizontal});
        break;
    case ConstraintType::Vertical:
        s.addConstraint(type, {vertical});
        break;
    case ConstraintType::Parallel:
        s.addConstraint(type, {horizontal, parallel});
        break;
    case ConstraintType::Perpendicular:
        s.addConstraint(type, {horizontal, vertical});
        break;
    case ConstraintType::Tangent:
        s.addConstraint(type, {horizontal, circle});
        break;
    case ConstraintType::Distance:
        s.addConstraint(type, {a, b}, 10);
        break;
    case ConstraintType::Angle:
        s.addConstraint(type, {horizontal, vertical}, std::numbers::pi / 2);
        break;
    case ConstraintType::Radius:
        s.addConstraint(type, {circle}, 5);
        break;
    case ConstraintType::Fixed:
        s.addConstraint(type, {a}, 0, 0);
        break;
    }
    (void)internal;
    return s;
}
constexpr std::array constraintTypes{
    ConstraintType::Coincident, ConstraintType::PointOnObject, ConstraintType::Horizontal,
    ConstraintType::Vertical,   ConstraintType::Parallel,      ConstraintType::Perpendicular,
    ConstraintType::Tangent,    ConstraintType::Distance,      ConstraintType::Angle,
    ConstraintType::Radius,     ConstraintType::Fixed};
void residualTests() {
    for (auto type : constraintTypes) {
        auto s = primitive(type);
        check(evaluate(s).satisfied(1e-10), "satisfied primitive residual");
        for (auto &row : evaluate(s).rows)
            check(row.constraint == 1, "equation owner ID");
        auto x = s.parameters();
        for (std::size_t i = 0; i < x.size(); ++i)
            x[i] += 0.2 * std::sin(i + 0.5);
        s.setParameters(x);
        check(evaluate(s).infinityNorm() > 1e-5, "violated primitive must have residual");
    }
    Sketch s;
    auto center = s.addPoint(0, 0), a = s.addPoint(10, 0), b = s.addPoint(0, 10);
    auto arc = s.addArc(center, 10, a, b);
    auto f = evaluate(s);
    check(f.rows.size() == 2, "arc intrinsic equation count");
    check(f.rows[0].constraint == 0 && f.rows[0].intrinsicEntity == arc, "intrinsic ownership");
    auto line = s.addLine(a, s.addPoint(10, 20));
    s.addConstraint(ConstraintType::Tangent, {line, arc});
    check(evaluate(s).satisfied(1e-10), "arc endpoint tangency");
    auto c1 = s.addCircle(center, 10);
    auto c2 = s.addCircle(s.addPoint(15, 0), 5);
    auto t = s.addConstraint(ConstraintType::Tangent, {c1, c2});
    check(evaluate(s).satisfied(1e-10), "external circle tangency");
    s.removeConstraint(t);
    auto c3 = s.addCircle(s.addPoint(5, 0), 5);
    s.addConstraint(ConstraintType::Tangent, {c1, c3}, 0, 0, TangencyBranch::Internal);
    check(evaluate(s).satisfied(1e-10), "internal circle tangency");
    auto vertical = s.addLine(s.addPoint(20, -5), s.addPoint(20, 15));
    auto distance = s.addConstraint(ConstraintType::Distance, {center, vertical}, 20);
    check(evaluate(s).satisfied(1e-10), "point-line signed branch");
    s.removeConstraint(distance);
    s.addConstraint(ConstraintType::PointOnObject, {s.addPoint(20, 2), vertical});
    check(evaluate(s).satisfied(1e-10), "point-line incidence");
    rejects([&] { s.addConstraint(ConstraintType::Radius, {line}, 2); });
    rejects([&] { s.addConstraint(ConstraintType::Angle, {line}); });
    rejects([&] { s.removeConstraint(999); });
}
void jacobianTests() {
    for (double tolerance : {1e-10, 1e-7}) {
        for (double radius : {1e-6, std::nextafter(tolerance, 1.0)}) {
            for (bool arc : {false, true}) {
                Sketch tiny;
                auto center = tiny.addPoint(0, 0);
                tiny.addPoint(100, 0);
                auto circular = arc ? tiny.addArc(center, radius, tiny.addPoint(radius, 0),
                                                  tiny.addPoint(0, radius))
                                    : tiny.addCircle(center, radius);
                tiny.addConstraint(ConstraintType::Radius, {circular}, radius);
                auto original = tiny.parameters();
                check(evaluate(tiny, 100, tolerance).satisfied(1e-12),
                      "small-radius fixture already satisfies constraints");
                auto derivative = jacobian(tiny, 0, 1e-7, tolerance);
                auto column = tiny.entity(circular).parameters[0];
                near(derivative(derivative.rows - 1, column), 1, 1e-5);
                if (arc) {
                    near(derivative(0, column), -1, 1e-5);
                    near(derivative(1, column), -1, 1e-5);
                }
                check(tiny.parameters() == original, "Jacobian probes preserve input geometry");
                auto invalid = original;
                for (double badRadius : {tolerance, 0.0, -radius}) {
                    invalid[column] = badRadius;
                    tiny.setParameters(invalid);
                    rejects([&] { jacobian(tiny, 100, 1e-7, tolerance); });
                }
            }
        }
    }
    Sketch s;
    auto a = s.addPoint(3, 4), b = s.addPoint(13, 4);
    auto line = s.addLine(a, b);
    s.addConstraint(ConstraintType::Fixed, {a}, 0, 0);
    s.addConstraint(ConstraintType::Horizontal, {line});
    s.addConstraint(ConstraintType::Distance, {a, b}, 8);
    auto j = jacobian(s, 10);
    near(j(0, 0), 1, 1e-5);
    near(j(1, 1), 1, 1e-5);
    near(j(2, 1), -1, 1e-5);
    near(j(2, 3), 1, 1e-5);
    near(j(3, 0), -1, 1e-5);
    near(j(3, 2), 1, 1e-5);
    near(j(3, 1), 0, 1e-5);
    auto coarse = jacobian(s, 10, 1e-5);
    for (std::size_t i = 0; i < j.values.size(); ++i)
        near(j.values[i], coarse.values[i], 1e-5);
    rejects([&] { jacobian(s, 10, 0); });
    Sketch angle;
    auto origin = angle.addPoint(0, 0), left = angle.addPoint(-10, 0),
         right = angle.addPoint(10, 0);
    auto l1 = angle.addLine(origin, right), l2 = angle.addLine(origin, left);
    angle.addConstraint(ConstraintType::Angle, {l1, l2}, std::numbers::pi);
    auto angleJ = jacobian(angle, 10);
    for (double entry : angleJ.values)
        check(std::isfinite(entry) && std::abs(entry) < 3, "angle derivative wraps at pi");
}
}
int main(int argc, char **argv) {
    const std::string group = argc > 1 ? argv[1] : "model";
    try {
        if (group == "model")
            modelTests();
        else if (group == "residual")
            residualTests();
        else if (group == "jacobian")
            jacobianTests();
        else
            throw std::invalid_argument("unknown test group");
        std::cout << group << " checks passed\n";
        return 0;
    } catch (const std::exception &failure) {
        std::cerr << group << ": " << failure.what() << '\n';
        return 1;
    }
}
