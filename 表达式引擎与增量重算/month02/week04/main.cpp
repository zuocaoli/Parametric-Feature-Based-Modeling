import std;
import month03.week04.engine;

namespace {
using namespace month03::week04;

void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string{message});
    }
}

double number(const RecomputeEngine &engine, const PropertyKey &key) {
    return std::get<double>(*engine.stored_value(key));
}

void report(const RecomputeEngine &engine) {
    for (const auto &step : engine.last_report()) {
        std::cout << key_text(step.target) << ' ' << state_name(step.state)
                  << " count=" << step.executions << " (" << step.message << ")\n";
    }
}

void rectangle_example() {
    RecomputeEngine engine;
    const auto id = engine.create_rectangle();
    require(engine.begin_batch().has_value(), "begin bindings");
    require(engine.bind({id, "area"}, "length * width").has_value(), "bind area");
    require(engine.bind({id, "width"}, "length / 2").has_value(), "bind width");
    require(engine.end_batch().has_value(), "end bindings");
    std::cout << "rectangle: width=" << number(engine, {id, "width"})
              << ", area=" << number(engine, {id, "area"}) << '\n';
    require(engine.set({id, "length"}, 200.0) == SetResult::Changed, "change length");
    report(engine);
    std::cout << "width=" << number(engine, {id, "width"})
              << ", area=" << number(engine, {id, "area"}) << '\n';
    require(engine.set({id, "name"}, std::string{"renamed"}) == SetResult::Changed, "change name");
    std::cout << "name change executions=" << engine.last_report().size() << '\n';
    const auto rejected = engine.bind({id, "length"}, "width * 2");
    require(!rejected, "cycle accepted");
    std::cout << "rejected cycle:";
    for (const auto &key : rejected.error().cycle) {
        std::cout << ' ' << key_text(key);
    }
    std::cout << '\n';
}

void failure_example() {
    RecomputeEngine engine;
    const auto a = engine.create_rectangle(), b = engine.create_rectangle();
    require(engine.set({a, "segments"}, std::int64_t{2}) == SetResult::Changed, "prepare divisor");
    require(engine.begin_batch().has_value(), "begin failure example");
    require(engine.bind({a, "width"}, "length / (segments - 1)").has_value(), "bind division");
    require(engine.bind({a, "area"}, "length * width").has_value(), "bind downstream");
    require(engine.bind({b, "width"}, "source_segments + 1", {{"source_segments", {a, "segments"}}})
                .has_value(),
            "bind independent branch");
    require(engine.end_batch().has_value(), "end failure example");
    require(engine.set({a, "segments"}, std::int64_t{1}) == SetResult::Changed,
            "cause zero divisor");
    std::cout << "failed round:\n";
    report(engine);
    std::cout << "cached: width=" << number(engine, {a, "width"})
              << ", area=" << number(engine, {a, "area"})
              << ", independent width=" << number(engine, {b, "width"}) << '\n';
    std::cout << std::boolalpha << "valid reads: width=" << engine.value({a, "width"}).has_value()
              << ", area=" << engine.value({a, "area"}).has_value() << '\n';
    require(engine.set({a, "segments"}, std::int64_t{3}) == SetResult::Changed, "repair divisor");
    std::cout << "repaired round:\n";
    report(engine);
    std::cout << "repaired: width=" << number(engine, {a, "width"})
              << ", area=" << number(engine, {a, "area"})
              << ", independent width=" << number(engine, {b, "width"}) << '\n';
    require(engine.bind({b, "area"}, "length * width").has_value(), "add deletion downstream");
    require(engine.erase(a).value(), "delete source");
    std::cout << "deleted source:\n";
    report(engine);
    const auto replacement = engine.create_rectangle();
    require(engine.set({replacement, "segments"}, std::int64_t{3}) == SetResult::Changed,
            "replacement input");
    std::cout << "replacement id=" << replacement
              << ", target state=" << state_name(engine.status({b, "width"}).state) << '\n';
    require(engine
                .bind({b, "width"}, "source_segments + 1",
                      {{"source_segments", {replacement, "segments"}}})
                .has_value(),
            "repair reference");
    std::cout << "rebound round:\n";
    report(engine);
    std::cout << "target width=" << number(engine, {b, "width"})
              << ", area=" << number(engine, {b, "area"}) << '\n';
}
} // namespace

int main(int argc, char **) {
    try {
        if (argc != 1) {
            throw std::invalid_argument("usage: demo");
        }
        rectangle_example();
        failure_example();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
