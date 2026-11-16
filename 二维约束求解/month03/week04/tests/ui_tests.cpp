import std;
#include "ui/workbench.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QTest>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    try {
        Workbench window;
        window.show();
        QTest::qWait(40);
        auto *canvas = window.findChild<QWidget *>("canvas");
        auto *status = window.findChild<QLabel *>("status");
        auto *reset = window.findChild<QPushButton *>("reset");
        auto *solve = window.findChild<QPushButton *>("solve");
        auto check = [](bool ok, const char *message) {
            if (!ok)
                throw std::runtime_error(message);
        };
        check(canvas && status && reset && solve, "observer widgets exist");
        check(!status->text().isEmpty(), "status is populated");
        QTest::mouseClick(solve, Qt::LeftButton);
        QTest::mouseClick(reset, Qt::LeftButton);
        auto picture = canvas->grab().toImage();
        check(!picture.isNull(), "observer produces an image");
        int nonBackground = 0;
        for (int y = 0; y < picture.height(); ++y)
            for (int x = 0; x < picture.width(); ++x)
                if (picture.pixelColor(x, y).blue() < 200)
                    ++nonBackground;
        check(nonBackground > 200, "geometry is rendered");
        check(status->text().contains("Success"), "GUI solver succeeds");
        auto *width = window.findChild<QDoubleSpinBox *>("width");
        auto *apply = window.findChild<QPushButton *>("applyWidth");
        check(width && apply, "dimension controls exist");
        width->setValue(110);
        QTest::mouseClick(apply, Qt::LeftButton);
        check(status->text().contains("Success"), "GUI dimension update converges");
        QTest::mouseClick(reset, Qt::LeftButton);
        QTest::mouseClick(window.findChild<QPushButton *>("duplicateWidth"), Qt::LeftButton);
        check(status->text().contains("Redundant"), "GUI reports redundancy");
        QTest::mouseClick(reset, Qt::LeftButton);
        QTest::mouseClick(window.findChild<QPushButton *>("conflictWidth"), Qt::LeftButton);
        check(status->text().contains("Conflicting"), "GUI reports conflict");
        QTest::mouseClick(reset, Qt::LeftButton);
        QTest::mouseClick(window.findChild<QPushButton *>("releaseWidth"), Qt::LeftButton);
        check(status->text().contains("UnderConstrained"), "GUI removes dimension");
        auto beforeDrag = canvas->grab().toImage();
        QPoint start(450, canvas->height() - 90);
        QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, start);
        QMouseEvent move(QEvent::MouseMove, QPointF(start + QPoint(20, 15)),
                         QPointF(canvas->mapToGlobal(start + QPoint(20, 15))), Qt::NoButton,
                         Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(canvas, &move);
        QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, start + QPoint(20, 15));
        check(status->text().contains("UnderConstrained"), "GUI drag respects hard constraints");
        check(canvas->grab().toImage() != beforeDrag, "mouse drag moves the rendered geometry");
        auto *mode = window.findChild<QComboBox *>("mode");
        mode->setCurrentText("Point");
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(550, 80));
        mode->setCurrentText("Line");
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(550, 100));
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(600, 100));
        mode->setCurrentText("Circle");
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(550, 160));
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(580, 160));
        QTest::mouseClick(solve, Qt::LeftButton);
        check(status->text().contains("dof=10"),
              "point, line and circle add nine degrees of freedom");
        window.grab().save("workbench.png");
        std::cout << "UI checks passed\n";
        return 0;
    } catch (const std::exception &failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
