import std;
import mini_gcs.lesson;
namespace {
using namespace gcs;
void check(bool condition, std::string_view message) {
    if (!condition)
        throw std::runtime_error(std::string(message));
}
void near(double actual, double expected, double tolerance = 1e-6) {
    if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance)
        throw std::runtime_error("expected " + std::to_string(expected) + ", got " +
                                 std::to_string(actual));
}
template <class F> void rejects(F operation) {
    bool rejected = false;
    try {
        operation();
    } catch (const std::exception &) {
        rejected = true;
    }
    check(rejected, "invalid input must be rejected");
}
void modelTests() {
    Sketch s;
    auto a = s.addPoint(0, 0), b = s.addPoint(10, 0), center = s.addPoint(0, 10);
    auto line = s.addLine(a, b);
    auto circle = s.addCircle(center, 10);
    auto arc = s.addArc(center, 10, a, s.addPoint(10, 10));
    check(s.parameters().size() == 10, "shared points do not duplicate parameters");
    check(s.entity(line).parameters.empty(), "line owns no parameters");
    auto parameters = s.parameters();
    parameters[0] = 2;
    s.setParameters(parameters);
    near(s.getEntityParams(line)[0], 2);
    near(s.getEntityParams(arc)[2], 2);
    near(s.radius(circle), 10);
    check(s.parameterIndices(arc).size() == 7, "arc parameter dependency count");
    rejects([&] { s.entity(0); });
    rejects([&] { s.addLine(a, 999); });
    rejects([&] { s.addCircle(a, -1); });
    rejects([&] { s.addArc(a, 1, b, b); });
    rejects([&] { s.addPoint(std::numeric_limits<double>::quiet_NaN(), 0); });
    rejects([&] { s.setParameters({1}); });
}
}
int main(int argc, char **argv) {
    const std::string group = argc > 1 ? argv[1] : "model";
    try {
        if (group == "model")
            modelTests();
        else
            throw std::invalid_argument("unknown test group");
        std::cout << group << " checks passed\n";
        return 0;
    } catch (const std::exception &failure) {
        std::cerr << group << ": " << failure.what() << '\n';
        return 1;
    }
}
