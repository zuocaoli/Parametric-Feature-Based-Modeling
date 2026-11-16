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
        if (benchmark) {
            auto lesson = gcs::makeLesson();
            gcs::Solver solver(lesson.sketch);
            if (!solver.solve().converged())
                throw std::runtime_error("benchmark initialization failed");
            std::vector<double> timings;
            for (int i = 0; i < 120; ++i) {
                auto start = std::chrono::steady_clock::now();
                auto report = solver.solve();
                auto stop = std::chrono::steady_clock::now();
                if (!report.converged())
                    throw std::runtime_error("benchmark solve failed");
                if (i >= 20)
                    timings.push_back(
                        std::chrono::duration<double, std::milli>(stop - start).count());
            }
            std::ranges::sort(timings);
            std::cout << "parameters=" << lesson.sketch.parameters().size()
                      << " samples=" << timings.size() << " median_ms=" << timings[50]
                      << " p95_ms=" << timings[94] << " max_ms=" << timings.back()
                      << " (includes enabled diagnostics; week11+ includes solve and drag)\n";
            return 0;
        }
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
