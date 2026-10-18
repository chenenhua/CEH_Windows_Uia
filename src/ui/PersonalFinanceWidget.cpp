#include "PersonalFinanceWidget.h"
#include "../core/DatabaseManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFrame>
#include <QDialog>
#include "CustomDialog.h"
#include <QRadioButton>
#include <QButtonGroup>
#include <QStackedWidget>
#include <QDoubleSpinBox>
#include <QDateEdit>
#include <QMessageBox>
#include <QCheckBox>

PersonalFinanceWidget::PersonalFinanceWidget(QWidget* parent) : QWidget(parent) {
    setupUi();
    refreshData();
}

void PersonalFinanceWidget::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 16, 20, 16);
    mainLayout->setSpacing(14);

    // 1. 顶部标题与记账按钮
    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(12);

    auto* titleLbl = new QLabel("个人私人独立财务体系", this);
    titleLbl->setStyleSheet("font-size: 16px; font-weight: bold; color: #ffffff;");
    topRow->addWidget(titleLbl);

    auto* subTitleLbl = new QLabel("(支持工资/股票投资/借款借出与借入/日常消费自适应表单)", this);
    subTitleLbl->setStyleSheet("font-size: 12px; color: #777777;");
    topRow->addWidget(subTitleLbl);

    topRow->addStretch();

    auto* btnAdd = new QPushButton("+ 记一笔个人账目 (自适应表单)", this);
    btnAdd->setStyleSheet(
        "QPushButton { background: #28c840; color: #ffffff; font-weight: bold; border-radius: 4px; padding: 7px 18px; font-size: 13px; }"
        "QPushButton:hover { background: #34db4e; }"
    );
    topRow->addWidget(btnAdd);
    mainLayout->addLayout(topRow);

    // 2. 指标卡片 (4张)
    auto* cardsLayout = new QHBoxLayout();
    cardsLayout->setSpacing(14);

    auto createStatCard = [](const QString& title, QLabel*& valLabel, const QString& accentColor) {
        auto* card = new QFrame();
        card->setStyleSheet("QFrame { background-color: #1a1a1a; border-radius: 8px; border: 1px solid #282828; padding: 12px; }");
        auto* layout = new QVBoxLayout(card);
        layout->setContentsMargins(8, 6, 8, 6);
        layout->setSpacing(4);

        auto* tLbl = new QLabel(title, card);
        tLbl->setStyleSheet("color: #888888; font-size: 12px; border: none;");

        valLabel = new QLabel("￥0.00", card);
        valLabel->setStyleSheet(QString("color: %1; font-size: 20px; font-weight: bold; border: none;").arg(accentColor));

        layout->addWidget(tLbl);
        layout->addWidget(valLabel);
        return card;
    };

    cardsLayout->addWidget(createStatCard("个人现金与活期总结余", m_lblPersonalBalance, "#00bcd4"));
    cardsLayout->addWidget(createStatCard("近30天个人综合收入", m_lblMonthlyIncome, "#28c840"));
    cardsLayout->addWidget(createStatCard("近30天个人综合生活支出", m_lblMonthlyExpense, "#ff5f57"));
    cardsLayout->addWidget(createStatCard("备用金安全覆盖月数", m_lblSafetyMonths, "#febc2f"));
    mainLayout->addLayout(cardsLayout);

    // 3. 筛选与搜索工具条
    auto* filterBar = new QHBoxLayout();
    filterBar->setSpacing(10);

    auto* lblFilter = new QLabel("流水筛选：", this);
    lblFilter->setStyleSheet("color: #888888; font-size: 12px;");
    filterBar->addWidget(lblFilter);

    m_filterCombo = new QComboBox(this);
    m_filterCombo->setStyleSheet("QComboBox { background: #1e1e1e; color: #ffffff; border: 1px solid #333333; border-radius: 4px; padding: 5px 12px; font-size: 12px; }");
    m_filterCombo->addItem("全部个人流水", "ALL");
    m_filterCombo->addItem("仅收入 (工资/理财/租金/借入)", "INCOME");
    m_filterCombo->addItem("仅支出 (日常消费/还贷/大额购置/借出)", "EXPENSE");
    m_filterCombo->addItem("仅借贷往来 (借入与借出)", "DEBT");
    filterBar->addWidget(m_filterCombo);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("🔍 搜索业务分类 / 说明备注 / 关联联系人...");
    m_searchEdit->setStyleSheet("QLineEdit { background: #1e1e1e; color: #ffffff; border: 1px solid #333333; border-radius: 4px; padding: 5px 12px; font-size: 12px; min-width: 260px; }");
    filterBar->addWidget(m_searchEdit);

    filterBar->addStretch();
    mainLayout->addLayout(filterBar);

    // 4. 流水表格
    m_table = new QTableWidget(this);
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({"发生时间", "类型", "细分业务类型", "分类说明", "发生金额 (元)", "关联对象 / 联系人", "备注"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->setVisible(false);
    m_table->setStyleSheet(
        "QTableWidget { background-color: #171717; border: 1px solid #282828; border-radius: 6px; gridline-color: #222222; color: #dddddd; font-size: 13px; }"
        "QHeaderView::section { background-color: #1f1f1f; color: #888888; border: none; padding: 8px; font-weight: bold; }"
        "QTableWidget::item { padding: 8px; border-bottom: 1px solid #202020; }"
        "QTableWidget::item:selected { background-color: #262626; color: #ffffff; }"
    );
    mainLayout->addWidget(m_table);

    connect(btnAdd, &QPushButton::clicked, this, &PersonalFinanceWidget::onAddTransactionClicked);
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PersonalFinanceWidget::onFilterChanged);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &PersonalFinanceWidget::onFilterChanged);
}

