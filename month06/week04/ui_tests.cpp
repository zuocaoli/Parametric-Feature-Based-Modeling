#include <QtCore/QBitArray>
#include "bridge.hpp"
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtQuickControls2/QQuickStyle>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

import std;

using month06::week04::ui::EditorBridge;

namespace {
QQuickItem *findItem(QQuickItem *root, const QString &name) {
    if (root->objectName() == name) {
        return root;
    }
    for (auto *child : root->childItems()) {
        if (auto *found = findItem(child, name)) {
            return found;
        }
    }
    return nullptr;
}
void check(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
QVariant role(EditorBridge &editor, int row, int role) {
    return editor.data(editor.index(row), role);
}
QVariantMap property(EditorBridge &editor, const QString &name) {
    for (const auto &row : editor.propertyRows()) {
        const auto map = row.toMap();
        if (map.value("key") == name) {
            return map;
        }
    }
    return {};
}
void adapterTests() {
    EditorBridge editor;
    editor.select("1");
    check(editor.previewExample() && !editor.modified() && editor.historyPosition() == 0,
          "UI operation preview no mutation");
    check(editor.confirmProposal() && editor.historyPosition() == 1 && editor.modified(),
          "confirmed batch one history entry");
    editor.undo();
    check(!editor.modified(), "batch undo clean");
    check(editor.previewExample(), "preview before intervening edit");
    check(editor.setValue("length", "155"), "intervening UI command");
    check(!editor.confirmProposal() && editor.errorMessage().startsWith("stale_revision"),
          "stale UI preview cannot overwrite newer edit");
    editor.undo();
    editor.discardProposal();
    check(!editor.modified(), "initial clean UI");
    editor.select("1");
    editor.editDraft("width", "length / 4");
    check(editor.hasDrafts() && !editor.modified() && editor.historyPosition() == 0,
          "draft is not document content");
    editor.select("");
    editor.select("1");
    check(property(editor, "width").value("editFormula") == "length / 4",
          "draft survives selection");
    check(editor.setValue("length", "150"), "unrelated commit while drafting");
    check(property(editor, "width").value("editFormula") == "length / 4",
          "draft survives model sync");
    editor.undo();
    check(!editor.modified(), "undo returns clean UI");
    editor.discardDraft("width");
    check(!editor.hasDrafts(), "discard draft");
    QSignalSpy updates(&editor, &QAbstractItemModel::dataChanged);
    editor.select("1");
    const auto initial = editor.historyPosition();
    check(editor.propertyRows().size() == 8 && editor.historyPosition() == initial,
          "selection does not edit");
    check(editor.setValue("length", "200"), "length input");
    check(role(editor, 0, EditorBridge::LengthRole) == 200 &&
              role(editor, 0, EditorBridge::WidthRole) == 100,
          "recomputed geometry");
    check(updates.count() == 1 && editor.historyPosition() == initial + 1,
          "one final update without echo");
    editor.createRectangle();
    check(editor.selectedId() == "2" && editor.rowCount() == 2, "create and select");
    updates.clear();
    editor.select("1");
    check(editor.setValue("length", "220") && updates.count() == 1, "incremental update");
    check(updates.front().front().value<QModelIndex>().row() == 0, "unrelated row not refreshed");
    check(editor.setValue("length", "100.12345678901234"), "precise input");
    const auto precisePosition = editor.historyPosition();
    check(editor.setValue("length", property(editor, "length").value("valueText").toString()) &&
              editor.historyPosition() == precisePosition,
          "display text roundtrips without rounding");
    check(editor.setValue("length", "220"), "restore geometry fixture");
    const auto beforeError = editor.historyPosition();
    check(!editor.setValue("width", "8") && editor.historyPosition() == beforeError,
          "formula result read-only");
    check(!editor.setFormula("width", "length width") && editor.errorStart() == 7 &&
              editor.errorLength() == 5,
          "token highlight range");
    check(!editor.setFormula("width", "length +") && editor.errorStart() == 8 &&
              editor.errorLength() == 0,
          "end cursor range");
    check(!editor.setFormula("width", QString::fromUtf8("length + 🙂")) &&
              editor.errorStart() == 9 && editor.errorLength() == 2,
          "Unicode codepoint range converted to UTF16");
    check(editor.historyPosition() == beforeError &&
              property(editor, "width").value("formula") == "length / 2",
          "failed formula preserves binding/history");
    check(editor.setFormula("width", "length / (segments - 1)"),
          "legal formula with evaluation failure");
    check(role(editor, 0, EditorBridge::StaleRole).toBool() &&
              role(editor, 0, EditorBridge::WidthRole) == 110,
          "previous valid geometry marked");
    check(property(editor, "width").value("status") == "Failed" &&
              property(editor, "area").value("status") == "Blocked",
          "failed and blocked status");
    check(editor.setValue("segments", "2"), "repair formula inputs");
    check(!role(editor, 0, EditorBridge::StaleRole).toBool() &&
              role(editor, 0, EditorBridge::WidthRole) == 220,
          "repair display");
    editor.undo();
    check(role(editor, 0, EditorBridge::StaleRole).toBool(), "undo failed state");
    editor.redo();
    check(property(editor, "width").value("status") == "Valid", "redo valid state");
    check(editor.clearFormula("width") && editor.setValue("width", "45"),
          "explicit unbind then edit");
    editor.undo();
    editor.undo();
    check(property(editor, "width").value("bound").toBool(), "undo restores formula");
    check(editor.setValue("segments", "9223372036854775807"), "64-bit integer text");
    check(property(editor, "segments").value("valueText") == "9223372036854775807",
          "no integer precision loss");
    check(!editor.setValue("segments", "9223372036854775808"), "integer overflow rejected");
    check(!editor.setValue("x", "nan"), "nonfinite position rejected");
    editor.select("2");
    const auto beforeDrag = editor.historyPosition();
    check(editor.beginDrag("2"), "begin preview");
    editor.previewDrag(-12, 35);
    check(role(editor, 1, EditorBridge::XRole) == 0 && editor.historyPosition() == beforeDrag &&
              editor.previewX() == -12,
          "preview does not edit");
    editor.cancelDrag();
    check(editor.previewId().isEmpty() && editor.historyPosition() == beforeDrag,
          "cancel no history");
    check(editor.beginDrag("2"), "restart preview");
    editor.previewDrag(-12, 35);
    check(editor.finishDrag() && editor.historyPosition() == beforeDrag + 1,
          "one move history entry");
    check(role(editor, 1, EditorBridge::XRole) == -12 && role(editor, 1, EditorBridge::YRole) == 35,
          "committed XY");
    editor.undo();
    check(role(editor, 1, EditorBridge::XRole) == 0 && role(editor, 1, EditorBridge::YRole) == 0,
          "one undo restores XY");
    check(editor.beginDrag("2") && editor.finishDrag() && editor.historyPosition() == beforeDrag,
          "no-op drag preserves redo");
    check(editor.setFormula("x", "length / 2"), "bound position");
    check(!editor.beginDrag("2"), "formula position prevents drag");
    editor.deleteSelected();
    check(editor.selectedId().isEmpty() && editor.propertyRows().isEmpty() &&
              editor.rowCount() == 1,
          "delete clears references");
    editor.undo();
    check(editor.rowCount() == 2 && editor.selectedId().isEmpty(),
          "restore ID without restoring selection");

    QTemporaryDir directory;
    check(directory.isValid(), "temporary test directory");
    const auto url = QUrl::fromLocalFile(directory.filePath("document.cad"));
    editor.select("1");
    check(editor.saveFile(url) && !editor.modified(), "save checkpoint UI");
    check(editor.setValue("length", "777"), "edit after save");
    check(!editor.saveFile(QUrl::fromLocalFile(directory.filePath("missing/invalid.cad"))) &&
              editor.modified(),
          "failed save keeps modified");
    editor.undo();
    check(!editor.modified(), "undo to disk content");
    QSignalSpy replacements(&editor, &EditorBridge::documentReplaced);
    check(editor.openFile(url) && replacements.count() == 1 && editor.historyPosition() == 0 &&
              editor.selectedId().isEmpty(),
          "replace clears history and selection");
    editor.select("1");
    check(role(editor, 0, EditorBridge::GeometryRole).toBool(), "valid loaded geometry");
    QFile file(url.toLocalFile());
    check(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "old format fixture");
    file.write("CAD_DOCUMENT 1\n");
    file.close();
    check(editor.beginDrag("1"), "preview before failed load");
    check(!editor.openFile(url) && editor.selectedId() == "1" && editor.previewId() == "1" &&
              replacements.count() == 1,
          "rejected old load preserves interface");
    editor.cancelDrag();
    QFile largeFile(url.toLocalFile());
    check(editor.saveFile(url) && largeFile.open(QIODevice::ReadOnly), "large ID fixture read");
    auto largeContent = largeFile.readAll();
    largeFile.close();
    largeContent.replace("HIGH_WATER 2", "HIGH_WATER 9007199254740994");
    check(largeFile.open(QIODevice::WriteOnly | QIODevice::Truncate), "large ID fixture write");
    largeFile.write(largeContent);
    largeFile.close();
    check(editor.openFile(url), "large allocator loads");
    editor.createRectangle();
    check(editor.selectedId() == "9007199254740995" &&
              role(editor, 2, EditorBridge::IdRole) == "9007199254740995",
          "uint64 ID has no QML precision loss");
    editor.select("1");
    // 加载失败公式的新对象没有历史有效几何，不能显示默认占位形状。
    check(editor.setFormula("width", "1 / 0") && editor.saveFile(url) && editor.openFile(url),
          "failed model load");
    check(!role(editor, 0, EditorBridge::GeometryRole).toBool() &&
              role(editor, 0, EditorBridge::StaleRole).toBool(),
          "invalid loaded cache hidden");
    editor.select("2");
    check(editor.setValue("name", "still subscribed"), "subscription after repeated replacement");
    check(role(editor, 1, EditorBridge::NameRole) == "still subscribed",
          "subscription remains active");
}

void qmlTests(QGuiApplication &application) {
    EditorBridge editor;
    QQmlApplicationEngine engine;
    int warnings = 0;
    QObject::connect(&engine, &QQmlEngine::warnings, &application,
                     [&](const QList<QQmlError> &messages) {
                         warnings += static_cast<int>(messages.size());
                         for (const auto &message : messages) {
                             qWarning().noquote() << message.toString();
                         }
                     });
    engine.setInitialProperties({{"editor", QVariant::fromValue(&editor)}});
    engine.load(QUrl("qrc:/ui/Main.qml"));
    check(!engine.rootObjects().isEmpty(), "QML application loads");
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().front());
    check(window != nullptr, "QML window");
    check(QTest::qWaitForWindowExposed(window), "offscreen window exposed");
    QTest::qWait(80);
    auto *canvas = findItem(window->contentItem(), "canvas");
    auto *shape = findItem(window->contentItem(), "rectangle-1");
    check(canvas && shape && shape->width() > 0, "canvas rectangle delegate");
    const auto before = editor.historyPosition();
    const auto center =
        shape->mapToScene(QPointF(shape->width() / 2, shape->height() / 2)).toPoint();
    if (true) {
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center);
        check(editor.selectedId() == "1" && editor.historyPosition() == before,
              "canvas selection shares ID, click no-op");
        auto *field = findItem(window->contentItem(), "value-length");
        check(field != nullptr, "generated editor exists");
        field->setProperty("text", "160");
        check(QMetaObject::invokeMethod(field, "accepted"), "submit user edit");
        QTest::qWait(30);
        check(role(editor, 0, EditorBridge::LengthRole) == 160 &&
                  editor.historyPosition() == before + 1,
              "QML edit exactly one command");
        // UI 刷新生成新字段，不能重复提交旧数值。
        QTest::qWait(30);
        check(editor.historyPosition() == before + 1, "refresh no feedback command");
    }
    {
        auto *field = findItem(window->contentItem(), "value-length");
        const auto history = editor.historyPosition();
        field->setProperty("text", "-1");
        check(QMetaObject::invokeMethod(field, "accepted"), "reject numeric QML input");
        check(field->property("text") == "160" && editor.historyPosition() == history,
              "rejected numeric field restores formal value");
        auto *draft = findItem(window->contentItem(), "formula-width");
        draft->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_End);
        QTest::keyClick(window, Qt::Key_Plus);
        check(editor.hasDrafts(), "real formula key event creates a draft");
        const auto text = draft->property("text");
        check(editor.setValue("length", "170"), "refresh during active draft");
        QTest::qWait(10);
        draft = findItem(window->contentItem(), "formula-width");
        check(draft && draft->property("text") == text, "QML draft survives delegate recreation");
        editor.undo();
        editor.discardDraft("width");
        QTest::qWait(10);
    }
    if (true) {
        auto *formula = findItem(window->contentItem(), "formula-width");
        check(formula != nullptr, "formula editor exists");
        formula->setProperty("text", "length width");
        check(QMetaObject::invokeMethod(formula, "accepted"), "QML formula submission");
        check(formula->property("selectionStart").toInt() == 7 &&
                  formula->property("selectionEnd").toInt() == 12,
              "QML range selected");
    }
    if (true) {
        editor.select("1");
        const auto geometryX = role(editor, 0, EditorBridge::XRole);
        const auto history = editor.historyPosition();
        check(QMetaObject::invokeMethod(canvas, "zoomAt", Q_ARG(QVariant, 200),
                                        Q_ARG(QVariant, 200), Q_ARG(QVariant, 2.0)),
              "view zoom");
        check(role(editor, 0, EditorBridge::XRole) == geometryX &&
                  editor.historyPosition() == history,
              "view transform does not edit model");
        const auto anchorX = canvas->property("panX").toDouble();
        const auto anchorY = canvas->property("panY").toDouble();
        const auto zoom = canvas->property("zoom").toDouble();
        check(std::abs((200 - anchorX) / zoom - 128) < 1e-8, "cursor-centered zoom X");
        check(std::isfinite(anchorY), "finite view Y");
        check(QMetaObject::invokeMethod(canvas, "fitView"), "fit view");
        QTest::qWait(30);
        const auto start =
            shape->mapToScene(QPointF(shape->width() / 2, shape->height() / 2)).toPoint();
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(window, start + QPoint(36, -24), 30);
        check(!editor.previewId().isEmpty() && editor.historyPosition() == history,
              "mouse move previews only");
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, start + QPoint(36, -24));
        check(editor.previewId().isEmpty() && editor.historyPosition() == history + 1,
              "mouse release commits once");
        check(role(editor, 0, EditorBridge::XRole).toDouble() > 0 &&
                  role(editor, 0, EditorBridge::YRole).toDouble() > 0,
              "drag converts screen coordinates");
        editor.undo();
        check(role(editor, 0, EditorBridge::XRole) == geometryX, "drag undo");
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(window, start + QPoint(15, 15), 20);
        QTest::keyClick(window, Qt::Key_Escape);
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, start + QPoint(15, 15));
        check(editor.previewId().isEmpty() && editor.historyPosition() == history,
              "Escape cancels mouse drag");
        const auto panStart = canvas->mapToScene(QPointF(15, canvas->height() / 2)).toPoint();
        const auto oldPan = canvas->property("panX").toDouble();
        QTest::mousePress(window, Qt::MiddleButton, Qt::NoModifier, panStart);
        QTest::mouseMove(window, panStart + QPoint(25, 10), 20);
        QTest::mouseRelease(window, Qt::MiddleButton, Qt::NoModifier, panStart + QPoint(25, 10));
        check(canvas->property("panX").toDouble() == oldPan + 25 &&
                  editor.historyPosition() == history,
              "middle-button pan only changes view");
    }
    QTest::qWait(20);
    editor.discardDraft("width");
    auto *previewButton = findItem(window->contentItem(), "previewOperations");
    check(previewButton, "preview button exists");
    const auto history = editor.historyPosition();
    QTest::mouseClick(
        window, Qt::LeftButton, Qt::NoModifier,
        previewButton->mapToScene(QPointF(previewButton->width() / 2, previewButton->height() / 2))
            .toPoint());
    QTest::qWait(30);
    auto *dialog = window->findChild<QObject *>("operationConfirmation");
    check(dialog && dialog->property("visible").toBool() && editor.historyPosition() == history,
          "click opens real confirmation without editing");
    const auto root = std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path();
    check(window->grabWindow().save(
              QString::fromStdString((root / "month06/week04/build/preview.png").string())),
          "preview screenshot");
    check(QMetaObject::invokeMethod(dialog, "accept"), "confirm actual QML dialog");
    check(editor.historyPosition() == history + 1 &&
              role(editor, 0, EditorBridge::LengthRole) == 200,
          "dialog acceptance submits exactly one transaction");
    editor.undo();
    check(editor.historyPosition() == history, "confirmed transaction undoes once");
    check(warnings == 0, "QML has no runtime warnings");
}
void uiBenchmarks() {
    std::cout << "objects,adapter_sync_us,qml_render_capture_us,data_changed_per_edit\n";
    for (int count : {20, 100, 300}) {
        EditorBridge editor;
        QQmlApplicationEngine engine;
        engine.setInitialProperties({{"editor", QVariant::fromValue(&editor)}});
        engine.load(QUrl("qrc:/ui/Main.qml"));
        check(!engine.rootObjects().isEmpty(), "benchmark QML load");
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().front());
        check(window && QTest::qWaitForWindowExposed(window), "benchmark window");
        for (int i = 1; i < count; ++i)
            editor.createRectangle();
        editor.select("1");
        QTest::qWait(30);
        std::vector<double> adapter, render;
        QSignalSpy changed(&editor, &QAbstractItemModel::dataChanged);
        for (int i = 0; i < 8; ++i) {
            check(editor.setValue("length", QString::number(200 + i)), "benchmark edit");
            const auto sync = editor.lastSyncMicros();
            const auto start = std::chrono::steady_clock::now();
            QCoreApplication::processEvents();
            check(!window->grabWindow().isNull(), "benchmark render capture");
            const auto time =
                std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start)
                    .count();
            if (i) {
                adapter.push_back(sync);
                render.push_back(time);
            }
        }
        std::ranges::sort(adapter);
        std::ranges::sort(render);
        std::cout << count << ',' << adapter[3] << ',' << render[3] << ',' << changed.count() / 8.0
                  << '\n';
    }
}
} // namespace

int main(int argc, char **argv) {
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication application(argc, argv);
    try {
        if (argc == 2 && std::string_view{argv[1]} == "--benchmark") {
            uiBenchmarks();
            return 0;
        }
        adapterTests();
        qmlTests(application);
        std::cout << "month06/week" << std::setfill('0') << std::setw(2) << COURSE_WEEK
                  << " UI tests passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
