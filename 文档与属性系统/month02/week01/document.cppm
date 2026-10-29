export module month04.week01.document;

import std;
export import month04.week01.engine;

export namespace month04::week01 {
// 命令保留值快照，undo/redo 不依赖对象地址。
class Command {
public:
    virtual ~Command() = default;
    virtual std::expected<bool, EngineError> execute(RecomputeEngine &model) const = 0;
    std::expected<std::unique_ptr<RecomputeEngine>, EngineError> undo(ObjectId floor) const;
    std::expected<std::unique_ptr<RecomputeEngine>, EngineError> redo(ObjectId floor) const;

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
    std::size_t history_size() const;
    std::size_t position() const;
    DocumentSignal::Connection observe(std::function<void(const DocumentEvent &)> callback);

private:
    std::unique_ptr<RecomputeEngine> model_;
    std::vector<std::unique_ptr<Command>> history_;
    std::size_t position_ = 0;
    bool busy_ = false;
    DocumentSignal changed_;
    std::expected<bool, EngineError> move_history(bool forward);
};
} // namespace month04::week01
