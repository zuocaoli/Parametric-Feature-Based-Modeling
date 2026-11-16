module;
#include <Eigen/Dense>
module mini_gcs.numerics;
import std;
namespace gcs {
Matrix::Matrix(std::size_t r, std::size_t c) : rows(r), cols(c), values(r * c) {}
double &Matrix::operator()(std::size_t r, std::size_t c) {
    return values.at(r * cols + c);
}
double Matrix::operator()(std::size_t r, std::size_t c) const {
    return values.at(r * cols + c);
}
Matrix jacobian(const Sketch &sketch, double scale, double step, double geometryTolerance) {
    if (scale == 0)
        scale = sketch.characteristicLength();
    if (!std::isfinite(step) || step <= 0 || !std::isfinite(scale) || scale <= 0)
        throw std::invalid_argument("invalid finite-difference settings");
    const auto initial = sketch.parameters();
    auto baseline = evaluate(sketch, scale, geometryTolerance);
    Matrix j(baseline.rows.size(), initial.size());
    for (std::size_t col = 0; col < initial.size(); ++col) {
        double delta = step * std::max(scale, std::abs(initial[col]));
        using Probe = std::pair<Residual, double>;
        std::optional<Probe> forward, backward;
        auto sample = [&](double position) -> std::optional<Probe> {
            if (!std::isfinite(position) || position == initial[col])
                return std::nullopt;
            auto candidate = sketch;
            auto values = initial;
            values[col] = position;
            candidate.setParameters(std::move(values));
            try {
                return Probe{evaluate(candidate, scale, geometryTolerance), position};
            } catch (const std::domain_error &) {
                return std::nullopt;
            }
        };
        for (int attempt = 0; attempt < 32; ++attempt) {
            auto plus = sample(initial[col] + delta);
            auto minus = sample(initial[col] - delta);
            if (plus && minus) {
                forward = std::move(plus);
                backward = std::move(minus);
                break;
            }
            if (plus) {
                forward = std::move(plus);
                backward.reset();
            } else if (minus) {
                backward = std::move(minus);
                forward.reset();
            }
            delta *= 0.5;
        }
        if (!forward && !backward)
            throw std::domain_error("no valid finite-difference probe");
        const auto &fp = forward ? forward->first : baseline;
        const auto &fm = backward ? backward->first : baseline;
        double separation = ((forward ? forward->second : initial[col]) -
                             (backward ? backward->second : initial[col])) /
                            scale;
        if (!std::isfinite(separation) || separation <= 0)
            throw std::domain_error("invalid finite-difference separation");
        for (std::size_t row = 0; row < j.rows; ++row) {
            double difference = fp.rows[row].value - fm.rows[row].value;
            if (fp.rows[row].constraint &&
                sketch.constraint(fp.rows[row].constraint).type == ConstraintType::Angle)
                difference = std::remainder(difference, 2 * std::numbers::pi);
            j(row, col) = difference / separation;
            if (!std::isfinite(j(row, col)))
                throw std::domain_error("non-finite Jacobian entry");
        }
    }
    return j;
}
}
