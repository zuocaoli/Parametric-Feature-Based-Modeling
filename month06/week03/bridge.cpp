#include "bridge.hpp"
#include <QtCore/QFile>
#include <QtCore/QSaveFile>
#include <QtCore/QUrl>

import std;
import month06.week03.storage;

namespace month06::week03::ui {

using namespace month06::week03;
namespace {
QString qtext(const std::string &text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}
std::string bytes(const QString &text) {
    const auto utf8 = text.toUtf8();
    return {utf8.constData(), static_cast<std::size_t>(utf8.size())};
}
struct DisplayRow {
    ObjectId id;
    QString name;
    double x = 0, y = 0, length = 0, width = 0;
    bool visible = true, hasGeometry = false, stale = false, draggable = false;
    bool operator==(const DisplayRow &) const = default;
};
double number(const RecomputeEngine &model, ObjectId id, const char *key) {
    return std::get<double>(*model.value({id, key}));
}
} // namespace

struct EditorBridge::State {
    Document document;
    double syncMicros = 0;
    struct Draft {
        QString text;
        std::string base;
    };
    std::map<PropertyKey, Draft> drafts;
    std::vector<DisplayRow> rows;
    ObjectId selected = 0, drag = 0;
    double previewX = 0, previewY = 0;
    QString error, key;
    int start = -1, length = 0;
    DocumentSignal::Connection connection;
};

EditorBridge::EditorBridge(QObject *parent) : QAbstractListModel(parent), state_(new State) {
    state_->connection =
        state_->document.observe([this](const DocumentEvent &event) { sync(event.replacement); });
    sync();
}
EditorBridge::~EditorBridge() {
    delete state_; // Connection 先于 Document 析构，回调不会保留失效 this。
}
double EditorBridge::lastSyncMicros() const {
    return state_->syncMicros;
}
bool EditorBridge::modified() const {
    return state_->document.modified();
}
bool EditorBridge::hasDrafts() const {
    return !state_->drafts.empty();
}
void EditorBridge::editDraft(const QString &key, const QString &text) {
    const PropertyKey target{state_->selected, bytes(key)};
    if (!state_->document.model().stored_value(target))
        return;
    const auto formal = state_->document.model().formula(target).value_or("");
    if (bytes(text) == formal)
        state_->drafts.erase(target);
    else {
        auto [it, inserted] = state_->drafts.try_emplace(target, State::Draft{text, formal});
        if (!inserted)
            it->second.text = text;
    }
    Q_EMIT draftChanged();
}
void EditorBridge::discardDraft(const QString &key) {
    state_->drafts.erase({state_->selected, bytes(key)});
    clearError();
    Q_EMIT draftChanged();
    Q_EMIT uiChanged();
}
int EditorBridge::courseWeek() const {
    return COURSE_WEEK;
}
int EditorBridge::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(state_->rows.size());
}
QHash<int, QByteArray> EditorBridge::roleNames() const {
    return {{IdRole, "objectId"},
            {NameRole, "objectNameText"},
            {XRole, "modelX"},
            {YRole, "modelY"},
            {LengthRole, "modelLength"},
            {WidthRole, "modelWidth"},
            {VisibleRole, "objectVisible"},
            {GeometryRole, "hasGeometry"},
            {StaleRole, "stale"},
            {DragRole, "draggable"}};
}
QVariant EditorBridge::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }
    const auto &row = state_->rows[static_cast<std::size_t>(index.row())];
    switch (role) {
    case IdRole:
        return QString::number(row.id);
    case NameRole:
        return row.name;
    case XRole:
        return row.x;
    case YRole:
        return row.y;
    case LengthRole:
        return row.length;
    case WidthRole:
        return row.width;
    case VisibleRole:
        return row.visible;
    case GeometryRole:
        return row.hasGeometry;
    case StaleRole:
        return row.stale;
    case DragRole:
        return row.draggable;
    default:
        return {};
    }
}
QString EditorBridge::selectedId() const {
    return state_->selected ? QString::number(state_->selected) : QString{};
}
QVariantList EditorBridge::propertyRows() const {
    QVariantList result;
    const auto &model = state_->document.model();
    for (const auto &property : model.properties(state_->selected)) {
        const PropertyKey key{state_->selected, property.name};
        const auto status = model.status(key);
        const auto formula = model.formula(key);
        const auto draft = state_->drafts.find(key);
        const bool hasDraft = draft != state_->drafts.end();
        result.push_back(QVariantMap{
            {"key", qtext(property.name)},
            {"label", qtext(property.label)},
            {"unit", qtext(property.unit)},
            {"type", qtext(std::string{type_name(property.value)})},
            {"valueText", qtext(value_text(property.value))},
            {"bound", formula.has_value()},
            {"readOnly", property.read_only || formula.has_value()},
            {"formula", formula ? qtext(*formula) : QString{}},
            {"editFormula", hasDraft ? draft->second.text : qtext(formula.value_or(""))},
            {"draftConflict", hasDraft && draft->second.base != formula.value_or("")},
            {"status", qtext(std::string{state_name(status.state)})},
            {"reason", qtext(status.reason)}});
    }
    return result;
}
bool EditorBridge::canUndo() const {
    return state_->document.position() != 0;
}
bool EditorBridge::canRedo() const {
    return state_->document.position() < state_->document.history_size();
}
int EditorBridge::historyPosition() const {
    return static_cast<int>(state_->document.position());
}
QString EditorBridge::errorMessage() const {
    return state_->error;
}
QString EditorBridge::errorKey() const {
    return state_->key;
}
int EditorBridge::errorStart() const {
    return state_->start;
}
int EditorBridge::errorLength() const {
    return state_->length;
}
QString EditorBridge::previewId() const {
    return state_->drag ? QString::number(state_->drag) : QString{};
}
double EditorBridge::previewX() const {
    return state_->previewX;
}
double EditorBridge::previewY() const {
    return state_->previewY;
}
void EditorBridge::clearError() {
    state_->error.clear();
    state_->key.clear();
    state_->start = -1;
    state_->length = 0;
    Q_EMIT errorChanged();
}
bool EditorBridge::fail(const QString &message, const QString &key, int start, int length) {
    state_->error = message;
    state_->key = key;
    state_->start = start;
    state_->length = length;
    Q_EMIT errorChanged();
    return false;
}
void EditorBridge::sync(bool replacement) {
    const auto syncStart = std::chrono::steady_clock::now();
    const auto &model = state_->document.model();
    const auto ids = model.object_ids();
    if (replacement) {
        beginResetModel();
        cancelDrag();
        state_->selected = 0;
        state_->rows.clear();
        state_->drafts.clear();
        // 替换事件保持文档协调器本身稳定，仅重建显示缓存与订阅。
        state_->connection.disconnect();
        state_->connection = state_->document.observe(
            [this](const DocumentEvent &event) { sync(event.replacement); });
    } else {
        for (int i = rowCount() - 1; i >= 0; --i) {
            if (std::ranges::find(ids, state_->rows[i].id) == ids.end()) {
                beginRemoveRows({}, i, i);
                state_->rows.erase(state_->rows.begin() + i);
                endRemoveRows();
            }
        }
    }
    for (std::size_t i = 0; i < ids.size(); ++i) {
        const auto id = ids[i];
        const bool exists = i < state_->rows.size() && state_->rows[i].id == id;
        DisplayRow row = exists ? state_->rows[i] : DisplayRow{id, {}};
        row.name = qtext(std::get<std::string>(*model.value({id, "name"})));
        row.visible = std::get<bool>(*model.value({id, "visible"}));
        const bool valid = model.value({id, "x"}) && model.value({id, "y"}) &&
                           model.value({id, "length"}) && model.value({id, "width"});
        if (valid) {
            row.x = number(model, id, "x");
            row.y = number(model, id, "y");
            row.length = number(model, id, "length");
            row.width = number(model, id, "width");
            row.hasGeometry = true;
        }
        row.stale = !valid;
        row.draggable = valid && !model.formula({id, "x"}) && !model.formula({id, "y"});
        if (!exists) {
            if (!replacement) {
                beginInsertRows({}, static_cast<int>(i), static_cast<int>(i));
            }
            state_->rows.insert(state_->rows.begin() + static_cast<std::ptrdiff_t>(i), row);
            if (!replacement) {
                endInsertRows();
            }
        } else if (row != state_->rows[i]) {
            state_->rows[i] = row;
            Q_EMIT dataChanged(index(static_cast<int>(i)), index(static_cast<int>(i)));
        }
    }
    if (replacement) {
        endResetModel();
    }
    if (std::ranges::find(ids, state_->selected) == ids.end()) {
        state_->selected = 0;
    }
    if (state_->drag) {
        cancelDrag();
    }
    std::erase_if(state_->drafts,
                  [&](const auto &item) { return !model.stored_value(item.first); });
    Q_EMIT draftChanged();
    Q_EMIT uiChanged();
    if (replacement) {
        Q_EMIT documentReplaced();
    }
    state_->syncMicros =
        std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - syncStart)
            .count();
}
void EditorBridge::select(const QString &text) {
    bool ok = false;
    const auto id = text.toULongLong(&ok);
    if (!ok || state_->document.model().properties(id).empty()) {
        state_->selected = 0;
    } else {
        state_->selected = id;
    }
    clearError();
    Q_EMIT uiChanged();
}
QVariantMap EditorBridge::geometryBounds() const {
    double left = 0, bottom = 0, right = 100, top = 50;
    for (const auto &row : state_->rows) {
        if (!row.hasGeometry || !row.visible) {
            continue;
        }
        left = std::min(left, row.x);
        bottom = std::min(bottom, row.y);
        right = std::max(right, row.x + row.length);
        top = std::max(top, row.y + row.width);
    }
    return {{"left", left}, {"bottom", bottom}, {"right", right}, {"top", top}};
}
bool EditorBridge::setValue(const QString &key, const QString &text) {
    cancelDrag();
    const auto property = state_->document.model().stored_value({state_->selected, bytes(key)});
    if (!property) {
        return fail(QStringLiteral("未选择对象或属性不存在"), key);
    }
    Value value;
    const auto raw = bytes(text);
    if (std::holds_alternative<double>(*property)) {
        double parsed = 0;
        auto result = std::from_chars(raw.data(), raw.data() + raw.size(), parsed);
        if (result.ec != std::errc{} || result.ptr != raw.data() + raw.size() ||
            !std::isfinite(parsed)) {
            return fail(QStringLiteral("请输入有限数值"), key);
        }
        value = parsed;
    } else if (std::holds_alternative<std::int64_t>(*property)) {
        std::int64_t parsed = 0;
        auto result = std::from_chars(raw.data(), raw.data() + raw.size(), parsed);
        if (result.ec != std::errc{} || result.ptr != raw.data() + raw.size()) {
            return fail(QStringLiteral("请输入 64 位整数"), key);
        }
        value = parsed;
    } else if (std::holds_alternative<bool>(*property)) {
        if (text != "true" && text != "false") {
            return fail(QStringLiteral("请输入 true 或 false"), key);
        }
        value = text == "true";
    } else {
        value = raw;
    }
    auto result = state_->document.execute(
        std::make_unique<SetProperty>(PropertyKey{state_->selected, bytes(key)}, value));
    if (!result) {
        return fail(qtext(result.error().message), key);
    }
    clearError();
    return true;
}
bool EditorBridge::setFormula(const QString &key, const QString &text) {
    cancelDrag();
    editDraft(key, text);
    VariableMap aliases;
    const PropertyKey target{state_->selected, bytes(key)};
    // 保留从文件加载的跨对象映射；界面本月只编辑文本。
    for (const auto &binding : state_->document.model().snapshot().formulas) {
        if (binding.target == target) {
            aliases = binding.aliases;
        }
    }
    auto result =
        state_->document.execute(std::make_unique<BindFormula>(target, bytes(text), aliases));
    if (!result) {
        int start = -1, length = 0;
        if (const auto range = result.error().range) {
            const auto raw = text.toUtf8();
            const auto offset =
                std::min<qsizetype>(static_cast<qsizetype>(range->position), raw.size());
            const auto end =
                std::min<qsizetype>(offset + static_cast<qsizetype>(range->length), raw.size());
            start = static_cast<int>(QString::fromUtf8(raw.constData(), offset).size());
            length =
                static_cast<int>(QString::fromUtf8(raw.constData() + offset, end - offset).size());
        }
        return fail(qtext(result.error().message), key, start, length);
    }
    discardDraft(key);
    return true;
}
bool EditorBridge::clearFormula(const QString &key) {
    cancelDrag();
    auto result = state_->document.execute(
        std::make_unique<UnbindFormula>(PropertyKey{state_->selected, bytes(key)}));
    if (!result) {
        return fail(qtext(result.error().message), key);
    }
    clearError();
    return true;
}
void EditorBridge::createRectangle() {
    cancelDrag();
    auto result = state_->document.execute(std::make_unique<CreateRectangle>(80, 40));
    if (!result) {
        fail(qtext(result.error().message));
        return;
    }
    select(QString::number(state_->document.model().high_water()));
}
void EditorBridge::deleteSelected() {
    cancelDrag();
    auto result = state_->document.execute(std::make_unique<DeleteRectangle>(state_->selected));
    if (!result) {
        fail(qtext(result.error().message));
        return;
    }
    clearError();
}
void EditorBridge::undo() {
    cancelDrag();
    auto result = state_->document.undo();
    if (!result) {
        fail(qtext(result.error().message));
        return;
    }
    clearError();
}
void EditorBridge::redo() {
    cancelDrag();
    auto result = state_->document.redo();
    if (!result) {
        fail(qtext(result.error().message));
        return;
    }
    clearError();
}
bool EditorBridge::beginDrag(const QString &text) {
    cancelDrag();
    select(text);
    auto row = std::ranges::find(state_->rows, state_->selected, &DisplayRow::id);
    if (row == state_->rows.end() || !row->draggable) {
        return fail(QStringLiteral("位置或尺寸无效，或 X/Y 已绑定公式；请先修复或解除公式"));
    }
    state_->drag = row->id;
    state_->previewX = row->x;
    state_->previewY = row->y;
    Q_EMIT previewChanged();
    return true;
}
void EditorBridge::previewDrag(double x, double y) {
    if (!state_->drag || !std::isfinite(x) || !std::isfinite(y)) {
        return;
    }
    state_->previewX = x;
    state_->previewY = y;
    Q_EMIT previewChanged();
}
bool EditorBridge::finishDrag() {
    if (!state_->drag) {
        return false;
    }
    std::vector<std::unique_ptr<Command>> commands;
    commands.push_back(
        std::make_unique<SetProperty>(PropertyKey{state_->drag, "x"}, state_->previewX));
    commands.push_back(
        std::make_unique<SetProperty>(PropertyKey{state_->drag, "y"}, state_->previewY));
    cancelDrag();
    auto result = state_->document.execute(std::make_unique<CompositeCommand>(std::move(commands)));
    if (!result) {
        return fail(qtext(result.error().message));
    }
    clearError();
    return true;
}
void EditorBridge::cancelDrag() {
    if (!state_->drag) {
        return;
    }
    state_->drag = 0;
    Q_EMIT previewChanged();
}
bool EditorBridge::saveFile(const QUrl &url) {
    if (!url.isLocalFile()) {
        return fail(QStringLiteral("请选择本地文件"));
    }
    std::ostringstream output;
    auto result = save_document(state_->document, output);
    if (!result) {
        return fail(qtext(result.error().message));
    }
    const auto text = output.str();
    QSaveFile file(url.toLocalFile());
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(text.data(), static_cast<qint64>(text.size())) !=
            static_cast<qint64>(text.size()) ||
        !file.commit()) {
        return fail(file.errorString());
    }
    state_->document.mark_saved();
    Q_EMIT uiChanged();
    clearError();
    return true;
}
bool EditorBridge::openFile(const QUrl &url) {
    if (!url.isLocalFile()) {
        return fail(QStringLiteral("请选择本地文件"));
    }
    QFile file(url.toLocalFile());
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(file.errorString());
    }
    const auto content = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        return fail(file.errorString());
    }
    std::istringstream input{{content.constData(), static_cast<std::size_t>(content.size())}};
    auto result = load_document(state_->document, input);
    if (!result) {
        return fail(qtext(result.error().message));
    }
    clearError();
    return true;
}

} // namespace month06::week03::ui
