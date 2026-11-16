export module month03.week04.property;

import std;

export namespace month03::week04 {
using ObjectId = std::uint64_t;

//
// `Value` 是一个**类型别名**，表示“属性值类型”。
// 一个 `Value` 对象可以保存 `double`、`std::int64_t`、`bool`、`std::string` 中的一种值。
// 一次只保存一种类型的值。
//
// std::variant 是 C++17 引入的类型安全联合体：
// 一个变量可以保存多种指定类型中的一种，但同一时刻只保存其中一种。
using Value = std::variant<double, std::int64_t, bool, std::string>;

// set()/set_many() 的修改结果；只有 Changed 会提交变化并发送通知。
// 其余结果均保持原状态，不发送通知。
enum class SetResult {
    Changed,      // 校验通过且至少一个值改变，已提交修改并发送一次通知。
    Unchanged,    // 校验通过但所有值均未改变，或批量修改列表为空。
    NotFound,     // 指定名称的属性不存在。
    TypeMismatch, // 新值类型与属性类型不一致，例如给 double 属性传入整数。
    InvalidValue, // 取值未通过校验，或同一批修改中出现重复属性名。
    ReadOnly,     // 属性只读，不允许通过修改接口赋值。
    Reentrant     // 正在通知回调，拒绝再次修改同一对象，避免递归通知。
};

std::string_view result_name(SetResult result);
std::string_view type_name(const Value &value);
std::string value_text(const Value &value);

// 仅返回快照，外部不能通过枚举绕过修改入口。
struct PropertyView {
    std::string name;  // 稳定名称，用于 get()/set() 查找，例如 "length"。
    std::string label; // 展示给用户的标签，例如 "Length"。
    std::string unit;  // 单位，例如 "mm"；空字符串表示无单位。
    bool read_only;    // 原属性是否只读；为 true 时 set() 拒绝修改。
    Value value;       // 获取快照时的值副本；派生属性保存当时计算出的结果。
};

class PropertyObject;

// 表示一条属性变化记录，由 ChangeEvent 携带，供订阅者了解修改前后的值。
struct Change {
    std::string name; // 发生变化的属性名，例如 "length"。
    Value before;     // 修改前的值副本，例如 5.0。
    Value after;      // 修改后的值副本，例如 10.0。
};

// 一次修改完成后要发送的事件，说明哪个对象的哪些属性发生了变化。
struct ChangeEvent {
    // 来源只在同步回调期间借用；可读取对象，但不延长其生命周期。
    const PropertyObject *source;
    ObjectId source_id;
    std::vector<Change> changes; // 本次实际变化的记录；批量修改时可以包含多条。
};

// set_many() 的一项输入：把 name 指定的属性改为 value。
struct Update {
    std::string name; // 待修改属性的稳定名称，例如 "length"。
    Value value;      // 期望的新值；类型必须与该属性现有值相同。
};

// 通知连接与属性对象共用模块，避免事件来源指针产生循环依赖。
// 属性变化的通知管理器：
// 管理属性变化的订阅，并把 ChangeEvent 同步传给各个订阅回调。
// 与属性对象共用模块，事件来源可直接使用 PropertyObject 指针。
class ChangeSignal {

    // 一个 Slot 对象表示一项订阅
    struct Slot {
        bool active = true; // 该项订阅是否仍然有效；退订后置为 false。
        std::function<void(const ChangeEvent &)> callback; // 收到事件的时候执行的回调函数
    };

public:
    // 表示一项订阅的连接：矩形发生变化时，调用某个回调函数。
    // 订阅连接：不可复制、可移动；销毁或调用 disconnect() 时自动退订。
    // ChangeSignal::Connection 表示定义在 ChangeSignal 内部的 Connection 类型。
    class Connection {
    public:
        // 默认构造为空连接，尚未关联任何订阅。
        Connection() = default;

        // 禁止复制，避免多个连接对象同时负责同一订阅的退订。
        Connection(const Connection &) = delete;
        Connection &operator=(const Connection &) = delete;