void PersonalFinanceWidget::refreshData() {
    loadPersonalStats();
    loadTransactions();
}

void PersonalFinanceWidget::loadPersonalStats() {
    // 1. 个人现金/活期账户总额 (book_type = 'PERSONAL')
    QSqlQuery q;
    q.exec("SELECT SUM(balance) FROM accounts WHERE book_type = 'PERSONAL';");
    double bal = 0.0;
    if (q.next()) bal = q.value(0).toDouble();
    m_lblPersonalBalance->setText(QString("￥%1").arg(bal, 0, 'f', 2));

    // 2. 近30天个人综合收入
    q.exec("SELECT SUM(amount) FROM transactions WHERE book_type = 'PERSONAL' AND trans_type = 'INCOME' AND trans_time >= datetime('now', '-30 day');");
    double income = 0.0;
    if (q.next()) income = q.value(0).toDouble();
    m_lblMonthlyIncome->setText(QString("￥%1").arg(income, 0, 'f', 2));

    // 3. 近30天个人综合支出
    q.exec("SELECT SUM(amount) FROM transactions WHERE book_type = 'PERSONAL' AND trans_type = 'EXPENSE' AND trans_time >= datetime('now', '-30 day');");
    double expense = 0.0;
    if (q.next()) expense = q.value(0).toDouble();
    m_lblMonthlyExpense->setText(QString("￥%1").arg(expense, 0, 'f', 2));

    // 4. 备用金安全月数
    if (expense > 0.01) {
        double months = bal / expense;
        m_lblSafetyMonths->setText(QString("%1 个月").arg(months, 0, 'f', 1));
    } else {
        m_lblSafetyMonths->setText(bal > 0 ? "> 36 个月" : "0 个月");
    }
}

void PersonalFinanceWidget::onFilterChanged() {
    loadTransactions();
}

