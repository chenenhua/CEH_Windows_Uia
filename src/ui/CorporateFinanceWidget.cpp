#include "CorporateFinanceWidget.h"
#include "../core/DatabaseManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QTabWidget>
#include <QDialog>
#include "CustomDialog.h"
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QDateEdit>
#include <QMessageBox>

CorporateFinanceWidget::CorporateFinanceWidget(QWidget* parent) : QWidget(parent) {
    setupUi();
    loadCompanies();
}

void CorporateFinanceWidget::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 16, 20, 16);
    mainLayout->setSpacing(14);

    // 1. 顶部公司主体切换栏
    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(12);

    auto* titleLbl = new QLabel("当前工作/企业主体：", this);
    titleLbl->setStyleSheet("font-size: 14px; font-weight: bold; color: #ffffff;");
    topRow->addWidget(titleLbl);

    m_companyCombo = new QComboBox(this);
    m_companyCombo->setStyleSheet(
        "QComboBox { background: #202020; color: #ffffff; border: 1px solid #333333; border-radius: 4px; padding: 6px 14px; min-width: 260px; font-weight: bold; }"
    );
    topRow->addWidget(m_companyCombo);

    m_lblRoleBadge = new QLabel("【身份: 法人/控股】", this);
    m_lblRoleBadge->setStyleSheet("color: #00bcd4; font-size: 12px; font-weight: bold;");
    topRow->addWidget(m_lblRoleBadge);

    m_btnAddCompany = new QPushButton("+ 添加新公司/工作主体", this);
    m_btnAddCompany->setStyleSheet("background: #202020; color: #28c840; border: 1px solid #28c840; border-radius: 4px; padding: 6px 14px; font-size: 12px;");
    topRow->addWidget(m_btnAddCompany);

    topRow->addStretch();
    mainLayout->addLayout(topRow);

    // 2. 指标卡片
    auto* cardsLayout = new QHBoxLayout();
    cardsLayout->setSpacing(14);

    auto createStatCard = [](const QString& title, QLabel*& valLabel, const QString& accentColor) {
        auto* card = new QFrame();
        card->setStyleSheet("QFrame { background-color: #1a1a1a; border-radius: 8px; border: 1px solid #282828; padding: 12px; }");
        auto* layout = new QVBoxLayout(card);
        layout->setContentsMargins(8, 6, 8, 6);
        layout->setSpacing(4);

        auto* titleLbl = new QLabel(title, card);
        titleLbl->setStyleSheet("color: #888888; font-size: 12px; border: none;");

        valLabel = new QLabel("￥0.00", card);
        valLabel->setStyleSheet(QString("color: %1; font-size: 20px; font-weight: bold; border: none;").arg(accentColor));

        layout->addWidget(titleLbl);
        layout->addWidget(valLabel);
        return card;
    };

    cardsLayout->addWidget(createStatCard("该主体对公结余", m_lblBalance, "#00bcd4"));
    cardsLayout->addWidget(createStatCard("该主体近30天回款收入", m_lblMonthlyIncome, "#28c840"));
    cardsLayout->addWidget(createStatCard("该主体近30天运营支出", m_lblMonthlyExpense, "#ff5f57"));
    cardsLayout->addWidget(createStatCard("关联业务伙伴/客户数", m_lblLinkedContactsCount, "#febc2f"));
    mainLayout->addLayout(cardsLayout);

    // 3. Tab: 1. 企业流水账本 | 2. 关联联系人的公司关系
    auto* tabWidget = new QTabWidget(this);
    tabWidget->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #282828; background: #171717; border-radius: 6px; }"
        "QTabBar::tab { background: #1c1c1c; color: #888888; padding: 8px 18px; font-size: 12px; font-weight: bold; }"
        "QTabBar::tab:selected { background: #262626; color: #ffffff; border-bottom: 2px solid #00bcd4; }"
    );

    // Page 1: 流水
    auto* p1 = new QWidget();
    auto* p1Layout = new QVBoxLayout(p1);
    p1Layout->setContentsMargins(10, 10, 10, 10);
    p1Layout->setSpacing(10);

    auto* p1Bar = new QHBoxLayout();
    p1Bar->addWidget(new QLabel("独立对公流水账本 (合同回款/采购/报销/工资)", p1));
    p1Bar->addStretch();
    auto* btnAddTrans = new QPushButton("+ 记录公司财务流水", p1);
    btnAddTrans->setStyleSheet("background: #00bcd4; color: #ffffff; font-weight: bold; padding: 6px 14px; border-radius: 4px;");
    p1Bar->addWidget(btnAddTrans);
    p1Layout->addLayout(p1Bar);

    m_transTable = new QTableWidget(p1);
    m_transTable->setColumnCount(6);
    m_transTable->setHorizontalHeaderLabels({"发生时间", "类型", "业务分类", "金额 (元)", "关联业务联系人", "备注说明"});
    m_transTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_transTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_transTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_transTable->verticalHeader()->setVisible(false);
    p1Layout->addWidget(m_transTable);
    tabWidget->addTab(p1, "📑 对公财务流水");

    // Page 2: 关联联系人的公司关系
    auto* p2 = new QWidget();
    auto* p2Layout = new QVBoxLayout(p2);
    p2Layout->setContentsMargins(10, 10, 10, 10);
    p2Layout->setSpacing(10);

    p2Layout->addWidget(new QLabel("与本公司产生交集的关键人脉网络 (客户 / 供应商 / 合伙股东 / 核心顾问)", p2));
    m_contactsTable = new QTableWidget(p2);
    m_contactsTable->setColumnCount(5);
    m_contactsTable->setHorizontalHeaderLabels({"姓名", "职务/单位", "业务关系分类", "合作生意次数", "历史商务累计投入"});
    m_contactsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_contactsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_contactsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_contactsTable->verticalHeader()->setVisible(false);
    p2Layout->addWidget(m_contactsTable);
    tabWidget->addTab(p2, "🤝 关联联系人的公司关系");

    mainLayout->addWidget(tabWidget);

    connect(m_companyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CorporateFinanceWidget::onCompanyChanged);
    connect(m_btnAddCompany, &QPushButton::clicked, this, &CorporateFinanceWidget::onAddCompanyClicked);
    connect(btnAddTrans, &QPushButton::clicked, this, &CorporateFinanceWidget::onAddTransactionClicked);
}

