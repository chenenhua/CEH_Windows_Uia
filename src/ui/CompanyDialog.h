#pragma once

#include "CustomDialog.h"
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QDoubleSpinBox>

#include <QVariantMap>

class CompanyDialog : public CustomDialog {
    Q_OBJECT
public:
    explicit CompanyDialog(QWidget* parent = nullptr);
    explicit CompanyDialog(const QVariantMap& companyData, QWidget* parent = nullptr);

    QString companyName() const;
    int companyId() const { return m_companyId; }
    bool isMyCompany() const;

private slots:
    void onSaveCompany();
    void onRoleChanged(int index);

private:
    void setupUi();
    void populateLegalRepCombo();

    int m_companyId{0};
    QString m_originalName;
    QString m_originalTaxNo;
    int m_initialLegalRepId{0};
    QString m_initialLegalRepName;
    int m_initialIsMyCompany{-1};

    QLineEdit* m_nameEdit{nullptr};
    QComboBox* m_legalRepCombo{nullptr}; // 更换 / 选择法人主体 (可关联联系人)
    QLineEdit* m_taxNoEdit{nullptr};
    QDoubleSpinBox* m_capitalSpin{nullptr};
    QDoubleSpinBox* m_shareRatioSpin{nullptr};
    QComboBox* m_roleCombo{nullptr};
    QComboBox* m_industryCombo{nullptr};
    QLineEdit* m_customIndustryEdit{nullptr};
    QPlainTextEdit* m_remarkEdit{nullptr};
};
