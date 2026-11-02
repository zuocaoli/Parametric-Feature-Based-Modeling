export module course.input;
import std;
export import course.quantity;
export namespace units {
struct UnitRef {
    std::string id;
    unsigned version = 1;
    bool operator==(const UnitRef &) const = default;
};
struct UnitBinding {
    std::size_t position;
    std::size_t length;
    UnitRef unit;
    bool operator==(const UnitBinding &) const = default;
};
struct ParsedUnit {
    UnitDefinition definition;
    std::vector<UnitBinding> bindings;
};
struct ParsedQuantity {
    Quantity value;
    std::vector<UnitBinding> bindings;
};
struct ScannedNumber {
    double value;
    std::size_t end;
};
Result<ScannedNumber> scan_number(std::string_view source, std::size_t position);
Result<ParsedUnit> parse_unit(std::string_view source, const UnitRegistry &registry,
                              std::size_t offset = 0, std::span<const UnitBinding> pinned = {},
                              bool require_pins = false);
struct InputContext {
    QuantitySpec spec;
    std::uint64_t document_identity = 0;
    std::uint64_t object = 1;
    UnitRef input_unit;
    std::uint64_t document_generation = 1;
    std::uint64_t config_version = 1;
    std::string target = "length";
    std::optional<UnitDefinition> frozen_unit;
};
Result<ParsedQuantity> parse_quantity(std::string_view source, const UnitRegistry &registry,
                                      const std::optional<InputContext> &context = {},
                                      std::size_t offset = 0,
                                      std::span<const UnitBinding> pinned = {},
                                      bool require_pins = false);
// 应用偏好只在创建配置时传入；之后输入/显示均不依赖应用全局状态。
struct UnitConfig {
    std::string schema = "Internal";
    UnitRef input{"", 1};
    UnitRef display{"", 1};
    std::map<std::string, UnitRef> input_overrides;
    std::map<std::string, UnitRef> display_overrides;
    bool input_follows_display = false;
    int precision = 4;
    bool significant = false;
    bool group_digits = false;
    std::uint64_t version = 1;
    bool operator==(const UnitConfig &) const = default;
};
InputContext begin_edit(QuantitySpec spec, const UnitConfig &config, std::string target,
                        std::uint64_t document_generation, UnitRef schema_input = {"mm", 1});
Result<Quantity> commit_edit(std::string_view source, const UnitRegistry &registry,
                             const InputContext &context, std::uint64_t document_generation);
std::string diagnostic(const Error &error);
Result<std::string> mask_comments(std::string_view source);
} // namespace units
