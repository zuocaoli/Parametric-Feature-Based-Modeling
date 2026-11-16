import std;
import month03.week03.engine;

namespace {
using namespace month03::week03;

// 打印指定矩形当前保存的 width 和 area 值
void show(const RecomputeEngine &engine, ObjectId id) {
    std::cout << "width=" << std::get<double>(*engine.stored_value({id, "width"})) << ", area=" << std::get<double>(*engine.stored_value({id, "area"})) << '\n';
}

// 打印引擎记录的重算执行顺序、执行次数和结果信息
void report(const RecomputeEngine &engine) {
    std::cout << "order:";
    for (const auto &step : engine.last_report()) {
        std::cout << ' ' << key_text(step.target) << "(count=" << step.executions << "," << step.message << ")";
    }
    std::cout << '\n';
}
} // namespace

int main(int argc, char **) {
    try {

        // 本演示不接收参数；拒绝额外参数，避免把未使用的输入误认为已生效.
        if (argc != 1) {
            throw std::invalid_argument("usage: demo");
        }

        RecomputeEngine engine;
        const auto id = engine.create_rectangle(); // 里面注册了属性更新回调，回调处理重算相关的的事情。
        // 给属性segments设置初始值, 会触发一次重算。
        // segments 是矩形对象的分段数属性，本例用它控制宽度，width = length / segments。
        const auto segments_result = engine.set({id, "segments"}, std::int64_t{2});
        if (segments_result != SetResult::Changed) {
            throw std::runtime_error("set segments: " + std::string{result_name(segments_result)});
        }

        //
        // =====================================================================================
        //

        // 开启一批次的属性更新
        const auto binding_batch = engine.begin_batch();
        if (!binding_batch) {
            throw std::runtime_error("begin binding batch: " + binding_batch.error().message);
        }

        // 绑定公式并标记待重算，批次结束后执行
        const auto area_binding = engine.bind({id, "area"}, "length * width");
        if (!area_binding) {
            throw std::runtime_error("bind area: " + area_binding.error().message);
        }

        // 绑定公式并标记待重算，批次结束后执行
        const auto width_binding = engine.bind({id, "width"}, "length / segments");
        if (!width_binding) {
            throw std::runtime_error("bind width: " + width_binding.error().message);
        }

        //
        // 上面的 bind 中也调用了 flush_if_ready()，但因为处于批次中，所以不会执行重算。
        //

        // 结束批次，触发重算
        const auto binding_end = engine.end_batch();
        if (!binding_end) {
            throw std::runtime_error("end binding batch: " + binding_end.error().message);
        }

        std::cout << "initial: ";
        show(engine, id); // 打印id当前的 width 和 area 值

        std::cout << "====================================================" << '\n';
        std::cout << '\n' << '\n' << '\n' << '\n';

        //
        //
        // =====================================================================================
        //
        //

        //
        // 把矩形的长度改为 200.0，演示输入属性变化后，依赖它的公式会自动重算。
        const auto length_result = engine.set({id, "length"}, 200.0);
        if (length_result != SetResult::Changed) {
            throw std::runtime_error("change length: " + std::string{result_name(length_result)});
        }
        report(engine);   // 计算顺序，先计算 width，再计算 area。
        show(engine, id); // 打印id当前的 width 和 area 值

        std::cout << "====================================================" << '\n';
        std::cout << '\n' << '\n' << '\n' << '\n';

        //
        // 修改矩形的名称，验证无关属性变化不会触发公式重算
        const auto name_result = engine.set({id, "name"}, std::string{"renamed"});
        if (name_result != SetResult::Changed) {
            throw std::runtime_error("change name: " + std::string{result_name(name_result)});
        }
        std::cout << "unrelated executions=" << engine.last_report().size() << '\n';

        std::cout << "====================================================" << '\n';
        std::cout << '\n' << '\n' << '\n' << '\n';

        //
        // 批量修改多个输入，把它们引起的公式重算合并到批次结束时执行
        //
        const auto input_batch = engine.begin_batch();
        if (!input_batch) {
            throw std::runtime_error("begin input batch: " + input_batch.error().message);
        }

        // 修改属性，但是批次没有结束，不会触发重算，width 和 area 仍然是旧值。
        const auto batched_length_result = engine.set({id, "length"}, 400.0);
        if (batched_length_result != SetResult::Changed) {
            throw std::runtime_error("batch length: " + std::string{result_name(batched_length_result)});
        }

        const auto batched_segments_result = engine.set({id, "segments"}, std::int64_t{4});
        if (batched_segments_result != SetResult::Changed) {
            throw std::runtime_error("batch segments: " + std::string{result_name(batched_segments_result)});
        }
        std::cout << "during batch: ";
        show(engine, id); // 打印id当前的 width 和 area 值，仍然是旧值。

        // 批次结束，触发重算，width 和 area 更新为新值。
        const auto input_end = engine.end_batch();
        if (!input_end) {
            throw std::runtime_error("end input batch: " + input_end.error().message);
        }

        report(engine);
        std::cout << "after batch: ";
        show(engine, id); // 打印id当前的 width 和 area 值，已经更新为新值。

    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