void CorporateFinanceWidget::loadCompanies() {
    m_companies.clear();
    m_companyCombo->clear();

    QSqlQuery q("SELECT id, name, role_type, industry, remark FROM companies ORDER BY id ASC;");
    while (q.next()) {
        CompanyItem item;
        item.id = q.value(0).toInt();
        item.name = q.value(1).toString();
        item.roleType = q.value(2).toString();
        item.industry = q.value(3).toString();
        item.remark = q.value(4).toString();
        m_companies.append(item);
        m_companyCombo->addItem(item.name, item.id);
    }

    if (!m_companies.isEmpty()) {
        onCompanyChanged(0);
    }
}

void CorporateFinanceWidget::refreshData() {
    if (m_currentCompanyId > 0) {
        loadCompanyFinance(m_currentCompanyId);
    }
}

void CorporateFinanceWidget::onCompanyChanged(int index) {
    if (index >= 0 && index < m_companies.size()) {
        const auto& comp = m_companies[index];
        m_currentCompanyId = comp.id;

        QString roleStr = (comp.roleType == "OWNER") ? "【身份: 法人/控股】" :
                          ((comp.roleType == "PARTNER") ? "【身份: 合伙投资】" : "【身份: 上班雇佣】");
        m_lblRoleBadge->setText(roleStr);

        loadCompanyFinance(comp.id);
    }
}

