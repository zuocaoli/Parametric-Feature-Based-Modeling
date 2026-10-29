#include "bridge.hpp"
#include <QtCore/QTimer>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtQuickControls2/QQuickStyle>

using month05::week03::ui::EditorBridge;

int main(int argc, char **argv) {
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("Learn CAD"));
    EditorBridge editor;
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{"editor", QVariant::fromValue(&editor)}});
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &application,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/ui/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    // 可重复的截图入口，产物由调用者指定。
    if (argc == 3 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--screenshot")) {
        editor.select(QStringLiteral("1"));
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().front());
        if (auto *canvas = window->findChild<QQuickItem *>(QStringLiteral("canvas"))) {
            QMetaObject::invokeMethod(canvas, "fitView");
        }
        const auto path = QString::fromLocal8Bit(argv[2]);
        QTimer::singleShot(300, &application, [window, path] {
            QCoreApplication::exit(window->grabWindow().save(path) ? 0 : 1);
        });
    }
    return application.exec();
}
