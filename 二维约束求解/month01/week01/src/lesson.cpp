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
    return text.str();
}
}
