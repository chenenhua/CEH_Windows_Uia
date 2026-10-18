#include "ContactDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QMessageBox>
#include <QLabel>

ContactDialog::ContactDialog(QWidget* parent)
    : CustomDialog("新建联系人", parent), m_isEditMode(false) {
    setupUi();
    loadCombos();
}

ContactDialog::ContactDialog(const ContactSummary& contact, QWidget* parent)
    : CustomDialog(QString("编辑联系人 - %1").arg(contact.name), parent),
      m_isEditMode(true), m_contactId(contact.id) {
    setupUi();
    loadCombos();

    m_nameEdit->setText(contact.name);
    m_phoneEdit->setText(contact.phone);
    m_relationCombo->setCurrentText(contact.relationType);
    m_closenessSpin->setValue(contact.closenessLevel);
    m_influenceSpin->setValue(contact.influenceLevel);
    m_creditSpin->setValue(contact.creditRating);
    selectRegions(contact.province, contact.city, contact.district);
    QString detailAddr = contact.address;
    QString prefix = contact.province + contact.city + contact.district;
    if (!prefix.isEmpty() && detailAddr.startsWith(prefix)) {
        detailAddr = detailAddr.mid(prefix.length());
    }
    m_addressEdit->setText(detailAddr);
    m_preferencesEdit->setPlainText(contact.preferences);
    m_taboosEdit->setPlainText(contact.taboos);

    int introIdx = m_introCombo->findData(contact.introducerId);
    if (introIdx >= 0) m_introCombo->setCurrentIndex(introIdx);

    QDate md = QDate::fromString(contact.meetDate.left(10), "yyyy-MM-dd");
    if (!md.isValid()) md = QDate::fromString(contact.meetDate.left(10), "yyyy/MM/dd");
    if (!md.isValid()) md = QDate::currentDate();
    m_meetDateEdit->setDate(md);

    m_companyCombo->setCurrentText(contact.company);
    m_titleCombo->setCurrentText(contact.title);
    m_industryCombo->setCurrentText(contact.industry);
    m_notesEdit->setPlainText(contact.notes);
}

