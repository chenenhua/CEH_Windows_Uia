#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QPoint>

class TopBar : public QWidget {
    Q_OBJECT
public:
    explicit TopBar(QWidget* parent = nullptr);

    void setStatus(const QString& text, bool isOnline = true);
    void updateAiStatus(bool isConfigured, const QString& modelName, bool isRunning = false, bool isAvailable = true);
    void updateMaxButtonState(bool isMaximized);
    void setTopologyButtonChecked(bool checked);
    void setBusinessNetButtonChecked(bool checked);

signals:
    void minimizeRequested();
    void maximizeRequested();
    void closeRequested();
    void topologyToggleRequested(bool active);
    void businessNetToggleRequested(bool active);
    void modelSettingsRequested();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    QLabel* m_logoIcon{nullptr};
    QLabel* m_appTitle{nullptr};
    QPushButton* m_aiStatusBtn{nullptr};
    QLabel* m_aiRunningLabel{nullptr};
    QPushButton* m_topologyBtn{nullptr};
    QPushButton* m_businessNetBtn{nullptr};
    QPushButton* m_minBtn{nullptr};
    QPushButton* m_maxBtn{nullptr};
    QPushButton* m_closeBtn{nullptr};
};
