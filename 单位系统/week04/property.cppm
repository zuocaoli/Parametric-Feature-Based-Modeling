export module course.property;
import std;
export import course.schema;
export namespace units {
using ScriptValue = std::variant<double, std::string, Quantity>;
class PropertyQuantity {
  public:
    PropertyQuantity(Quantity value, QuantityFormat format = {});
    virtual ~PropertyQuantity() = default;
    Quantity quantity() const;
    QuantityFormat format() const;
    Result<void> set_internal(double value);
    virtual Result<void> set_script(const ScriptValue &value, const UnitRegistry &registry);
    Result<void> set_expression(const Quantity &value);

  protected:
    double value_;
    Unit unit_;
    QuantityFormat format_;
    Result<Quantity> script_quantity(const ScriptValue &value, const UnitRegistry &registry) const;
};
class PropertyQuantityConstraint final : public PropertyQuantity {
  public:
    PropertyQuantityConstraint(Quantity value, double lower, double upper, double step = 1);
    Result<void> set_script(const ScriptValue &value, const UnitRegistry &registry) override;
    double minimum() const;
    double maximum() const;
    double step() const;

  private:
    double lower_, upper_, step_;
};
} // namespace units
