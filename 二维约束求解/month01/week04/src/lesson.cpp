module mini_gcs.lesson;
import std;
namespace gcs {
Lesson makeRectangle(bool perturbed) {
    Lesson l;
    auto &s = l.sketch;
    for (auto p : std::array<Vec2, 4>{{{0, 0}, {100, 0}, {100, 60}, {0, 60}}})
        l.points.push_back(s.addPoint(p.x, p.y));
    for (std::size_t i = 0; i < 4; ++i)
        l.lines.push_back(s.addLine(l.points[i], l.points[(i + 1) % 4]));
    s.addConstraint(ConstraintType::Horizontal, {l.lines[0]});
    s.addConstraint(ConstraintType::Vertical, {l.lines[1]});
    s.addConstraint(ConstraintType::Horizontal, {l.lines[2]});
    s.addConstraint(ConstraintType::Vertical, {l.lines[3]});
    s.addConstraint(ConstraintType::Fixed, {l.points[0]}, 0, 0);
    l.width = s.addConstraint(ConstraintType::Distance, {l.points[0], l.points[1]}, 100);
    l.height = s.addConstraint(ConstraintType::Distance, {l.points[1], l.points[2]}, 60);
    if (perturbed) {
        auto values = s.parameters();
        values[2] += 3;
        values[3] += 2;
        values[4] -= 2;
        values[5] -= 3;
        s.setParameters(values);
    }
    return l;
}
Lesson makeLesson() {
    return makeRectangle();
}
std::string runLesson(Lesson &l) {
    std::ostringstream text;
    text << "Parameters: " << l.sketch.parameters().size();
    auto residual = evaluate(l.sketch);
    text << " | equations: " << residual.rows.size() << " | residual: " << residual.infinityNorm();
    Solver solver(l.sketch);
    auto report = solver.solve();
    text << " | " << statusName(report.status) << " | iterations: " << report.iterations
         << " | final residual: " << report.finalResidual;
    return text.str();
}
}
