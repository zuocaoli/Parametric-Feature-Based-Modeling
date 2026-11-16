export module mini_gcs.lesson;
export import mini_gcs.model;
export import mini_gcs.equations;
import std;
export namespace gcs {
struct Lesson {
    Sketch sketch;
    std::vector<PointId> points;
    std::vector<EntityId> lines, circles, arcs;
    ConstraintId width = 0, height = 0;
};
Lesson makeRectangle(bool perturbed = true);
Lesson makeLesson();
std::string runLesson(Lesson &lesson);
}
