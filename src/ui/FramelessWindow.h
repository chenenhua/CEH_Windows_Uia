#pragma once

#include <QMainWindow>
#include <QMouseEvent>
#include "TopBar.h"

class FramelessWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit FramelessWindow(QWidget* parent = nullptr);

protected:
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void changeEvent(QEvent* event) override;

    TopBar* m_topBar{nullptr};

private:
    Qt::Edges calculateEdges(const QPoint& pos);
    void updateCursorForEdges(Qt::Edges edges);

    const int m_borderWidth = 8;
};
