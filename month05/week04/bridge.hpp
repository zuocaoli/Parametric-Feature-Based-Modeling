#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QVariantList>
#include <QtCore/QUrl>

namespace month05::week04::ui {

// Qt 的元对象接口只使用 Qt 类型；核心模块留在实现文件。
class EditorBridge final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int courseWeek READ courseWeek CONSTANT)
    Q_PROPERTY(QString selectedId READ selectedId NOTIFY uiChanged)
    Q_PROPERTY(QVariantList propertyRows READ propertyRows NOTIFY uiChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY uiChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY uiChanged)
    Q_PROPERTY(int historyPosition READ historyPosition NOTIFY uiChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorChanged)
    Q_PROPERTY(QString errorKey READ errorKey NOTIFY errorChanged)
    Q_PROPERTY(int errorStart READ errorStart NOTIFY errorChanged)
    Q_PROPERTY(int errorLength READ errorLength NOTIFY errorChanged)
    Q_PROPERTY(QString previewId READ previewId NOTIFY previewChanged)
    Q_PROPERTY(double previewX READ previewX NOTIFY previewChanged)
    Q_PROPERTY(double previewY READ previewY NOTIFY previewChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        NameRole,
        XRole,
        YRole,
        LengthRole,
        WidthRole,
        VisibleRole,
        GeometryRole,
        StaleRole,
        DragRole
    };
    explicit EditorBridge(QObject *parent = nullptr);
    ~EditorBridge() override;
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int courseWeek() const;
    QString selectedId() const;
    QVariantList propertyRows() const;
    bool canUndo() const;
    bool canRedo() const;
    int historyPosition() const;
    QString errorMessage() const;
    QString errorKey() const;
    int errorStart() const;
    int errorLength() const;
    QString previewId() const;
    double previewX() const;
    double previewY() const;

    Q_INVOKABLE void select(const QString &id);
    Q_INVOKABLE QVariantMap geometryBounds() const;
    Q_INVOKABLE bool setValue(const QString &key, const QString &text);
    Q_INVOKABLE bool setFormula(const QString &key, const QString &text);
    Q_INVOKABLE bool clearFormula(const QString &key);
    Q_INVOKABLE void createRectangle();
    Q_INVOKABLE void deleteSelected();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE bool beginDrag(const QString &id);
    Q_INVOKABLE void previewDrag(double x, double y);
    Q_INVOKABLE bool finishDrag();
    Q_INVOKABLE void cancelDrag();
    Q_INVOKABLE bool saveFile(const QUrl &url);
    Q_INVOKABLE bool openFile(const QUrl &url);

Q_SIGNALS:
    void uiChanged();
    void errorChanged();
    void previewChanged();
    void documentReplaced();

private:
    struct State;
    State *state_;
    void sync(bool replacement = false);
    void clearError();
    bool fail(const QString &message, const QString &key = {}, int start = -1, int length = 0);
};

} // namespace month05::week04::ui
