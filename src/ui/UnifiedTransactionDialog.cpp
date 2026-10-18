#include "UnifiedTransactionDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QScrollBar>
#include <QCalendarWidget>
#include <QLocale>

UnifiedTransactionDialog::UnifiedTransactionDialog(const QString& defaultType,
                                                   QWidget* parent,
                                                   bool hideTransTypeRow,
                                                   const QString& defaultBookType,
                                                   bool hideBookTypeRow)
    : CustomDialog(hideTransTypeRow
                       ? (defaultType == "INCOME" ? "记账 · 收入" : "记账 · 支出")
                       : (hideBookTypeRow
                              ? ((defaultBookType == "WORK" || defaultBookType == "COMPANY") ? "记账 · 工作" : "记账 · 个人")
                              : "记账"),
                   parent),
      m_currentTransType(defaultType.isEmpty() ? "EXPENSE" : defaultType),
      m_currentBookType((defaultBookType == "WORK" || defaultBookType == "COMPANY") ? "COMPANY" : "PERSONAL"),
      m_hideTransTypeRow(hideTransTypeRow),
      m_hideBookTypeRow(hideBookTypeRow) {
    setupUi();
    loadCompanies();
    loadRelations();
    onBookTypeToggled(m_currentBookType == "PERSONAL");
}

