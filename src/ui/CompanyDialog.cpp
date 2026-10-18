#include "CompanyDialog.h"
#include "../core/DatabaseManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>

CompanyDialog::CompanyDialog(QWidget* parent)
    : CustomDialog("添加公司与工作主体", parent),
      m_initialIsMyCompany(1) {
    setupUi();
    populateLegalRepCombo();
    if (m_roleCombo) {
        int idx = m_roleCombo->findData("OWNER");
        if (idx >= 0) m_roleCombo->setCurrentIndex(idx);
    }
}

CompanyDialog::CompanyDialog(const QVariantMap& companyData, QWidget* parent)
    : CustomDialog(
          ((companyData.contains("is_my_company") && companyData["is_my_company"].toInt() == 0) ||
           companyData["role_type"].toString() == "EXTERNAL") ? "编辑企业主体信息" : "编辑公司与工作主体",
          parent),
      m_companyId(companyData["id"].toInt()),
      m_originalName(companyData["name"].toString().trimmed()),
      m_originalTaxNo(companyData["tax_no"].toString().trimmed()),
      m_initialLegalRepId(companyData["legal_rep_contact_id"].toInt()),
      m_initialLegalRepName(companyData["legal_rep_name"].toString().trimmed()) {

    // 严密判定主体初识归属：是“我的公司”还是“别人的公司”
    if (companyData.contains("is_my_company") && !companyData["is_my_company"].isNull()) {
        m_initialIsMyCompany = companyData["is_my_company"].toInt();
    } else {
        QString role = companyData["role_type"].toString().trimmed();
        UserProfile prof = DatabaseManager::instance().getUserProfile();
        if (m_originalName == prof.company.trimmed() && !prof.company.trimmed().isEmpty()) {
            m_initialIsMyCompany = 1;
        } else if (role == "EXTERNAL") {
            m_initialIsMyCompany = 0;
        } else if ((role == "OWNER" || role == "PARTNER") && (m_initialLegalRepId == -1 || m_initialLegalRepName == "本人")) {
            m_initialIsMyCompany = 1;
        } else if (m_companyId > 0 && m_companyId <= 6 && (role == "OWNER" || role == "PARTNER" || role == "EMPLOYEE")) {
            m_initialIsMyCompany = 1;
        } else {
            // 默认只要不是明确持有的自有主体，就属于“外部企业 / 别人的公司”
            m_initialIsMyCompany = 0;
        }
    }

    setupUi();
    populateLegalRepCombo();

    if (m_nameEdit) m_nameEdit->setText(m_originalName);
    if (m_taxNoEdit) m_taxNoEdit->setText(m_originalTaxNo);
    if (m_capitalSpin && companyData.contains("registered_capital")) {
        m_capitalSpin->setValue(companyData["registered_capital"].toDouble());
    }
    if (m_shareRatioSpin && companyData.contains("share_ratio")) {
        m_shareRatioSpin->setValue(companyData["share_ratio"].toDouble());
    }

    // 设置角色归属下拉框
    QString role = companyData["role_type"].toString().trimmed();
    if (m_initialIsMyCompany == 0) {
        role = "EXTERNAL";
    } else if (role.isEmpty() || role == "EXTERNAL") {
        role = "OWNER";
    }

    if (m_roleCombo) {
        int rIdx = m_roleCombo->findData(role);
        if (rIdx >= 0) {
            m_roleCombo->setCurrentIndex(rIdx);
        } else {
            m_roleCombo->setCurrentIndex(m_initialIsMyCompany == 0 ? 0 : 1);
        }
    }
    onRoleChanged(m_roleCombo ? m_roleCombo->currentIndex() : 0);

    QString ind = companyData["industry"].toString().trimmed();
    if (m_industryCombo) {
        int indIdx = m_industryCombo->findText(ind);
        if (indIdx >= 0) {
            m_industryCombo->setCurrentIndex(indIdx);
        } else if (!ind.isEmpty()) {
            if (m_customIndustryEdit) m_customIndustryEdit->setText(ind);
        }
    }

    if (m_remarkEdit) {
        m_remarkEdit->setPlainText(companyData["notes"].toString().trimmed());
    }
}

QString CompanyDialog::companyName() const {
    return m_nameEdit ? m_nameEdit->text().trimmed() : "";
}

bool CompanyDialog::isMyCompany() const {
    return m_roleCombo && m_roleCombo->currentData().toString() != "EXTERNAL";
}

