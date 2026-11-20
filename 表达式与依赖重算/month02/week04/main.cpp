import std;
import month03.week04.engine;

namespace {
using namespace month03::week04;

double number(const RecomputeEngine &engine, const PropertyKey &key) { return std::get<double>(*engine.stored_value(key)); }

void report(const RecomputeEngine &engine) {

    for (const auto &step : engine.last_report()) {

        // 这里的重点就是 state_name
        std::cout << key_text(step.target) << ' ' << state_name(step.state) << " count=" << step.executions << " (" << step.message << ")\n";
    }
}

void act(std::string_view title) { std::cout << "==== " << title << " ====\n"; }

void rectangle_example() {

    RecomputeEngine engine;

    //
    // 建立依赖图：
    // 创建带默认属性的矩形，
    // 然后在 begin_batch()/end_batch() 之间以批处理方式绑定两个表达式：
    // area = length * width、
    // width = length / 2，
    // 形成 length → width → area 的依赖链，依赖分析和首次求值在 end_batch() 时统一完成。
    const auto id = engine.create_rectangle();
    std::ignore = engine.begin_batch();
    std::ignore = engine.bind({id, "area"}, "length * width");
    std::ignore = engine.bind({id, "width"}, "length / 2");
    std::ignore = engine.end_batch();

    //
    std::cout << "rectangle: width="                       //
              << number(engine, {id, "width"})             //
              << ", area=" << number(engine, {id, "area"}) //
              << '\n';

    //
    // 演示增量重算：
    // 把 length 改成 200 后，report() 打印重算报告（哪些目标被执行、执行次数、状态），
    // 新值只在受影响的下游属性上传播（width 变 100、area 变 20000）。
    engine.set({id, "length"}, 200.0);

    //
    report(engine);

    //
    std::cout << "width="                      //
              << number(engine, {id, "width"}) //
              << ", area="                     //
              << number(engine, {id, "area"})  //
              << '\n';

    //
    // 验证无关变更不触发重算：
    // 改 name 这种没有任何依赖的属性，last_report().size() 为 0，证明引擎只重算受影响的子图。
    engine.set({id, "name"}, std::string{"renamed"});
    // 检查有没有触发重算
    std::cout << "name change executions="   //
              << engine.last_report().size() //
              << '\n';

    //
    // 验证环检测：
    // 尝试绑定 length = width * 2，这会形成 length → width → length 的环；
    // 绑定被拒绝，并打印出错误里携带的环路径（rejected.error().cycle）。
    const auto rejected = engine.bind({id, "length"}, "width * 2");
    // 打印环
    std::cout << "rejected cycle:";
    for (const auto &key : rejected.error().cycle) {
        std::cout << ' ' << key_text(key);
    }
    std::cout << '\n';
}

void failure_example() {

    act("第一幕：制造失败");

    //
    RecomputeEngine engine;

    //
    const auto a = engine.create_rectangle();
    const auto b = engine.create_rectangle();

    //
    engine.set({a, "segments"}, std::int64_t{2});

    std::ignore = engine.begin_batch();

    // 对象 a 的 width 属性绑定公式 length / (segments - 1)，同时用 std::ignore 显式丢弃 bind 的返回值。
    std::ignore = engine.bind({a, "width"}, "length / (segments - 1)");
    // 给 a 的 area 绑定公式 length * width——std::ignore = 丢返回值的含义和上一行完全一样.
    std::ignore = engine.bind({a, "area"}, "length * width");

    // 给 b（第二个矩形）的 width 绑定公式 source_segments + 1，第三个参数是一个参数别名表 用来跨对象引用属性.
    // 也就是引用其他对象的属性。
    std::ignore = engine.bind({b, "width"},          //
                              "source_segments + 1", //
                              {{"source_segments", {a, "segments"}}});

    std::ignore = engine.end_batch();

    //
    //
    // 把 a 的输入属性 segments 从 2 改成 1，
    // 这是整个失败测试开始点，segments == 1 本身没有错，问题是下游公式 width = length/(segments-1) 在这个合法输入下分母归零，
    // set 用于修改单个输入属性，并同步触发所有受影响公式的重算。
    engine.set({a, "segments"}, std::int64_t{1});

    std::cout << "failed round:\n";

    //
    // 打印重算记录
    //
    // 1.width Failed count=1 (evaluation at byte 7: division by zero)
    // 1.area Blocked count=0 (blocked by: 1.width)
    // 2.width Valid count=1 (Changed)
    //
    // 从最后一行开始看， 2.width 成功了，1.area 被 1.width 阻塞了，1.width 阻塞是因为计算的时候分母为0。
    report(engine);

    std::cout << std::endl;
    std::cout << std::endl;
    std::cout << std::endl;

    // 在失败轮重算结束后读出三个属性的当前值打印
    std::cout << "cached: width="             //
              << number(engine, {a, "width"}) //
              << ", area="                    //
              << number(engine, {a, "area"})  //
              << ", independent width="       //
              << number(engine, {b, "width"}) //
              << '\n';

    std::cout << std::endl;
    std::cout << std::endl;
    std::cout << std::endl;

    //
    // 失败轮的可信度复核：用 value() 逐个询问两个被污染的属性“现在还能读吗”，输出：
    // valid reads: width=false, area=false，
    std::cout << std::boolalpha                         //
              << "valid reads: width="                  //
              << engine.value({a, "width"}).has_value() //
              << ", area="                              //
              << engine.value({a, "area"}).has_value()  //
              << '\n';

    std::cout << std::endl;
    std::cout << std::endl;
    std::cout << std::endl;

    act("第二幕：修复输入");

    //
    // 把 segments 从 1 改回 3， 修复上面的错误，会触发重算。
    engine.set({a, "segments"}, std::int64_t{3});
    std::cout << "repaired round:\n";
    // 打印本轮重算记录：
    //    1.width Valid count=1 (Changed)
    //    1.area Valid count=1 (Changed)
    //    2.width Valid count=1 (Changed)
    report(engine);

    std::cout << std::endl;
    std::cout << std::endl;
    std::cout << std::endl;

    // 修复后的数值验证： repaired: width=50, area=5000, independent width=4
    std::cout << "repaired: width="           //
              << number(engine, {a, "width"}) //
              << ", area="                    //
              << number(engine, {a, "area"})  //
              << ", independent width="       //
              << number(engine, {b, "width"}) //
              << '\n';

    std::cout << std::endl;
    std::cout << std::endl;
    std::cout << std::endl;

    act("第三幕：删除数据源");

    // 给 b 的 area 绑定公式 length * width
    std::ignore = engine.bind({b, "area"}, "length * width");

    // 把对象 a（从引擎中整个删除，
    // 矩形对象销毁、它的公式和状态清空、订阅断开，然后立即触发一轮重算(这次重算的产出不是值，是状态)
    std::ignore = engine.erase(a);

    std::cout << "deleted source:\n";
    report(engine);

    std::cout << std::endl;
    std::cout << std::endl;
    std::cout << std::endl;

    //
    // 创建一个全新对象，充当被删掉的 a 的替补数据源
    act("第四幕：重绑恢复");

    const auto replacement = engine.create_rectangle();

    //
    // 前面的 engine.erase(a)制造了悬空引用：b.width 的公式通过别名表引用 {a, "segments"}，而 a 已被整个删除。
    // 此时 b.width 进入 MissingReference 状态,引擎只会标记状态，不会自动修复，引用也不会自己“复活”。
    //
    // a 已经无法复活：erase 把对象、公式、状态、订阅全部清掉了，所以想修复只能另起一个新对象。
    // create_rectangle() 返回一个全新的 ObjectId，这个新对象就是给后续重绑用的“替身”。
    //

    // 给替补对象 set 一个 segments = 3，让它有可供引用的值。
    engine.set({replacement, "segments"}, std::int64_t{3});

    std::cout << "replacement id="                             //
              << replacement                                   //
              << ", target state="                             //
              << state_name(engine.status({b, "width"}).state) //
              << '\n';

    // 重新 bind，把 source_segments 别名从已删除的 {a, "segments"} 改指到 {replacement, "segments"}，b.width 才恢复计算。
    std::ignore = engine.bind({b, "width"},          //
                              "source_segments + 1", //
                              {{"source_segments", {replacement, "segments"}}});

    std::cout << "rebound round:\n";
    // 把“重绑后立即触发的那轮重算”的执行记录打印出来，作为重绑生效的过程证据。
    report(engine);

    std::cout << "target width="              //
              << number(engine, {b, "width"}) //
              << ", area="                    //
              << number(engine, {b, "area"})  //
              << '\n';
}
} // namespace

int main(int argc, char **) {

    try {

        // 本演示不接收参数；拒绝额外参数，避免把未使用的输入误认为已生效.
        if (argc != 1) {
            throw std::invalid_argument("usage: demo");
        }

        //
        // 用一个矩形模型展示“表达式绑定 → 增量重算 → 无关变更隔离 → 环路拒绝”这条完整链路。
        rectangle_example();

        //
        // 完整演示重算引擎在“出错”场景下的整个生命周期——错误如何发生、如何传播、如何隔离、值如何失去可信度，以及两条恢复路径。
        failure_example();

    } catch (const std::exception &error) {

        std::cerr << error.what() << '\n';
        return 1;
    }
}
