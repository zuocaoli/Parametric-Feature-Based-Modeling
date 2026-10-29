import std;
import month02.week04.storage;

namespace {
using namespace month02::week04;

// 检查条件的辅助函数：条件成立就继续执行，不成立就抛出异常
void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string{message});
    }
}

//
double number(const PropertyObject &object, std::string_view name) {
    return std::get<double>(*object.get(name));
}

// 输出矩形当前的 width 和 area 属性值
void show(const RectangleModel &model) {
    std::cout << "width=" << number(model.rectangle, "width")
              << ", area=" << number(model.rectangle, "area") << '\n';
}

// 先计算 width，再计算 area；任何一步失败，就抛出异常
void compute_in_order(RectangleModel &model) {

    for (auto target : {"width", "area"}) {

        // 计算并且更新属性值
        const auto result = model.engine.compute(target);

        //
        if (!result) {
            const auto &error = result.error();
            throw std::runtime_error(std::string{stage_name(error.stage)} + " at byte " +
                                     std::to_string(error.position) + ": " + error.message);
        }
    }
}
} // namespace

int main() {
    try {
        RectangleModel model;

        std::size_t notifications = 0;

        auto connection = model.rectangle.observe([&](const ChangeEvent &) { ++notifications; });

        //
        //
        // ===========================================================================================================
        // 1. 先把属性, 表达式绑定并求解的流程跑一遍
        // ===========================================================================================================

        // 属性绑定表达式
        require(model.engine.bind("width", "length / 2").has_value(), "bind width failed");
        require(model.engine.bind("area", "length * width").has_value(), "bind area failed");

        // 计算并更新属性
        compute_in_order(model);

        // 输出矩形当前的 width 和 area 属性值
        show(model);

        //
        //
        // =============================================================================================================
        // 2. 改变属性值，再次执行之前已经绑定好的表达式，观察变化
        // =============================================================================================================

        // 改变 length 属性值，由于 width 在前面绑定了关于 length 的表达式，后面执行 compute 的时候 width 得到值就会刷新
        require(model.rectangle.set("length", 200.0) == SetResult::Changed, "change length failed");
        // 先打印width刷新前的值
        std::cout << "after length=200, before compute: ";
        show(model);
        // 执行表达式计算
        compute_in_order(model);
        // 打印刷新后的width
        show(model);

        //
        //
        // =============================================================================================================
        // 3. 不改变表达式，不改关联属性，不会触发属性变更
        // =============================================================================================================

        const auto repeated = model.engine.compute("width");
        // 检查对象是否持有期望值（即 SetResult 类型的成功结果）
        require(repeated.has_value(), "repeat failed");
        // 打印此次属性更新的结果，
        // 这次width的值没有更新，notifications 自然也不会增加
        std::cout << "repeat=" << result_name(*repeated) << ", notifications=" << notifications
                  << '\n';

        //
        //
        // ==============================================================================================================
        // 4. 非法表达式如何处理
        // ==============================================================================================================

        // 给 width 绑定一个非法的表达式，解析会失败，而且非法输入不会污染已有状态
        const auto invalid = model.engine.bind("width", "length +");
        // 打印错误类型，位置， messasge
        std::cout << stage_name(invalid.error().stage) << " at byte " << invalid.error().position
                  << ": " << invalid.error().message << "; old formula preserved\n";

        //
        //
        // ======================================================================================================
        // 5. 把当前模型序列化到文件，再从文件完整地恢复成一个新模型，并验证恢复出来的模型不只是“数值对得上”，而且公式还活着
        // ======================================================================================================

        // __FILE__ 展开为源文件的绝对路径的： /home/zcli/dev/learn-cad/month02/week04/main.cpp
        // std::filesystem::path 类用于从字符串、字符序列或其他路径对象创建一个路径对象。它只处理路径的语法表示，不要求路径在文件系统上实际存在。
        // parent_path： 获取路径的父目录路径。它仅进行语法层面的解析，不检查该目录是否真实存在
        const auto course_dir = std::filesystem::path{__FILE__}.parent_path();
        // 在打开文件前，确保 build/ 目录存在
        const std::filesystem::path path = course_dir / "build/rectangle.txt";
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }

        // 保存模型
        {
            std::ofstream output{path};
            const auto saved = save_model(model, output);
            if (!saved) {
                throw std::runtime_error(saved.error().message);
            }
            output.close();
            require(!output.fail(), "closing snapshot failed");
        }

        // 读取模型
        std::ifstream input{path};
        auto restored = load_model(input);
        if (!restored) {
            throw std::runtime_error(restored.error().message);
        }
        std::cout << "saved and restored " << path.generic_string() << '\n';

        // 计算属性值
        compute_in_order(**restored);
        std::cout << "loaded: ";
        show(**restored);

    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