void UnifiedTransactionDialog::setupUi() {
    setFixedWidth(480);

    auto* layout = new QVBoxLayout();
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(10);

    QString bigToggleStyle =
        "QPushButton { background-color: #1c1c1f; color: #8e8e93; border: none; border-radius: 3px; font-size: 16px; font-weight: bold; }"
        "QPushButton:hover { background-color: #242428; color: #e1e1e5; }"
        "QPushButton:checked { background-color: #00a361; color: #ffffff; font-weight: bold; }";

    // ==========================================
    // Row 1: 支出 | 收入 (从综合页点添加时隐藏，默认选中对应类型)
    // ==========================================
    m_transTypeRowWidget = new QWidget(this);
    auto* rowType = new QHBoxLayout(m_transTypeRowWidget);
    rowType->setContentsMargins(0, 0, 0, 0);
    rowType->setSpacing(10);

    m_btnExpense = new QPushButton("支出", m_transTypeRowWidget);
    m_btnExpense->setCheckable(true);
    m_btnExpense->setFixedHeight(40);
    m_btnExpense->setCursor(Qt::PointingHandCursor);
    m_btnExpense->setStyleSheet(bigToggleStyle);
    m_btnExpense->setChecked(m_currentTransType == "EXPENSE");

    m_btnIncome = new QPushButton("收入", m_transTypeRowWidget);
    m_btnIncome->setCheckable(true);
    m_btnIncome->setFixedHeight(40);
    m_btnIncome->setCursor(Qt::PointingHandCursor);
    m_btnIncome->setStyleSheet(bigToggleStyle);
    m_btnIncome->setChecked(m_currentTransType == "INCOME");

    auto* typeGroup = new QButtonGroup(this);
    typeGroup->addButton(m_btnExpense);
    typeGroup->addButton(m_btnIncome);

    connect(m_btnExpense, &QPushButton::toggled, this, &UnifiedTransactionDialog::onTransTypeToggled);

    rowType->addWidget(m_btnExpense, 1);
    rowType->addWidget(m_btnIncome, 1);
    m_transTypeRowWidget->setVisible(!m_hideTransTypeRow);
    layout->addWidget(m_transTypeRowWidget);

    // ==========================================
    // Row 2: 个人 | 工作 (从财务页点击个人/工作账单添加时隐藏，默认选中对应账本)
    // ==========================================
    m_bookTypeRowWidget = new QWidget(this);
    auto* rowBook = new QHBoxLayout(m_bookTypeRowWidget);
    rowBook->setContentsMargins(0, 0, 0, 0);
    rowBook->setSpacing(10);

    m_btnPersonal = new QPushButton("个人", m_bookTypeRowWidget);
    m_btnPersonal->setCheckable(true);
    m_btnPersonal->setFixedHeight(40);
    m_btnPersonal->setCursor(Qt::PointingHandCursor);
    m_btnPersonal->setStyleSheet(bigToggleStyle);
    m_btnPersonal->setChecked(m_currentBookType == "PERSONAL");

    m_btnCompany = new QPushButton("工作", m_bookTypeRowWidget);
    m_btnCompany->setCheckable(true);
    m_btnCompany->setFixedHeight(40);
    m_btnCompany->setCursor(Qt::PointingHandCursor);
    m_btnCompany->setStyleSheet(bigToggleStyle);
    m_btnCompany->setChecked(m_currentBookType == "COMPANY");

    auto* bookGroup = new QButtonGroup(this);
    bookGroup->addButton(m_btnPersonal);
    bookGroup->addButton(m_btnCompany);

    connect(m_btnPersonal, &QPushButton::toggled, this, &UnifiedTransactionDialog::onBookTypeToggled);

    rowBook->addWidget(m_btnPersonal, 1);
    rowBook->addWidget(m_btnCompany, 1);
    m_bookTypeRowWidget->setVisible(!m_hideBookTypeRow);
    layout->addWidget(m_bookTypeRowWidget);

    // ==========================================
    // Row 3: 选择公司... (若选工作且公司>1时显示，否则隐藏)
    // ==========================================
    m_companyRow = new QFrame(this);
    m_companyRow->setFixedHeight(38);
    m_companyRow->setStyleSheet("QFrame { background-color: #18181b; border: 1px solid #28282b; border-radius: 3px; }");
    auto* compLayout = new QHBoxLayout(m_companyRow);
    compLayout->setContentsMargins(12, 0, 8, 0);
    compLayout->setSpacing(10);

    auto* compLbl = new QLabel("公司", m_companyRow);
    compLbl->setStyleSheet("color: #8e8e93; font-size: 13px; border: none; background: transparent;");

    m_companyCombo = new QComboBox(m_companyRow);
    m_companyCombo->setStyleSheet("QComboBox { background: transparent; border: none; color: #e1e1e5; font-size: 13px; }");
    compLayout->addWidget(compLbl);
    compLayout->addWidget(m_companyCombo, 1);
    m_companyRow->setVisible(false);
    layout->addWidget(m_companyRow);

    // ==========================================
    // Row 4: 大类 | 小类 双列列表
    // ==========================================
    auto* listLayout = new QHBoxLayout();
    listLayout->setSpacing(10);

    QString catListStyle =
        "QListWidget { background-color: #18181b; border: 1px solid #28282b; border-radius: 3px; color: #e1e1e5; padding: 4px; font-size: 13px; outline: none; }"
        "QListWidget::item { height: 26px; padding-left: 8px; border-radius: 3px; margin-bottom: 2px; }"
        "QListWidget::item:selected { background-color: #00a361; color: #ffffff; font-weight: bold; }"
        "QListWidget::item:hover:!selected { background-color: #242428; }";

    m_parentListWidget = new QListWidget(this);
    m_parentListWidget->setFixedHeight(152);
    m_parentListWidget->setStyleSheet(catListStyle);

    m_subListWidget = new QListWidget(this);
    m_subListWidget->setFixedHeight(152);
    m_subListWidget->setStyleSheet(catListStyle);

    connect(m_parentListWidget, &QListWidget::currentItemChanged, this, &UnifiedTransactionDialog::onParentCategorySelected);
    connect(m_parentListWidget, &QListWidget::itemClicked, this, [this]() {
        if (m_customParentEdit) m_customParentEdit->clear();
    });
    connect(m_subListWidget, &QListWidget::itemClicked, this, [this]() {
        if (m_customSubEdit) m_customSubEdit->clear();
    });

    listLayout->addWidget(m_parentListWidget, 1);
    listLayout->addWidget(m_subListWidget, 1);
    layout->addLayout(listLayout);

    // ==========================================
    // Row 5: 其他自定义 | 其他自定义
    // ==========================================
    auto* customCatLayout = new QHBoxLayout();
    customCatLayout->setSpacing(10);

    m_customParentEdit = new QLineEdit(this);
    m_customParentEdit->setPlaceholderText("其他自定义");
    m_customParentEdit->setFixedHeight(38);
    m_customParentEdit->setStyleSheet(
        "QLineEdit { background-color: #18181b; border: 1px solid #28282b; border-radius: 3px; padding: 0 12px; color: #e1e1e5; font-size: 13px; }"
    );

    m_customSubEdit = new QLineEdit(this);
    m_customSubEdit->setPlaceholderText("其他自定义");
    m_customSubEdit->setFixedHeight(38);
    m_customSubEdit->setStyleSheet(
        "QLineEdit { background-color: #18181b; border: 1px solid #28282b; border-radius: 3px; padding: 0 12px; color: #e1e1e5; font-size: 13px; }"
    );

    customCatLayout->addWidget(m_customParentEdit, 1);
    customCatLayout->addWidget(m_customSubEdit, 1);
    layout->addLayout(customCatLayout);

    // ==========================================
    // Row 6: [金额  100.00] | [时间  2026-10-11 14:28]
    // ==========================================
    auto* amountTimeLayout = new QHBoxLayout();
    amountTimeLayout->setSpacing(10);

    auto* amtBox = new QFrame(this);
    amtBox->setFixedHeight(38);
    amtBox->setStyleSheet("QFrame { background-color: #18181b; border: 1px solid #28282b; border-radius: 3px; }");
    auto* amtBoxLayout = new QHBoxLayout(amtBox);
    amtBoxLayout->setContentsMargins(12, 0, 8, 0);
    amtBoxLayout->setSpacing(10);

    auto* amtLbl = new QLabel("金额", amtBox);
    amtLbl->setStyleSheet("color: #8e8e93; font-size: 13px; border: none; background: transparent;");

    m_amountSpin = new QDoubleSpinBox(amtBox);
    m_amountSpin->setLocale(QLocale::c());
    m_amountSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_amountSpin->setRange(0.01, 999999999.0);
    m_amountSpin->setValue(100.00);
    m_amountSpin->setDecimals(2);
    m_amountSpin->setStyleSheet("QDoubleSpinBox { background: transparent; border: none; color: #e1e1e5; font-size: 14px; }");
    amtBoxLayout->addWidget(amtLbl);
    amtBoxLayout->addWidget(m_amountSpin, 1);

    auto* timeBox = new QFrame(this);
    timeBox->setFixedHeight(38);
    timeBox->setStyleSheet("QFrame { background-color: #18181b; border: 1px solid #28282b; border-radius: 3px; }");
    auto* timeBoxLayout = new QHBoxLayout(timeBox);
    timeBoxLayout->setContentsMargins(12, 0, 8, 0);
    timeBoxLayout->setSpacing(10);

    auto* timeLbl = new QLabel("时间", timeBox);
    timeLbl->setStyleSheet("color: #8e8e93; font-size: 13px; border: none; background: transparent;");

    m_timeEdit = new QDateTimeEdit(QDateTime::currentDateTime(), timeBox);
    m_timeEdit->setLocale(QLocale::c());
    m_timeEdit->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_timeEdit->setDisplayFormat("yyyy-MM-dd HH:mm");
    m_timeEdit->setCalendarPopup(true);
    m_timeEdit->setStyleSheet(
        "QDateTimeEdit { background: transparent; border: none; color: #e1e1e5; font-size: 13px; }"
    );
    timeBoxLayout->addWidget(timeLbl);
    timeBoxLayout->addWidget(m_timeEdit, 1);

    amountTimeLayout->addWidget(amtBox, 1);
    amountTimeLayout->addWidget(timeBox, 1);
    layout->addLayout(amountTimeLayout);

    // ==========================================
    // Row 7: [关联   不关联            ▼]
    // ==========================================
    auto* relBox = new QFrame(this);
    relBox->setFixedHeight(38);
    relBox->setStyleSheet("QFrame { background-color: #18181b; border: 1px solid #28282b; border-radius: 3px; }");
    auto* relRowLayout = new QHBoxLayout(relBox);
    relRowLayout->setContentsMargins(12, 0, 8, 0);
    relRowLayout->setSpacing(10);

    auto* relLbl = new QLabel("关联", relBox);
    relLbl->setStyleSheet("color: #8e8e93; font-size: 13px; border: none; background: transparent;");

    m_relationCombo = new QComboBox(relBox);
    m_relationCombo->setStyleSheet(
        "QComboBox { background: transparent; border: none; color: #e1e1e5; font-size: 13px; }"
        "QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: top right; width: 24px; border: none; }"
        "QComboBox::down-arrow { image: none; border-left: 5px solid transparent; border-right: 5px solid transparent; border-top: 6px solid #e1e1e5; margin-right: 4px; }"
    );
    relRowLayout->addWidget(relLbl);
    relRowLayout->addWidget(m_relationCombo, 1);
    layout->addWidget(relBox);

    // ==========================================
    // Row 8: 外框容器内嵌 [凭证] 方形按钮 + 补充说明....
    // ==========================================
    auto* voucherOuterBox = new QFrame(this);
    voucherOuterBox->setFixedHeight(78);
    voucherOuterBox->setStyleSheet("QFrame { background-color: #18181b; border: 1px solid #28282b; border-radius: 3px; }");
    auto* voucherRemarkLayout = new QHBoxLayout(voucherOuterBox);
    voucherRemarkLayout->setContentsMargins(10, 10, 10, 10);
    voucherRemarkLayout->setSpacing(12);

    m_btnVoucher = new QPushButton("凭证", voucherOuterBox);
    m_btnVoucher->setFixedSize(56, 56);
    m_btnVoucher->setCursor(Qt::PointingHandCursor);
    m_btnVoucher->setStyleSheet(
        "QPushButton { background-color: #141416; color: #8e8e93; border: 1px solid #28282b; border-radius: 3px; font-size: 13px; padding: 0px; }"
        "QPushButton:hover { border-color: #00a361; color: #e1e1e5; }"
    );
    connect(m_btnVoucher, &QPushButton::clicked, this, &UnifiedTransactionDialog::onSelectVoucherClicked);

    m_remarkEdit = new QLineEdit(voucherOuterBox);
    m_remarkEdit->setPlaceholderText("补充说明....");
    m_remarkEdit->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_remarkEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_remarkEdit->setStyleSheet("QLineEdit { background: transparent; border: none; color: #e1e1e5; font-size: 13px; padding: 2px; }");

    voucherRemarkLayout->addWidget(m_btnVoucher);
    voucherRemarkLayout->addWidget(m_remarkEdit, 1);
    layout->addWidget(voucherOuterBox);

    // ==========================================
    // Row 9: 取消 | 保存
    // ==========================================
    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);

    m_btnCancel = new QPushButton("取消", this);
    m_btnCancel->setFixedHeight(40);
    m_btnCancel->setCursor(Qt::PointingHandCursor);
    m_btnCancel->setStyleSheet(
        "QPushButton { background-color: #1c1c1f; color: #e1e1e5; border: 1px solid #28282b; border-radius: 3px; font-size: 16px; }"
        "QPushButton:hover { background-color: #28282b; color: #ffffff; }"
    );
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    m_btnSave = new QPushButton("保存", this);
    m_btnSave->setObjectName("primaryBtn");
    m_btnSave->setFixedHeight(40);
    m_btnSave->setCursor(Qt::PointingHandCursor);
    m_btnSave->setStyleSheet(
        "QPushButton { background-color: #00a361; color: #ffffff; border: none; border-radius: 3px; font-size: 16px; font-weight: bold; }"
        "QPushButton:hover { background-color: #00b86d; color: #ffffff; }"
    );
    connect(m_btnSave, &QPushButton::clicked, this, &UnifiedTransactionDialog::onSaveTransaction);

    btnRow->addWidget(m_btnCancel, 1);
    btnRow->addWidget(m_btnSave, 1);
    layout->addLayout(btnRow);

    bodyLayout()->addLayout(layout);

    normalizeFormWidgets(this);
}

