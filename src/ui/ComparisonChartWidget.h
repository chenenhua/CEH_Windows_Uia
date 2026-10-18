#pragma once

#include <QWidget>
#include <QLabel>
#include <QFrame>
#include "../core/DatabaseManager.h"

class ComparisonChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit ComparisonChartWidget(QWidget* parent = nullptr);

    void setStats(const ContactInteractionStats& stats);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void setupUi();

    QLabel* m_lblTitle{nullptr};
    QLabel* m_lblMeetDate{nullptr};
    QLabel* m_lblCostVsValue{nullptr};

    ContactInteractionStats m_stats;
};
