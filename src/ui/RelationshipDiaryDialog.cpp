#include "RelationshipDiaryDialog.h"
#include "../core/DatabaseManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QButtonGroup>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>

namespace {
static QString normalizeSocialCategory(const QString& rawCat, const QString& scope, const QString& ioType) {
    QString s = rawCat.trimmed();
    if (s.isEmpty()) return s;

    // 1. 英文历史枚举代码规范化映射
    if (s.compare("GIFT", Qt::CaseInsensitive) == 0 ||
        s.compare("GIFT_OUT", Qt::CaseInsensitive) == 0 ||
        s.compare("GIFT_IN", Qt::CaseInsensitive) == 0) {
        return (scope == "COMPANY") ? "礼品" : "礼物";
    }
    if (s.compare("DINNER", Qt::CaseInsensitive) == 0 ||
        s.compare("TREAT", Qt::CaseInsensitive) == 0) {
        return (scope == "COMPANY") ? "餐饮" : "聚餐";
    }
    if (s.compare("ENTERTAIN", Qt::CaseInsensitive) == 0) {
        return "娱乐";
    }
    if (s.compare("MEETING", Qt::CaseInsensitive) == 0) {
        return (scope == "COMPANY") ? "差旅" : "通勤";
    }
    if (s.compare("HELP", Qt::CaseInsensitive) == 0 ||
        s.compare("HELP_GIVEN", Qt::CaseInsensitive) == 0 ||
        s.compare("HELP_RECEIVED", Qt::CaseInsensitive) == 0) {
        return "帮扶";
    }
    if (s.compare("BUSINESS", Qt::CaseInsensitive) == 0) {
        return (scope == "COMPANY") ? (ioType == "INCOME" ? "业务经营" : "餐饮") : "应酬";
    }

    // 2. 旧中文冗余/已删分类规范化映射
    if (s == "送礼随份子" || s == "送礼往来" || s == "受赠回礼" || s == "礼品馈赠") {
        return (scope == "COMPANY") ? "礼品" : "礼物";
    }
    if (s == "请客吃饭" || s == "餐饮宴请" || s == "品茗茶叙") {
        return (scope == "COMPANY") ? "餐饮" : "聚餐";
    }
    if (s == "休闲娱乐应酬" || s == "娱乐休闲" || s == "高端圈层维护") {
        return "娱乐";
    }
    if (s == "互相帮扶" || s == "人情帮扶" || s == "人情探望") {
        return "帮扶";
    }
    if (s == "客户招待应酬" || s == "形象包装") {
        return (scope == "COMPANY") ? "形象" : "应酬";
    }
    if (s == "差旅接待") {
        return (scope == "COMPANY") ? "差旅" : "通勤";
    }
    if (s == "业务") {
        return (scope == "COMPANY") ? (ioType == "INCOME" ? "业务经营" : "餐饮") : "应酬";
    }
    if (s == "咨询顾问") return "顾问";
    if (s == "佣金返点") return "返点";

    return s;
}

static const QStringList kDisallowedCustomCodes = {
    "GIFT", "GIFT_OUT", "GIFT_IN", "DINNER", "TREAT", "ENTERTAIN",
    "MEETING", "HELP", "HELP_GIVEN", "HELP_RECEIVED", "BUSINESS", "业务", "FINANCIAL"
};
}

RelationshipDiaryDialog::RelationshipDiaryDialog(int contactId, const QString& contactName, QWidget* parent)
    : CustomDialog(QString("礼尚往来 - %1").arg(contactName), parent),
      m_contactId(contactId), m_contactName(contactName) {
    setupUi();
    reloadScopeData();
}

RelationshipDiaryDialog::RelationshipDiaryDialog(int eventId, int contactId, const QString& contactName, QWidget* parent)
    : CustomDialog(QString("编辑礼尚往来 - %1").arg(contactName), parent),
      m_contactId(contactId), m_editEventId(eventId), m_contactName(contactName) {
    setupUi();
    reloadScopeData();
    loadEventData(eventId);
}

