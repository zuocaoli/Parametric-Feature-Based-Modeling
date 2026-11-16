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
void solverTests() {
    for (double radius : {1e-6, std::nextafter(1e-10, 1.0)}) {
        Sketch tiny;
        auto center = tiny.addPoint(0, 0), far = tiny.addPoint(100, 0);
        auto circle = tiny.addCircle(center, radius);
        tiny.addConstraint(ConstraintType::Fixed, {center}, 0, 0);
        tiny.addConstraint(ConstraintType::Fixed, {far}, 100, 0);
        auto dimension = tiny.addConstraint(ConstraintType::Radius, {circle}, radius);
        Solver exact(tiny);
        auto original = tiny.parameters();
        auto result = exact.solve();
        check(result.status == SolveStatus::Success,
              "already-satisfied small circle solves successfully");
        check(tiny.parameters() == original, "satisfied small circle remains unchanged");
        tiny.removeConstraint(dimension);
        tiny.addConstraint(ConstraintType::Radius, {circle}, 2e-6);
        Solver driven(tiny, {.solveTolerance = 1e-14});
        result = driven.solve();
        check(result.status == SolveStatus::Success, "Newton updates a small circle radius");
        near(tiny.radius(circle), 2e-6, 1e-12);
    }
    auto l = makeRectangle();
    Solver solver(l.sketch);
    auto r = solver.solve();
    check(r.converged(), "perturbed rectangle converges");
    check(r.iterations <= 10, "rectangle iteration budget");
    near(l.sketch.point(l.points[1]).x, 100);
    check(evaluate(l.sketch).satisfied(1e-8), "all rectangle constraints satisfied");
    auto previous = l.sketch.parameters();
    auto changed = previous;
    changed[2] += 10;
    l.sketch.setParameters(changed);
    check(solver.solve().converged(), "warm start succeeds");
    for (std::size_t i = 0; i < previous.size(); ++i)
        near(l.sketch.parameters()[i], previous[i]);
    for (auto type : constraintTypes) {
        auto p = primitive(type);
        auto x = p.parameters();
        for (std::size_t i = 0; i < x.size(); ++i)
            x[i] += 0.1 * std::sin(i + 0.5);
        p.setParameters(x);
        Solver engine(p);
        check(engine.solve().converged(), "11 constraint classes solve from perturbed input");
        check(evaluate(p).satisfied(1e-8), "solved primitive satisfies constraint");
    }
    auto bad = makeRectangle();
    auto before = bad.sketch.parameters();
    Solver limited(bad.sketch, {.maxIterations = 0});
    check(limited.solve().status == SolveStatus::Failed, "zero iteration limit fails explicitly");
    check(bad.sketch.parameters() == before, "failed solve is atomic");
}
void pathologyTests() {
    Matrix invalid(1, 1);
    invalid(0, 0) = std::numeric_limits<double>::quiet_NaN();
    rejects([&] { leastSquares(invalid, {0}); });
    invalid(0, 0) = 1;
    rejects([&] { leastSquares(invalid, {std::numeric_limits<double>::infinity()}); });
    Sketch zero;
    auto a = zero.addPoint(0, 0), b = zero.addPoint(0, 0);
    auto line = zero.addLine(a, b);
    zero.addConstraint(ConstraintType::Horizontal, {line});
    Solver z(zero);
    check(z.solve().status == SolveStatus::Failed, "zero-length line fails safely");
    Sketch circles;
    auto c = circles.addPoint(0, 0);
    auto c1 = circles.addCircle(c, 5), c2 = circles.addCircle(c, 5);
    circles.addConstraint(ConstraintType::Tangent, {c1, c2});
    Solver coincident(circles);
    check(coincident.solve().status == SolveStatus::Failed, "concentric tangency fails safely");
    auto regular = makeRectangle();
    Solver badOption(regular.sketch, {.differenceStep = std::numeric_limits<double>::infinity()});
    check(badOption.solve().status == SolveStatus::Failed, "invalid options fail safely");
    Sketch empty;
    Solver nothing(empty);
    check(nothing.solve().converged(), "empty system handled");
    Sketch isolated;
    isolated.addPoint(1, 2);
    Solver free(isolated);
    check(free.solve().converged(), "unconstrained point handled");
}
void structureTests() {
    auto l = makeRectangle(false);
    auto full = analyzeStructure(evaluate(l.sketch), l.sketch.parameters().size());
    check(full.dof == 0 && full.matching == 8, "full rectangle structural matching");
    l.sketch.removeConstraint(l.width);
    auto under = analyzeStructure(evaluate(l.sketch), l.sketch.parameters().size());
    check(under.dof == 1 && under.unmatchedParameters.size() == 1,
          "removed dimension has structural dof one");
    auto first = l.sketch.constraints().front().id;
    l.sketch.addConstraint(ConstraintType::Horizontal, l.sketch.constraint(first).refs);
    check(analyzeStructure(evaluate(l.sketch), l.sketch.parameters().size()).dof == 0,
          "structural matching can miss geometric dependence");
}
void rankTests() {
    auto l = makeRectangle(false);
    Solver solver(l.sketch);
    check(solver.solve().dof == 0, "full rectangle numeric dof zero");
    l.sketch.removeConstraint(l.width);
    auto r = solver.solve();
    check(r.status == SolveStatus::UnderConstrained && r.dof == 1, "local dof one");
    auto indices = l.sketch.entity(l.points[0]).parameters;
    for (auto index : indices)
        check(std::ranges::find(r.movableParameters, index) == r.movableParameters.end(),
              "anchor is immovable");
    check(!r.movableParameters.empty(), "moving coordinates identified");
    l.sketch.addConstraint(ConstraintType::Horizontal, {l.lines[0]});
    r = solver.solve();
    check(r.structuralDof == 0 && r.dof == 1, "numerical rank corrects structural estimate");
    Matrix singular(2, 2);
    singular(0, 0) = 1;
    singular(1, 0) = 2;
    auto analysis = analyzeRank(singular);
    check(analysis.rank == 1 && analysis.nullspace.cols == 1, "known rank and nullspace");
}
void redundancyTests() {
    auto l = makeRectangle(false);
    const auto width = l.sketch.constraint(l.width);
    auto repeated = l.sketch.addConstraint(width.type, width.refs, width.value);
    Solver solver(l.sketch);
    auto report = solver.solve();
    check(report.status == SolveStatus::Redundant, "duplicate dimension classified");
    check(report.redundant == std::vector<ConstraintId>{repeated},
          "new duplicate ID located exactly");
    auto fixed = l.sketch.addConstraint(ConstraintType::Fixed, {l.points[0]}, 0, 0);
    report = solver.solve();
    check(std::ranges::find(report.redundant, fixed) != report.redundant.end(),
          "two-equation constraint mapped to ID");
    l.sketch.removeConstraint(l.height);
    report = solver.solve();
    check(report.status == SolveStatus::Redundant && report.dof == 1,
          "redundancy and underconstraint coexist");
}
void conflictTests() {
    auto l = makeRectangle(false);
    Solver solver(l.sketch);
    check(solver.solve().converged(), "conflict fixture solved");
    auto previous = l.sketch.parameters();
    const auto width = l.sketch.constraint(l.width);
    auto conflicting = l.sketch.addConstraint(width.type, {width.refs[1], width.refs[0]}, 120);
    auto report = solver.solve();
    check(report.status == SolveStatus::Conflicting, "contradictory dimensions classified");
    check(report.conflicting == std::vector<ConstraintId>{l.width, conflicting},
          "both conflicting IDs returned");
    check(l.sketch.parameters() == previous, "conflict does not alter successful geometry");
    l.sketch.removeConstraint(conflicting);
    check(solver.solve().converged(), "removing conflict recovers");
    auto duplicate = l.sketch.addConstraint(ConstraintType::Fixed, {l.points[0]}, 3, 4);
    report = solver.solve();
    check(report.status == SolveStatus::Conflicting && report.conflicting.back() == duplicate,
          "conflicting composite constraint");
}
void dimensionTests() {
    auto l = makeRectangle(false);
    Solver solver(l.sketch);
    for (int step = 0; step < 20; ++step) {
        double width = 100 + step * 0.5;
        l.sketch.setConstraintValue(l.width, width);
        auto report = solver.solve();
        check(report.converged(), "dimension updates converge");
        near(l.sketch.point(l.points[1]).x, width);
    }
    rejects([&] { l.sketch.setConstraintValue(l.width, -1); });
    rejects(
        [&] { l.sketch.setConstraintValue(l.height, std::numeric_limits<double>::quiet_NaN()); });
    rejects([&] { l.sketch.setConstraintValue(l.sketch.constraints()[0].id, 5); });
    auto values = l.sketch.parameters();
    l.sketch.addConstraint(ConstraintType::Distance, l.sketch.constraint(l.width).refs, 90);
    check(solver.solve().status == SolveStatus::Conflicting, "bad dimension update diagnosed");
    check(values == l.sketch.parameters(), "failed dimension solve preserves geometry");
    Sketch circles;
    auto a = circles.addPoint(0, 0), b = circles.addPoint(15, 0);
    auto c1 = circles.addCircle(a, 10), c2 = circles.addCircle(b, 5);
    circles.addConstraint(ConstraintType::Fixed, {a}, 0, 0);
    circles.addConstraint(ConstraintType::Radius, {c1}, 10);
    auto radius = circles.addConstraint(ConstraintType::Radius, {c2}, 5);
    circles.addConstraint(ConstraintType::Tangent, {c1, c2});
    Solver tangent(circles);
    for (int i = 0; i < 20; ++i) {
        circles.setConstraintValue(radius, 5 + i * 0.025);
        check(tangent.solve().converged(), "continuous tangent dimension path");
        auto p = circles.point(b);
        check(p.x > 0, "tangent solution remains on original branch");
        near(std::hypot(p.x, p.y), 10 + 5 + i * 0.025);
    }
}
void roundedTests() {
    auto l = makeRoundedRectangle();
    check(l.sketch.parameters().size() == 28 && evaluate(l.sketch).rows.size() == 28,
          "rounded benchmark size");
    Solver solver(l.sketch);
    auto report = solver.solve();
    check(report.status == SolveStatus::Success && report.dof == 0,
          "rounded rectangle is fully constrained");
    check(report.iterations <= 10, "rounded benchmark iteration budget");
    check(report.redundant.empty(), "rounded constraints are independent");
    near(l.sketch.point(l.points[2]).x, 100);
    near(l.sketch.point(l.points[4]).y, 60);
    for (int i = 0; i < 20; ++i) {
        l.sketch.setConstraintValue(l.width, 100 + i * 0.5);
        report = solver.solve();
        check(report.status == SolveStatus::Success, "rounded continuous dimension updates");
        near(l.sketch.point(l.points[2]).x, 100 + i * 0.5);
        near(l.sketch.point(l.points[2]).y, 10);
    }
    auto width = l.sketch.constraint(l.width);
    auto duplicate = l.sketch.addConstraint(width.type, width.refs, width.value);
    check(solver.solve().redundant == std::vector<ConstraintId>{duplicate},
          "rounded duplicate dimension");
    l.sketch.removeConstraint(duplicate);
    auto bad = l.sketch.addConstraint(width.type, width.refs, 120);
    check(solver.solve().conflicting == std::vector<ConstraintId>{l.width, bad},
          "rounded conflict pair");
    l.sketch.removeConstraint(bad);
    l.sketch.removeConstraint(l.width);
    report = solver.solve();
    check(report.dof == 1, "rounded width removal leaves one dof");
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
        else if (group == "solver")
            solverTests();
        else if (group == "pathology")
            pathologyTests();
        else if (group == "structure")
            structureTests();
        else if (group == "rank")
            rankTests();
        else if (group == "redundancy")
            redundancyTests();
        else if (group == "conflict")
            conflictTests();
        else if (group == "dimensions")
            dimensionTests();
        else if (group == "rounded")
            roundedTests();
        else
            throw std::invalid_argument("unknown test group");
        std::cout << group << " checks passed\n";
        return 0;
    } catch (const std::exception &failure) {
        std::cerr << group << ": " << failure.what() << '\n';
        return 1;
    }
}
