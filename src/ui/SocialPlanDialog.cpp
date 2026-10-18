#include "SocialPlanDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QButtonGroup>
#include <QMessageBox>
#include <QLabel>
#include <QFrame>

SocialPlanDialog::SocialPlanDialog(int preselectedContactId, QWidget* parent)
    : CustomDialog("制定计划", parent),
      m_isExecuteMode(false),
      m_preselectedContactId(preselectedContactId) {
    setupCreateUi(preselectedContactId);
}

SocialPlanDialog::SocialPlanDialog(const SocialPlanItem& item, QWidget* parent)
    : CustomDialog("计划总结", parent),
      m_isExecuteMode(true),
      m_existingItem(item) {
    setupExecuteUi(item);
}

void SocialPlanDialog::setupCreateUi(int preselectedContactId) {
    setFixedWidth(480);

    auto* layout = new QVBoxLayout();
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(10);

    QString bigToggleStyle =
        "QPushButton { background-color: #1c1c1f; color: #8e8e93; border: none; border-radius: 3px; font-size: 15px; font-weight: bold; }"
        "QPushButton:hover { background-color: #242428; color: #e1e1e5; }"
        "QPushButton:checked { background-color: #00a361; color: #ffffff; font-weight: bold; }";

    // 1. 个人 | 工作 两个选择
    auto* scopeRow = new QHBoxLayout();
    scopeRow->setSpacing(10);

    m_btnPersonal = new QPushButton("个人", this);
    m_btnPersonal->setCheckable(true);
    m_btnPersonal->setChecked(true);
    m_btnPersonal->setFixedHeight(38);
    m_btnPersonal->setCursor(Qt::PointingHandCursor);
    m_btnPersonal->setStyleSheet(bigToggleStyle);

    m_btnWork = new QPushButton("工作", this);
    m_btnWork->setCheckable(true);
    m_btnWork->setFixedHeight(38);
    m_btnWork->setCursor(Qt::PointingHandCursor);
    m_btnWork->setStyleSheet(bigToggleStyle);

    auto* scopeGroup = new QButtonGroup(this);
    scopeGroup->addButton(m_btnPersonal);
    scopeGroup->addButton(m_btnWork);

    connect(m_btnPersonal, &QPushButton::toggled, this, &SocialPlanDialog::onBookScopeToggled);

    scopeRow->addWidget(m_btnPersonal, 1);
    scopeRow->addWidget(m_btnWork, 1);
    layout->addLayout(scopeRow);

    auto* formLayout = new QFormLayout();
    formLayout->setSpacing(10);
    formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // 3. 归属类别 (下拉: 个人就人名字，工作就公司名字)
    m_ownerCombo = new QComboBox(this);
    formLayout->addRow("归属类别", m_ownerCombo);

    // 4. 事项类型 (左下拉联动 记账->支出->个人->人脉社交 / 工作->商务社交 + 右边自定义输入)
    auto* typeBox = new QWidget(this);
    auto* typeLayout = new QHBoxLayout(typeBox);
    typeLayout->setContentsMargins(0, 0, 0, 0);
    typeLayout->setSpacing(10);

    m_planTypeCombo = new QComboBox(typeBox);
    m_customPlanTypeEdit = new QLineEdit(typeBox);
    m_customPlanTypeEdit->setPlaceholderText("或自定义类型");
    typeLayout->addWidget(m_planTypeCombo, 1);
    typeLayout->addWidget(m_customPlanTypeEdit, 1);
    formLayout->addRow("事项类型", typeBox);

    // 5. 计划主题 (3行可见 textarea)
    m_titleEdit = createMultiLineEdit("请输入计划主题（例如：请陈总吃饭商谈二期合作...）", this, 3);
    formLayout->addRow("计划主题", m_titleEdit);

    // 6. 预定日期 (点击弹出日历选择，无需手动打字)
    m_dateEdit = new QDateEdit(QDate::currentDate().addDays(3), this);
    m_dateEdit->setCalendarPopup(true);
    m_dateEdit->setDisplayFormat("yyyy-MM-dd");
    formLayout->addRow("预定日期", m_dateEdit);

    // 7. 提前预警
    m_remindSpin = new QSpinBox(this);
    m_remindSpin->setRange(1, 30);
    m_remindSpin->setValue(3);
    m_remindSpin->setSuffix(" 天前提醒");
    formLayout->addRow("提前预警", m_remindSpin);

    // 8. 补充备忘 (3行可见 textarea)
    m_remarkEdit = createMultiLineEdit("地点安排、备选餐厅、注意事项...", this, 3);
    formLayout->addRow("补充备忘", m_remarkEdit);

    layout->addLayout(formLayout);

    auto* btnBox = new QHBoxLayout();
    btnBox->setSpacing(10);

    auto* btnCancel = new QPushButton("取消", this);
    btnCancel->setCursor(Qt::PointingHandCursor);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    auto* btnSave = new QPushButton("制定计划", this);
    btnSave->setObjectName("primaryBtn");
    btnSave->setCursor(Qt::PointingHandCursor);
    connect(btnSave, &QPushButton::clicked, this, &SocialPlanDialog::onCreatePlan);

    btnBox->addWidget(btnCancel, 1);
    btnBox->addWidget(btnSave, 1);
    layout->addLayout(btnBox);

    bodyLayout()->addLayout(layout);

    Q_UNUSED(preselectedContactId);
    reloadScopeCategoriesAndTargets();
    normalizeFormWidgets(this);
}

