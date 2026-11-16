export module mini_gcs.solver;
export import mini_gcs.numerics;
import std;
export namespace gcs {
enum class SolveStatus { Success, UnderConstrained, Redundant, Conflicting, Failed };
struct SolveOptions {
    int maxIterations = 50;
    double solveTolerance = 1e-8;
    double geometryTolerance = 1e-10;
    double differenceStep = 1e-7;
    double rankTolerance = 1e-8;
};
struct SolveReport {
    SolveStatus status = SolveStatus::Failed;
    int dof = -1, structuralDof = -1, rank = -1, iterations = 0;
    double finalResidual = std::numeric_limits<double>::infinity();
    std::vector<ConstraintId> redundant, conflicting;
    std::vector<std::size_t> movableParameters;
    std::string reason;
    bool converged() const;
};
std::string_view statusName(SolveStatus status);
class Solver {
  public:
    explicit Solver(Sketch &sketch, SolveOptions options = {});
    SolveReport solve();

  private:
    Sketch &sketch_;
    SolveOptions options_;
    std::vector<double> successful_;
};
}
