export module month06.week02.property;

import std;

export namespace month06::week02 {
using ObjectId = std::uint64_t;

using Value = std::variant<double, std::int64_t, bool, std::string>;
enum class SetResult {
    Changed,
    Unchanged,
    NotFound,
    TypeMismatch,
    InvalidValue,
    ReadOnly,
    Reentrant
};

std::string_view result_name(SetResult result);
std::string_view type_name(const Value &value);
std::string value_text(const Value &value);

// 仅返回快照，外部不能通过枚举绕过修改入口。
struct PropertyView {
    std::string name;
    std::string label;
    std::string unit;
    bool read_only;
    Value value;
};

class PropertyObject;
struct Change {
    std::string name;
    Value before;
    Value after;
};
struct ChangeEvent {
    const PropertyObject *source; // 只在同步回调期间借用，不延长对象生命周期。
    ObjectId source_id;
    std::vector<Change> changes;
};
struct Update {
    std::string name;
    Value value;
    bool operator==(const Update &) const = default;
};

// 通知连接与属性对象共用模块，避免事件来源指针产生循环依赖。
class ChangeSignal {
    struct Slot {
        bool active = true;
        std::function<void(const ChangeEvent &)> callback;
    };

public:
    class Connection {
    public:
        Connection() = default;
        Connection(const Connection &) = delete;
        Connection &operator=(const Connection &) = delete;
        Connection(Connection &&other) noexcept;
        Connection &operator=(Connection &&other) noexcept;
        ~Connection();
        void disconnect() noexcept;

    private:
        friend class ChangeSignal;
        explicit Connection(const std::shared_ptr<Slot> &slot);
        std::weak_ptr<Slot> slot_;
    };

    Connection connect(std::function<void(const ChangeEvent &)> callback);

    std::size_t emit(const ChangeEvent &event);

private:
    std::vector<std::shared_ptr<Slot>> slots_;
};

class PropertyObject {
public:
    explicit PropertyObject(ObjectId id = 0);
    ObjectId id() const;
    virtual ~PropertyObject() = default;
    // 派生属性的读取函数捕获 this，因此对象不可复制或移动。
    PropertyObject(const PropertyObject &) = delete;
    PropertyObject &operator=(const PropertyObject &) = delete;

    std::vector<PropertyView> properties() const;

    std::optional<Value> get(std::string_view name) const;

    ChangeSignal::Connection observe(std::function<void(const ChangeEvent &)> callback);

    std::size_t notification_errors() const;

    SetResult set(std::string_view name, const Value &value);

    SetResult set_many(const std::vector<Update> &updates);

protected:
    using Validator = std::function<bool(const Value &)>;
    using Reader = std::function<Value()>;

    void add(std::string name, std::string label, std::string unit, Value value,
             Validator validate = {}, Reader compute = {});

private:
    struct Property {
        std::string name;
        std::string label;
        std::string unit;
        Value value;
        bool read_only;
        Validator validate;
        Reader compute;
    };

    static Value read(const Property &property);

    const ObjectId id_;
    std::vector<Property> properties_;
    ChangeSignal changed_;
    bool notifying_ = false;
    std::size_t notification_errors_ = 0;
};
} // namespace month06::week02
