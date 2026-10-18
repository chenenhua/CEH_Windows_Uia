#include "UserProfileDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QDate>

UserProfileDialog::UserProfileDialog(QWidget* parent)
    : CustomDialog("编辑个人名片", parent) {
    setupUi();
}

void UserProfileDialog::setupUi() {
    setFixedWidth(500);

    auto* layout = new QVBoxLayout();
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    auto profile = DatabaseManager::instance().getUserProfile();

    auto* formLayout = new QFormLayout();
    formLayout->setSpacing(10);
    formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_nameEdit = new QLineEdit(profile.name, this);
    m_nameEdit->setPlaceholderText("姓名 (必填)");
    formLayout->addRow("姓名", m_nameEdit);

    m_phoneEdit = new QLineEdit(profile.phone, this);
    m_phoneEdit->setPlaceholderText("手机号 / 常用电话");
    formLayout->addRow("电话", m_phoneEdit);

    // 1. 地址上面加 省 / 市 / 区 选择（名片主人选择了哪个地区，添加通讯录时预先选中这个区域）
    auto* areaBox = new QWidget(this);
    auto* areaLayout = new QHBoxLayout(areaBox);
    areaLayout->setContentsMargins(0, 0, 0, 0);
    areaLayout->setSpacing(10);
    m_provinceCombo = new QComboBox(areaBox);
    m_cityCombo = new QComboBox(areaBox);
    m_districtCombo = new QComboBox(areaBox);
    areaLayout->addWidget(m_provinceCombo, 1);
    areaLayout->addWidget(m_cityCombo, 1);
    areaLayout->addWidget(m_districtCombo, 1);
    formLayout->addRow("地区", areaBox);

    connect(m_provinceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UserProfileDialog::onProvinceChanged);
    connect(m_cityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UserProfileDialog::onCityChanged);

    QString detailAddr = profile.detailAddress;
    if (detailAddr.isEmpty() && !profile.address.isEmpty()) {
        detailAddr = profile.address;
        QString prefix = profile.province + profile.city + profile.district;
        if (!prefix.isEmpty() && detailAddr.startsWith(prefix)) {
            detailAddr = detailAddr.mid(prefix.length()).trimmed();
        }
    }
    m_addressEdit = new QLineEdit(detailAddr, this);
    m_addressEdit->setPlaceholderText("街道门牌号 / 详细地址");
    formLayout->addRow("地址", m_addressEdit);

    // 2. 行业：下拉选择 + 右侧添加新行业
    auto* indBox = new QWidget(this);
    auto* indLayout = new QHBoxLayout(indBox);
    indLayout->setContentsMargins(0, 0, 0, 0);
    indLayout->setSpacing(10);
    m_industryCombo = new QComboBox(indBox);
    m_customIndustryEdit = new QLineEdit(indBox);
    m_customIndustryEdit->setPlaceholderText("或添加新行业");
    indLayout->addWidget(m_industryCombo, 1);
    indLayout->addWidget(m_customIndustryEdit, 1);
    formLayout->addRow("行业", indBox);

    // 3. 公司：下拉选择 + 右侧添加新公司
    auto* compBox = new QWidget(this);
    auto* compLayout = new QHBoxLayout(compBox);
    compLayout->setContentsMargins(0, 0, 0, 0);
    compLayout->setSpacing(10);
    m_companyCombo = new QComboBox(compBox);
    m_customCompanyEdit = new QLineEdit(compBox);
    m_customCompanyEdit->setPlaceholderText("或添加新公司");
    compLayout->addWidget(m_companyCombo, 1);
    compLayout->addWidget(m_customCompanyEdit, 1);
    formLayout->addRow("公司", compBox);

    // 4. 职务：下拉选择 + 右侧添加新职务
    auto* titleBox = new QWidget(this);
    auto* titleLayout = new QHBoxLayout(titleBox);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(10);
    m_titleCombo = new QComboBox(titleBox);
    m_customTitleEdit = new QLineEdit(titleBox);
    m_customTitleEdit->setPlaceholderText("或添加新职务");
    titleLayout->addWidget(m_titleCombo, 1);
    titleLayout->addWidget(m_customTitleEdit, 1);
    formLayout->addRow("职务", titleBox);

    // 5. 生日：点击弹出日历选择，无需手动输入
    QDate initDate = QDate::fromString(profile.birthDate, "yyyy-MM-dd");
    if (!initDate.isValid()) initDate = QDate::currentDate();
    m_dateEdit = new QDateEdit(initDate, this);
    m_dateEdit->setCalendarPopup(true);
    m_dateEdit->setDisplayFormat("yyyy-MM-dd");
    formLayout->addRow("生日", m_dateEdit);

    // 6. 规则：默认3行可见输入（主要设置要求人工智能的回答规则）
    m_aiRulesEdit = CustomDialog::createMultiLineEdit(
        "设置要求人工智能的回答规则（例如：回答精炼直击重点、结合我的行业与财务状况分析...）", this, 3);
    m_aiRulesEdit->setPlainText(profile.aiRules);
    formLayout->addRow("规则", m_aiRulesEdit);

    layout->addLayout(formLayout);

    auto* btnBox = new QHBoxLayout();
    btnBox->setSpacing(10);

    auto* btnCancel = new QPushButton("取消", this);
    btnCancel->setCursor(Qt::PointingHandCursor);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    auto* btnSave = new QPushButton("保存", this);
    btnSave->setObjectName("primaryBtn");
    btnSave->setCursor(Qt::PointingHandCursor);
    connect(btnSave, &QPushButton::clicked, this, &UserProfileDialog::onSaveProfile);

    btnBox->addWidget(btnCancel, 1);
    btnBox->addWidget(btnSave, 1);
    layout->addLayout(btnBox);

    bodyLayout()->addLayout(layout);
    loadCombos(profile);
    normalizeFormWidgets(this);
}