void PersonalFinanceWidget::loadTransactions() {
    QString filterMode = m_filterCombo->currentData().toString();
    QString kw = m_searchEdit->text().trimmed();

    QString sql = "SELECT t.trans_time, t.trans_type, t.sub_type, t.category, t.amount, c.name, t.remark "
                  "FROM transactions t LEFT JOIN contacts c ON t.contact_id = c.id "
                  "WHERE t.book_type = 'PERSONAL' ";

    if (filterMode == "INCOME") {
        sql += "AND t.trans_type = 'INCOME' ";
    } else if (filterMode == "EXPENSE") {
        sql += "AND t.trans_type = 'EXPENSE' ";
    } else if (filterMode == "DEBT") {
        sql += "AND (t.sub_type LIKE '%借%' OR t.category LIKE '%借%' OR t.category LIKE '%贷%') ";
    }

    if (!kw.isEmpty()) {
        sql += QString("AND (t.category LIKE '%%1%' OR t.sub_type LIKE '%%1%' OR t.remark LIKE '%%1%' OR c.name LIKE '%%1%') ")
                   .arg(kw);
    }

    sql += "ORDER BY t.trans_time DESC LIMIT 150;";

    QSqlQuery q(sql);
    QList<QVariantList> rows;
    while (q.next()) {
        rows.append({q.value(0), q.value(1), q.value(2), q.value(3), q.value(4), q.value(5), q.value(6)});
    }

    m_table->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const auto& r = rows[i];
        m_table->setItem(i, 0, new QTableWidgetItem(r[0].toString()));

        QString tType = r[1].toString();
        auto* typeItem = new QTableWidgetItem(tType == "INCOME" ? "🟢 收入" : "🔴 支出");
        typeItem->setForeground(tType == "INCOME" ? QColor("#28c840") : QColor("#ff5f57"));
        m_table->setItem(i, 1, typeItem);

        m_table->setItem(i, 2, new QTableWidgetItem(r[2].toString().isEmpty() ? "常规收支" : r[2].toString()));
        m_table->setItem(i, 3, new QTableWidgetItem(r[3].toString()));

        double amt = r[4].toDouble();
        auto* amtItem = new QTableWidgetItem(QString("￥%1").arg(amt, 0, 'f', 2));
        amtItem->setForeground(tType == "INCOME" ? QColor("#28c840") : QColor("#ff5f57"));
        amtItem->setFont(QFont("Microsoft YaHei UI", 10, QFont::Bold));
        m_table->setItem(i, 4, amtItem);

        m_table->setItem(i, 5, new QTableWidgetItem(r[5].toString().isEmpty() ? "—" : r[5].toString()));
        m_table->setItem(i, 6, new QTableWidgetItem(r[6].toString()));
    }
}