void RelationshipDiaryDialog::setupUi() {
    setFixedWidth(480);

    auto* layout = contentLayout();
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(10);

    QString bigToggleStyle =
        "QPushButton { background-color: #1c1c1f; color: #8e8e93; border: none; border-radius: 3px; font-size: 15px; font-weight: bold; }"
        "QPushButton:hover { background-color: #242428; color: #e1e1e5; }"
        "QPushButton:checked { background-color: #00a361; color: #ffffff; font-weight: bold; }";

    // 1. 先选择 “个人” | “工作”
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
    connect(m_btnPersonal, &QPushButton::toggled, this, &RelationshipDiaryDialog::onScopeToggled);

    scopeRow->addWidget(m_btnPersonal, 1);
    scopeRow->addWidget(m_btnWork, 1);
    layout->addLayout(scopeRow);

    // 2. 选择 “支出（投资这个人）” | “收入（这个人回报）”
    auto* ioRow = new QHBoxLayout();
    ioRow->setSpacing(10);

    m_btnExpense = new QPushButton("支出（投资这个人）", this);
    m_btnExpense->setCheckable(true);
    m_btnExpense->setChecked(true);
    m_btnExpense->setFixedHeight(38);
    m_btnExpense->setCursor(Qt::PointingHandCursor);
    m_btnExpense->setStyleSheet(bigToggleStyle);

    m_btnIncome = new QPushButton("收入（这个人回报）", this);
    m_btnIncome->setCheckable(true);
    m_btnIncome->setFixedHeight(38);
    m_btnIncome->setCursor(Qt::PointingHandCursor);
    m_btnIncome->setStyleSheet(bigToggleStyle);

    auto* ioGroup = new QButtonGroup(this);
    ioGroup->addButton(m_btnExpense);
    ioGroup->addButton(m_btnIncome);
    connect(m_btnExpense, &QPushButton::toggled, this, &RelationshipDiaryDialog::onIoTypeToggled);

    ioRow->addWidget(m_btnExpense, 1);
    ioRow->addWidget(m_btnIncome, 1);
    layout->addLayout(ioRow);

    auto* form = new QFormLayout();
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // 3. 归属类别：个人就人名字，工作就公司名字
    m_ownerCombo = new QComboBox(this);
    form->addRow("归属类别", m_ownerCombo);

    // 4. 事件分类：联动四象限分类 + 右侧自定义输入
    auto* catBox = new QWidget(this);
    auto* catLayout = new QHBoxLayout(catBox);
    catLayout->setContentsMargins(0, 0, 0, 0);
    catLayout->setSpacing(10);

    m_categoryCombo = new QComboBox(catBox);
    m_customCategoryEdit = new QLineEdit(catBox);
    m_customCategoryEdit->setPlaceholderText("或自定义分类");
    catLayout->addWidget(m_categoryCombo, 1);
    catLayout->addWidget(m_customCategoryEdit, 1);
    form->addRow("事件分类", catBox);

    // 5. 往来金额
    m_amountSpin = new QDoubleSpinBox(this);
    m_amountSpin->setRange(0, 10000000);
    m_amountSpin->setDecimals(2);
    m_amountSpin->setValue(0);
    m_amountSpin->setPrefix("¥ ");
    form->addRow("往来金额", m_amountSpin);

    // 6. 事件摘要：3行可见 textarea
    m_titleEdit = createMultiLineEdit("事项摘要（例如：请客聚餐 / 敬赠茶礼 / 项目引荐回馈...）", this, 3);
    form->addRow("事件摘要", m_titleEdit);

    // 7. 预定日期：点击弹出日历选择
    m_dateEdit = new QDateEdit(QDate::currentDate(), this);
    m_dateEdit->setCalendarPopup(true);
    m_dateEdit->setDisplayFormat("yyyy-MM-dd");
    form->addRow("发生日期", m_dateEdit);

    // 8. 补充说明：3行可见 textarea
    m_detailEdit = createMultiLineEdit("详细纪要与交流成果...", this, 3);
    form->addRow("补充说明", m_detailEdit);

    layout->addLayout(form);

    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);

    auto* cancelBtn = new QPushButton("取消", this);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    auto* saveBtn = new QPushButton(m_editEventId > 0 ? "保存修改" : "保存", this);
    saveBtn->setObjectName("primaryBtn");
    saveBtn->setCursor(Qt::PointingHandCursor);

    btnRow->addWidget(cancelBtn, 1);
    btnRow->addWidget(saveBtn, 1);
    layout->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(saveBtn, &QPushButton::clicked, this, [this]() {
        if (saveRecord()) accept();
    });

    normalizeFormWidgets(this);
}

void RelationshipDiaryDialog::onScopeToggled(bool isPersonal) {
    m_currentScope = isPersonal ? "PERSONAL" : "COMPANY";
    reloadScopeData();
}

void RelationshipDiaryDialog::onIoTypeToggled(bool isExpense) {
    m_currentIoType = isExpense ? "EXPENSE" : "INCOME";
    reloadCategories();
}