void UserProfileDialog::loadCombos(const UserProfile& profile) {
    auto& db = DatabaseManager::instance();

    // 行业下拉
    m_industryCombo->clear();
    QStringList inds = db.getDictionaryValues("INDUSTRY");
    if (inds.isEmpty()) inds = {"制造业", "批发零售", "信息技术", "金融业"};
    if (!profile.industry.isEmpty() && !inds.contains(profile.industry)) {
        inds.prepend(profile.industry);
    }
    m_industryCombo->addItems(inds);
    if (!profile.industry.isEmpty()) m_industryCombo->setCurrentText(profile.industry);

    // 公司下拉
    m_companyCombo->clear();
    QStringList comps = db.getDictionaryValues("UNIT");
    auto compRows = db.getCompaniesList();
    for (const auto& r : compRows) {
        QString cn = r["name"].toString().trimmed();
        if (!cn.isEmpty() && !comps.contains(cn)) comps.append(cn);
    }
    if (!profile.company.isEmpty() && !comps.contains(profile.company)) {
        comps.prepend(profile.company);
    }
    if (comps.isEmpty()) comps = {"台州木头智能科技有限公司"};
    m_companyCombo->addItems(comps);
    if (!profile.company.isEmpty()) m_companyCombo->setCurrentText(profile.company);

    // 职务下拉
    m_titleCombo->clear();
    QStringList titles = db.getDictionaryValues("TITLE");
    if (titles.isEmpty()) titles = {"市场/品牌", "技术/研发", "财务", "运营/推广"};
    if (!profile.title.isEmpty() && !titles.contains(profile.title)) {
        titles.prepend(profile.title);
    }
    m_titleCombo->addItems(titles);
    if (!profile.title.isEmpty()) m_titleCombo->setCurrentText(profile.title);

    // 省级行政区
    m_provinceCombo->blockSignals(true);
    m_provinceCombo->clear();
    auto provinces = db.getRegionsByParent("");
    for (const auto& p : provinces) {
        m_provinceCombo->addItem(p.name, p.code);
    }
    m_provinceCombo->blockSignals(false);

    QString defProv = profile.province.isEmpty() ? "浙江省" : profile.province;
    QString defCity = profile.city.isEmpty() ? "台州市" : profile.city;
    QString defDist = profile.district.isEmpty() ? "路桥区" : profile.district;
    selectRegions(defProv, defCity, defDist);
}

