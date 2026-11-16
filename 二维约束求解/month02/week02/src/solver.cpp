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
}
