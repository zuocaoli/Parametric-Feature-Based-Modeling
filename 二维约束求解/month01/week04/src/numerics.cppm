export module mini_gcs.numerics;
export import mini_gcs.equations;
import std;
export namespace gcs {
struct Matrix {
    std::size_t rows = 0, cols = 0;
    std::vector<double> values;
    Matrix() = default;
    Matrix(std::size_t r, std::size_t c);
    double &operator()(std::size_t r, std::size_t c);
    double operator()(std::size_t r, std::size_t c) const;
};
Matrix jacobian(const Sketch &sketch, double lengthScale = 0, double differenceStep = 1e-7,
                double geometryTolerance = 1e-10);
std::vector<double> leastSquares(const Matrix &matrix, const std::vector<double> &rhs,
                                 double rankTolerance = 1e-8);
}