void PersonalFinanceWidget::onAddTransactionClicked() {
    CustomDialog dlg("记一笔个人账目 (自适应动态表单)", this);
    dlg.resize(520, 640);

    auto* dlgLayout = dlg.contentLayout();
    dlgLayout->setSpacing(12);

    // 1. 顶部大单选：收入 vs 支出
    auto* typeRow = new QHBoxLayout();
    auto* rbExpense = new QRadioButton("🔴 个人支出 (日常/借出/还贷/大额购置/送礼)", &dlg);
    auto* rbIncome = new QRadioButton("🟢 个人收入 (工资/借入借款/股票理财/租金/礼金)", &dlg);
    rbExpense->setChecked(true);
    rbExpense->setStyleSheet("QRadioButton { font-size: 13px; font-weight: bold; color: #ff5f57; }");
    rbIncome->setStyleSheet("QRadioButton { font-size: 13px; font-weight: bold; color: #28c840; }");

    auto* typeGroup = new QButtonGroup(&dlg);
    typeGroup->addButton(rbExpense, 0);
    typeGroup->addButton(rbIncome, 1);
    typeRow->addWidget(rbExpense);
    typeRow->addWidget(rbIncome);
    dlgLayout->addLayout(typeRow);

    // 2. 细分类别下拉框
    dlgLayout->addWidget(new QLabel("细分业务类型 (选择后下方表单将自动联动切换)：", &dlg));
    auto* subTypeCombo = new QComboBox(&dlg);
    subTypeCombo->setStyleSheet("QComboBox { background: #222222; color: #ffffff; border: 1px solid #3a3a3a; border-radius: 4px; padding: 6px 12px; font-size: 13px; font-weight: bold; }");
    dlgLayout->addWidget(subTypeCombo);

    // 3. 动态表单 Stack (针对不同场景切换输入表单)
    auto* formStack = new QStackedWidget(&dlg);
    formStack->setStyleSheet("QStackedWidget { background-color: #1d1d1d; border: 1px solid #2d2d2d; border-radius: 6px; padding: 10px; }");

    // ===== Page 0: 日常生活消费 =====
    auto* p0 = new QWidget();
    auto* p0Layout = new QVBoxLayout(p0);
    p0Layout->setContentsMargins(8, 8, 8, 8);
    p0Layout->setSpacing(8);
    auto* p0SceneCombo = new QComboBox(p0);
    p0SceneCombo->setEditable(true);
    p0SceneCombo->addItems({"餐饮美食 / 日常餐饮", "数码科技 / 电子产品", "服装鞋帽 / 个人形象", "交通出行 / 加油打车", "生活商超 / 居家百货", "医疗健康 / 保健药品", "文化娱乐 / 健身旅游"});
    auto* p0MerchantEdit = new QLineEdit(p0);
    p0MerchantEdit->setPlaceholderText("消费商户或商家名称 (选填)");
    p0Layout->addWidget(new QLabel("消费场景与类别：", p0));
    p0Layout->addWidget(p0SceneCombo);
    p0Layout->addWidget(new QLabel("商家/消费渠道：", p0));
    p0Layout->addWidget(p0MerchantEdit);
    formStack->addWidget(p0); // Index 0

    // ===== Page 1: 借出借款 / 借入借款 =====
    auto* p1 = new QWidget();
    auto* p1Layout = new QVBoxLayout(p1);
    p1Layout->setContentsMargins(8, 8, 8, 8);
    p1Layout->setSpacing(8);
    auto* p1ContactBox = new QComboBox(p1);
    p1ContactBox->addItem("选择联系人 (好友/熟人)", 0);
    auto allContacts = DatabaseManager::instance().getContacts();
    for (const auto& c : allContacts) {
        p1ContactBox->addItem(QString("%1 (%2)").arg(c.name, c.company.isEmpty() ? c.relationType : c.company), c.id);
    }
    auto* p1DueDateEdit = new QDateEdit(QDate::currentDate().addMonths(3), p1);
    p1DueDateEdit->setCalendarPopup(true);
    p1DueDateEdit->setDisplayFormat("yyyy-MM-dd");
    auto* p1RateSpin = new QDoubleSpinBox(p1);
    p1RateSpin->setRange(0, 100);
    p1RateSpin->setValue(0);
    p1RateSpin->setSuffix(" %");
    p1Layout->addWidget(new QLabel("借款关联人 (选择好友/联系人)：", p1));
    p1Layout->addWidget(p1ContactBox);
    auto* p1DateRow = new QHBoxLayout();
    p1DateRow->addWidget(new QLabel("约定归还日期：", p1));
    p1DateRow->addWidget(p1DueDateEdit);
    p1DateRow->addWidget(new QLabel("约定年利率：", p1));
    p1DateRow->addWidget(p1RateSpin);
    p1Layout->addLayout(p1DateRow);
    formStack->addWidget(p1); // Index 1

    // ===== Page 2: 人情送礼 / 请客 =====
    auto* p2 = new QWidget();
    auto* p2Layout = new QVBoxLayout(p2);
    p2Layout->setContentsMargins(8, 8, 8, 8);
    p2Layout->setSpacing(8);
    auto* p2ContactBox = new QComboBox(p2);
    for (const auto& c : allContacts) {
        p2ContactBox->addItem(QString("%1 (%2)").arg(c.name, c.company), c.id);
    }
    auto* p2GiftSceneEdit = new QLineEdit(p2);
    p2GiftSceneEdit->setPlaceholderText("例如: 中秋送茅台礼盒 / 宴请请客 / 满月酒红包");
    auto* p2NeedReturnCheck = new QCheckBox("标记为待后续回礼/待还人情闭环", p2);
    p2Layout->addWidget(new QLabel("受礼/宴请联系人：", p2));
    p2Layout->addWidget(p2ContactBox);
    p2Layout->addWidget(new QLabel("礼品/宴请场景内容：", p2));
    p2Layout->addWidget(p2GiftSceneEdit);
    p2Layout->addWidget(p2NeedReturnCheck);
    formStack->addWidget(p2); // Index 2

    // ===== Page 3: 还贷 / 还借款 =====
    auto* p3 = new QWidget();
    auto* p3Layout = new QVBoxLayout(p3);
    p3Layout->setContentsMargins(8, 8, 8, 8);
    p3Layout->setSpacing(8);
    auto* p3DebtBox = new QComboBox(p3);
    p3DebtBox->addItem("银行房贷按揭", "银行房贷");
    p3DebtBox->addItem("银行车辆贷款", "车辆按揭");
    p3DebtBox->addItem("网银微粒贷/借呗/授信", "网银消费信贷");
    p3DebtBox->addItem("偿还朋友/熟人私人借款", "朋友熟人借款");
    auto debts = DatabaseManager::instance().getDebts("BORROW_IN");
    for (const auto& d : debts) {
        p3DebtBox->addItem(QString("已立债务: %1 (欠￥%2)").arg(d.institutionName.isEmpty() ? d.contactName : d.institutionName).arg(d.principal, 0, 'f', 0), d.id);
    }
    auto* p3RemarkEdit = new QLineEdit(p3);
    p3RemarkEdit->setPlaceholderText("本期还款说明 (例如: 第24期本息)");
    p3Layout->addWidget(new QLabel("偿还借贷项目：", p3));
    p3Layout->addWidget(p3DebtBox);
    p3Layout->addWidget(new QLabel("还款说明：", p3));
    p3Layout->addWidget(p3RemarkEdit);
    formStack->addWidget(p3); // Index 3

    // ===== Page 4: 工资 / 薪酬奖金 =====
    auto* p4 = new QWidget();
    auto* p4Layout = new QVBoxLayout(p4);
    p4Layout->setContentsMargins(8, 8, 8, 8);
    p4Layout->setSpacing(8);
    auto* p4EmployerEdit = new QLineEdit(p4);
    p4EmployerEdit->setPlaceholderText("发放单位/企业名称 (例如: 某某实业有限公司)");
    auto* p4DeductionEdit = new QLineEdit(p4);
    p4DeductionEdit->setPlaceholderText("五险一金/个税扣除说明 (选填)");
    p4Layout->addWidget(new QLabel("发放薪资单位：", p4));
    p4Layout->addWidget(p4EmployerEdit);
    p4Layout->addWidget(new QLabel("税费扣除及备注：", p4));
    p4Layout->addWidget(p4DeductionEdit);
    formStack->addWidget(p4); // Index 4

    // ===== Page 5: 股票 / 理财投资收益 =====
    auto* p5 = new QWidget();
    auto* p5Layout = new QVBoxLayout(p5);
    p5Layout->setContentsMargins(8, 8, 8, 8);
    p5Layout->setSpacing(8);
    auto* p5StockEdit = new QLineEdit(p5);
    p5StockEdit->setPlaceholderText("标的名称或代码 (例如: 招商银行 600036 / 纳指ETF)");
    auto* p5PlatformEdit = new QLineEdit(p5);
    p5PlatformEdit->setPlaceholderText("开户券商或理财平台 (例如: 华泰证券 / 招商银行APP)");
    p5Layout->addWidget(new QLabel("投资标的名称/代码：", p5));
    p5Layout->addWidget(p5StockEdit);
    p5Layout->addWidget(new QLabel("交易账户/平台：", p5));
    p5Layout->addWidget(p5PlatformEdit);
    formStack->addWidget(p5); // Index 5

    // ===== Page 6: 房租 / 资产被动收益 / 大额固定资产购置 =====
    auto* p6 = new QWidget();
    auto* p6Layout = new QVBoxLayout(p6);
    p6Layout->setContentsMargins(8, 8, 8, 8);
    p6Layout->setSpacing(8);
    auto* p6AssetNameEdit = new QLineEdit(p6);
    p6AssetNameEdit->setPlaceholderText("资产名称 (例如: 某某花园商品房 / 保时捷卡宴)");
    auto* p6CycleEdit = new QLineEdit(p6);
    p6CycleEdit->setPlaceholderText("周期/合同说明 (例如: 半年租金押一付三 / 一次性全款购置)");
    p6Layout->addWidget(new QLabel("关联资产标的：", p6));
    p6Layout->addWidget(p6AssetNameEdit);
    p6Layout->addWidget(new QLabel("合同与交付说明：", p6));
    p6Layout->addWidget(p6CycleEdit);
    formStack->addWidget(p6); // Index 6

    dlgLayout->addWidget(formStack);

    // 联动刷新下拉框选项与对应表单页面
    auto updateSubTypes = [&]() {
        subTypeCombo->clear();
        if (rbExpense->isChecked()) {
            subTypeCombo->addItem("🛒 日常生活购买/消费 (餐饮/数码/服饰/交通)", 0);
            subTypeCombo->addItem("🤝 借出借款 (借给朋友/熟人周转)", 1);
            subTypeCombo->addItem("💳 偿还借款/银行房贷车贷还贷", 3);
            subTypeCombo->addItem("🎁 人情往来/请客送礼开销", 2);
            subTypeCombo->addItem("🏡 大额固定资产购置 (房产/车辆/贵重物)", 6);
            subTypeCombo->addItem("📈 追加金融投资本金", 5);
            subTypeCombo->addItem("🏷️ 其他自定义个人支出", 0);
        } else {
            subTypeCombo->addItem("💼 工资/薪酬与年终奖金", 4);
            subTypeCombo->addItem("💰 借入借款 (向他人借款/私人周转借入)", 1);
            subTypeCombo->addItem("📈 股票/理财/基金分红与投资收益", 5);
            subTypeCombo->addItem("🏢 房租/物业出租资产被动收益", 6);
            subTypeCombo->addItem("🎁 收到人情礼金/红包礼遇", 2);
            subTypeCombo->addItem("🤝 兼职副业/项目提成所得", 4);
            subTypeCombo->addItem("🏷️ 其他自定义个人收入", 0);
        }
    };

    connect(rbExpense, &QRadioButton::toggled, updateSubTypes);
    connect(subTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [&](int /*idx*/) {
        int pageIdx = subTypeCombo->currentData().toInt();
        formStack->setCurrentIndex(pageIdx);
    });

    updateSubTypes();

    // 4. 通用基础字段：发生金额、日期、账户、通用备注
    auto* amountSpin = new QDoubleSpinBox(&dlg);
    amountSpin->setRange(0.01, 100000000);
    amountSpin->setValue(200);
    amountSpin->setPrefix("￥ ");
    amountSpin->setStyleSheet("QDoubleSpinBox { background: #202020; color: #28c840; font-size: 16px; font-weight: bold; padding: 6px; }");

    auto* dateEdit = new QDateEdit(QDate::currentDate(), &dlg);
    dateEdit->setCalendarPopup(true);
    dateEdit->setDisplayFormat("yyyy-MM-dd");

    auto* accCombo = new QComboBox(&dlg);
    accCombo->setStyleSheet("QComboBox { background: #202020; color: #ffffff; border: 1px solid #333333; border-radius: 4px; padding: 6px; }");
    auto accs = DatabaseManager::instance().getAccounts("PERSONAL");
    for (const auto& a : accs) {
        accCombo->addItem(QString("%1 (余额￥%2)").arg(a["name"].toString()).arg(a["balance"].toDouble(), 0, 'f', 2), a["id"].toInt());
    }
    if (accCombo->count() == 0) {
        accCombo->addItem("个人默认日常账户", 1);
    }

    auto* remarkEdit = new QLineEdit(&dlg);
    remarkEdit->setPlaceholderText("补充说明 (发票情况、凭证号等)");

    auto addField = [&](const QString& label, QWidget* w) {
        dlgLayout->addWidget(new QLabel(label, &dlg));
        dlgLayout->addWidget(w);
    };

    auto* bottomRow = new QHBoxLayout();
    auto* amtCol = new QVBoxLayout();
    amtCol->addWidget(new QLabel("发生金额 (元) *：", &dlg));
    amtCol->addWidget(amountSpin);
    bottomRow->addLayout(amtCol);

    auto* dateCol = new QVBoxLayout();
    dateCol->addWidget(new QLabel("发生日期：", &dlg));
    dateCol->addWidget(dateEdit);
    bottomRow->addLayout(dateCol);
    dlgLayout->addLayout(bottomRow);

    addField("结算资金账户：", accCombo);
    addField("备注补充说明：", remarkEdit);

    // 5. 底部按钮
    auto* btnRow = new QHBoxLayout();
    auto* cancelBtn = new QPushButton("取消", &dlg);
    auto* saveBtn = new QPushButton("保存个人记账", &dlg);
    saveBtn->setStyleSheet("background-color: #28c840; color: #ffffff; font-weight: bold; padding: 8px 24px; border-radius: 4px; font-size: 13px;");
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(saveBtn);
    dlgLayout->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(saveBtn, &QPushButton::clicked, [&]() {
        bool isIncome = rbIncome->isChecked();
        QString transType = isIncome ? "INCOME" : "EXPENSE";
        QString subTypeText = subTypeCombo->currentText();
        int pageIdx = subTypeCombo->currentData().toInt();
        double amount = amountSpin->value();
        QDate dt = dateEdit->date();
        int accId = accCombo->currentData().toInt();
        if (accId <= 0) accId = 1;

        int contactId = 0;
        QString category = "";
        QString fullRemark = remarkEdit->text().trimmed();

        if (pageIdx == 0) { // 日常消费 / 其他
            category = p0SceneCombo->currentText();
            if (!p0MerchantEdit->text().trimmed().isEmpty()) {
                fullRemark = QString("[%1] %2").arg(p0MerchantEdit->text().trimmed(), fullRemark);
            }
        } else if (pageIdx == 1) { // 借入 / 借出
            contactId = p1ContactBox->currentData().toInt();
            QString contactName = p1ContactBox->currentText();
            category = isIncome ? "私人借款-借入" : "私人借款-借出";
            fullRemark = QString("约定归还: %1 | 利率: %2% | %3")
                             .arg(p1DueDateEdit->date().toString("yyyy-MM-dd"))
                             .arg(p1RateSpin->value())
                             .arg(fullRemark);

            // 自动同步插入借贷账本
            DatabaseManager::instance().addDebt(
                "PERSON",
                isIncome ? "BORROW_IN" : "LEND_OUT",
                contactId,
                contactName,
                amount,
                p1RateSpin->value(),
                "到期一次还本付息",
                dt,
                p1DueDateEdit->date(),
                0, 0.0,
                fullRemark
            );
        } else if (pageIdx == 2) { // 人情送礼 / 请客
            contactId = p2ContactBox->currentData().toInt();
            category = isIncome ? "收受人情礼遇" : "人情送礼请客";
            QString giftContent = p2GiftSceneEdit->text().trimmed();
            fullRemark = QString("人情内容: %1 | %2").arg(giftContent, fullRemark);

            // 自动同步插入人情事件簿
            DatabaseManager::instance().addSocialEvent(
                contactId,
                isIncome ? "GIFT_IN" : "GIFT_OUT",
                giftContent.isEmpty() ? category : giftContent,
                fullRemark,
                amount,
                dt,
                p2NeedReturnCheck->isChecked() ? "PENDING_RETURN" : "CLOSED"
            );
        } else if (pageIdx == 3) { // 还贷 / 还借款
            category = "还贷还本付息";
            fullRemark = QString("债务: %1 | %2").arg(p3DebtBox->currentText(), fullRemark);
        } else if (pageIdx == 4) { // 工资薪酬
            category = "工资薪酬奖金";
            fullRemark = QString("发放单位: %1 | %2 | %3")
                             .arg(p4EmployerEdit->text().trimmed())
                             .arg(p4DeductionEdit->text().trimmed())
                             .arg(fullRemark);
        } else if (pageIdx == 5) { // 股票理财
            category = isIncome ? "股票理财投资收益" : "追加金融投资";
            fullRemark = QString("标的: %1 | 平台: %2 | %3")
                             .arg(p5StockEdit->text().trimmed())
                             .arg(p5PlatformEdit->text().trimmed())
                             .arg(fullRemark);
        } else if (pageIdx == 6) { // 房租 / 资产
            category = isIncome ? "房租资产被动收益" : "大额固定资产购置";
            fullRemark = QString("资产: %1 | 周期: %2 | %3")
                             .arg(p6AssetNameEdit->text().trimmed())
                             .arg(p6CycleEdit->text().trimmed())
                             .arg(fullRemark);
        }

        bool ok = DatabaseManager::instance().addTransaction(
            "PERSONAL",
            accId,
            transType,
            amount,
            category,
            QDateTime(dt, QTime::currentTime()),
            contactId,
            fullRemark,
            subTypeText,
            0
        );

        if (ok) {
            dlg.accept();
        } else {
            QMessageBox::critical(&dlg, "错误", "记账失败！请检查数据库。");
        }
    });

    if (dlg.exec() == QDialog::Accepted) {
        refreshData();
    }
}
