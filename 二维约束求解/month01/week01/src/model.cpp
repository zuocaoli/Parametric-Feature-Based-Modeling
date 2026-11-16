module mini_gcs.model;
import std;
namespace gcs {
namespace {
void finite(double value) {
    if (!std::isfinite(value))
        throw std::invalid_argument("non-finite input");
}
void kind(const Sketch &s, EntityId id, EntityKind expected) {
    if (s.entity(id).kind != expected)
        throw std::invalid_argument("unexpected entity kind");
}
bool circular(EntityKind k) {
    return k == EntityKind::Circle || k == EntityKind::Arc;
}
}
EntityId Sketch::insert(EntityKind k, std::vector<EntityId> refs, std::vector<double> values,
                        bool clockwise) {
    for (double v : values)
        finite(v);
    Entity e{static_cast<EntityId>(entities_.size() + 1), k, std::move(refs), {}, clockwise};
    for (double v : values) {
        e.parameters.push_back(parameters_.size());
        parameters_.push_back(v);
    }
    entities_.push_back(e);
    return e.id;
}
EntityId Sketch::addPoint(double x, double y) {
    return insert(EntityKind::Point, {}, {x, y});
}
EntityId Sketch::addLine(PointId a, PointId b) {
    kind(*this, a, EntityKind::Point);
    kind(*this, b, EntityKind::Point);
    if (a == b)
        throw std::invalid_argument("line endpoints must have different IDs");
    return insert(EntityKind::Line, {a, b}, {});
}
EntityId Sketch::addCircle(PointId c, double r) {
    kind(*this, c, EntityKind::Point);
    finite(r);
    if (r <= 0)
        throw std::invalid_argument("radius must be positive");
    return insert(EntityKind::Circle, {c}, {r});
}
EntityId Sketch::addArc(PointId c, double r, PointId a, PointId b, bool cw) {
    kind(*this, c, EntityKind::Point);
    kind(*this, a, EntityKind::Point);
    kind(*this, b, EntityKind::Point);
    finite(r);
    if (r <= 0 || a == b || a == c || b == c)
        throw std::invalid_argument("invalid arc");
    return insert(EntityKind::Arc, {c, a, b}, {r}, cw);
}
const Entity &Sketch::entity(EntityId id) const {
    if (id == 0 || id > entities_.size())
        throw std::out_of_range("unknown entity ID");
    return entities_[id - 1];
}
const std::vector<Entity> &Sketch::entities() const {
    return entities_;
}
const std::vector<double> &Sketch::parameters() const {
    return parameters_;
}
void Sketch::setParameters(std::vector<double> values) {
    if (values.size() != parameters_.size())
        throw std::invalid_argument("parameter count mismatch");
    for (double v : values)
        finite(v);
    parameters_ = std::move(values);
}
Vec2 Sketch::point(PointId id) const {
    kind(*this, id, EntityKind::Point);
    const auto &p = entity(id).parameters;
    return {parameters_[p[0]], parameters_[p[1]]};
}
double Sketch::radius(EntityId id) const {
    const auto &e = entity(id);
    if (!circular(e.kind))
        throw std::invalid_argument("expected circle or arc");
    return parameters_[e.parameters[0]];
}
std::vector<double> Sketch::getEntityParams(EntityId id) const {
    const auto &e = entity(id);
    if (e.kind == EntityKind::Point) {
        auto p = point(id);
        return {p.x, p.y};
    }
    std::vector<double> result;
    for (auto ref : e.refs) {
        auto p = point(ref);
        result.insert(result.end(), {p.x, p.y});
    }
    if (circular(e.kind))
        result.push_back(radius(id));
    return result;
}
std::vector<std::size_t> Sketch::parameterIndices(EntityId id) const {
    const auto &e = entity(id);
    auto result = e.parameters;
    for (auto ref : e.refs) {
        auto nested = parameterIndices(ref);
        result.insert(result.end(), nested.begin(), nested.end());
    }
    std::ranges::sort(result);
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}
double Sketch::characteristicLength() const {
    double xmin = 0, xmax = 0, ymin = 0, ymax = 0, r = 1;
    bool first = true;
    for (const auto &e : entities_) {
        if (e.kind == EntityKind::Point) {
            auto p = point(e.id);
            if (first) {
                xmin = xmax = p.x;
                ymin = ymax = p.y;
                first = false;
            }
            xmin = std::min(xmin, p.x);
            xmax = std::max(xmax, p.x);
            ymin = std::min(ymin, p.y);
            ymax = std::max(ymax, p.y);
        } else if (circular(e.kind)) {
            r = std::max(r, std::abs(radius(e.id)));
        }
    }
    return std::max({1.0, xmax - xmin, ymax - ymin, r});
}
}