void RelationshipDiaryDialog::reloadScopeData() {
    if (!m_ownerCombo) return;
    auto& db = DatabaseManager::instance();

    m_ownerCombo->clear();
    if (m_currentScope == "PERSONAL") {
        auto contacts = db.getContacts();
        for (const auto& c : contacts) {
            m_ownerCombo->addItem(c.name, c.id);
            if (m_contactId > 0 && c.id == m_contactId) {
                m_ownerCombo->setCurrentIndex(m_ownerCombo->count() - 1);
            }
        }
    } else {
        auto companies = db.getCompaniesList();
        int targetCompId = 0;
        if (m_contactId > 0) {
            auto cs = db.getContactDetails(m_contactId);
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
    }

    reloadCategories();
}

void RelationshipDiaryDialog::reloadCategories() {
    if (!m_categoryCombo) return;
    auto& db = DatabaseManager::instance();

    QString currentSel = m_categoryCombo->currentText().trimmed();
    m_categoryCombo->clear();

    QString parentCat;
    if (m_currentScope == "PERSONAL") {
        if (m_currentIoType == "EXPENSE") {
            // 个人 -> 支出: 人脉社交
            parentCat = "人脉社交";
        } else {
            // 个人 -> 收入: 职业收入 (如 顾问、返点)
            parentCat = "职业收入";
        }
    } else {
        if (m_currentIoType == "EXPENSE") {
            // 工作 -> 支出: 商务社交
            parentCat = "商务社交";
        } else {
            // 工作 -> 收入: 业务经营 (职业收入已全删，严格变更为业务经营)
            parentCat = "业务经营";
        }
    }

    // 严格按数据库里的实际数据动态加载，绝不硬编码假分类
    QStringList subs = db.getSubCategoriesByParent(m_currentScope, m_currentIoType, parentCat);
    m_categoryCombo->addItems(subs);

    int idx = m_categoryCombo->findText(currentSel);
    if (idx >= 0) {
        m_categoryCombo->setCurrentIndex(idx);
    }
}

void RelationshipDiaryDialog::loadEventData(int eventId) {
    if (eventId <= 0) return;
    auto map = DatabaseManager::instance().getSocialEventById(eventId);
    if (map.isEmpty()) return;

    QString scope = map["scope"].toString().trimmed();
    bool isPersonal = (scope != "COMPANY");
    m_btnPersonal->setChecked(isPersonal);
    m_btnWork->setChecked(!isPersonal);
    m_currentScope = isPersonal ? "PERSONAL" : "COMPANY";

    QString ioType = map["io_type"].toString().trimmed();
    bool isExpense = (ioType != "INCOME");
    m_btnExpense->setChecked(isExpense);
    m_btnIncome->setChecked(!isExpense);
    m_currentIoType = isExpense ? "EXPENSE" : "INCOME";

    reloadScopeData();

    // 预选归属主体
    if (m_ownerCombo) {
        if (isPersonal) {
            int cid = map["contact_id"].toInt();
            for (int i = 0; i < m_ownerCombo->count(); ++i) {
                if (m_ownerCombo->itemData(i).toInt() == cid) {
                    m_ownerCombo->setCurrentIndex(i);
                    break;
                }
            }
        } else {
            int compId = map["company_id"].toInt();
            for (int i = 0; i < m_ownerCombo->count(); ++i) {
                if (m_ownerCombo->itemData(i).toInt() == compId) {
                    m_ownerCombo->setCurrentIndex(i);
                    break;
                }
            }
        }
    }

    // 预选分类 (智能映射历史遗留英文代号及已规范化分类，绝不在自定义输入框显示内部代号)
    if (m_customCategoryEdit) m_customCategoryEdit->clear();
    QString cat = map["event_category"].toString().trimmed();
    QString targetCat = normalizeSocialCategory(cat, m_currentScope, m_currentIoType);

    int cIdx = m_categoryCombo->findText(targetCat);
    if (cIdx < 0 && !targetCat.isEmpty()) {
        for (int i = 0; i < m_categoryCombo->count(); ++i) {
            QString itemTxt = m_categoryCombo->itemText(i);
            if (itemTxt == targetCat || itemTxt.contains(targetCat) || targetCat.contains(itemTxt)) {
                cIdx = i;
                break;
            }
        }
    }

    if (cIdx >= 0) {
        m_categoryCombo->setCurrentIndex(cIdx);
        if (m_customCategoryEdit) m_customCategoryEdit->clear();
    } else {
        bool isLegacyInternal = false;
        for (const auto& code : kDisallowedCustomCodes) {
            if (cat.compare(code, Qt::CaseInsensitive) == 0) {
                isLegacyInternal = true;
                break;
            }
        }

        if (isLegacyInternal) {
            // 内部遗留代号绝对不显示在自定义框，默认选中下拉框第一项
            if (m_categoryCombo->count() > 0) {
                m_categoryCombo->setCurrentIndex(0);
            }
            if (m_customCategoryEdit) m_customCategoryEdit->clear();
        } else if (!cat.isEmpty()) {
            // 真正属于用户手打的分类载入下拉框并选中，自定义框保持清空
            if (m_categoryCombo->findText(cat) < 0) {
                m_categoryCombo->addItem(cat);
            }
            m_categoryCombo->setCurrentIndex(m_categoryCombo->findText(cat));
            if (m_customCategoryEdit) m_customCategoryEdit->clear();
        }
    }

    // 预选金额
    double myCost = map["my_cost"].toDouble();
    double theirVal = map["their_value"].toDouble();
    double amt = (isExpense) ? (myCost > 0 ? myCost : theirVal) : (theirVal > 0 ? theirVal : myCost);
    if (m_amountSpin) m_amountSpin->setValue(amt);

    // 预选摘要
    if (m_titleEdit) m_titleEdit->setPlainText(map["title"].toString());

    // 预选日期
    QString dtStr = map["event_date"].toString().trimmed();
    QDate dt = QDate::fromString(dtStr.left(10), "yyyy-MM-dd");
    if (!dt.isValid()) dt = QDate::fromString(dtStr.left(10), "yyyy/MM/dd");
    if (dt.isValid() && m_dateEdit) m_dateEdit->setDate(dt);

    // 预选补充说明
    if (m_detailEdit) m_detailEdit->setPlainText(map["detail"].toString());
}

bool RelationshipDiaryDialog::saveRecord() {
    QString title = m_titleEdit->toPlainText().trimmed();
    if (title.isEmpty()) {
        markFieldInvalid(m_titleEdit);
        return false;
    }

    auto& db = DatabaseManager::instance();
    QString customCat = m_customCategoryEdit->text().trimmed();
    bool isDisallowed = false;
    for (const auto& code : kDisallowedCustomCodes) {
        if (customCat.compare(code, Qt::CaseInsensitive) == 0) {
            isDisallowed = true;
            break;
        }
    }
    if (isDisallowed) {
        customCat.clear();
        if (m_customCategoryEdit) m_customCategoryEdit->clear();
    }

    QString cat = customCat.isEmpty() ? m_categoryCombo->currentText().trimmed() : customCat;
    if (cat.isEmpty() && m_categoryCombo->count() > 0) {
        cat = m_categoryCombo->itemText(0);
    }

    if (!customCat.isEmpty()) {
        QString parentCat;
        if (m_currentScope == "PERSONAL") {
            parentCat = (m_currentIoType == "EXPENSE") ? "人脉社交" : "职业收入";
        } else {
            parentCat = (m_currentIoType == "EXPENSE") ? "商务社交" : "业务经营";
        }
        db.addCustomCategory(m_currentIoType, parentCat, customCat, m_currentScope);
    }

    int targetContactId = m_contactId;
    int companyId = 0;
    if (m_currentScope == "PERSONAL") {
        int selId = m_ownerCombo->currentData().toInt();
        if (selId > 0) targetContactId = selId;
    } else {
        companyId = m_ownerCombo->currentData().toInt();
    }

    double amt = m_amountSpin->value();
    double myCost = (m_currentIoType == "EXPENSE") ? amt : 0.0;
    double theirVal = (m_currentIoType == "INCOME") ? amt : 0.0;
    QDate dt = m_dateEdit->date();
    QString detail = m_detailEdit->toPlainText().trimmed();

    if (m_editEventId > 0) {
        bool ok = db.updateSocialEventFull(
            m_editEventId, targetContactId, m_currentScope, companyId,
            m_currentIoType, cat, title, myCost, theirVal, detail, dt
        );
        if (!ok) {
            QMessageBox::critical(this, "错误", "更新礼尚往来记录失败！");
            return false;
        }
        return true;
    }

    QSqlQuery q(db.database());
    q.prepare("INSERT INTO social_events (contact_id, scope, company_id, io_type, event_category, title, detail, "
              "event_date, my_cost, their_value, status) "
              "VALUES (:cid, :sc, :comp, :io, :cat, :title, :detail, :dt, :cost, :val, 'CLOSED');");
    q.bindValue(":cid", targetContactId);
    q.bindValue(":sc", m_currentScope == "COMPANY" ? "COMPANY" : "PRIVATE");
    if (companyId > 0) {
        q.bindValue(":comp", companyId);
    } else {
        q.bindValue(":comp", QVariant());
    }
    q.bindValue(":io", m_currentIoType);
    q.bindValue(":cat", cat);
    q.bindValue(":title", title);
    q.bindValue(":detail", detail);
    q.bindValue(":dt", dt.toString("yyyy-MM-dd"));
    q.bindValue(":cost", myCost);
    q.bindValue(":val", theirVal);

    if (!q.exec()) {
        QMessageBox::critical(this, "错误", "保存失败！" + q.lastError().text());
        return false;
    }

    return true;
}