void SocialPlanDialog::onBookScopeToggled(bool isPersonal) {
    m_currentScope = isPersonal ? "PERSONAL" : "COMPANY";
    reloadScopeCategoriesAndTargets();
}

void SocialPlanDialog::reloadScopeCategoriesAndTargets() {
    if (!m_ownerCombo || !m_planTypeCombo) return;

    auto& db = DatabaseManager::instance();
    m_ownerCombo->clear();

    if (m_currentScope == "PERSONAL") {
        // 归属类别：个人 -> 人名字
        auto contacts = db.getContacts();
        for (const auto& c : contacts) {
            m_ownerCombo->addItem(c.name, c.id);
            if (m_preselectedContactId > 0 && c.id == m_preselectedContactId) {
                m_ownerCombo->setCurrentIndex(m_ownerCombo->count() - 1);
            }
        }
        if (m_ownerCombo->count() == 0) {
            m_ownerCombo->addItem("综合走动", 0);
        }

        // 事项类型：联动 记账 -> 支出 -> 个人 -> 人脉社交
        m_planTypeCombo->clear();
        QStringList subs = db.getSubCategoriesByParent("PERSONAL", "EXPENSE", "人脉社交");
        if (subs.isEmpty()) subs = {"聚餐", "娱乐", "礼物", "帮扶", "应酬"};
        m_planTypeCombo->addItems(subs);
    } else {
        // 归属类别：工作 -> 公司名字
        auto companies = db.getCompaniesList();
        int targetCompId = 0;
        if (m_preselectedContactId > 0) {
            auto cs = db.getContactDetails(m_preselectedContactId);
            targetCompId = cs.companyId;
        }
        for (const auto& comp : companies) {
            int cid = comp["id"].toInt();
            m_ownerCombo->addItem(comp["name"].toString(), cid);
            if (targetCompId > 0 && cid == targetCompId) {
                m_ownerCombo->setCurrentIndex(m_ownerCombo->count() - 1);
            }
        }
        if (m_ownerCombo->count() == 0) {
            m_ownerCombo->addItem("默认工作主体", 0);
        }

        // 事项类型：联动 记账 -> 支出 -> 工作 -> 商务社交
        m_planTypeCombo->clear();
        QStringList subs = db.getSubCategoriesByParent("COMPANY", "EXPENSE", "商务社交");
        if (subs.isEmpty()) subs = {"餐饮", "娱乐", "礼品", "差旅", "形象"};
        m_planTypeCombo->addItems(subs);
    }
}