void CorporateFinanceWidget::loadCompanyFinance(int companyId) {
    // 1. 统计余额
    QSqlQuery q;
    q.prepare("SELECT SUM(balance) FROM accounts WHERE company_id = :cid;");
    q.bindValue(":cid", companyId);
    double bal = 0.0;
    if (q.exec() && q.next()) bal = q.value(0).toDouble();
    m_lblBalance->setText(QString("￥%1").arg(bal, 0, 'f', 2));

    // 2. 近30天收支
    q.prepare("SELECT SUM(amount) FROM transactions WHERE company_id = :cid AND trans_type = 'INCOME' AND trans_time >= date('now', '-30 day');");
    q.bindValue(":cid", companyId);
    double income = 0.0;
    if (q.exec() && q.next()) income = q.value(0).toDouble();
    m_lblMonthlyIncome->setText(QString("￥%1").arg(income, 0, 'f', 2));

    q.prepare("SELECT SUM(amount) FROM transactions WHERE company_id = :cid AND trans_type = 'EXPENSE' AND trans_time >= date('now', '-30 day');");
    q.bindValue(":cid", companyId);
    double expense = 0.0;
    if (q.exec() && q.next()) expense = q.value(0).toDouble();
    m_lblMonthlyExpense->setText(QString("￥%1").arg(expense, 0, 'f', 2));

    // 3. 流水表
    q.prepare("SELECT t.trans_time, t.trans_type, t.category, t.amount, c.name, t.remark "
              "FROM transactions t LEFT JOIN contacts c ON t.contact_id = c.id "
              "WHERE t.company_id = :cid ORDER BY t.trans_time DESC LIMIT 100;");
    q.bindValue(":cid", companyId);

    QList<QVariantList> rows;
    if (q.exec()) {
        while (q.next()) {
            rows.append({q.value(0), q.value(1), q.value(2), q.value(3), q.value(4), q.value(5)});
        }
    }

    m_transTable->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const auto& r = rows[i];
        m_transTable->setItem(i, 0, new QTableWidgetItem(r[0].toString()));
        QString typeStr = (r[1].toString() == "INCOME") ? "回款/收入" : ((r[1].toString() == "EXPENSE") ? "对公支出" : "垫资调拨");
        m_transTable->setItem(i, 1, new QTableWidgetItem(typeStr));
        m_transTable->setItem(i, 2, new QTableWidgetItem(r[2].toString()));

        double amt = r[3].toDouble();
        auto* amtItem = new QTableWidgetItem(QString("￥%1").arg(amt, 0, 'f', 2));
        if (r[1].toString() == "INCOME") amtItem->setForeground(QColor("#28c840"));
        else amtItem->setForeground(QColor("#ff5f57"));
        m_transTable->setItem(i, 3, amtItem);

        m_transTable->setItem(i, 4, new QTableWidgetItem(r[4].toString().isEmpty() ? "—" : r[4].toString()));
        m_transTable->setItem(i, 5, new QTableWidgetItem(r[5].toString()));
    }

    // 4. 关联业务联系人
    q.prepare("SELECT c.id, c.name, c.company, c.title, c.relation_type, "
              "(SELECT count(*) FROM social_events WHERE contact_id = c.id AND scope = 'COMPANY') as biz_count, "
              "IFNULL((SELECT SUM(my_cost) FROM social_events WHERE contact_id = c.id AND scope = 'COMPANY'), 0) as cost "
              "FROM contacts c WHERE c.company_id = :cid OR c.id IN (SELECT DISTINCT contact_id FROM transactions WHERE company_id = :cid2);");
    q.bindValue(":cid", companyId);
    q.bindValue(":cid2", companyId);

    QList<QVariantList> contactRows;
    if (q.exec()) {
        while (q.next()) {
            contactRows.append({q.value(1), QString("%1 / %2").arg(q.value(2).toString(), q.value(3).toString()), q.value(4), q.value(5), q.value(6)});
        }
    }

    m_lblLinkedContactsCount->setText(QString("%1 位").arg(contactRows.size()));
    m_contactsTable->setRowCount(contactRows.size());
    for (int i = 0; i < contactRows.size(); ++i) {
        const auto& cr = contactRows[i];
        m_contactsTable->setItem(i, 0, new QTableWidgetItem(cr[0].toString()));
        m_contactsTable->setItem(i, 1, new QTableWidgetItem(cr[1].toString()));
        m_contactsTable->setItem(i, 2, new QTableWidgetItem(cr[2].toString()));
        m_contactsTable->setItem(i, 3, new QTableWidgetItem(QString("%1 次").arg(cr[3].toInt())));
        auto* costItem = new QTableWidgetItem(QString("￥%1").arg(cr[4].toDouble(), 0, 'f', 0));
        costItem->setForeground(QColor("#ff5f57"));
        m_contactsTable->setItem(i, 4, costItem);
    }
}

