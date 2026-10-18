#include "FramelessWindow.h"
#include <QWindow>
#include <QScreen>
#include <QGuiApplication>
#include <QApplication>
#include <QIcon>
#include <QEvent>

FramelessWindow::FramelessWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowMinMaxButtonsHint | Qt::WindowSystemMenuHint);
    if (!QApplication::windowIcon().isNull()) {
        setWindowIcon(QApplication::windowIcon());
    }
    setAttribute(Qt::WA_Hover, true);
    setMouseTracking(true);

    resize(1920, 1080);
    setMinimumSize(1280, 720);

    // 居中屏幕
    if (QScreen* screen = QGuiApplication::primaryScreen()) {
        QRect availableGeometry = screen->availableGeometry();
        // 如果屏幕小于 1920x1080，自适应缩放到可用区域的 95%
        if (availableGeometry.width() < 1920 || availableGeometry.height() < 1080) {
            resize(qMax(1280, int(availableGeometry.width() * 0.95)),
                   qMax(720, int(availableGeometry.height() * 0.95)));
        }
        int x = (availableGeometry.width() - width()) / 2 + availableGeometry.x();
        int y = (availableGeometry.height() - height()) / 2 + availableGeometry.y();
        move(x, y);
    }

    m_topBar = new TopBar(this);

    connect(m_topBar, &TopBar::minimizeRequested, this, &QWidget::showMinimized);
    connect(m_topBar, &TopBar::maximizeRequested, this, [this]() {
        if (isMaximized()) {
            showNormal();
            m_topBar->updateMaxButtonState(false);
        } else {
            showMaximized();
            m_topBar->updateMaxButtonState(true);
        }
    });
    connect(m_topBar, &TopBar::closeRequested, this, &QWidget::close);
}

void FramelessWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::WindowStateChange) {
        if (m_topBar) {
            m_topBar->updateMaxButtonState(isMaximized());
        }
    }
    QMainWindow::changeEvent(event);
}

Qt::Edges FramelessWindow::calculateEdges(const QPoint& pos) {
    if (isMaximized()) return Qt::Edges();

    Qt::Edges edges;
    if (pos.x() <= m_borderWidth) edges |= Qt::LeftEdge;
    if (pos.x() >= width() - m_borderWidth) edges |= Qt::RightEdge;
    if (pos.y() <= m_borderWidth) edges |= Qt::TopEdge;
    if (pos.y() >= height() - m_borderWidth) edges |= Qt::BottomEdge;

    return edges;
}

void FramelessWindow::updateCursorForEdges(Qt::Edges edges) {
    if (isMaximized()) {
        setCursor(Qt::ArrowCursor);
        return;
    }

    if ((edges & Qt::LeftEdge && edges & Qt::TopEdge) ||
        (edges & Qt::RightEdge && edges & Qt::BottomEdge)) {
        setCursor(Qt::SizeFDiagCursor);
    } else if ((edges & Qt::RightEdge && edges & Qt::TopEdge) ||
               (edges & Qt::LeftEdge && edges & Qt::BottomEdge)) {
        setCursor(Qt::SizeBDiagCursor);
    } else if (edges & Qt::LeftEdge || edges & Qt::RightEdge) {
        setCursor(Qt::SizeHorCursor);
    } else if (edges & Qt::TopEdge || edges & Qt::BottomEdge) {
        setCursor(Qt::SizeVerCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
}

void FramelessWindow::mouseMoveEvent(QMouseEvent* event) {
    Qt::Edges edges = calculateEdges(event->position().toPoint());
    updateCursorForEdges(edges);
    QMainWindow::mouseMoveEvent(event);
}

void FramelessWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && !isMaximized()) {
        Qt::Edges edges = calculateEdges(event->position().toPoint());
        if (edges != Qt::Edges() && windowHandle()) {
            windowHandle()->startSystemResize(edges);
            return;
        }
    }
    QMainWindow::mousePressEvent(event);
}

void FramelessWindow::mouseReleaseEvent(QMouseEvent* event) {
    setCursor(Qt::ArrowCursor);
    QMainWindow::mouseReleaseEvent(event);
}