void UnifiedTransactionDialog::onTransTypeToggled(bool isExpense) {
    m_currentTransType = isExpense ? "EXPENSE" : "INCOME";
    loadCategories();
}

void UnifiedTransactionDialog::onBookTypeToggled(bool isPersonal) {
    m_currentBookType = isPersonal ? "PERSONAL" : "COMPANY";
    auto companies = DatabaseManager::instance().getCompaniesList();
    if (m_companyRow) {
        m_companyRow->setVisible(!isPersonal && companies.size() > 1);
    }
    loadCategories();
}

void UnifiedTransactionDialog::loadCategories() {
    m_hierarchyList = DatabaseManager::instance().getCategoryHierarchy(m_currentTransType, m_currentBookType);
    if (m_hierarchyList.isEmpty()) {
        m_hierarchyList = DatabaseManager::instance().getCategoryHierarchy(m_currentTransType);
    }
    m_parentListWidget->clear();
    m_subListWidget->clear();

    for (const auto& item : m_hierarchyList) {
        m_parentListWidget->addItem(item.parentCategory);
    }

    if (m_parentListWidget->count() > 0) {
        m_parentListWidget->setCurrentRow(0);
    }
}

void UnifiedTransactionDialog::onParentCategorySelected(QListWidgetItem* current, QListWidgetItem* previous) {
    Q_UNUSED(previous);
    m_subListWidget->clear();
    if (!current) return;
    QString parentCat = current->text();

    for (const auto& item : m_hierarchyList) {
        if (item.parentCategory == parentCat) {
            for (const auto& sub : item.subCategories) {
                m_subListWidget->addItem(sub);
            }
            break;
        }
    }

    if (m_subListWidget->count() > 0) {
        m_subListWidget->setCurrentRow(0);
    }
}

