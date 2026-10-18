#pragma once

#include "CustomDialog.h"
#include "../core/DatabaseManager.h"
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QDateEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>

class SocialPlanDialog : public CustomDialog {
    Q_OBJECT
public:
    // 制定新计划 (走动计划)
    explicit SocialPlanDialog(int preselectedContactId = 0, QWidget* parent = nullptr);
    // 执行/修改既有计划 (计划总结)
    explicit SocialPlanDialog(const SocialPlanItem& item, QWidget* parent = nullptr);

private slots:
    void onBookScopeToggled(bool isPersonal);
    void onCreatePlan();
    void onCompletePlanWithExpense();
    void onCancelPlan();

private:
    void setupCreateUi(int preselectedContactId);
    void setupExecuteUi(const SocialPlanItem& item);
    void reloadScopeCategoriesAndTargets();

    bool m_isExecuteMode{false};
    int m_preselectedContactId{0};
    QString m_currentScope{"PERSONAL"};
    SocialPlanItem m_existingItem;

    // 制定计划 / 走动计划字段
    QPushButton* m_btnPersonal{nullptr};
    QPushButton* m_btnWork{nullptr};
    QComboBox* m_ownerCombo{nullptr};       // 归属类别: 个人->人名字, 工作->公司名字
    QComboBox* m_planTypeCombo{nullptr};    // 事项类型下拉 (联动 人脉社交 / 商务社交)
    QLineEdit* m_customPlanTypeEdit{nullptr}; // 右侧自定义事项类型输入
    QPlainTextEdit* m_titleEdit{nullptr};   // 计划主题 (3行可见 textarea)
    QDateEdit* m_dateEdit{nullptr};         // 预定日期 (点击弹出日历选择)
    QSpinBox* m_remindSpin{nullptr};        // 提前预警天数
    QPlainTextEdit* m_remarkEdit{nullptr};  // 补充备忘 / 结案总结 (3行可见 textarea)

    // 计划总结 (执行结案) 字段
    QDoubleSpinBox* m_expenseSpin{nullptr};
    QComboBox* m_expenseCategoryCombo{nullptr};
    QLineEdit* m_customExpenseCategoryEdit{nullptr};
};
