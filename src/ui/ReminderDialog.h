#pragma once

#include "CustomDialog.h"
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>

class ReminderDialog : public CustomDialog {
    Q_OBJECT
public:
    explicit ReminderDialog(QWidget* parent = nullptr);

    static bool checkAndShowIfNeeded(QWidget* parent = nullptr);

private slots:
    void onMarkPlanCompleted(int planId);

private:
    void setupUi();
    void loadReminders();

    QTableWidget* m_table{nullptr};
    QLabel* m_summaryLabel{nullptr};
};