        // 移动时转移退订责任，原连接变空；移动赋值会先断开自身原有订阅。
        Connection(Connection &&other) noexcept;
        Connection &operator=(Connection &&other) noexcept;

        // 析构时自动调用 disconnect()，将订阅生命周期绑定到连接对象。
        ~Connection();

        // 将订阅标记为无效；可重复调用，不抛异常。
        void disconnect() noexcept;

    private:
        // 允许 ChangeSignal 通过私有构造函数创建关联订阅的连接。
        friend class ChangeSignal;
        explicit Connection(const std::shared_ptr<Slot> &slot);

        // 弱引用订阅状态，不延长其生命周期；发布者先销毁时也可安全断开。
        std::weak_ptr<Slot> slot_;
    };

    // 注册回调(创建订阅)，并返回退订凭证
    Connection connect(std::function<void(const ChangeEvent &)> callback);

    // 按注册顺序通知有效订阅者，返回回调抛出异常的次数；单个回调失败不阻断其他回调。
    std::size_t emit(const ChangeEvent &event);

private:
    // 当前的订阅。
    // 强持有订阅状态；Connection 仅弱引用状态，发布者先销毁也可安全退订。
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

    // 获取属性快照。
    std::vector<PropertyView> properties() const;
    std::optional<Value> get(std::string_view name) const;

    // 建立了订阅关系，返回一张退订凭证，通过调用 disconnect() 退订
    ChangeSignal::Connection observe(std::function<void(const ChangeEvent &)> callback);

    // 返回本对象发送通知时，订阅回调抛出异常的累计次数。
    std::size_t notification_errors() const;

    // 单项修改复用批量修改的校验、提交和通知流程。
    SetResult set(std::string_view name, const Value &value);

    //
    // 批量修改同一个对象的多个属性.
    //
    // 同时修改 updates 中的多项属性，例如 {{"length", 20.0}, {"width", 4.0}}。
    // 先校验全部输入，再提交所有实际变化，最后发送一次 ChangeEvent；
    // 订阅者读取对象时已看不到只修改了长或宽的中间状态。
    // 返回 Changed：至少一项已改变；Unchanged：空列表或所有值与原值相同。
    // 名称重复、属性不存在、只读、类型不符或取值非法时返回对应错误，
    // 保持所有属性原值且不通知；通知期间再次调用则返回 Reentrant。
    SetResult set_many(const std::vector<Update> &updates);

protected:
    // 参数有效性检验接口
    using Validator = std::function<bool(const Value &)>;
    using Reader = std::function<Value()>;

    // 向当前对象注册一个新属性。
    void add(std::string name, std::string label, std::string unit, Value value,
             Validator validate = {}, Reader compute = {});

private:
    // 对象内部保存的属性，包含元数据、存储值及校验和计算规则。
    // 一个 Property 对象就是一个属性。
    struct Property {
        std::string name;   // 稳定名称，用于查找属性，例如 "length"。
        std::string label;  // 展示给用户的标签，例如 "Length"。
        std::string unit;   // 单位，例如 "mm"；空字符串表示无单位。
        Value value;        // 普通属性的存储值；派生属性读取时改用 compute() 的结果。
        bool read_only;     // 是否只读；为 true 时 set() 拒绝修改。
        Validator validate; // 可选的取值校验函数，返回 true 表示合法；为空则不做取值校验。
        Reader compute;     // 可选的派生值计算函数，每次读取时调用；设置后属性为只读。
    };

    static Value read(const Property &property);

    const ObjectId id_;
    std::vector<Property> properties_;

    ChangeSignal changed_;

    // 是否正在发送变化通知；
    // 为 true 时拒绝回调再次修改同一对象。
    // NotifyGuard 在通知前设为 true，离开作用域时恢复为 false。
    bool notifying_ = false;

    // 本对象的通知回调异常累计计数；初始为 0，每次 emit() 返回后累加。
    std::size_t notification_errors_ = 0;
};
} // namespace month03::week04
