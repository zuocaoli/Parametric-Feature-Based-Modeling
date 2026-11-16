export module course.quantity;
import std;
export import course.unit;

export namespace units {

//
//
// Quantity 表示一个具体的量，由“内部单位下的数值”和“量纲”组成。
// 例如长度 500 mm、面积 200 mm²，也可以表示无量纲数值 3。
class Quantity {
  public:
    static Result<Quantity> make(double value, Unit unit = {});

    double value() const;

    Unit unit() const;

    bool operator==(const Quantity &) const = default;

  private:
    Quantity(double value, Unit unit);

    double value_; // 按内部单位表示的数值, 50cm 保存的就是 500
    Unit unit_;    // 八维量纲
};

struct QuantitySpec {
    Unit unit;
    std::optional<double> lower, upper;
    bool operator==(const QuantitySpec &) const = default;
};
Result<void> validate(const Quantity &quantity, const QuantitySpec &spec);
Result<Quantity> calculate(char operation, const Quantity &left, const Quantity &right);
Result<Quantity> power(const Quantity &quantity, double exponent);
Result<Quantity> square_root(const Quantity &quantity);
Result<Quantity> sine(const Quantity &angle);

//
// 表示一个具体单位的定义：它叫什么、属于什么量纲，以及 1 个这样的单位等于多少内部单位.
struct UnitDefinition {
    std::string id;                //  单位定义的标识，与 version 组成注册表的键, 例如 “cm”
    unsigned version = 1;          //  该定义的版本号，默认 1,
    std::string symbol;            //  输入、显示时使用的单位符号,
    Unit unit;                     //  单位对应的八维量纲, Unit::length()
    double scale = 1;              //  此符号的一个单位对应多少内部单位
    std::string source = "custom"; //  定义来源的文字标记，默认 "custom"
    bool builtin = false;          //  是否为内置单位，默认 false

    bool operator==(const UnitDefinition &) const = default; //
};

//
// 集中管理“单位符号对应什么量纲、怎样换算到内部单位”，为输入、输出和自定义单位提供统一的查询入口.
//
class UnitRegistry {
  public:
    UnitRegistry();

    Result<void> add(UnitDefinition definition);

    Result<UnitDefinition> find(std::string_view symbol) const;

    Result<UnitDefinition> find_id(std::string_view id, unsigned version) const;

    std::vector<UnitDefinition> custom_definitions() const;

  private:
    std::map<std::pair<std::string, unsigned>, UnitDefinition> definitions_;
};

Result<Quantity> from_unit(double value, const UnitDefinition &unit);
Result<double> to_unit(const Quantity &quantity, const UnitDefinition &unit);
Result<UnitDefinition> unit_product(char operation, const UnitDefinition &left,
                                    const UnitDefinition &right);
Result<UnitDefinition> unit_power(const UnitDefinition &unit, double exponent);
bool nearly_equal(const Quantity &left, const Quantity &right, double absolute_tolerance,
                  double relative_tolerance = 1e-12);
} // namespace units
