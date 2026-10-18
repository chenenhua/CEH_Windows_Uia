#pragma once

#include "CustomDialog.h"
#include <QComboBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QDoubleSpinBox>
#include <QDateEdit>
#include <QPushButton>

class RelationshipDiaryDialog : public CustomDialog {
    Q_OBJECT
public:
    explicit RelationshipDiaryDialog(int contactId, const QString& contactName, QWidget* parent = nullptr);
    explicit RelationshipDiaryDialog(int eventId, int contactId, const QString& contactName, QWidget* parent = nullptr);

    bool saveRecord();

private slots:
    void onScopeToggled(bool isPersonal);
    void onIoTypeToggled(bool isExpense);

private:
    void setupUi();
    void reloadScopeData();
    void reloadCategories();
    void loadEventData(int eventId);

    int m_contactId{0};
    int m_editEventId{0};
    QString m_contactName;
    QString m_currentScope{"PERSONAL"}; // 'PERSONAL', 'COMPANY'
    QString m_currentIoType{"EXPENSE"}; // 'EXPENSE' (投资这个人), 'INCOME' (这个人回报)

    // 顶部切换: 个人 | 工作
    QPushButton* m_btnPersonal{nullptr};
    QPushButton* m_btnWork{nullptr};

    // 第二行切换: 支出(投资这个人) | 收入(这个人回报)
    QPushButton* m_btnExpense{nullptr};
    QPushButton* m_btnIncome{nullptr};

    // 表单字段
    QComboBox* m_ownerCombo{nullptr};          // 归属类别: 个人->人名字, 工作->公司名字
    QComboBox* m_categoryCombo{nullptr};       // 事件分类下拉
    QLineEdit* m_customCategoryEdit{nullptr};  // 自定义分类输入
    QDoubleSpinBox* m_amountSpin{nullptr};     // 往来金额
    QPlainTextEdit* m_titleEdit{nullptr};      // 事件摘要 (3行可见 textarea)
    QDateEdit* m_dateEdit{nullptr};            // 预定日期 (点击弹出日历选择)
    QPlainTextEdit* m_detailEdit{nullptr};     // 补充说明 (3行可见 textarea)
};
