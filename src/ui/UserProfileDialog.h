#pragma once

#include "CustomDialog.h"
#include "../core/DatabaseManager.h"
#include <QLineEdit>
#include <QComboBox>
#include <QDateEdit>
#include <QPlainTextEdit>

class UserProfileDialog : public CustomDialog {
    Q_OBJECT
public:
    explicit UserProfileDialog(QWidget* parent = nullptr);

private slots:
    void onProvinceChanged(int index);
    void onCityChanged(int index);
    void onSaveProfile();

private:
    void setupUi();
    void loadCombos(const UserProfile& profile);
    void selectRegions(const QString& prov, const QString& city, const QString& dist);

    QLineEdit* m_nameEdit{nullptr};
    QLineEdit* m_phoneEdit{nullptr};
    QComboBox* m_provinceCombo{nullptr};
    QComboBox* m_cityCombo{nullptr};
    QComboBox* m_districtCombo{nullptr};
    QLineEdit* m_addressEdit{nullptr};
    QComboBox* m_industryCombo{nullptr};
    QLineEdit* m_customIndustryEdit{nullptr};
    QComboBox* m_companyCombo{nullptr};
    QLineEdit* m_customCompanyEdit{nullptr};
    QComboBox* m_titleCombo{nullptr};
    QLineEdit* m_customTitleEdit{nullptr};
    QDateEdit* m_dateEdit{nullptr};
    QPlainTextEdit* m_aiRulesEdit{nullptr};
};