void UnifiedTransactionDialog::loadCompanies() {
    m_companyCombo->clear();
    auto comps = DatabaseManager::instance().getCompaniesList();
    for (const auto& c : comps) {
        int isMine = c.contains("is_my_company") ? c["is_my_company"].toInt() : -1;
        QString r = c["role_type"].toString().trimmed();
        bool isMyCorp = (isMine == 1) || (isMine != 0 && r != "EXTERNAL" && (r == "OWNER" || r == "PARTNER" || r == "EMPLOYEE"));
        if (!isMyCorp) continue;
        m_companyCombo->addItem(c["name"].toString(), c["id"]);
    }
}

void UnifiedTransactionDialog::loadRelations() {
    m_relationCombo->clear();
    QVariantMap noneMap;
    noneMap["type"] = "NONE";
    noneMap["id"] = 0;
    m_relationCombo->addItem("不关联", noneMap);

    auto contacts = DatabaseManager::instance().getContacts();
    for (const auto& c : contacts) {
        QVariantMap cmap;
        cmap["type"] = "CONTACT";
        cmap["id"] = c.id;
        QString label = QString("%1 (%2%3)")
                            .arg(c.name)
                            .arg(c.relationType.isEmpty() ? "朋友" : c.relationType)
                            .arg(c.company.isEmpty() ? "" : " - " + c.company);
        m_relationCombo->addItem(label, cmap);
    }

    auto comps = DatabaseManager::instance().getCompaniesList();
    for (const auto& c : comps) {
        QVariantMap compMap;
        compMap["type"] = "COMPANY";
        compMap["id"] = c["id"].toInt();
        m_relationCombo->addItem(QString("[工作] %1").arg(c["name"].toString()), compMap);
    }
}

