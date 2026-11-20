import std;
import mini_gcs.lesson;
#include "workbench.hpp"
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

namespace {
class Canvas : public QWidget {
  public:
    gcs::Lesson &lesson;
    explicit Canvas(gcs::Lesson &l) : lesson(l) {
        setObjectName("canvas");
        setMinimumSize(640, 420);
    }
    QPointF screen(gcs::Vec2 p) const {
        return {50 + p.x * 4, height() - 50 - p.y * 4};
    }
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), QColor("#f5f7fa"));
        auto &s = lesson.sketch;
        painter.setPen(QPen(QColor("#245a81"), 2));
        for (const auto &e : s.entities()) {
            if (e.kind == gcs::EntityKind::Line)
                painter.drawLine(screen(s.point(e.refs[0])), screen(s.point(e.refs[1])));
            else if (e.kind == gcs::EntityKind::Circle || e.kind == gcs::EntityKind::Arc) {
                auto center = s.point(e.refs[0]);
                double r = s.radius(e.id);
                QRectF bounds(screen({center.x - r, center.y + r}),
                              screen({center.x + r, center.y - r}));
                if (e.kind == gcs::EntityKind::Circle)
                    painter.drawEllipse(bounds);
                else {
                    auto a = s.point(e.refs[1]), b = s.point(e.refs[2]);
                    double angle = std::atan2(a.y - center.y, a.x - center.x);
                    double sweep = std::atan2(b.y - center.y, b.x - center.x) - angle;
                    if (e.clockwise) {
                        while (sweep >= 0)
                            sweep -= 2 * std::numbers::pi;
                    } else {
                        while (sweep <= 0)
                            sweep += 2 * std::numbers::pi;
                    }
                    painter.drawArc(bounds, static_cast<int>(angle * 180 / std::numbers::pi * 16),
                                    static_cast<int>(sweep * 180 / std::numbers::pi * 16));
                }
            } else {
                auto position = screen(s.point(e.id));
                painter.setBrush(QColor("#245a81"));
                painter.drawEllipse(position, 4, 4);
                painter.drawText(position + QPointF(6, -6), QString::number(e.id));
                painter.setBrush(Qt::NoBrush);
            }
        }
    }
};
}
struct Workbench::Impl {
    gcs::Lesson lesson = gcs::makeLesson();
    Canvas *canvas = nullptr;
    QLabel *status = nullptr;
    void refresh() {
        try {
            auto message = gcs::runLesson(lesson);
            status->setText(QString::fromStdString(message));
            canvas->update();
        } catch (const std::exception &error) {
            status->setText(QString::fromUtf8(error.what()));
        }
    }
};
Workbench::Workbench(QWidget *parent) : QWidget(parent), impl_(std::make_unique<Impl>()) {
    setWindowTitle("Mini 2D GCS — 几何元素与参数管理");
    auto *layout = new QVBoxLayout(this);
    impl_->canvas = new Canvas(impl_->lesson);
    impl_->status = new QLabel;
    impl_->status->setObjectName("status");
    impl_->status->setWordWrap(true);
    layout->addWidget(impl_->canvas, 1);
    layout->addWidget(impl_->status);
    resize(1100, 600);
    impl_->refresh();
}
Workbench::~Workbench() = default;
