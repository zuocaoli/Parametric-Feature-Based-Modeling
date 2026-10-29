import std;
import month06.week04.storage;

namespace month06::week04 {
namespace {
using Clock = std::chrono::steady_clock;
double micros(Clock::time_point start) {
    return std::chrono::duration<double, std::micro>(Clock::now() - start).count();
}
double median(std::vector<double> values) {
    std::ranges::sort(values);
    return values[values.size() / 2];
}
} // namespace
void run_benchmarks() {
    std::cout << "topology,objects,formulas,edges,parse_us,incremental_us,recompute_us,full_"
                 "restore_us,command_us,notifications,executions,ast_parse_delta\n";
    for (bool chain : {false, true})
        for (const int size : {20, 100, 300}) {
            RecomputeEngine seed;
            for (int i = 0; i < size; ++i)
                seed.create_rectangle();
            seed.begin_batch().value();
            for (ObjectId id = 1; id <= static_cast<ObjectId>(size); ++id) {
                if (chain && id > 1)
                    seed.bind({id, "width"}, "source / 2 + length / 2",
                              {{"source", {id - 1, "width"}}})
                        .value();
                else
                    seed.bind({id, "width"}, "length / 2").value();
                seed.bind({id, "area"}, "length * width").value();
            }
            seed.end_batch().value();
            auto engine = RecomputeEngine::restore(seed.snapshot(), true).value();
            Document document;
            document.replace_model(RecomputeEngine::restore(seed.snapshot(), true).value()).value();
            std::size_t notifications = 0;
            auto connection = document.observe([&](const DocumentEvent &) { ++notifications; });
            std::vector<double> parse_times, incremental, recompute, full, command;
            std::size_t executed = 0, parse_delta = 0;
            for (int sample = 0; sample < 8; ++sample) {
                auto start = Clock::now();
                for (int i = 0; i < 100; ++i) {
                    auto parsed = parse("source / 2 + length / 2");
                    if (!parsed)
                        throw std::runtime_error("benchmark parse failed");
                }
                double parse_time = micros(start) / 100;
                start = Clock::now();
                engine->set({1, "length"}, double(200 + sample));
                double incremental_time = micros(start);
                double recompute_time = engine->recompute_nanoseconds() / 1000.0;
                executed = engine->last_report().size();
                start = Clock::now();
                auto reference = RecomputeEngine::restore(engine->snapshot(), true).value();
                double full_time = micros(start);
                for (ObjectId id = 1; id <= static_cast<ObjectId>(size); ++id)
                    if (engine->value({id, "area"}) != reference->value({id, "area"}))
                        throw std::runtime_error("benchmark correctness mismatch");
                const auto parses = document.model().ast_cache()->metrics().parses;
                start = Clock::now();
                auto result = document.execute(
                    std::make_unique<SetProperty>(PropertyKey{1, "length"}, double(200 + sample)));
                double command_time = micros(start);
                if (!result || !*result)
                    throw std::runtime_error("benchmark command failed");
                parse_delta += document.model().ast_cache()->metrics().parses - parses;
                if (sample) {
                    parse_times.push_back(parse_time);
                    incremental.push_back(incremental_time);
                    recompute.push_back(recompute_time);
                    full.push_back(full_time);
                    command.push_back(command_time);
                }
            }
            std::cout << (chain ? "chain" : "independent") << ',' << size << ',' << size * 2 << ','
                      << (chain ? size * 4 - 1 : size * 3) << ',' << median(parse_times) << ','
                      << median(incremental) << ',' << median(recompute) << ',' << median(full)
                      << ',' << median(command) << ',' << notifications << ',' << executed << ','
                      << parse_delta << '\n';
        }
}
} // namespace month06::week04
