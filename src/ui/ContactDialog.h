#pragma once

#include "CustomDialog.h"
#include "../core/DatabaseManager.h"
#include <QLineEdit>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QDateEdit>
#include <QTabWidget>

class ContactDialog : public CustomDialog {
    Q_OBJECT
public:
    // 新增联系人模式
    explicit ContactDialog(QWidget* parent = nullptr);
    // 编辑既有联系人模式
    explicit ContactDialog(const ContactSummary& contact, QWidget* parent = nullptr);

private slots:
    void onSaveContact();
    void onProvinceChanged(int index);
    void onCityChanged(int index);

private:
    void setupUi();
    void loadCombos();
    void selectRegions(const QString& prov, const QString& city, const QString& dist);

    bool m_isEditMode{false};
    int m_contactId{0};

    // Tab 1: 个人信息
    QLineEdit* m_nameEdit{nullptr};
    QLineEdit* m_phoneEdit{nullptr};
    QComboBox* m_relationCombo{nullptr};
    QLineEdit* m_customRelationEdit{nullptr};
    StepperSpinBox* m_closenessSpin{nullptr};
    StepperSpinBox* m_influenceSpin{nullptr};
    StepperSpinBox* m_creditSpin{nullptr};
    QComboBox* m_provinceCombo{nullptr};
    QComboBox* m_cityCombo{nullptr};
    QComboBox* m_districtCombo{nullptr};
    QLineEdit* m_addressEdit{nullptr};
    QComboBox* m_introCombo{nullptr};
    QDateEdit* m_meetDateEdit{nullptr};
    QPlainTextEdit* m_preferencesEdit{nullptr};
    QPlainTextEdit* m_taboosEdit{nullptr};

    // Tab 2: 工作信息
    QComboBox* m_companyCombo{nullptr};
    QLineEdit* m_customCompanyEdit{nullptr};
    QComboBox* m_titleCombo{nullptr};
    QLineEdit* m_customTitleEdit{nullptr};
    QComboBox* m_industryCombo{nullptr};
    QLineEdit* m_customIndustryEdit{nullptr};
    QPlainTextEdit* m_notesEdit{nullptr};
};
