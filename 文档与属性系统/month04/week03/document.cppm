export module month06.week03.document;

import std;
export import month06.week03.engine;

export namespace month06::week03 {
// 命令保留值快照，undo/redo 不依赖对象地址。
class Command {
public:
    virtual ~Command() = default;
    virtual std::expected<bool, EngineError> execute(RecomputeEngine &model) const = 0;
    std::expected<std::unique_ptr<RecomputeEngine>, EngineError>
    undo(ObjectId floor, std::shared_ptr<AstCache> cache) const;
    std::expected<std::unique_ptr<RecomputeEngine>, EngineError>
    redo(ObjectId floor, std::shared_ptr<AstCache> cache) const;

private:
    friend class Document;
    std::optional<EngineSnapshot> before_;
    std::optional<EngineSnapshot> after_;
};

class SetProperty final : public Command {
public:
    SetProperty(PropertyKey key, Value value);
    std::expected<bool, EngineError> execute(RecomputeEngine &model) const override;

private:
    PropertyKey key_;
    Value value_;
};

class BindFormula final : public Command {
public:
    BindFormula(PropertyKey target, std::string source, VariableMap aliases = {});
    std::expected<bool, EngineError> execute(RecomputeEngine &model) const override;

private:
    PropertyKey target_;
    std::string source_;
    VariableMap aliases_;
};
class UnbindFormula final : public Command {
public:
    explicit UnbindFormula(PropertyKey target);
    std::expected<bool, EngineError> execute(RecomputeEngine &model) const override;

private:
    PropertyKey target_;
};
class CreateRectangle final : public Command {
public:
    explicit CreateRectangle(double length = 100, double width = 1);
    std::expected<bool, EngineError> execute(RecomputeEngine &model) const override;

private:
    double length_;
    double width_;
};
class DeleteRectangle final : public Command {
public:
    explicit DeleteRectangle(ObjectId id);
    std::expected<bool, EngineError> execute(RecomputeEngine &model) const override;

private:
    ObjectId id_;
};

// 一层组合：一个用户操作、一条历史、一轮重算。
class CompositeCommand final : public Command {
public:
    explicit CompositeCommand(std::vector<std::unique_ptr<Command>> commands);
    std::expected<bool, EngineError> execute(RecomputeEngine &model) const override;

private:
    std::vector<std::unique_ptr<Command>> commands_;
};

struct DocumentEvent {
    EngineSnapshot before;
    EngineSnapshot after;
    std::vector<RecomputeStep> report;
    bool replacement = false;
};

// 订阅属于稳定的文档协调层，而非每次提交替换的对象实例。
class DocumentSignal {
    struct Slot {
        bool active = true;
        std::function<void(const DocumentEvent &)> callback;
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
        friend class DocumentSignal;
        explicit Connection(const std::shared_ptr<Slot> &slot);
        std::weak_ptr<Slot> slot_;
    };
    Connection connect(std::function<void(const DocumentEvent &)> callback);
    void emit(const DocumentEvent &event);

private:
    std::vector<std::shared_ptr<Slot>> slots_;
};

class Document {
public:
    // 初始矩形 ID=1，width=length/2，area=length*width；初始化不计入历史。
    Document();
    Document(const Document &) = delete;
    Document &operator=(const Document &) = delete;
    const RecomputeEngine &model() const;
    std::expected<bool, EngineError> execute(std::unique_ptr<Command> command);
    std::expected<bool, EngineError> undo();
    std::expected<bool, EngineError> redo();
    // 显式打开另一份文档：调用者必须先完成候选模型校验与重算。
    std::expected<void, EngineError> replace_model(std::unique_ptr<RecomputeEngine> candidate);
    bool modified() const;
    void mark_saved(); // Call only after the file has been committed successfully.
    std::uint64_t revision() const;
    std::size_t history_size() const;
    std::size_t position() const;
    DocumentSignal::Connection observe(std::function<void(const DocumentEvent &)> callback);

private:
    std::unique_ptr<RecomputeEngine> model_;
    std::vector<std::unique_ptr<Command>> history_;
    std::size_t position_ = 0;
    bool busy_ = false;
    EngineSnapshot saved_;
    std::uint64_t revision_ = 0;
    DocumentSignal changed_;
    std::expected<bool, EngineError> move_history(bool forward);
};
} // namespace month06::week03