void CorporateFinanceWidget::onAddCompanyClicked() {
    CustomDialog dlg("添加新的公司或工作雇佣主体", this);
    dlg.resize(460, 380);

    auto* layout = dlg.contentLayout();
    layout->setSpacing(12);

    auto* nameEdit = new QLineEdit(&dlg);
    nameEdit->setPlaceholderText("公司/主体名称 (例如: 某某实业有限公司, 某某外企中国分部)");

    auto* roleCombo = new QComboBox(&dlg);
    roleCombo->addItem("我是法人 / 控股创始人 (OWNER)", "OWNER");
    roleCombo->addItem("我是合伙人 / 投资入股 (PARTNER)", "PARTNER");
    roleCombo->addItem("我在该公司上班 (EMPLOYEE)", "EMPLOYEE");

    auto* indEdit = new QLineEdit(&dlg);
    indEdit->setPlaceholderText("行业领域 (例如: 新能源, 跨境电商)");

    auto* remarkEdit = new QLineEdit(&dlg);
    remarkEdit->setPlaceholderText("股权比例或职位备注");

    auto addField = [&](const QString& label, QWidget* w) {
        layout->addWidget(new QLabel(label, &dlg));
        layout->addWidget(w);
    };

    addField("公司/工作主体名称 *：", nameEdit);
    addField("我的身份属性：", roleCombo);
    addField("行业领域：", indEdit);
    addField("备注说明：", remarkEdit);

    auto* btnRow = new QHBoxLayout();
    auto* cancelBtn = new QPushButton("取消", &dlg);
    auto* saveBtn = new QPushButton("创建主体", &dlg);
    saveBtn->setStyleSheet("background-color: #00bcd4; color: #ffffff; font-weight: bold; padding: 6px 18px;");
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(saveBtn);
    layout->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(saveBtn, &QPushButton::clicked, [&]() {
        QString name = nameEdit->text().trimmed();
        if (name.isEmpty()) {
            CustomDialog::markFieldInvalid(nameEdit);
            return;
        }

        QSqlQuery q;
        q.prepare("INSERT INTO companies (name, role_type, industry, remark) VALUES (:n, :r, :ind, :rem);");
        q.bindValue(":n", name);
        q.bindValue(":r", roleCombo->currentData().toString());
        q.bindValue(":ind", indEdit->text().trimmed());
        q.bindValue(":rem", remarkEdit->text().trimmed());
        if (q.exec()) {
            // 自动为该主体创建一个基本对公账户
            int newCompId = q.lastInsertId().toInt();
            QSqlQuery qAcc;
            qAcc.prepare("INSERT INTO accounts (company_id, name, book_type, balance, remark) "
                         "VALUES (:cid, :aname, 'COMPANY', 0.0, '企业基本户');");
            qAcc.bindValue(":cid", newCompId);
            qAcc.bindValue(":aname", name + "-对公账户");
            qAcc.exec();

            dlg.accept();
        } else {
            QMessageBox::critical(&dlg, "错误", "创建失败：" + q.lastError().text());
        }
    });

    if (dlg.exec() == QDialog::Accepted) {
        loadCompanies();
    }
}

