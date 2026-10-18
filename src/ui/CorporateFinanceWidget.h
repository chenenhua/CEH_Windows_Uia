#pragma once

#include <QWidget>
#include <QComboBox>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>

struct CompanyItem {
    int id{0};
    QString name;
    QString roleType; // 'OWNER', 'PARTNER', 'EMPLOYEE'
    QString industry;
    QString remark;
};

class CorporateFinanceWidget : public QWidget {
    Q_OBJECT
public:
    explicit CorporateFinanceWidget(QWidget* parent = nullptr);

    void refreshData();

private slots:
    void onCompanyChanged(int index);
    void onAddCompanyClicked();
    void onAddTransactionClicked();

private:
    void setupUi();
    void loadCompanies();
    void loadCompanyFinance(int companyId);

    QComboBox* m_companyCombo{nullptr};
    QPushButton* m_btnAddCompany{nullptr};

    QLabel* m_lblRoleBadge{nullptr};
    QLabel* m_lblBalance{nullptr};
    QLabel* m_lblMonthlyIncome{nullptr};
    QLabel* m_lblMonthlyExpense{nullptr};
    QLabel* m_lblLinkedContactsCount{nullptr};

    QTableWidget* m_transTable{nullptr};
    QTableWidget* m_contactsTable{nullptr};

    QList<CompanyItem> m_companies;
    int m_currentCompanyId{1};
};
