export module mini_gcs.model;
import std;

export namespace gcs {
using EntityId = unsigned;
using PointId = EntityId;
using ConstraintId = unsigned;
enum class EntityKind { Point, Line, Circle, Arc };
struct Vec2 {
    double x = 0, y = 0;
};
struct Entity {
    EntityId id = 0;
    EntityKind kind = EntityKind::Point;
    std::vector<EntityId> refs;
    std::vector<std::size_t> parameters;
    bool clockwise = false;
};
class Sketch {
  public:
    EntityId addPoint(double x, double y);
    EntityId addLine(PointId a, PointId b);
    EntityId addCircle(PointId center, double radius);
    EntityId addArc(PointId center, double radius, PointId start, PointId end,
                    bool clockwise = false);
    const Entity &entity(EntityId id) const;
    const std::vector<Entity> &entities() const;
    const std::vector<double> &parameters() const;
    void setParameters(std::vector<double> values);
    Vec2 point(PointId id) const;
    double radius(EntityId id) const;
    std::vector<double> getEntityParams(EntityId id) const;
    std::vector<std::size_t> parameterIndices(EntityId id) const;
    double characteristicLength() const;

  private:
    EntityId insert(EntityKind kind, std::vector<EntityId> refs, std::vector<double> values,
                    bool clockwise = false);
    std::vector<Entity> entities_;
    std::vector<double> parameters_;
};
}