void ContactDialog::setupUi() {
    setMinimumWidth(520);

    auto* layout = new QVBoxLayout();
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    auto* tabs = new QTabWidget(this);

    // ==================== Tab 1: 个人属性 ====================
    auto* tabPersonal = new QWidget(tabs);
    auto* tabPersonalVLayout = new QVBoxLayout(tabPersonal);
    tabPersonalVLayout->setContentsMargins(12, 12, 12, 12);
    tabPersonalVLayout->setSpacing(0);

    auto* form1 = new QFormLayout();
    form1->setContentsMargins(0, 0, 0, 0);
    form1->setSpacing(10);
    form1->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_nameEdit = new QLineEdit(tabPersonal);
    m_nameEdit->setPlaceholderText("姓名 (必填)");
    form1->addRow("姓名", m_nameEdit);

    m_phoneEdit = new QLineEdit(tabPersonal);
    m_phoneEdit->setPlaceholderText("手机号 / 常用电话");
    form1->addRow("电话", m_phoneEdit);

    auto* relBox = new QWidget(tabPersonal);
    auto* relLayout = new QHBoxLayout(relBox);
    relLayout->setContentsMargins(0, 0, 0, 0);
    relLayout->setSpacing(10);
    m_relationCombo = new QComboBox(relBox);
    m_customRelationEdit = new QLineEdit(relBox);
    m_customRelationEdit->setPlaceholderText("或自定义关系");
    relLayout->addWidget(m_relationCombo, 1);
    relLayout->addWidget(m_customRelationEdit, 1);
    form1->addRow("社会关系", relBox);

    auto* starsBox = new QWidget(tabPersonal);
    auto* starsLayout = new QHBoxLayout(starsBox);
    starsLayout->setContentsMargins(0, 0, 0, 0);
    starsLayout->setSpacing(10);

    m_closenessSpin = new StepperSpinBox("亲疏", 1, 5, 3, starsBox);
    m_influenceSpin = new StepperSpinBox("能量", 1, 5, 3, starsBox);
    m_creditSpin = new StepperSpinBox("信用", 1, 5, 5, starsBox);

    starsLayout->addWidget(m_closenessSpin, 1);
    starsLayout->addWidget(m_influenceSpin, 1);
    starsLayout->addWidget(m_creditSpin, 1);
    form1->addRow("三维评价", starsBox);

    // 省 / 市 / 区 三级行政区划联动下拉 (关联 regions 表)
    auto* areaBox = new QWidget(tabPersonal);
    auto* areaLayout = new QHBoxLayout(areaBox);
    areaLayout->setContentsMargins(0, 0, 0, 0);
    areaLayout->setSpacing(10);
    m_provinceCombo = new QComboBox(areaBox);
    m_cityCombo = new QComboBox(areaBox);
    m_districtCombo = new QComboBox(areaBox);
    areaLayout->addWidget(m_provinceCombo, 1);
    areaLayout->addWidget(m_cityCombo, 1);
    areaLayout->addWidget(m_districtCombo, 1);
    form1->addRow("所在地区", areaBox);

    connect(m_provinceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ContactDialog::onProvinceChanged);
    connect(m_cityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ContactDialog::onCityChanged);

    m_addressEdit = new QLineEdit(tabPersonal);
    m_addressEdit->setPlaceholderText("街道门牌号 / 详细地址");
    form1->addRow("详细地址", m_addressEdit);

    m_introCombo = new QComboBox(tabPersonal);
    m_introCombo->addItem("直接相识", 0);
    form1->addRow("引导人", m_introCombo);

    m_meetDateEdit = new QDateEdit(QDate::currentDate(), tabPersonal);
    m_meetDateEdit->setCalendarPopup(true);
    m_meetDateEdit->setDisplayFormat("yyyy-MM-dd");
    form1->addRow("认识时间", m_meetDateEdit);

    m_preferencesEdit = CustomDialog::createMultiLineEdit("如: 喜饮红茶、好打网球、收藏古玩...", tabPersonal, 3);
    form1->addRow("喜好", m_preferencesEdit);

    m_taboosEdit = CustomDialog::createMultiLineEdit("如: 忌辛辣海鲜、忌迟到、不谈过往争议...", tabPersonal, 3);
    form1->addRow("禁忌", m_taboosEdit);

    tabPersonalVLayout->addLayout(form1);
    tabPersonalVLayout->addStretch(1);

    tabs->addTab(tabPersonal, "个人档案");

    // ==================== Tab 2: 工作职务 ====================
    auto* tabWork = new QWidget(tabs);
    auto* tabWorkVLayout = new QVBoxLayout(tabWork);
    tabWorkVLayout->setContentsMargins(12, 12, 12, 12);
    tabWorkVLayout->setSpacing(0);

    auto* form2 = new QFormLayout();
    form2->setContentsMargins(0, 0, 0, 0);
    form2->setSpacing(10);
    form2->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto* compBox = new QWidget(tabWork);
    auto* compLayout = new QHBoxLayout(compBox);
    compLayout->setContentsMargins(0, 0, 0, 0);
    compLayout->setSpacing(10);
    m_companyCombo = new QComboBox(compBox);
    m_customCompanyEdit = new QLineEdit(compBox);
    m_customCompanyEdit->setPlaceholderText("或自定义公司");
    compLayout->addWidget(m_companyCombo, 1);
    compLayout->addWidget(m_customCompanyEdit, 1);
    form2->addRow("所属公司", compBox);

    auto* titleBox = new QWidget(tabWork);
    auto* titleLayout = new QHBoxLayout(titleBox);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(10);
    m_titleCombo = new QComboBox(titleBox);
    m_customTitleEdit = new QLineEdit(titleBox);
    m_customTitleEdit->setPlaceholderText("或自定义职务");
    titleLayout->addWidget(m_titleCombo, 1);
    titleLayout->addWidget(m_customTitleEdit, 1);
    form2->addRow("担任职务", titleBox);

    auto* indBox = new QWidget(tabWork);
    auto* indLayout = new QHBoxLayout(indBox);
    indLayout->setContentsMargins(0, 0, 0, 0);
    indLayout->setSpacing(10);
    m_industryCombo = new QComboBox(indBox);
    m_customIndustryEdit = new QLineEdit(indBox);
    m_customIndustryEdit->setPlaceholderText("或自定义行业");
    indLayout->addWidget(m_industryCombo, 1);
    indLayout->addWidget(m_customIndustryEdit, 1);
    form2->addRow("所处行业", indBox);

    m_notesEdit = CustomDialog::createMultiLineEdit("合作经历备忘、人脉背景、资源互补点...", tabWork, 3);
    form2->addRow("背景备忘", m_notesEdit);

    tabWorkVLayout->addLayout(form2);
    tabWorkVLayout->addStretch(1);

    tabs->addTab(tabWork, "工作职务");

    layout->addWidget(tabs);

    auto* btnBox = new QHBoxLayout();
    btnBox->setSpacing(10);

    auto* btnCancel = new QPushButton("取消", this);
    btnCancel->setCursor(Qt::PointingHandCursor);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    auto* btnSave = new QPushButton("保存", this);
    btnSave->setObjectName("primaryBtn");
    btnSave->setCursor(Qt::PointingHandCursor);
    connect(btnSave, &QPushButton::clicked, this, &ContactDialog::onSaveContact);

    btnBox->addWidget(btnCancel, 1);
    btnBox->addWidget(btnSave, 1);
    layout->addLayout(btnBox);

    bodyLayout()->addLayout(layout);
    normalizeFormWidgets(this);
}