void SocialPlanDialog::setupExecuteUi(const SocialPlanItem& item) {
    setFixedWidth(480);

    auto* layout = new QVBoxLayout();
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(10);

    auto* infoCard = new QFrame(this);
    infoCard->setStyleSheet("QFrame { background-color: #18181b; border: 1px solid #28282b; border-radius: 3px; padding: 8px; }");
    auto* cardLayout = new QVBoxLayout(infoCard);
    cardLayout->setContentsMargins(8, 6, 8, 6);
    cardLayout->setSpacing(6);

    auto* titleLbl = new QLabel(QString("事项: %1").arg(item.title), infoCard);
    titleLbl->setWordWrap(true);
    titleLbl->setStyleSheet("font-size: 14px; font-weight: bold; color: #e1e1e5; border: none;");
    cardLayout->addWidget(titleLbl);

    QString whoStr = !item.contactName.isEmpty() ? item.contactName : (!item.companyName.isEmpty() ? item.companyName : "综合走动");
    QString scopeLabel = (item.scope == "COMPANY") ? "工作" : "个人";
    auto* detailLbl = new QLabel(QString("属性: %1  |  关联: %2  |  预定日期: %3")
                                 .arg(scopeLabel, whoStr, item.plannedDate.toString("yyyy-MM-dd")), infoCard);
    detailLbl->setStyleSheet("color: #8e8e93; font-size: 12px; border: none;");
    cardLayout->addWidget(detailLbl);

    if (!item.remark.isEmpty()) {
        auto* remkLbl = new QLabel(QString("备忘: %1").arg(item.remark), infoCard);
        remkLbl->setWordWrap(true);
        remkLbl->setStyleSheet("color: #8e8e93; font-size: 12px; border: none;");
        cardLayout->addWidget(remkLbl);
    }
    layout->addWidget(infoCard);

    auto* formLayout = new QFormLayout();
    formLayout->setSpacing(10);
    formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_expenseSpin = new QDoubleSpinBox(this);
    m_expenseSpin->setRange(0.0, 10000000.0);
    m_expenseSpin->setValue(item.budget > 0 ? item.budget : 0.0);
    m_expenseSpin->setDecimals(2);
    m_expenseSpin->setPrefix("¥ ");
    formLayout->addRow("实际消费金额", m_expenseSpin);

    // 1. 开支分类：联动 记账 -> 支出 -> 个人 -> 人脉社交 (或工作 -> 商务社交)，下拉选择 + 右侧自定义输入
    auto* catBox = new QWidget(this);
    auto* catLayout = new QHBoxLayout(catBox);
    catLayout->setContentsMargins(0, 0, 0, 0);
    catLayout->setSpacing(10);

    m_expenseCategoryCombo = new QComboBox(catBox);
    m_expenseCategoryCombo->setMaxVisibleItems(8);
    QString bookScope = (item.scope == "COMPANY") ? "COMPANY" : "PERSONAL";
    QString parentCat = (bookScope == "COMPANY") ? "商务社交" : "人脉社交";
    QStringList cats = DatabaseManager::instance().getSubCategoriesByParent(bookScope, "EXPENSE", parentCat);
    if (cats.isEmpty()) cats = {"餐饮", "娱乐", "礼品", "差旅"};
    m_expenseCategoryCombo->addItems(cats);
    int matchIdx = m_expenseCategoryCombo->findText(item.planType);
    if (matchIdx >= 0) m_expenseCategoryCombo->setCurrentIndex(matchIdx);

    m_customExpenseCategoryEdit = new QLineEdit(catBox);
    m_customExpenseCategoryEdit->setPlaceholderText("或自定义开支分类");
    catLayout->addWidget(m_expenseCategoryCombo, 1);
    catLayout->addWidget(m_customExpenseCategoryEdit, 1);
    formLayout->addRow("开支分类", catBox);

    // 2. 扣款账户已按要求删除

    // 3. 结案总结：3行可见 textarea
    m_remarkEdit = createMultiLineEdit("结案总结 / 达成意向 / 人情增进评价...", this, 3);
    m_remarkEdit->setPlainText(item.remark);
    formLayout->addRow("结案总结", m_remarkEdit);

    layout->addLayout(formLayout);

    auto* btnBox = new QHBoxLayout();
    btnBox->setSpacing(10);

    // 4. 取消计划 文字颜色 红色 (#fa5151)
    auto* btnCancelPlan = new QPushButton("取消计划", this);
    btnCancelPlan->setObjectName("dangerBtn");
    btnCancelPlan->setCursor(Qt::PointingHandCursor);
    connect(btnCancelPlan, &QPushButton::clicked, this, &SocialPlanDialog::onCancelPlan);

    auto* btnComplete = new QPushButton("完成并转记账", this);
    btnComplete->setObjectName("primaryBtn");
    btnComplete->setCursor(Qt::PointingHandCursor);
    connect(btnComplete, &QPushButton::clicked, this, &SocialPlanDialog::onCompletePlanWithExpense);

    btnBox->addWidget(btnCancelPlan, 1);
    btnBox->addWidget(btnComplete, 1);
    layout->addLayout(btnBox);

    bodyLayout()->addLayout(layout);
    normalizeFormWidgets(this);
}

