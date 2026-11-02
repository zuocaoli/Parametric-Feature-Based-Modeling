export module course.quantity;
import std;
export import course.unit;
export namespace units {
class Quantity {
  public:
    static Result<Quantity> make(double value, Unit unit = {});
    double value() const;
    Unit unit() const;
    bool operator==(const Quantity &) const = default;

  private:
    Quantity(double value, Unit unit);
    double value_;
    Unit unit_;
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
struct UnitDefinition {
    std::string id;
    unsigned version = 1;
    std::string symbol;
    Unit unit;
    double scale = 1; // 此符号的一个单位对应多少内部单位
    std::string source = "custom";
    bool builtin = false;
    bool operator==(const UnitDefinition &) const = default;
};
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
