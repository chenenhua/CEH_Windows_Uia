#pragma once

#include "CustomDialog.h"
#include "../core/DatabaseManager.h"
#include <QButtonGroup>
#include <QPushButton>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QListWidget>
#include <QLabel>

class UnifiedTransactionDialog : public CustomDialog {
    Q_OBJECT
public:
    explicit UnifiedTransactionDialog(const QString& defaultType = "EXPENSE",
                                      QWidget* parent = nullptr,
                                      bool hideTransTypeRow = false,
                                      const QString& defaultBookType = "PERSONAL",
                                      bool hideBookTypeRow = false);

    void setPreselectedContact(int contactId);
    void setEditTransactionId(int transactionId);

private slots:
    void onTransTypeToggled(bool isExpense);
    void onBookTypeToggled(bool isPersonal);
    void onParentCategorySelected(QListWidgetItem* current, QListWidgetItem* previous);
    void onSelectVoucherClicked();
    void onSaveTransaction();

private:
    void setupUi();
    void loadCategories();
    void loadCompanies();
    void loadRelations();

    int m_editTransId{0};
    QString m_currentTransType{"EXPENSE"};
    QString m_currentBookType{"PERSONAL"};
    bool m_hideTransTypeRow{false};
    bool m_hideBookTypeRow{false};
    QString m_voucherPath;

    // Row 1: 支出 / 收入 (可按调用场景隐藏)
    QWidget* m_transTypeRowWidget{nullptr};
    QPushButton* m_btnExpense{nullptr};
    QPushButton* m_btnIncome{nullptr};

    // Row 2: 个人 / 工作 (可按调用场景隐藏)
    QWidget* m_bookTypeRowWidget{nullptr};
    QPushButton* m_btnPersonal{nullptr};
    QPushButton* m_btnCompany{nullptr};

    // Row 3: 选择公司... (若选工作且公司数>1时显示，否则隐藏)
    QWidget* m_companyRow{nullptr};
    QComboBox* m_companyCombo{nullptr};

    // Row 4: 大类与小类双列滚动选择列表 (~5项可视)
    QListWidget* m_parentListWidget{nullptr};
    QListWidget* m_subListWidget{nullptr};

    // Row 5: 自定义大类 | 自定义小类
    QLineEdit* m_customParentEdit{nullptr};
    QLineEdit* m_customSubEdit{nullptr};

    // Row 6: 金额 | 时间
    QDoubleSpinBox* m_amountSpin{nullptr};
    QDateTimeEdit* m_timeEdit{nullptr};

    // Row 7: 关联人、公司、商家
    QComboBox* m_relationCombo{nullptr};

    // Row 8: [凭证] 方形按钮 | 补充说明...
    QPushButton* m_btnVoucher{nullptr};
    QLineEdit* m_remarkEdit{nullptr};

    // Row 9: 取消 | 保存
    QPushButton* m_btnCancel{nullptr};
    QPushButton* m_btnSave{nullptr};

    QList<CategoryHierarchyItem> m_hierarchyList;
};
