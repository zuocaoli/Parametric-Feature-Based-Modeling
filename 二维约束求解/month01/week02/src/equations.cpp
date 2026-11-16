module mini_gcs.equations;
import std;
namespace gcs {
namespace {
Vec2 sub(Vec2 a, Vec2 b) {
    return {a.x - b.x, a.y - b.y};
}
double dot(Vec2 a, Vec2 b) {
    return a.x * b.x + a.y * b.y;
}
double cross(Vec2 a, Vec2 b) {
    return a.x * b.y - a.y * b.x;
}
double length(Vec2 a) {
    return std::hypot(a.x, a.y);
}
}
double Residual::infinityNorm() const {
    double result = 0;
    for (const auto &r : rows)
        result = std::max(result, std::abs(r.value));
    return result;
}
double Residual::squaredNorm() const {
    double result = 0;
    for (const auto &r : rows)
        result += r.value * r.value;
    return result;
}
bool Residual::satisfied(double tolerance) const {
    return std::ranges::all_of(rows, [=](const auto &r) {
        return std::isfinite(r.value) && std::abs(r.value) <= std::min(tolerance, r.tolerance);
    });
}
Residual evaluate(const Sketch &s, double scale, double geometricTolerance) {
    if (scale == 0)
        scale = s.characteristicLength();
    if (!std::isfinite(scale) || scale <= 0 || !std::isfinite(geometricTolerance) ||
        geometricTolerance <= 0)
        throw std::invalid_argument("invalid evaluation scale or geometry tolerance");
    auto direction = [&](EntityId id) {
        const auto &e = s.entity(id);
        auto d = sub(s.point(e.refs[1]), s.point(e.refs[0]));
        if (length(d) <= geometricTolerance)
            throw std::domain_error("zero-length line");
        return d;
    };
    auto distanceToLine = [&](Vec2 p, EntityId line) {
        auto d = direction(line);
        return cross(d, sub(p, s.point(s.entity(line).refs[0]))) / length(d);
    };
    Residual out;
    for (const auto &e : s.entities()) {
        if (e.kind == EntityKind::Line)
            direction(e.id);
        if ((e.kind == EntityKind::Circle || e.kind == EntityKind::Arc) &&
            s.radius(e.id) <= geometricTolerance)
            throw std::domain_error("nonpositive or degenerate radius");
        if (e.kind == EntityKind::Arc) {
            auto center = s.point(e.refs[0]);
            for (std::size_t i = 1; i <= 2; ++i) {
                auto dep = s.parameterIndices(e.refs[i]);
                auto centerDep = s.parameterIndices(e.refs[0]);
                dep.insert(dep.end(), centerDep.begin(), centerDep.end());
                dep.insert(dep.end(), e.parameters.begin(), e.parameters.end());
                out.rows.push_back(
                    {(length(sub(s.point(e.refs[i]), center)) - s.radius(e.id)) / scale, 0, e.id,
                     1e-8, std::move(dep)});
            }
        }
    }
    for (const auto &c : s.constraints()) {
        std::vector<std::size_t> deps;
        for (auto ref : c.refs) {
            auto indices = s.parameterIndices(ref);
            deps.insert(deps.end(), indices.begin(), indices.end());
        }
        std::ranges::sort(deps);
        deps.erase(std::unique(deps.begin(), deps.end()), deps.end());
        auto add = [&](double value) {
            if (!std::isfinite(value))
                throw std::domain_error("non-finite residual");
            out.rows.push_back({value, c.id, 0, c.tolerance, deps});
        };
        auto a = c.refs[0];
        auto b = c.refs.size() > 1 ? c.refs[1] : 0;
        switch (c.type) {
        case ConstraintType::Coincident: {
            auto d = sub(s.point(a), s.point(b));
            add(d.x / scale);
            add(d.y / scale);
            break;
        }
        case ConstraintType::Fixed: {
            auto p = s.point(a);
            add((p.x - c.value) / scale);
            add((p.y - c.second) / scale);
            break;
        }
        case ConstraintType::Horizontal:
            add(direction(a).y / scale);
            break;
        case ConstraintType::Vertical:
            add(direction(a).x / scale);
            break;
        case ConstraintType::Radius:
            add((s.radius(a) - c.value) / scale);
            break;
        case ConstraintType::Distance:
            if (s.entity(b).kind == EntityKind::Point)
                add((length(sub(s.point(a), s.point(b))) - c.value) / scale);
            else
                add((c.side * distanceToLine(s.point(a), b) - c.value) / scale);
            break;
        case ConstraintType::PointOnObject:
            if (s.entity(b).kind == EntityKind::Line)
                add(distanceToLine(s.point(a), b) / scale);
            else
                add((length(sub(s.point(a), s.point(s.entity(b).refs[0]))) - s.radius(b)) / scale);
            break;
        case ConstraintType::Parallel:
        case ConstraintType::Perpendicular:
        case ConstraintType::Angle: {
            auto u = direction(a), v = direction(b);
            auto norm = length(u) * length(v);
            if (c.type == ConstraintType::Parallel)
                add(cross(u, v) / norm);
            else if (c.type == ConstraintType::Perpendicular)
                add(dot(u, v) / norm);
            else
                add(std::remainder(std::atan2(cross(u, v), dot(u, v)) - c.value,
                                   2 * std::numbers::pi));
            break;
        }
        case ConstraintType::Tangent: {
            if (s.entity(a).kind == EntityKind::Line) {
                const auto &circle = s.entity(b);
                EntityId endpoint = 0;
                if (circle.kind == EntityKind::Arc)
                    for (auto p : s.entity(a).refs)
                        if (p == circle.refs[1] || p == circle.refs[2])
                            endpoint = p;
                if (endpoint) {
                    auto radial = sub(s.point(endpoint), s.point(circle.refs[0]));
                    auto d = direction(a);
                    add(dot(radial, d) / (s.radius(b) * length(d)));
                } else
                    add((c.side * distanceToLine(s.point(circle.refs[0]), a) - s.radius(b)) /
                        scale);
            } else {
                double d = length(sub(s.point(s.entity(a).refs[0]), s.point(s.entity(b).refs[0])));
                if (d <= geometricTolerance)
                    throw std::domain_error("concentric tangent circles");
                double target = c.branch == TangencyBranch::External
                                    ? s.radius(a) + s.radius(b)
                                    : std::abs(s.radius(a) - s.radius(b));
                add((d - target) / scale);
            }
            break;
        }
        }
    }
    return out;
}
}
