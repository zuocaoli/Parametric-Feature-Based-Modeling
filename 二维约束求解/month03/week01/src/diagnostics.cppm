export module mini_gcs.diagnostics;
export import mini_gcs.equations;
export import mini_gcs.numerics;
import std;
export namespace gcs {
struct StructuralAnalysis {
    int matching = 0;
    int dof = 0;
    std::vector<std::size_t> unmatchedParameters;
};
StructuralAnalysis analyzeStructure(const Residual &residual, std::size_t parameterCount);
std::vector<ConstraintId> findRedundant(const Sketch &sketch, const Matrix &jacobian,
                                        const Residual &residual, double rankTolerance = 1e-8);
std::vector<ConstraintId> confirmedDimensionConflict(const Sketch &sketch, double lengthScale,
                                                     double solveTolerance = 1e-8);
}
