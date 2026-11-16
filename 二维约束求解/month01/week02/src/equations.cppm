export module mini_gcs.equations;
export import mini_gcs.model;
import std;
export namespace gcs {
struct EquationRow {
    double value = 0;
    ConstraintId constraint = 0;
    EntityId intrinsicEntity = 0;
    double tolerance = 1e-8;
    std::vector<std::size_t> dependencies;
};
struct Residual {
    std::vector<EquationRow> rows;
    double infinityNorm() const;
    double squaredNorm() const;
    bool satisfied(double solveTolerance) const;
};
Residual evaluate(const Sketch &sketch, double lengthScale = 0, double geometryTolerance = 1e-10);
}
