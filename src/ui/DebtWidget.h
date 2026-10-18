#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include "../core/DatabaseManager.h"

class DebtWidget : public QWidget {
    Q_OBJECT
public:
    explicit DebtWidget(QWidget* parent = nullptr);

    void refreshData();

private slots:
    void onFilterChanged();
    void onAddDebtClicked();

private:
    void setupUi();

    QLabel* m_lblTotalBorrowIn{nullptr};
    QLabel* m_lblTotalLendOut{nullptr};
    QLabel* m_lblUpcomingCount{nullptr};

    QComboBox* m_filterCombo{nullptr};
    QTableWidget* m_table{nullptr};

    QList<DebtItem> m_debtList;
};
