module mini_gcs.diagnostics;
import std;
namespace gcs {
StructuralAnalysis analyzeStructure(const Residual &residual, std::size_t count) {
    std::vector<int> parameterToRow(count, -1);
    std::function<bool(int, std::vector<bool> &)> augment = [&](int row, auto &seen) {
        for (auto p : residual.rows[row].dependencies) {
            if (p >= count)
                throw std::out_of_range("invalid equation dependency");
            if (seen[p])
                continue;
            seen[p] = true;
            if (parameterToRow[p] < 0 || augment(parameterToRow[p], seen)) {
                parameterToRow[p] = row;
                return true;
            }
        }
        return false;
    };
    StructuralAnalysis out;
    for (std::size_t r = 0; r < residual.rows.size(); ++r) {
        std::vector<bool> seen(count);
        if (augment(static_cast<int>(r), seen))
            ++out.matching;
    }
    out.dof = static_cast<int>(count) - out.matching;
    for (std::size_t i = 0; i < count; ++i)
        if (parameterToRow[i] < 0)
            out.unmatchedParameters.push_back(i);
    return out;
}
std::vector<ConstraintId> findRedundant(const Sketch &s, const Matrix &j, const Residual &f,
                                        double tolerance) {
    std::vector<std::size_t> accepted;
    for (std::size_t r = 0; r < f.rows.size(); ++r)
        if (f.rows[r].intrinsicEntity)
            accepted.push_back(r);
    auto rank = [&](const auto &rows) {
        Matrix subset(rows.size(), j.cols);
        for (std::size_t r = 0; r < rows.size(); ++r)
            for (std::size_t c = 0; c < j.cols; ++c)
                subset(r, c) = j(rows[r], c);
        return analyzeRank(subset, tolerance).rank;
    };
    int previousRank = rank(accepted);
    std::vector<ConstraintId> result;
    for (const auto &constraint : s.constraints()) {
        auto candidate = accepted;
        for (std::size_t r = 0; r < f.rows.size(); ++r)
            if (f.rows[r].constraint == constraint.id)
                candidate.push_back(r);
        int nextRank = rank(candidate);
        if (nextRank - previousRank < static_cast<int>(candidate.size() - accepted.size()))
            result.push_back(constraint.id);
        accepted = std::move(candidate);
        previousRank = nextRank;
    }
    return result;
}
std::vector<ConstraintId> confirmedDimensionConflict(const Sketch &s, double scale,
                                                     double tolerance) {
    auto canonical = [&](const Constraint &c) {
        auto refs = c.refs;
        if (c.type == ConstraintType::Distance && s.entity(refs[1]).kind == EntityKind::Point)
            std::ranges::sort(refs);
        return refs;
    };
    const auto &constraints = s.constraints();
    for (std::size_t i = 0; i < constraints.size(); ++i) {
        const auto &a = constraints[i];
        if (a.type != ConstraintType::Distance && a.type != ConstraintType::Radius &&
            a.type != ConstraintType::Angle && a.type != ConstraintType::Fixed)
            continue;
        for (std::size_t k = i + 1; k < constraints.size(); ++k) {
            const auto &b = constraints[k];
            if (a.type != b.type || canonical(a) != canonical(b))
                continue;
            double divisor = a.type == ConstraintType::Angle ? 1 : scale;
            double av = a.value, bv = b.value;
            if (a.type == ConstraintType::Distance &&
                s.entity(a.refs[1]).kind == EntityKind::Line) {
                av *= a.side;
                bv *= b.side;
            }
            double diff = a.type == ConstraintType::Angle
                              ? std::remainder(av - bv, 2 * std::numbers::pi)
                              : (av - bv) / divisor;
            double allowed = std::min(a.tolerance, tolerance) + std::min(b.tolerance, tolerance);
            bool contradiction = std::abs(diff) > allowed;
            if (a.type == ConstraintType::Fixed)
                contradiction |= std::abs(a.second - b.second) / scale > allowed;
            if (contradiction)
                return {a.id, b.id};
        }
    }
    return {};
}
}
