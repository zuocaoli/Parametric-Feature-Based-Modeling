import std;
#include "ui/workbench.hpp"
#include <QApplication>
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
        auto check = [](bool ok, const char *message) {
            if (!ok)
                throw std::runtime_error(message);
        };
        check(canvas && status, "observer widgets exist");
        check(status->text() == "Parameters: 8", "four points own eight coordinate parameters");
        check(window.findChildren<QPushButton *>().isEmpty(),
              "observation-only lesson has no action buttons");
        auto picture = canvas->grab().toImage();
        check(!picture.isNull(), "observer produces an image");
        int nonBackground = 0;
        for (int y = 0; y < picture.height(); ++y)
            for (int x = 0; x < picture.width(); ++x)
                if (picture.pixelColor(x, y).blue() < 200)
                    ++nonBackground;
        check(nonBackground > 200, "geometry is rendered");
        window.grab().save("workbench.png");
        std::cout << "UI checks passed\n";
        return 0;
    } catch (const std::exception &failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
