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
ConstraintId Sketch::addConstraint(ConstraintType type, std::vector<EntityId> refs, double value,
                                   double second, TangencyBranch branch, double tolerance) {
    finite(value);
    finite(second);
    finite(tolerance);
    if (tolerance <= 0)
        throw std::invalid_argument("tolerance must be positive");
    auto is = [&](std::size_t i, EntityKind k) {
        return i < refs.size() && entity(refs[i]).kind == k;
    };
    for (auto ref : refs)
        entity(ref);
    auto circ = [&](std::size_t i) { return i < refs.size() && circular(entity(refs[i]).kind); };
    bool ok = false;
    switch (type) {
    case ConstraintType::Fixed:
        ok = refs.size() == 1 && is(0, EntityKind::Point);
        break;
    case ConstraintType::Horizontal:
    case ConstraintType::Vertical:
        ok = refs.size() == 1 && is(0, EntityKind::Line);
        break;
    case ConstraintType::Radius:
        ok = refs.size() == 1 && circ(0) && value > 0;
        break;
    case ConstraintType::Coincident:
        ok = refs.size() == 2 && is(0, EntityKind::Point) && is(1, EntityKind::Point);
        break;
    case ConstraintType::PointOnObject:
        ok = refs.size() == 2 && is(0, EntityKind::Point) && (is(1, EntityKind::Line) || circ(1));
        break;
    case ConstraintType::Distance:
        ok = refs.size() == 2 && value >= 0 && is(0, EntityKind::Point) &&
             (is(1, EntityKind::Point) || is(1, EntityKind::Line));
        break;
    case ConstraintType::Parallel:
    case ConstraintType::Perpendicular:
    case ConstraintType::Angle:
        ok = refs.size() == 2 && is(0, EntityKind::Line) && is(1, EntityKind::Line);
        break;
    case ConstraintType::Tangent:
        ok = refs.size() == 2 && ((is(0, EntityKind::Line) && circ(1)) || (circ(0) && circ(1)));
        break;
    }
    if (!ok)
        throw std::invalid_argument("invalid constraint references or target");
    int side = 1;
    if ((type == ConstraintType::Distance && is(1, EntityKind::Line)) ||
        (type == ConstraintType::Tangent && is(0, EntityKind::Line))) {
        auto line = entity(refs[type == ConstraintType::Distance ? 1 : 0]);
        auto a = point(line.refs[0]), b = point(line.refs[1]);
        auto p = type == ConstraintType::Distance ? point(refs[0]) : point(entity(refs[1]).refs[0]);
        side = ((b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x)) < 0 ? -1 : 1;
    }
    constraints_.push_back(
        {nextConstraint_++, type, std::move(refs), value, second, tolerance, branch, side});
    return constraints_.back().id;
}
void Sketch::removeConstraint(ConstraintId id) {
    constraint(id);
    std::erase_if(constraints_, [=](const auto &c) { return c.id == id; });
}
const Constraint &Sketch::constraint(ConstraintId id) const {
    auto it = std::ranges::find(constraints_, id, &Constraint::id);
    if (it == constraints_.end())
        throw std::out_of_range("unknown constraint ID");
    return *it;
}
const std::vector<Constraint> &Sketch::constraints() const {
    return constraints_;
}
void Sketch::setConstraintValue(ConstraintId id, double value) {
    finite(value);
    const auto &c = constraint(id);
    if (c.type != ConstraintType::Distance && c.type != ConstraintType::Angle &&
        c.type != ConstraintType::Radius)
        throw std::invalid_argument("only dimensional targets are editable");
    if ((c.type == ConstraintType::Radius && value <= 0) ||
        (c.type == ConstraintType::Distance && value < 0))
        throw std::invalid_argument("invalid dimension target");
    for (auto &entry : constraints_)
        if (entry.id == id)
            entry.value = value;
}
}