void UnifiedTransactionDialog::setPreselectedContact(int contactId) {
    for (int i = 0; i < m_relationCombo->count(); ++i) {
        QVariant data = m_relationCombo->itemData(i);
        if (data.isValid() && data.toMap()["type"].toString() == "CONTACT" && data.toMap()["id"].toInt() == contactId) {
            m_relationCombo->setCurrentIndex(i);
            break;
        }
    }
}

void UnifiedTransactionDialog::setEditTransactionId(int transactionId) {
    m_editTransId = transactionId;
    if (m_editTransId <= 0) return;

    setDialogTitle("编辑流水交易");
    if (m_btnSave) m_btnSave->setText("保存修改");

    auto t = DatabaseManager::instance().getTransactionById(transactionId);
    if (t.isEmpty()) return;

    QString bType = t["book_type"].toString();
    QString tType = t["trans_type"].toString();
    m_currentBookType = bType.isEmpty() ? "PERSONAL" : bType;
    m_currentTransType = tType.isEmpty() ? "EXPENSE" : tType;

    if (m_btnExpense && m_btnIncome) {
        m_btnExpense->setChecked(m_currentTransType == "EXPENSE");
        m_btnIncome->setChecked(m_currentTransType == "INCOME");
    }
    if (m_btnPersonal && m_btnCompany) {
        m_btnPersonal->setChecked(m_currentBookType == "PERSONAL");
        m_btnCompany->setChecked(m_currentBookType == "COMPANY");
    }

    if (m_amountSpin) {
        m_amountSpin->setValue(t["amount"].toDouble());
    }

    if (m_timeEdit) {
        QString transTimeStr = t["trans_time"].toString().trimmed();
        if (transTimeStr.isEmpty()) transTimeStr = t["trans_date"].toString().trimmed();
        QDateTime dt;
        const QStringList formats = {
            "yyyy-MM-dd HH:mm:ss", "yyyy-MM-dd HH:mm", "yyyy-MM-dd",
            "yyyy/MM/dd HH:mm:ss", "yyyy/MM/dd HH:mm", "yyyy/MM/dd"
        };
        for (const auto& fmt : formats) {
            dt = QDateTime::fromString(transTimeStr, fmt);
            if (dt.isValid()) break;
        }
        if (dt.isValid()) {
            m_timeEdit->setDateTime(dt);
        }
    }

    if (m_remarkEdit) {
        m_remarkEdit->setText(t["remark"].toString());
    }

    if (m_companyCombo && m_currentBookType == "COMPANY") {
        int cid = t["company_id"].toInt();
        for (int i = 0; i < m_companyCombo->count(); ++i) {
            if (m_companyCombo->itemData(i).toInt() == cid) {
                m_companyCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    m_voucherPath = t["voucher_path"].toString();
    if (!m_voucherPath.isEmpty() && m_btnVoucher) {
        m_btnVoucher->setText("已选");
        m_btnVoucher->setStyleSheet(
            "QPushButton { background-color: #192b22; color: #00a361; border: 1px solid #00a361; border-radius: 3px; font-size: 13px; font-weight: bold; padding: 0px; }"
        );
    }

    if (m_customParentEdit) m_customParentEdit->clear();
    if (m_customSubEdit) m_customSubEdit->clear();

    loadCategories();

    // 智能预选分类 (支持父子双向反查，防止把预设分类填入自定义输入框)
    QString oldParent = t["category_parent"].toString().trimmed();
    QString oldSub = t["category_sub"].toString().trimmed();

    // 兼容历史命名与错别字规范化
    if (oldParent == "抵押借贷") oldParent = "押抵借贷";
    if (oldParent == "业务") oldParent = "业务经营";
    if (m_currentBookType == "COMPANY" && oldParent == "人脉社交") oldParent = "商务社交";
    if (m_currentBookType == "PERSONAL" && oldParent == "商务社交") oldParent = "人脉社交";
    if (oldSub == "咨询顾问") oldSub = "顾问";
    if (oldSub == "佣金返点") oldSub = "返点";

    // 个人支出借贷/还款历史分类映射规范化 (还网贷、还银行、借个人、还个人)
    if (m_currentBookType == "PERSONAL" && m_currentTransType == "EXPENSE") {
        if (oldParent == "还网贷" || oldParent == "还银行" || oldParent == "借个人" || oldParent == "还个人") {
            oldSub = oldParent;
            oldParent = "押抵借贷";
        }
        if (oldParent == "押抵借贷" && (oldSub == "花呗" || oldSub == "美团" || oldSub == "微信" || oldSub == "抖音")) {
            oldSub = "还网贷";
        }
    }

    // 工作支出借贷/还款历史分类规范化 (还贷、还息)
    if (m_currentBookType == "COMPANY" && m_currentTransType == "EXPENSE") {
        if (oldParent == "还贷" || oldParent == "还息") {
            oldSub = oldParent;
            oldParent = "押抵借贷";
        }
    }

    int targetParentRow = -1;
    // 1. 先用 oldParent 在大分类列表中寻找
    if (!oldParent.isEmpty()) {
        for (int i = 0; i < m_parentListWidget->count(); ++i) {
            if (m_parentListWidget->item(i)->text() == oldParent) {
                targetParentRow = i;
                break;
            }
        }
    }

    // 2. 若 oldParent 未找到或为空，用 oldSub 反查其所属的大分类
    if (targetParentRow < 0 && !oldSub.isEmpty()) {
        for (int i = 0; i < m_hierarchyList.size(); ++i) {
            if (m_hierarchyList[i].subCategories.contains(oldSub)) {
                oldParent = m_hierarchyList[i].parentCategory;
                for (int j = 0; j < m_parentListWidget->count(); ++j) {
                    if (m_parentListWidget->item(j)->text() == oldParent) {
                        targetParentRow = j;
                        break;
                    }
                }
                break;
            }
        }
    }

    // 3. 选中对应大分类（触发刷新右侧子分类列表）
    if (targetParentRow >= 0) {
        m_parentListWidget->setCurrentRow(targetParentRow);
    } else if (!oldParent.isEmpty()) {
        // 确实不在预设树中，才填入自定义大分类输入框
        if (m_customParentEdit) m_customParentEdit->setText(oldParent);
    }

    // 4. 在右侧子分类列表中精确查找并选中 oldSub
    int targetSubRow = -1;
    if (!oldSub.isEmpty()) {
        for (int i = 0; i < m_subListWidget->count(); ++i) {
            if (m_subListWidget->item(i)->text() == oldSub) {
                targetSubRow = i;
                break;
            }
        }
    }

    if (targetSubRow >= 0) {
        m_subListWidget->setCurrentRow(targetSubRow);
    } else if (!oldSub.isEmpty() && targetParentRow < 0) {
        // 确实不在任何预设分类中，才填入自定义小分类输入框
        if (m_customSubEdit) m_customSubEdit->setText(oldSub);
    }

    int contactId = t["contact_id"].toInt();
    if (contactId > 0) {
        setPreselectedContact(contactId);
    }
}

void UnifiedTransactionDialog::onSelectVoucherClicked() {
    QString path = QFileDialog::getOpenFileName(this, "选择发票凭证/票据图片", "", "图片文件 (*.png *.jpg *.jpeg *.bmp *.webp *.pdf)");
    if (!path.isEmpty()) {
        QString savedRel = DatabaseManager::instance().saveUploadedFile(path);
        m_voucherPath = savedRel.isEmpty() ? path : savedRel;
        m_btnVoucher->setText("已选");
        m_btnVoucher->setStyleSheet(
            "QPushButton { background-color: #192b22; color: #00a361; border: 1px solid #00a361; border-radius: 3px; font-size: 13px; font-weight: bold; padding: 0px; }"
        );
        m_btnVoucher->setToolTip(QString("已存凭证: %1").arg(m_voucherPath));
    }
}

void UnifiedTransactionDialog::onSaveTransaction() {
    double amount = m_amountSpin->value();
    if (amount <= 0.0) {
        markFieldInvalid(m_amountSpin);
        return;
    }

    QString finalParent = m_customParentEdit->text().trimmed();
    QString finalSub = m_customSubEdit->text().trimmed();

    if (finalParent.isEmpty()) {
        if (m_parentListWidget->currentItem()) {
            finalParent = m_parentListWidget->currentItem()->text();
        } else {
            finalParent = (m_currentBookType == "PERSONAL") ? "生活开支" : "运营成本";
        }
    }

    if (finalSub.isEmpty()) {
        if (m_subListWidget->currentItem()) {
            finalSub = m_subListWidget->currentItem()->text();
        } else {
            finalSub = finalParent;
        }
    }

    if (!m_customParentEdit->text().trimmed().isEmpty() || !m_customSubEdit->text().trimmed().isEmpty()) {
        DatabaseManager::instance().addCustomCategory(m_currentTransType, finalParent, finalSub, m_currentBookType);
    }

    int companyId = 0;
    if (m_currentBookType == "COMPANY") {
        if (m_companyCombo->count() > 0) {
            companyId = m_companyCombo->currentData().toInt();
        }
    }

    int contactId = 0;
    if (m_relationCombo->currentIndex() > 0) {
        QVariant data = m_relationCombo->currentData();
        if (data.isValid()) {
            QVariantMap map = data.toMap();
            if (map["type"].toString() == "CONTACT") {
                contactId = map["id"].toInt();
            } else if (map["type"].toString() == "COMPANY" && companyId == 0) {
                companyId = map["id"].toInt();
            }
        }
    }

    QString remark = m_remarkEdit->text().trimmed();
    QDateTime transTime = m_timeEdit->dateTime();

    bool ok = false;
    if (m_editTransId > 0) {
        ok = DatabaseManager::instance().updateTransaction(
            m_editTransId, m_currentBookType, 0, m_currentTransType,
            amount, finalSub, transTime, contactId, remark,
            finalParent, companyId, m_voucherPath
        );
    } else {
        ok = DatabaseManager::instance().addTransaction(
            m_currentBookType, 0, m_currentTransType,
            amount, finalSub, transTime, contactId, remark,
            finalParent, companyId, m_voucherPath
        );
    }

    if (ok) {
        accept();
    } else {
        CustomDialog::showError(this, "错误", "账目保存失败，请检查数据库状态。");
    }
}
