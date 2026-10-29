import std;
import month03.week03.engine;

namespace {
using namespace month03::week03;

void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string{message});
    }
}

void show(const RecomputeEngine &engine, ObjectId id) {
    std::cout << "width=" << std::get<double>(*engine.stored_value({id, "width"}))
              << ", area=" << std::get<double>(*engine.stored_value({id, "area"})) << '\n';
}

void report(const RecomputeEngine &engine) {
    std::cout << "order:";
    for (const auto &step : engine.last_report()) {
        std::cout << ' ' << key_text(step.target) << "(count=" << step.executions << ","
                  << step.message << ")";
    }
    std::cout << '\n';
}
} // namespace

int main(int argc, char **) {
    try {
        if (argc != 1) {
            throw std::invalid_argument("usage: demo");
        }
        RecomputeEngine engine;
        const auto id = engine.create_rectangle();
        require(engine.set({id, "segments"}, std::int64_t{2}) == SetResult::Changed,
                "set segments");
        require(engine.begin_batch().has_value(), "begin binding batch");
        require(engine.bind({id, "area"}, "length * width").has_value(), "bind area");
        require(engine.bind({id, "width"}, "length / segments").has_value(), "bind width");
        require(engine.end_batch().has_value(), "end binding batch");
        std::cout << "initial: ";
        show(engine, id);
        require(engine.set({id, "length"}, 200.0) == SetResult::Changed, "change length");
        report(engine);
        show(engine, id);
        require(engine.set({id, "name"}, std::string{"renamed"}) == SetResult::Changed,
                "change name");
        std::cout << "unrelated executions=" << engine.last_report().size() << '\n';
        require(engine.begin_batch().has_value(), "begin input batch");
        require(engine.set({id, "length"}, 400.0) == SetResult::Changed, "batch length");
        require(engine.set({id, "segments"}, std::int64_t{4}) == SetResult::Changed,
                "batch segments");
        std::cout << "during batch: ";
        show(engine, id);
        require(engine.end_batch().has_value(), "end input batch");
        report(engine);
        std::cout << "after batch: ";
        show(engine, id);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
