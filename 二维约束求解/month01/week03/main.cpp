import std;
import mini_gcs.lesson;
#ifdef MINI_GCS_WITH_GUI
#include <QApplication>
#include "ui/workbench.hpp"
#endif

int main(int argc, char **argv) {
    try {
        bool headless = false;
        bool benchmark = false;
        for (int i = 1; i < argc; ++i) {
            std::string_view arg(argv[i]);
            if (arg == "--headless")
                headless = true;
            else if (arg == "--benchmark") {
                headless = true;
                benchmark = true;
            } else
                throw std::invalid_argument("usage: demo [--headless | --benchmark]");
        }
        if (benchmark)
            throw std::invalid_argument(
                "benchmark is introduced with the solver in month01/week04");
#ifdef MINI_GCS_WITH_GUI
        if (!headless) {
            QApplication app(argc, argv);
            Workbench window;
            window.show();
            return app.exec();
        }
#else
        (void)headless;
#endif
        auto lesson = gcs::makeLesson();
        std::cout << gcs::runLesson(lesson) << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
