#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>

class PersonalFinanceWidget : public QWidget {
    Q_OBJECT
public:
    explicit PersonalFinanceWidget(QWidget* parent = nullptr);

    void refreshData();

private slots:
    void onAddTransactionClicked();
    void onFilterChanged();

private:
    void setupUi();
    void loadPersonalStats();
    void loadTransactions();

    QLabel* m_lblPersonalBalance{nullptr};
    QLabel* m_lblMonthlyIncome{nullptr};
    QLabel* m_lblMonthlyExpense{nullptr};
    QLabel* m_lblSafetyMonths{nullptr};

    QComboBox* m_filterCombo{nullptr};
    QLineEdit* m_searchEdit{nullptr};
    QTableWidget* m_table{nullptr};
};