void UserProfileDialog::onProvinceChanged(int index) {
    m_cityCombo->blockSignals(true);
    m_cityCombo->clear();
    m_districtCombo->blockSignals(true);
    m_districtCombo->clear();

    if (index >= 0) {
        QString provCode = m_provinceCombo->itemData(index).toString();
        QString provName = m_provinceCombo->itemText(index);
        auto children = DatabaseManager::instance().getRegionsByParent(provCode);
        if (!children.isEmpty()) {
            if (children.first().level == 3) {
                m_cityCombo->addItem(provName, provCode);
                for (const auto& d : children) {
                    m_districtCombo->addItem(d.name, d.code);
                }
            } else {
                for (const auto& c : children) {
                    m_cityCombo->addItem(c.name, c.code);
                }
            }
        }
    }

    m_cityCombo->blockSignals(false);
    m_districtCombo->blockSignals(false);

    if (m_districtCombo->count() == 0 && m_cityCombo->count() > 0) {
        onCityChanged(0);
    }
}

void UserProfileDialog::onCityChanged(int index) {
    if (index < 0) return;
    QString cityCode = m_cityCombo->itemData(index).toString();
    QString provCode = m_provinceCombo->currentData().toString();
    if (cityCode == provCode && m_districtCombo->count() > 0) {
        return;
    }

    m_districtCombo->blockSignals(true);
    m_districtCombo->clear();
    auto districts = DatabaseManager::instance().getRegionsByParent(cityCode);
    for (const auto& d : districts) {
        m_districtCombo->addItem(d.name, d.code);
    }
    m_districtCombo->blockSignals(false);
}

void UserProfileDialog::selectRegions(const QString& prov, const QString& city, const QString& dist) {
    if (!prov.isEmpty()) {
        int pIdx = m_provinceCombo->findText(prov, Qt::MatchContains);
        if (pIdx >= 0) {
            m_provinceCombo->setCurrentIndex(pIdx);
            onProvinceChanged(pIdx);
        }
    } else if (m_provinceCombo->count() > 0) {
        onProvinceChanged(m_provinceCombo->currentIndex());
    }

    if (!city.isEmpty()) {
        int cIdx = m_cityCombo->findText(city, Qt::MatchContains);
        if (cIdx >= 0) {
            m_cityCombo->setCurrentIndex(cIdx);
            onCityChanged(cIdx);
        }
    }

    if (!dist.isEmpty()) {
        int dIdx = m_districtCombo->findText(dist, Qt::MatchContains);
        if (dIdx >= 0) {
            m_districtCombo->setCurrentIndex(dIdx);
        }
    }
}

void UserProfileDialog::onSaveProfile() {
    QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty()) {
        markFieldInvalid(m_nameEdit);
        return;
    }

    QString ind = m_customIndustryEdit->text().trimmed();
    if (ind.isEmpty()) ind = m_industryCombo->currentText().trimmed();

    QString comp = m_customCompanyEdit->text().trimmed();
    if (comp.isEmpty()) comp = m_companyCombo->currentText().trimmed();

    QString title = m_customTitleEdit->text().trimmed();
    if (title.isEmpty()) title = m_titleCombo->currentText().trimmed();

    UserProfile p;
    p.name = name;
    p.phone = m_phoneEdit->text().trimmed();
    p.province = m_provinceCombo->currentText().trimmed();
    p.city = m_cityCombo->currentText().trimmed();
    p.district = m_districtCombo->currentText().trimmed();
    p.detailAddress = m_addressEdit->text().trimmed();
    QString regionPrefix = p.province + p.city + p.district;
    p.address = p.detailAddress.isEmpty() ? regionPrefix : (regionPrefix + " " + p.detailAddress);
    p.industry = ind;
    p.company = comp;
    p.title = title;
    p.birthDate = m_dateEdit->date().toString("yyyy-MM-dd");
    p.aiRules = m_aiRulesEdit->toPlainText().trimmed();

    if (DatabaseManager::instance().saveUserProfile(p)) {
        accept();
    } else {
        CustomDialog::showError(this, "错误", "名片更新失败！");
    }
}
