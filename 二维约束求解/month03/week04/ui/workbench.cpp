import std;
import mini_gcs.lesson;
#include "workbench.hpp"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
class Canvas : public QWidget {
  public:
    gcs::Lesson &lesson;
    std::vector<std::size_t> movable;
    std::vector<gcs::ConstraintId> highlighted;
    std::function<void(gcs::PointId, double, double)> drag;
    std::function<void()> endDrag;
    std::function<void(QString)> error;
    QString mode = "Select / drag";
    gcs::PointId selected = 0;
    std::optional<gcs::Vec2> constructionStart;
    explicit Canvas(gcs::Lesson &l) : lesson(l) {
        setObjectName("canvas");
        setMinimumSize(640, 420);
        setMouseTracking(true);
    }
    QPointF screen(gcs::Vec2 p) const {
        return {50 + p.x * 4, height() - 50 - p.y * 4};
    }
    gcs::Vec2 model(QPointF p) const {
        return {(p.x() - 50) / 4, (height() - 50 - p.y()) / 4};
    }
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), QColor("#f5f7fa"));
        auto &s = lesson.sketch;
        std::vector<gcs::EntityId> highlightEntities;
        for (auto id : highlighted) {
            const auto &c = s.constraint(id);
            highlightEntities.insert(highlightEntities.end(), c.refs.begin(), c.refs.end());
        }
        for (const auto &e : s.entities()) {
            bool active = std::ranges::find(highlightEntities, e.id) != highlightEntities.end();
            painter.setPen(QPen(active ? QColor("#c33c54") : QColor("#245a81"), active ? 3 : 2));
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
                bool moving = std::ranges::any_of(e.parameters, [&](auto index) {
                    return std::ranges::find(movable, index) != movable.end();
                });
                auto position = screen(s.point(e.id));
                painter.setBrush(moving ? QColor("#e9a23b") : QColor("#245a81"));
                painter.drawEllipse(position, 4, 4);
                painter.drawText(position + QPointF(6, -6), QString::number(e.id));
                painter.setBrush(Qt::NoBrush);
            }
        }
    }
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() != Qt::LeftButton)
            return;
        auto p = model(event->position());
        try {
            auto &s = lesson.sketch;
            if (mode == "Point")
                s.addPoint(p.x, p.y);
            else if (mode == "Line" || mode == "Circle") {
                if (!constructionStart) {
                    constructionStart = p;
                    return;
                }
                auto start = *constructionStart;
                constructionStart.reset();
                double radius = std::hypot(start.x - p.x, start.y - p.y);
                if (radius < 1e-6)
                    throw std::invalid_argument("choose two different positions");
                auto first = s.addPoint(start.x, start.y);
                if (mode == "Line")
                    s.addLine(first, s.addPoint(p.x, p.y));
                else
                    s.addCircle(first, radius);
            } else {
                double best = 12;
                for (const auto &e : s.entities()) {
                    if (e.kind != gcs::EntityKind::Point)
                        continue;
                    double distance = QLineF(screen(s.point(e.id)), event->position()).length();
                    if (distance < best) {
                        best = distance;
                        selected = e.id;
                    }
                }
            }
            update();
        } catch (const std::exception &failure) {
            if (error)
                error(QString::fromUtf8(failure.what()));
        }
    }
    void mouseMoveEvent(QMouseEvent *event) override {
        if (selected && (event->buttons() & Qt::LeftButton) && drag) {
            auto p = model(event->position());
            drag(selected, p.x, p.y);
            update();
        }
    }
    void mouseReleaseEvent(QMouseEvent *) override {
        if (selected && endDrag)
            endDrag();
        selected = 0;
    }
};
}
struct Workbench::Impl {
    gcs::Lesson lesson = gcs::makeLesson();
    std::unique_ptr<gcs::Solver> solver = std::make_unique<gcs::Solver>(lesson.sketch);
    Canvas *canvas = nullptr;
    QLabel *status = nullptr;
    QDoubleSpinBox *width = nullptr;
    void refresh() {
        try {
            auto report = solver->solve();
            display(report);
            canvas->update();
        } catch (const std::exception &error) {
            status->setText(QString::fromUtf8(error.what()));
        }
    }
    void display(const gcs::SolveReport &report) {
        std::ostringstream text;
        text << gcs::statusName(report.status) << " | dof=" << report.dof
             << " | structural=" << report.structuralDof << " | residual=" << report.finalResidual;
        if (!report.reason.empty())
            text << " | " << report.reason;
        for (auto id : report.redundant)
            text << " | redundant #" << id;
        for (auto id : report.conflicting)
            text << " | conflict #" << id;
        status->setText(QString::fromStdString(text.str()));
        canvas->movable = report.movableParameters;
        canvas->highlighted = report.conflicting.empty() ? report.redundant : report.conflicting;
    }
};
Workbench::Workbench(QWidget *parent) : QWidget(parent), impl_(std::make_unique<Impl>()) {
    setWindowTitle("Mini 2D GCS — 交互验证端");
    auto *layout = new QVBoxLayout(this);
    impl_->canvas = new Canvas(impl_->lesson);
    impl_->status = new QLabel;
    impl_->status->setObjectName("status");
    impl_->status->setWordWrap(true);
    auto *buttons = new QHBoxLayout;
    auto *reset = new QPushButton("Reset");
    reset->setObjectName("reset");
    auto *solve = new QPushButton("Evaluate / solve");
    solve->setObjectName("solve");
    buttons->addWidget(reset);
    buttons->addWidget(solve);
    connect(solve, &QPushButton::clicked, this, [this] { impl_->refresh(); });
    connect(reset, &QPushButton::clicked, this, [this] {
        impl_->lesson = gcs::makeLesson();
        impl_->solver = std::make_unique<gcs::Solver>(impl_->lesson.sketch);
        if (impl_->width)
            impl_->width->setValue(100);
        impl_->canvas->movable.clear();
        impl_->canvas->highlighted.clear();
        impl_->canvas->selected = 0;
        impl_->canvas->constructionStart.reset();
        impl_->refresh();
    });
    impl_->width = new QDoubleSpinBox;
    impl_->width->setObjectName("width");
    impl_->width->setRange(1, 1000);
    impl_->width->setValue(100);
    buttons->addWidget(new QLabel("Width (mm)"));
    buttons->addWidget(impl_->width);
    auto *apply = new QPushButton("Apply width");
    apply->setObjectName("applyWidth");
    buttons->addWidget(apply);
    connect(apply, &QPushButton::clicked, this, [this] {
        try {
            impl_->lesson.sketch.setConstraintValue(impl_->lesson.width, impl_->width->value());
            impl_->refresh();
        } catch (const std::exception &e) {
            impl_->status->setText(QString::fromUtf8(e.what()));
        }
    });
    auto *mode = new QComboBox;
    mode->setObjectName("mode");
    mode->addItems({"Select / drag", "Point", "Line", "Circle"});
    buttons->addWidget(mode);
    connect(mode, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        impl_->canvas->mode = text;
        impl_->canvas->constructionStart.reset();
    });
    auto *release = new QPushButton("Remove width");
    release->setObjectName("releaseWidth");
    buttons->addWidget(release);
    connect(release, &QPushButton::clicked, this, [this, release] {
        try {
            impl_->lesson.sketch.removeConstraint(impl_->lesson.width);
            impl_->lesson.width = 0;
            impl_->refresh();
        } catch (const std::exception &e) {
            impl_->status->setText(QString::fromUtf8(e.what()));
        }
        (void)release;
    });
    auto diagnostic = [&](const char *label, const char *name, double target) {
        auto *button = new QPushButton(label);
        button->setObjectName(name);
        buttons->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, target] {
            try {
                const auto &width = impl_->lesson.sketch.constraint(impl_->lesson.width);
                impl_->lesson.sketch.addConstraint(width.type, width.refs,
                                                   target ? target : width.value);
                impl_->refresh();
            } catch (const std::exception &e) {
                impl_->status->setText(QString::fromUtf8(e.what()));
            }
        });
    };
    diagnostic("Duplicate width", "duplicateWidth", 0);
    diagnostic("Conflicting width", "conflictWidth", 120);
    impl_->canvas->drag = [this](gcs::PointId p, double x, double y) {
        impl_->display(impl_->solver->drag(p, x, y));
        impl_->canvas->update();
    };
    impl_->canvas->endDrag = [this] { impl_->solver->endDrag(); };
    impl_->canvas->error = [this](QString text) { impl_->status->setText(text); };
    layout->addLayout(buttons);
    layout->addWidget(impl_->canvas, 1);
    layout->addWidget(impl_->status);
    resize(1100, 600);
    impl_->refresh();
}
Workbench::~Workbench() = default;
