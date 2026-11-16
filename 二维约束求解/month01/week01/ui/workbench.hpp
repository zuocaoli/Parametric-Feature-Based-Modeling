#pragma once
#include <QWidget>
#include <memory>

class Workbench : public QWidget {
  public:
    explicit Workbench(QWidget *parent = nullptr);
    ~Workbench() override;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
