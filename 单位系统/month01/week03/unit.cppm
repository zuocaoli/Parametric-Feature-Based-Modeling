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
// 量纲与单位符号无关；默认是无量纲。指数顺序L/M/T/I/温度/物质/发光/角度。
class Unit {
  public:
    Unit() = default;
    static Result<Unit> make(std::array<int, 8> exponents);
    const std::array<int, 8> &exponents() const;
    bool dimensionless() const;
    bool operator==(const Unit &) const = default;
    auto operator<=>(const Unit &) const = default;
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
    std::array<int, 8> exponents_{};
};
Result<Unit> unit_product(char operation, Unit left, Unit right);
Result<Unit> unit_power(Unit unit, double exponent);
Result<Unit> unit_root(Unit unit, unsigned degree);
std::string unit_name(Unit unit);
std::string internal_symbol(Unit unit);
std::string describe(Unit unit);
} // namespace units