void ContactDialog::loadCombos() {
    auto& db = DatabaseManager::instance();

    m_relationCombo->clear();
    QStringList rels = db.getDictionaryValues("RELATION_TYPE");
    if (rels.isEmpty()) rels = {"朋友", "亲人", "核心合伙人", "战略客户", "投资人", "政务关系", "同学校友"};
    m_relationCombo->addItems(rels);

    m_companyCombo->clear();
    m_companyCombo->addItem("暂不填");
    QStringList comps = db.getDictionaryValues("UNIT");
    m_companyCombo->addItems(comps);

    m_titleCombo->clear();
    m_titleCombo->addItem("暂不填");
    QStringList titles = db.getDictionaryValues("TITLE");
    if (titles.isEmpty()) titles = {"总经理 / 总裁", "创始人 / 董事长", "技术副总裁 / CTO", "管理合伙人"};
    m_titleCombo->addItems(titles);

    m_industryCombo->clear();
    QStringList inds = db.getDictionaryValues("INDUSTRY");
    if (inds.isEmpty()) inds = {"人工智能与大数据", "风险投资与私募基金", "智能制造与供应链", "政务与公共管理"};
    m_industryCombo->addItems(inds);

    m_introCombo->clear();
    m_introCombo->addItem("直接相识", 0);
    auto contacts = db.getContacts();
    for (const auto& c : contacts) {
        if (!m_isEditMode || c.id != m_contactId) {
            m_introCombo->addItem(c.name, c.id);
        }
    }

    // 加载省级行政区 (level = 1)
    m_provinceCombo->blockSignals(true);
    m_provinceCombo->clear();
    auto provinces = db.getRegionsByParent("");
    for (const auto& p : provinces) {
        m_provinceCombo->addItem(p.name, p.code);
    }
    m_provinceCombo->blockSignals(false);

    // 预先选中个人名片主人设置的省市区（防止全国找）
    auto profile = db.getUserProfile();
    QString defProv = profile.province.isEmpty() ? "浙江省" : profile.province;
    QString defCity = profile.city.isEmpty() ? "杭州市" : profile.city;
    QString defDist = profile.district.isEmpty() ? "西湖区" : profile.district;
    selectRegions(defProv, defCity, defDist);
}

void ContactDialog::onProvinceChanged(int index) {
    m_cityCombo->blockSignals(true);
    m_cityCombo->clear();
    m_districtCombo->blockSignals(true);
    m_districtCombo->clear();

    if (index >= 0) {
        QString provCode = m_provinceCombo->itemData(index).toString();
        QString provName = m_provinceCombo->itemText(index);
        auto children = DatabaseManager::instance().getRegionsByParent(provCode);
        if (!children.isEmpty()) {
            // 直辖市在 regions 表中直接挂载 level=3 区县
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

void ContactDialog::onCityChanged(int index) {
    if (index < 0) return;
    QString cityCode = m_cityCombo->itemData(index).toString();
    QString provCode = m_provinceCombo->currentData().toString();
    if (cityCode == provCode && m_districtCombo->count() > 0) {
        return; // 直辖市已在 onProvinceChanged 填充区县
    }

    m_districtCombo->blockSignals(true);
    m_districtCombo->clear();
    auto districts = DatabaseManager::instance().getRegionsByParent(cityCode);
    for (const auto& d : districts) {
        m_districtCombo->addItem(d.name, d.code);
    }
    m_districtCombo->blockSignals(false);
}

void ContactDialog::selectRegions(const QString& prov, const QString& city, const QString& dist) {
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

void ContactDialog::onSaveContact() {
    QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty()) {
        markFieldInvalid(m_nameEdit);
        return;
    }

    QString rel = m_customRelationEdit->text().trimmed();
    if (rel.isEmpty()) rel = m_relationCombo->currentText();

    QString comp = m_customCompanyEdit->text().trimmed();
    if (comp.isEmpty()) comp = m_companyCombo->currentText();
    if (comp == "暂不填") comp = "";

    QString title = m_customTitleEdit->text().trimmed();
    if (title.isEmpty()) title = m_titleCombo->currentText();
    if (title == "暂不填") title = "";

    QString ind = m_customIndustryEdit->text().trimmed();
    if (ind.isEmpty()) ind = m_industryCombo->currentText();

    QString phone = m_phoneEdit->text().trimmed();
    int close = m_closenessSpin->value();
    int inf = m_influenceSpin->value();
    int cred = m_creditSpin->value();
    QString prov = m_provinceCombo->currentText().trimmed();
    QString city = m_cityCombo->currentText().trimmed();
    QString dist = m_districtCombo->currentText().trimmed();
    QString addr = m_addressEdit->text().trimmed();
    int introId = m_introCombo->currentData().toInt();
    QString pref = m_preferencesEdit->toPlainText().trimmed();
    QString taboos = m_taboosEdit ? m_taboosEdit->toPlainText().trimmed() : "";
    QString notes = m_notesEdit->toPlainText().trimmed();
    QString meetDate = m_meetDateEdit ? m_meetDateEdit->date().toString("yyyy-MM-dd") : "";

    bool ok = false;
    if (m_isEditMode) {
        ok = DatabaseManager::instance().updateContactFull(
            m_contactId, name, phone, rel, comp, title, ind, close, inf, cred, pref, prov, city, dist, addr, notes, taboos, meetDate
        );
    } else {
        ok = DatabaseManager::instance().addContactFull(
            name, phone, rel, comp, title, ind, close, inf, cred, introId, pref, notes, prov, city, dist, addr, taboos, meetDate
        );
    }

    if (ok) {
        accept();
    } else {
        CustomDialog::showError(this, "错误", "联系人保存失败！");
    }
}

