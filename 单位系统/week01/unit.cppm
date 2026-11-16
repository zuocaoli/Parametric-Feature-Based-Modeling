export module course.unit;
import std;

export namespace units {

struct Error {
    std::string code, message;
    std::string stage = "unit";
    std::size_t position = 0, length = 0;
    std::string expected, actual, target;
};

template <class T> using Result = std::expected<T, Error>;

Error error(std::string code, std::string message);

//
// 表示量纲：一个量属于长度、面积、时间，还是它们组合而成的类型。
//
// Unit 负责表达“是什么量纲”，
// UnitDefinition 补充具体单位及换算规则，
// Quantity 再赋予它具体数值。
class Unit {
  public:
    Unit() = default;

    // 带校验的静态工厂函数：接收八个量纲指数，检查合法性，成功后创建 Unit
    static Result<Unit> make(std::array<int, 8> exponents);

    // 只读访问函数，用来取得 Unit 保存的八个量纲指数
    const std::array<int, 8> &exponents() const;

    // 来判断当前 Unit 是否为无量纲，即八个量纲指数是否全部为零
    bool dimensionless() const;

    bool operator==(const Unit &) const = default;

    //
    // <=> 是 C++20 引入的三路比较运算符。对于这里的整数数组，返回类型由 auto 推导为
    // std::strong_ordering： |             比较结果            |        含义         |
    // |--------------------------------|--------------------|
    // | std::strong_ordering::less     | 左侧排在右侧之前      |
    // | std::strong_ordering::equal    | 两侧相等             |
    // | std::strong_ordering::greater  | 左侧排在右侧之后      |
    auto operator<=>(const Unit &) const = default;

    // 常见量纲的静态工厂函数，让你直接通过名称创建 Unit，不用每次手写八个指数.
    static Unit length();
    static Unit area();
    static Unit volume();
    static Unit mass();
    static Unit time();
    static Unit angle();
    static Unit temperature();
    static Unit force();
    static Unit pressure();
    static Unit work();
    static Unit moment();

  private:
    explicit Unit(std::array<int, 8> exponents);

    // 这八个整数依次表示：
    // 长度、质量、时间、电流、温度、物质的量、发光强度、角度，
    // 每个整数代表对应基本量纲的幂指数：
    //
    // |   量纲   |        指数数组       |        数学含义        |
    // |---------|----------------------|-----------------------|
    // | 无量纲   | {0,0,0,0,0,0,0,0}    | 所有基本量纲的指数均为零  |
    // | 长度     | {1,0,0,0,0,0,0,0}    | 长度¹                 |
    // | 面积     | {2,0,0,0,0,0,0,0}    | 长度²                 |
    // | 速度     | {1,0,-1,0,0,0,0,0}   | 长度 / 时间            |
    // | 力       | {1,1,-2,0,0,0,0,0}   | 质量 × 长度 / 时间²     |
    // | 角度     | {0,0,0,0,0,0,0,1}    | 独立的角度量纲           |
    //
    // 这样表示，可以把量纲运算转化为指数运算：
    // - 乘法：对应指数相加。
    // - 除法：对应指数相减。
    // - 幂运算：各指数乘以幂数；结果需要满足当前整数指数表示的要求。
    // - 加减法：通过比较两个 Unit 是否相同，判断量纲是否兼容。
    std::array<int, 8> exponents_{};
};

Result<Unit> unit_product(char operation, Unit left, Unit right);
Result<Unit> unit_power(Unit unit, double exponent);
Result<Unit> unit_root(Unit unit, unsigned degree);
std::string unit_name(Unit unit);
std::string internal_symbol(Unit unit);
std::string describe(Unit unit);
} // namespace units