void CorporateFinanceWidget::onAddTransactionClicked() {
    CustomDialog dlg("记录对公财务流水", this);
    dlg.resize(460, 500);

    auto* layout = dlg.contentLayout();
    layout->setSpacing(12);

    auto* typeBox = new QComboBox(&dlg);
    typeBox->addItem("回款收入 (项目款/货款/服务费)", "INCOME");
    typeBox->addItem("对公支出 (采购/租金/应酬/薪资)", "EXPENSE");
    typeBox->addItem("公私调拨 (垫资借款/分红提现)", "TRANSFER");

    auto* catCombo = new QComboBox(&dlg);
    catCombo->setEditable(true);
    catCombo->addItems({"项目合同款", "采购原材料", "商务应酬宴请", "高档送礼", "员工工资薪资", "办公房租物业", "差旅报销", "技术研发采购"});

    auto* amtSpin = new QDoubleSpinBox(&dlg);
    amtSpin->setRange(0.01, 100000000);
    amtSpin->setValue(10000);
    amtSpin->setPrefix("￥ ");

    auto* contactBox = new QComboBox(&dlg);
    contactBox->addItem("无关联联系人", 0);
    auto contacts = DatabaseManager::instance().getContacts();
    for (const auto& c : contacts) {
        contactBox->addItem(QString("%1 (%2)").arg(c.name, c.company), c.id);
    }

    auto* dateEdit = new QDateEdit(QDate::currentDate(), &dlg);
    dateEdit->setCalendarPopup(true);
    dateEdit->setDisplayFormat("yyyy-MM-dd");

    auto* remarkEdit = new QLineEdit(&dlg);
    remarkEdit->setPlaceholderText("合同号、发票状态、业务说明等");

    auto addField = [&](const QString& label, QWidget* w) {
        layout->addWidget(new QLabel(label, &dlg));
        layout->addWidget(w);
    };

    addField("流水类型：", typeBox);
    addField("业务分类 (可选择或现场输入)：", catCombo);
    addField("发生金额 (元)：", amtSpin);
    addField("关联业务联系人 (客户/供应商/合作伙伴)：", contactBox);
    addField("发生日期：", dateEdit);
    addField("备注说明：", remarkEdit);

    auto* btnRow = new QHBoxLayout();
    auto* cancelBtn = new QPushButton("取消", &dlg);
    auto* saveBtn = new QPushButton("保存公司流水", &dlg);
    saveBtn->setStyleSheet("background-color: #00bcd4; color: #ffffff; font-weight: bold; padding: 6px 18px;");
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(saveBtn);
    layout->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(saveBtn, &QPushButton::clicked, [&]() {
        QSqlQuery q;
        q.prepare("INSERT INTO transactions (book_type, company_id, account_id, trans_type, amount, category, "
                  "trans_time, contact_id, remark) "
                  "VALUES ('COMPANY', :cid, 2, :type, :amt, :cat, :dt, :cid_contact, :rem);");
        q.bindValue(":cid", m_currentCompanyId);
        q.bindValue(":type", typeBox->currentData().toString());
        q.bindValue(":amt", amtSpin->value());
        q.bindValue(":cat", catCombo->currentText());
        q.bindValue(":dt", dateEdit->date().toString("yyyy-MM-dd") + " 12:00:00");
        int cid = contactBox->currentData().toInt();
        if (cid > 0) q.bindValue(":cid_contact", cid);
        else q.bindValue(":cid_contact", QVariant());
        q.bindValue(":rem", remarkEdit->text().trimmed());

        if (q.exec()) {
            dlg.accept();
        } else {
            QMessageBox::critical(&dlg, "错误", "保存失败！" + q.lastError().text());
        }
    });

    if (dlg.exec() == QDialog::Accepted) {
        refreshData();
    }
}