void CompanyDialog::populateLegalRepCombo() {
    if (!m_legalRepCombo) return;
    m_legalRepCombo->clear();

    // 默认首选必须为（未指定），绝不可擅自默认设为“本人”
    m_legalRepCombo->addItem("（未指定）", 0);
    m_legalRepCombo->addItem("本人 (当前用户)", -1);

    auto allContacts = DatabaseManager::instance().getContacts("");
    QList<ContactSummary> myCompStaff;
    QList<ContactSummary> otherStaff;

    for (const auto& c : allContacts) {
        bool isThisComp = (m_companyId > 0 && c.companyId == m_companyId) ||
                          (!m_originalName.isEmpty() && c.company.trimmed() == m_originalName);
        if (isThisComp) {
            myCompStaff.append(c);
        } else {
            otherStaff.append(c);
        }
    }

    if (!myCompStaff.isEmpty()) {
        for (const auto& c : myCompStaff) {
            QString label = QString("%1 (%2) [本公司人员]").arg(c.name, c.title.isEmpty() ? (c.phone.isEmpty() ? "成员" : c.phone) : c.title);
            m_legalRepCombo->addItem(label, c.id);
        }
    }

    for (const auto& c : otherStaff) {
        QString label = QString("%1 (%2)").arg(c.name, c.title.isEmpty() ? (c.company.isEmpty() ? (c.phone.isEmpty() ? "联系人" : c.phone) : c.company) : c.title);
        m_legalRepCombo->addItem(label, c.id);
    }

    // 设置初始选中项
    if (m_initialLegalRepId != 0) {
        int idx = m_legalRepCombo->findData(m_initialLegalRepId);
        if (idx >= 0) {
            m_legalRepCombo->setCurrentIndex(idx);
        } else if (!m_initialLegalRepName.isEmpty()) {
            m_legalRepCombo->setEditText(m_initialLegalRepName);
        }
    } else if (!m_initialLegalRepName.isEmpty()) {
        int idx = -1;
        for (int i = 0; i < m_legalRepCombo->count(); ++i) {
            if (m_legalRepCombo->itemText(i).startsWith(m_initialLegalRepName + " ") || m_legalRepCombo->itemText(i) == m_initialLegalRepName) {
                idx = i;
                break;
            }
        }
        if (idx >= 0) {
            m_legalRepCombo->setCurrentIndex(idx);
        } else {
            m_legalRepCombo->setEditText(m_initialLegalRepName);
        }
    } else {
        // 未指定法人主体时，始终保持默认（未指定），绝不自动擅自选为“本人”
        m_legalRepCombo->setCurrentIndex(0);
    }
}

void CompanyDialog::onRoleChanged(int /*index*/) {
    if (!m_roleCombo || !m_shareRatioSpin) return;
    QString role = m_roleCombo->currentData().toString();
    if (role == "EXTERNAL") {
        m_shareRatioSpin->setValue(0.0);
        m_shareRatioSpin->setEnabled(false);
    } else {
        m_shareRatioSpin->setEnabled(true);
        if (m_shareRatioSpin->value() <= 0.0) {
            m_shareRatioSpin->setValue(role == "OWNER" ? 100.0 : 25.0);
        }
    }
}

