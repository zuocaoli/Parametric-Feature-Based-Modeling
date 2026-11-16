export module mini_gcs.lesson;
export import mini_gcs.model;
import std;
export namespace gcs {
struct Lesson {
    Sketch sketch;
    std::vector<PointId> points;
    std::vector<EntityId> lines, circles, arcs;
};
Lesson makeRectangle(bool perturbed = true);
Lesson makeLesson();
std::string runLesson(Lesson &lesson);
}
