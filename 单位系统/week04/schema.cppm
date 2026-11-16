export module course.schema;
import std;
export import course.input;
export namespace units {
struct QuantityFormat {
    int precision = 4;
    bool significant = false;
    bool group_digits = false;
    bool operator==(const QuantityFormat &) const = default;
};
struct UnitTranslationSpec {
    double threshold;
    std::string unit_string;
    double factor;
};
struct UnitsSchemaSpec {
    unsigned id;
    std::string name;
    UnitRef basic_length;
    std::map<Unit, std::vector<UnitTranslationSpec>> translations;
};
struct Translation {
    double value, factor;
    std::string unit_string, text;
};
class UnitsSchema {
  public:
    explicit UnitsSchema(UnitsSchemaSpec spec);
    const UnitsSchemaSpec &spec() const;
    Result<Translation> translate(const Quantity &quantity, QuantityFormat format = {}) const;

  private:
    UnitsSchemaSpec spec_;
};
class UnitsSchemas {
  public:
    UnitsSchemas();
    Result<void> add(UnitsSchemaSpec spec);
    Result<void> select(std::string_view name);
    Result<void> select(unsigned id);
    const UnitsSchema &current() const;
    Result<UnitsSchema> get(std::string_view name) const;
    std::vector<std::string> names() const;

  private:
    std::map<unsigned, UnitsSchema> schemas_;
    unsigned current_ = 0;
};
class UnitsApi {
  public:
    explicit UnitsApi(UnitRegistry registry = {});
    UnitRegistry &registry();
    const UnitRegistry &registry() const;
    UnitsSchemas &schemas();
    const UnitsSchemas &schemas() const;
    Result<ParsedQuantity> parse(std::string_view source,
                                 const std::optional<InputContext> &context = {}) const;
    Result<Translation> format(const Quantity &quantity, QuantityFormat format = {},
                               std::optional<UnitRef> override_unit = {}) const;
    Result<InputContext> capture(QuantitySpec spec, const UnitConfig &config, std::string target,
                                 std::uint64_t generation) const;

  private:
    UnitRegistry registry_;
    UnitsSchemas schemas_;
};
} // namespace units