void CompanyDialog::setupUi() {
    setFixedWidth(500);

    auto* layout = new QVBoxLayout();
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(10);

    auto* formLayout = new QFormLayout();
    formLayout->setSpacing(10);
    formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("例如: 台州木头智能科技有限公司 或 合作单位全称");
    formLayout->addRow("公司名称", m_nameEdit);

    // 核心明确归属：别人的公司 vs 我的公司，一目了然
    m_roleCombo = new QComboBox(this);
    m_roleCombo->addItem("外部企业 / 别人的公司（业务往来 / 客户 / 朋友公司）", "EXTERNAL");
    m_roleCombo->addItem("我的企业 - 本人控股 / 独资主体", "OWNER");
    m_roleCombo->addItem("我的企业 - 合伙投资主体", "PARTNER");
    m_roleCombo->addItem("我的企业 - 受聘任职主体", "EMPLOYEE");
    connect(m_roleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CompanyDialog::onRoleChanged);
    formLayout->addRow("公司归属", m_roleCombo);

    m_legalRepCombo = new QComboBox(this);
    m_legalRepCombo->setEditable(true);
    m_legalRepCombo->setMaxVisibleItems(15);
    if (m_legalRepCombo->lineEdit()) {
        m_legalRepCombo->lineEdit()->setPlaceholderText("选择联系人或输入法人姓名（默认未指定）");
    }
    formLayout->addRow("法人主体", m_legalRepCombo);

    m_taxNoEdit = new QLineEdit(this);
    m_taxNoEdit->setPlaceholderText("例如: 91331004MA28GXXXXX (统一社会信用代码/税号)");
    formLayout->addRow("公司税号", m_taxNoEdit);

    m_capitalSpin = new QDoubleSpinBox(this);
    m_capitalSpin->setRange(0.0, 9999999999.0);
    m_capitalSpin->setDecimals(2);
    m_capitalSpin->setValue(1000000.0);
    m_capitalSpin->setPrefix("¥ ");
    formLayout->addRow("注册资本", m_capitalSpin);

    m_shareRatioSpin = new QDoubleSpinBox(this);
    m_shareRatioSpin->setRange(0.0, 100.0);
    m_shareRatioSpin->setDecimals(2);
    m_shareRatioSpin->setValue(0.0);
    m_shareRatioSpin->setSuffix(" %");
    formLayout->addRow("占股比例", m_shareRatioSpin);

    // 所属行业：关联数据库 dictionary (INDUSTRY) 下拉选择 + 右侧自定义输入
    auto* indBox = new QWidget(this);
    auto* indLayout = new QHBoxLayout(indBox);
    indLayout->setContentsMargins(0, 0, 0, 0);
    indLayout->setSpacing(10);

    m_industryCombo = new QComboBox(indBox);
    QStringList inds = DatabaseManager::instance().getDictionaryValues("INDUSTRY");
    if (inds.isEmpty()) {
        inds = {"人工智能与大数据", "风险投资与私募基金", "智能制造与供应链", "政务与公共管理", "金融银行与信贷", "跨境电商与贸易"};
    }
    m_industryCombo->addItems(inds);

    m_customIndustryEdit = new QLineEdit(indBox);
    m_customIndustryEdit->setPlaceholderText("或自定义行业");

    indLayout->addWidget(m_industryCombo, 1);
    indLayout->addWidget(m_customIndustryEdit, 1);
    formLayout->addRow("所属行业", indBox);

    // 补充备注：默认3行可见输入 textarea
    m_remarkEdit = createMultiLineEdit("主营业务范围、股权占比或补充说明...", this, 3);
    formLayout->addRow("补充备注", m_remarkEdit);

    layout->addLayout(formLayout);

    auto* btnBox = new QHBoxLayout();
    btnBox->setSpacing(10);

    auto* btnCancel = new QPushButton("取消", this);
    btnCancel->setCursor(Qt::PointingHandCursor);
    btnCancel->setFixedHeight(36);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    auto* btnSave = new QPushButton("保存", this);
    btnSave->setObjectName("primaryBtn");
    btnSave->setCursor(Qt::PointingHandCursor);
    btnSave->setFixedHeight(36);
    connect(btnSave, &QPushButton::clicked, this, &CompanyDialog::onSaveCompany);

    btnBox->addWidget(btnCancel, 1);
    btnBox->addWidget(btnSave, 1);
    layout->addLayout(btnBox);

    bodyLayout()->addLayout(layout);
    normalizeFormWidgets(this);
}

void CompanyDialog::onSaveCompany() {
    QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty()) {
        markFieldInvalid(m_nameEdit);
        return;
    }

    QString taxNo = m_taxNoEdit ? m_taxNoEdit->text().trimmed() : "";
    double regCapital = m_capitalSpin ? m_capitalSpin->value() : 0.0;
    QString role = m_roleCombo ? m_roleCombo->currentData().toString() : "EXTERNAL";
    bool isMine = (role != "EXTERNAL");
    double shareRatio = isMine ? (m_shareRatioSpin ? m_shareRatioSpin->value() : 100.0) : 0.0;
    QString customInd = m_customIndustryEdit ? m_customIndustryEdit->text().trimmed() : "";
    QString ind = customInd.isEmpty() ? (m_industryCombo ? m_industryCombo->currentText().trimmed() : "") : customInd;
    QString rem = m_remarkEdit ? m_remarkEdit->toPlainText().trimmed() : "";

    // 获取法人主体
    int legalRepContactId = 0;
    QString legalRepName;
    if (m_legalRepCombo) {
        int curIdx = m_legalRepCombo->currentIndex();
        if (curIdx >= 0) {
            legalRepContactId = m_legalRepCombo->itemData(curIdx).toInt();
        }
        if (legalRepContactId == -1) {
            legalRepName = "本人";
        } else if (legalRepContactId > 0) {
            QString itemText = m_legalRepCombo->currentText().trimmed();
            int spIdx = itemText.indexOf(' ');
            legalRepName = (spIdx > 0) ? itemText.left(spIdx).trimmed() : itemText;
        } else {
            QString manualText = m_legalRepCombo->currentText().trimmed();
            if (!manualText.isEmpty() && manualText != "（未指定）") {
                legalRepName = manualText;
            }
        }
    }

    // 严密区隔主体性质：外部企业为 0，自有企业为 1
    int isMyCompanyInt = isMine ? 1 : 0;

    // 核心保存与更新：直接更新当前公司主体及其所有关联人员，绝不误变成新增，且绝不把别人的公司变成我的公司
    if (DatabaseManager::instance().saveOrUpdateCompany(
            m_companyId,
            m_originalName,
            name,
            role,
            ind,
            rem,
            taxNo,
            regCapital,
            shareRatio,
            legalRepContactId,
            legalRepName,
            isMyCompanyInt)) {
        accept();
    } else {
        CustomDialog::showWarning(this, "错误", "公司信息保存失败，可能已存在同名主体。");
    }
}
