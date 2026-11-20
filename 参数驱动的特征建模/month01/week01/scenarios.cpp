module course_scenarios;

import std;
import feature_model;

namespace cad {
namespace {
constexpr RectangleProfile plateProfile{0, 0, 100, 60};

void check(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

void near(double actual, double expected, double tolerance, std::string_view description) {
    if (!(std::abs(actual - expected) <= tolerance)) {
        throw std::runtime_error(std::string(description) + ": expected " + std::to_string(expected) + ", got " + std::to_string(actual));
    }
}

const Geometry &result_of(const FeatureModel &model, FeatureId id) {
    const auto *result = model.result(id);
    check(result != nullptr, "expected current geometry");
    return *result;
}

void show(std::ostream &out, const FeatureModel &model) {
    for (const auto &step : model.last_report()) {
        out << "feature " << step.id.value << ' ' << state_name(step.state) << " count=" << step.executions << " (" << step.message << ")\n";
    }
}

void check_step(const FeatureModel &model, std::size_t index, FeatureId id, FeatureState state, std::size_t executions) {
    const auto &report = model.last_report();
    check(index < report.size(), "missing report step");
    const auto &step = report[index];
    check(step.id == id && step.state == state && step.executions == executions, "unexpected report step: " + step.message);
}

//
// single_extrusion 是 "single-extrusion" 用例的执行函数。它演示的是：修改参数 t 后，依赖它的拉伸特征会重新计算。
//
// 1. 用 100 × 60 mm 的矩形创建拉伸特征，将拉伸高度绑定到模型参数 height，然后调用 model.rebuild()。
// 2. 检查初始高度为 20 mm，体积为 120000 mm³。
// 3. 调用 model.set_parameters({{"t", 15.0}})，把 t 从 10 mm 改为 15 mm。这次参数更新触发重算，随后检查高度变为 30 mm，体积变为 180000 mm³。
// 4. 把参数变化、几何结果和重算报告写入 out。
//
void single_extrusion(std::ostream &out) {

    FeatureModel model;

    // 在 model 里创建一个拉伸特征，并让它的拉伸高度由参数 height 驱动
    const auto plate = model.add_extrusion(plateProfile, model.parameter("height"));

    model.rebuild();

    near(bounds(result_of(model, plate))[5], 20, 1e-6, "initial height");

    near(volume(result_of(model, plate)), 120000, 1e-4, "initial volume");

    check_step(model, 0, plate, FeatureState::Valid, 1);

    check(model.set_parameters({{"t", 15.0}}) == SetResult::Changed, "update t");

    const auto &geometry = result_of(model, plate);

    const auto height = bounds(geometry)[5];

    const auto mass = volume(geometry);

    near(height, 30, 1e-6, "updated height");

    near(mass, 180000, 1e-4, "updated volume");

    check_step(model, 0, plate, FeatureState::Valid, 1);

    out << "t=10 -> 15 mm, plate height=20 -> " << height << " mm, plate volume=120000 -> " << mass << " mm^3\n";

    show(out, model);
}

//
// 定义本周的用例表
constexpr Scenario courseScenarios[] = {
    {"single-extrusion", single_extrusion},
};

}

//
// 把 courseScenarios 数组隐式转换成 std::span<const Scenario>，返回一个可遍历的只读视图。
std::span<const Scenario> scenarios() { return courseScenarios; }

//
// 为什么使用 std::span ：
// 因为 courseScenarios 已经保存了全部用例，scenarios() 只需让调用者读取和遍历它们。返回 span 只传递起始地址和元素个数。
//
//

//
// 三个问题：
// - 为什么能直接返回数组？
//   std::span 有接收原生数组引用的构造函数，能从数组类型推导出元素个数。本周数组只有一个用例，所以 scenarios().size() 返回 1。
//
// - const Scenario 表示什么？
//   调用者可以读取 scenario.name、调用 scenario.run(std::cout)。通过这个视图访问的元素是只读的。
//
// - 函数返回后为什么还能访问？
//   courseScenarios 定义在命名空间作用域，具有静态存储期。返回的 span 记录数组的起始地址和元素个数，调用者遍历时直接访问原数组
//
}
