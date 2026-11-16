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
Lesson makeRoundedRectangle(bool perturbed) {
    Lesson l;
    auto &s = l.sketch;
    for (auto p : std::array<Vec2, 8>{
             {{10, 0}, {90, 0}, {100, 10}, {100, 50}, {90, 60}, {10, 60}, {0, 50}, {0, 10}}})
        l.points.push_back(s.addPoint(p.x, p.y));
    for (std::size_t i = 0; i < 4; ++i)
        l.lines.push_back(s.addLine(l.points[2 * i], l.points[2 * i + 1]));
    std::array<Vec2, 4> centers{{{90, 10}, {90, 50}, {10, 50}, {10, 10}}};
    for (std::size_t i = 0; i < 4; ++i) {
        auto center = s.addPoint(centers[i].x, centers[i].y);
        auto arc = s.addArc(center, 10, l.points[2 * i + 1], l.points[(2 * i + 2) % 8]);
        l.arcs.push_back(arc);
        s.addConstraint(ConstraintType::Radius, {arc}, 10);
        s.addConstraint(ConstraintType::Tangent, {l.lines[i], arc});
        s.addConstraint(ConstraintType::Tangent, {l.lines[(i + 1) % 4], arc});
    }
    for (std::size_t i = 0; i < 4; ++i)
        s.addConstraint(i % 2 ? ConstraintType::Vertical : ConstraintType::Horizontal,
                        {l.lines[i]});
    s.addConstraint(ConstraintType::Fixed, {l.points[0]}, 10, 0);
    l.width = s.addConstraint(ConstraintType::Distance, {l.points[7], l.lines[1]}, 100);
    l.height = s.addConstraint(ConstraintType::Distance, {l.points[0], l.lines[2]}, 60);
    if (perturbed) {
        auto values = s.parameters();
        for (std::size_t i = 0; i < values.size(); ++i)
            values[i] += 0.3 * std::sin(static_cast<double>(i));
        s.setParameters(values);
    }
    return l;
}
Lesson makeLesson() {
    return makeRoundedRectangle();
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
    text << " | structural dof: " << report.structuralDof;
    text << " | local dof: " << report.dof;
    if (l.width) {
        l.sketch.setConstraintValue(l.width, 105);
        report = solver.solve();
        text << " | width 105: " << statusName(report.status);
        l.sketch.setConstraintValue(l.width, 100);
        solver.solve();
    }
    l.sketch.removeConstraint(l.width);
    report = solver.solve();
    auto p = l.sketch.point(l.points[2]);
    report = solver.drag(l.points[2], p.x + 5, p.y + 3);
    solver.endDrag();
    text << " | drag: " << statusName(report.status) << " dof=" << report.dof;
    return text.str();
}
}