void SocialPlanDialog::onCreatePlan() {
    QString title = m_titleEdit->toPlainText().trimmed();
    if (title.isEmpty()) {
        markFieldInvalid(m_titleEdit);
        return;
    }

    auto& db = DatabaseManager::instance();
    QString customType = m_customPlanTypeEdit->text().trimmed();
    QString planType = customType.isEmpty() ? m_planTypeCombo->currentText().trimmed() : customType;
    if (planType.isEmpty()) planType = "请客吃饭";

    if (!customType.isEmpty()) {
        QString parentCat = (m_currentScope == "COMPANY") ? "商务社交" : "人脉社交";
        db.addCustomCategory("EXPENSE", parentCat, customType, m_currentScope);
    }

    int contactId = m_preselectedContactId;
    int companyId = 0;
    if (m_currentScope == "PERSONAL") {
        contactId = m_ownerCombo->currentData().toInt();
    } else {
        companyId = m_ownerCombo->currentData().toInt();
    }

    QDate plannedDate = m_dateEdit->date();
    int remindDays = m_remindSpin->value();
    QString remark = m_remarkEdit->toPlainText().trimmed();

    bool ok = db.addSocialPlan(
        contactId, planType, title, plannedDate, remindDays, remark, m_currentScope, companyId, 0.0
    );

    if (ok) {
        accept();
    } else {
        CustomDialog::showError(this, "错误", "计划保存失败！");
    }
}

void SocialPlanDialog::onCompletePlanWithExpense() {
    auto& db = DatabaseManager::instance();
    double expense = m_expenseSpin->value();
    QString customCat = m_customExpenseCategoryEdit->text().trimmed();
    QString cat = customCat.isEmpty() ? m_expenseCategoryCombo->currentText().trimmed() : customCat;

    if (!customCat.isEmpty()) {
        QString bookScope = (m_existingItem.scope == "COMPANY") ? "COMPANY" : "PERSONAL";
        QString parentCat = (bookScope == "COMPANY") ? "商务社交" : "人脉社交";
        db.addCustomCategory("EXPENSE", parentCat, customCat, bookScope);
    }

    QString remark = m_remarkEdit->toPlainText().trimmed();

    bool ok = db.completeSocialPlanWithExpense(
        m_existingItem.id, expense, cat, 0, remark
    );

    if (ok) {
        accept();
    } else {
        CustomDialog::showError(this, "错误", "结案保存失败！");
    }
}

void SocialPlanDialog::onCancelPlan() {
    if (CustomDialog::confirm(this, "确认取消计划", "确定要取消该走动计划吗？", "取消计划", "返回", true)) {
        DatabaseManager::instance().updateSocialPlanStatus(m_existingItem.id, "CANCELLED");
        accept();
    }
}

