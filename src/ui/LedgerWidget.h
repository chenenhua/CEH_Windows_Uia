#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>

class LedgerWidget : public QWidget {
    Q_OBJECT
public:
    explicit LedgerWidget(QWidget* parent = nullptr);

    void refreshData();

private slots:
    void onFilterChanged();
    void onAddTransactionClicked();

private:
    void setupUi();

    QLabel* m_lblPersonalBal{nullptr};
    QLabel* m_lblCompanyBal{nullptr};
    QLabel* m_lblRunway{nullptr};

    QComboBox* m_bookFilter{nullptr};
    QTableWidget* m_table{nullptr};
};
