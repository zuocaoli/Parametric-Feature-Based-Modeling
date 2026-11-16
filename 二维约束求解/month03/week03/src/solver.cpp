module mini_gcs.solver;
import std;
namespace gcs {
bool SolveReport::converged() const {
    return status == SolveStatus::Success || status == SolveStatus::UnderConstrained ||
           status == SolveStatus::Redundant;
}
std::string_view statusName(SolveStatus status) {
    switch (status) {
    case SolveStatus::Success:
        return "Success";
    case SolveStatus::UnderConstrained:
        return "UnderConstrained";
    case SolveStatus::Redundant:
        return "Redundant";
    case SolveStatus::Conflicting:
        return "Conflicting";
    case SolveStatus::Failed:
        return "Failed";
    }
    return "Failed";
}
namespace {
SolveReport newton(Sketch &s, const SolveOptions &o, double scale) {
    SolveReport out;
    for (int i = 0; i <= o.maxIterations; ++i) {
        auto f = evaluate(s, scale, o.geometryTolerance);
        out.finalResidual = f.infinityNorm();
        out.iterations = i;
        if (f.satisfied(o.solveTolerance)) {
            out.status = SolveStatus::Success;
            return out;
        }
        if (i == o.maxIterations) {
            out.reason = "iteration limit";
            break;
        }
        auto j = jacobian(s, scale, o.differenceStep, o.geometryTolerance);
        std::vector<double> rhs;
        for (const auto &r : f.rows)
            rhs.push_back(-r.value);
        auto step = leastSquares(j, rhs, o.rankTolerance);
        auto x = s.parameters();
        bool accepted = false;
        for (double alpha = 1; alpha >= 1.0 / 4096; alpha *= 0.5) {
            auto candidate = s;
            auto trial = x;
            for (std::size_t p = 0; p < trial.size(); ++p)
                trial[p] += alpha * scale * step[p];
            try {
                candidate.setParameters(trial);
                auto next = evaluate(candidate, scale, o.geometryTolerance);
                if (next.satisfied(o.solveTolerance) ||
                    next.squaredNorm() < f.squaredNorm() * (1 - 1e-4 * alpha)) {
                    s.setParameters(std::move(trial));
                    accepted = true;
                    break;
                }
            } catch (const std::exception &) {
            }
        }
        if (!accepted) {
            out.reason = "line search stalled";
            break;
        }
    }
    return out;
}
void validateOptions(const SolveOptions &o) {
    if (o.maxIterations < 0 || o.maxIterations > 1000)
        throw std::invalid_argument("iteration limit must be in [0,1000]");
    for (double value : {o.solveTolerance, o.geometryTolerance, o.differenceStep, o.rankTolerance})
        if (!std::isfinite(value) || value <= 0)
            throw std::invalid_argument("invalid solver tolerance");
}
}
Solver::Solver(Sketch &sketch, SolveOptions options) : sketch_(sketch), options_(options) {}
SolveReport Solver::solve() {
    auto original = sketch_.parameters();
    if (successful_.size() == original.size())
        sketch_.setParameters(successful_);
    auto rollback = sketch_.parameters();
    SolveReport out;
    try {
        validateOptions(options_);
        double scale = sketch_.characteristicLength();
        auto initial = evaluate(sketch_, scale, options_.geometryTolerance);
        out.structuralDof = analyzeStructure(initial, sketch_.parameters().size()).dof;
        out.conflicting = confirmedDimensionConflict(sketch_, scale, options_.solveTolerance);
        if (!out.conflicting.empty()) {
            out.status = SolveStatus::Conflicting;
            out.reason = "incompatible targets on the same dimensional subject";
            out.finalResidual = initial.infinityNorm();
            return out;
        }
        auto iteration = newton(sketch_, options_, scale);
        out.status = iteration.status;
        out.reason = iteration.reason;
        out.iterations = iteration.iterations;
        out.finalResidual = iteration.finalResidual;
        if (out.converged()) {
            auto j = jacobian(sketch_, scale, options_.differenceStep, options_.geometryTolerance);
            auto numerical = analyzeRank(j, options_.rankTolerance);
            out.rank = numerical.rank;
            out.dof = static_cast<int>(sketch_.parameters().size()) - out.rank;
            out.movableParameters = numerical.movableParameters;
            if (out.dof > 0)
                out.status = SolveStatus::UnderConstrained;
            out.redundant =
                findRedundant(sketch_, j, evaluate(sketch_, scale, options_.geometryTolerance),
                              options_.rankTolerance);
            if (!out.redundant.empty())
                out.status = SolveStatus::Redundant;
            successful_ = sketch_.parameters();
        } else
            sketch_.setParameters(rollback);
    } catch (const std::exception &error) {
        sketch_.setParameters(rollback);
        out.status = SolveStatus::Failed;
        out.reason = error.what();
    }
    return out;
}
SolveReport Solver::drag(PointId point, double x, double y) {
    auto report = solve();
    if (!report.converged())
        return report;
    const auto initial = sketch_.parameters();
    try {
        if (!std::isfinite(x) || !std::isfinite(y))
            throw std::invalid_argument("invalid drag target");
        auto p = sketch_.entity(point);
        if (p.kind != EntityKind::Point)
            throw std::invalid_argument("drag expects a point");
        if (report.dof == 0) {
            report.reason = "fully constrained: drag has no effect";
            return report;
        }
        double scale = sketch_.characteristicLength();
        auto error = [&](const Sketch &s) {
            auto current = s.point(point);
            return std::hypot(current.x - x, current.y - y) / scale;
        };
        bool progressed = false;
        for (int iteration = 0; iteration < 20; ++iteration) {
            auto rank = analyzeRank(
                jacobian(sketch_, scale, options_.differenceStep, options_.geometryTolerance),
                options_.rankTolerance);
            auto &n = rank.nullspace;
            if (!n.cols || error(sketch_) < options_.solveTolerance)
                break;
            Matrix target(2, n.cols);
            for (std::size_t c = 0; c < n.cols; ++c) {
                target(0, c) = n(p.parameters[0], c);
                target(1, c) = n(p.parameters[1], c);
            }
            auto current = sketch_.point(point);
            auto weights = leastSquares(target, {(x - current.x) / scale, (y - current.y) / scale},
                                        options_.rankTolerance);
            std::vector<double> delta(n.rows, 0);
            for (std::size_t r = 0; r < n.rows; ++r)
                for (std::size_t c = 0; c < n.cols; ++c)
                    delta[r] += n(r, c) * weights[c];
            double maximum = 0;
            for (double d : delta)
                maximum = std::max(maximum, std::abs(d));
            if (maximum < 1e-10)
                break;
            double limiter = std::min(1.0, 0.15 / maximum);
            bool accepted = false;
            for (double alpha = limiter; alpha >= limiter / 256; alpha *= 0.5) {
                auto candidate = sketch_;
                auto values = candidate.parameters();
                for (std::size_t i = 0; i < values.size(); ++i)
                    values[i] += alpha * scale * delta[i];
                try {
                    candidate.setParameters(values);
                    auto projected = newton(candidate, options_, scale);
                    if (projected.converged() && error(candidate) < error(sketch_) - 1e-12) {
                        sketch_.setParameters(candidate.parameters());
                        accepted = progressed = true;
                        break;
                    }
                } catch (const std::exception &) {
                }
            }
            if (!accepted)
                break;
        }
        successful_ = sketch_.parameters();
        report = solve();
        if (!progressed)
            report.reason = "target has no locally reachable improvement";
        return report;
    } catch (const std::exception &failure) {
        sketch_.setParameters(initial);
        successful_ = initial;
        report.status = SolveStatus::Failed;
        report.reason = failure.what();
        return report;
    }
}
void Solver::endDrag() {
    successful_ = sketch_.parameters();
}
}
