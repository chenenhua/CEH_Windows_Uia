#include "DebtWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDialog>
#include "CustomDialog.h"
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QDateEdit>
#include <QMessageBox>

DebtWidget::DebtWidget(QWidget* parent) : QWidget(parent) {
    setupUi();
    refreshData();
}

void DebtWidget::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 20, 24, 20);
    mainLayout->setSpacing(18);

    // 1. 统计卡片
    auto* cardsLayout = new QHBoxLayout();
    cardsLayout->setSpacing(16);

    auto createStatCard = [](const QString& title, QLabel*& valLabel, const QString& accentColor) {
        auto* card = new QFrame();
        card->setStyleSheet("QFrame { background-color: #1a1a1a; border-radius: 8px; border: 1px solid #282828; padding: 12px; }");
        auto* layout = new QVBoxLayout(card);
        layout->setContentsMargins(8, 6, 8, 6);
        layout->setSpacing(4);

        auto* titleLbl = new QLabel(title, card);
        titleLbl->setStyleSheet("color: #888888; font-size: 12px; border: none;");

        valLabel = new QLabel("￥0.00", card);
        valLabel->setStyleSheet(QString("color: %1; font-size: 22px; font-weight: bold; border: none;").arg(accentColor));

        layout->addWidget(titleLbl);
        layout->addWidget(valLabel);
        return card;
    };

    cardsLayout->addWidget(createStatCard("当前借入负债总额 (银行/网银/个人)", m_lblTotalBorrowIn, "#ff5f57"));
    cardsLayout->addWidget(createStatCard("借出待回收总额 (熟人借贷/垫资)", m_lblTotalLendOut, "#34db4e"));
    cardsLayout->addWidget(createStatCard("7天内需还款/催收笔数", m_lblUpcomingCount, "#febc2f"));

    mainLayout->addLayout(cardsLayout);

    // 2. 操作栏
    auto* toolbar = new QHBoxLayout();
    toolbar->setSpacing(12);

    auto* filterTitle = new QLabel("维度筛选：", this);
    filterTitle->setStyleSheet("color: #aaaaaa; font-size: 13px;");
    toolbar->addWidget(filterTitle);

    m_filterCombo = new QComboBox(this);
    m_filterCombo->addItem("全部借贷记录", "");
    m_filterCombo->addItem("我借入的 (银行贷款/向人借入)", "BORROW_IN");
    m_filterCombo->addItem("我借出的 (熟人借贷/周转垫资)", "LEND_OUT");
    m_filterCombo->setStyleSheet(
        "QComboBox { background: #1e1e1e; color: #ffffff; border: 1px solid #333333; border-radius: 4px; padding: 6px 12px; min-width: 180px; }"
    );
    toolbar->addWidget(m_filterCombo);

    toolbar->addStretch();

    auto* addBtn = new QPushButton("+ 登记借贷 / 银行贷款", this);
    addBtn->setStyleSheet(
        "QPushButton { background: #28c840; color: #ffffff; font-weight: bold; border-radius: 4px; padding: 8px 18px; }"
        "QPushButton:hover { background: #34db4e; }"
    );
    toolbar->addWidget(addBtn);

    mainLayout->addLayout(toolbar);

    // 3. 借贷表格
    m_table = new QTableWidget(this);
    m_table->setColumnCount(8);
    m_table->setHorizontalHeaderLabels({"借贷属性", "对方/金融机构", "方向", "本金 (元)", "约定到期日", "每月还款日/月供", "状态", "备注"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setStyleSheet(
        "QTableWidget { background-color: #171717; border: 1px solid #282828; border-radius: 6px; gridline-color: #222222; color: #dddddd; font-size: 13px; }"
        "QHeaderView::section { background-color: #1f1f1f; color: #888888; border: none; padding: 8px; font-weight: bold; }"
        "QTableWidget::item { padding: 8px; border-bottom: 1px solid #202020; }"
    );

    mainLayout->addWidget(m_table);

    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DebtWidget::onFilterChanged);
    connect(addBtn, &QPushButton::clicked, this, &DebtWidget::onAddDebtClicked);
}

void DebtWidget::refreshData() {
    FinancialSnapshot snap = DatabaseManager::instance().getFinancialSnapshot();
    m_lblTotalBorrowIn->setText(QString("￥%1").arg(snap.totalBorrowIn, 0, 'f', 2));
    m_lblTotalLendOut->setText(QString("￥%1").arg(snap.totalLendOut, 0, 'f', 2));

    auto upcoming = DatabaseManager::instance().getUpcomingDebts(7);
    m_lblUpcomingCount->setText(QString("%1 笔").arg(upcoming.size()));

    onFilterChanged();
}

void DebtWidget::onFilterChanged() {
    QString dir = m_filterCombo->currentData().toString();
    m_debtList = DatabaseManager::instance().getDebts(dir);

    m_table->setRowCount(m_debtList.size());
    QDate today = QDate::currentDate();

    for (int i = 0; i < m_debtList.size(); ++i) {
        const auto& d = m_debtList[i];
        QString btypeStr = (d.borrowerType == "PERSON") ? "个人借贷" : "银行/网银";
        m_table->setItem(i, 0, new QTableWidgetItem(btypeStr));

        QString who = (d.borrowerType == "PERSON") ? d.contactName : d.institutionName;
        m_table->setItem(i, 1, new QTableWidgetItem(who));

        QString dirStr = (d.direction == "BORROW_IN") ? "借入 (负债)" : "借出 (应收)";
        auto* dirItem = new QTableWidgetItem(dirStr);
        if (d.direction == "BORROW_IN") dirItem->setForeground(QColor("#ff5f57"));
        else dirItem->setForeground(QColor("#34db4e"));
        m_table->setItem(i, 2, dirItem);

        m_table->setItem(i, 3, new QTableWidgetItem(QString("￥%1").arg(d.principal, 0, 'f', 2)));
        m_table->setItem(i, 4, new QTableWidgetItem(d.dueDate.toString("yyyy-MM-dd")));

        QString monthlyInfo = (d.monthlyPayDay > 0) ? QString("每月%1号 (￥%2)").arg(d.monthlyPayDay).arg(d.monthlyPayment, 0, 'f', 0) : "无";
        m_table->setItem(i, 5, new QTableWidgetItem(monthlyInfo));

        int daysLeft = today.daysTo(d.dueDate);
        QString statusText = "履约中";
        QColor statusColor = QColor("#28c840");
        if (daysLeft < 0) {
            statusText = QString("逾期 %1 天 🚨").arg(-daysLeft);
            statusColor = QColor("#ff5f57");
        } else if (daysLeft <= 7) {
            statusText = QString("剩 %1 天到期 ⚠️").arg(daysLeft);
            statusColor = QColor("#febc2f");
        }
        auto* sItem = new QTableWidgetItem(statusText);
        sItem->setForeground(statusColor);
        m_table->setItem(i, 6, sItem);

        m_table->setItem(i, 7, new QTableWidgetItem(d.remark));
    }
}

void DebtWidget::onAddDebtClicked() {
    CustomDialog dlg("登记借贷契约 / 银行网银贷款", this);
    dlg.resize(500, 580);

    auto* layout = dlg.contentLayout();
    layout->setSpacing(12);

    auto* btypeCombo = new QComboBox(&dlg);
    btypeCombo->addItem("个人熟人借贷", "PERSON");
    btypeCombo->addItem("银行 / 金融机构 / 网银贷款", "INSTITUTION");

    auto* dirCombo = new QComboBox(&dlg);
    dirCombo->addItem("借入 (增加现金，形成负债/需偿还)", "BORROW_IN");
    dirCombo->addItem("借出 (我借钱给别人/垫资，待回收)", "LEND_OUT");

    auto* instEdit = new QLineEdit(&dlg);
    instEdit->setPlaceholderText("机构名: 招商银行闪电贷, 微粒贷, 网商贷, 房贷等");

    auto* contactCombo = new QComboBox(&dlg);
    contactCombo->addItem("请选择关联联系人", 0);
    auto contacts = DatabaseManager::instance().getContacts();
    for (const auto& c : contacts) {
        contactCombo->addItem(QString("%1 (%2)").arg(c.name, c.company), c.id);
    }

    auto* principalSpin = new QDoubleSpinBox(&dlg);
    principalSpin->setRange(100, 100000000);
    principalSpin->setValue(20000);
    principalSpin->setPrefix("￥ ");

    auto* rateSpin = new QDoubleSpinBox(&dlg);
    rateSpin->setRange(0, 100);
    rateSpin->setValue(3.8);
    rateSpin->setSuffix(" % (年化)");

    auto* repayCombo = new QComboBox(&dlg);
    repayCombo->addItem("先息后本");
    repayCombo->addItem("等额本息");
    repayCombo->addItem("到期一次还本付息");

    auto* startDateEdit = new QDateEdit(QDate::currentDate(), &dlg);
    startDateEdit->setCalendarPopup(true);
    startDateEdit->setDisplayFormat("yyyy-MM-dd");

    auto* dueDateEdit = new QDateEdit(QDate::currentDate().addYears(1), &dlg);
    dueDateEdit->setCalendarPopup(true);
    dueDateEdit->setDisplayFormat("yyyy-MM-dd");

    auto* monthlyPayDaySpin = new QSpinBox(&dlg);
    monthlyPayDaySpin->setRange(0, 31);
    monthlyPayDaySpin->setValue(10);
    monthlyPayDaySpin->setSuffix(" 号");

    auto* monthlyPayAmtSpin = new QDoubleSpinBox(&dlg);
    monthlyPayAmtSpin->setRange(0, 1000000);
    monthlyPayAmtSpin->setValue(0);
    monthlyPayAmtSpin->setPrefix("￥ ");

    auto* remarkEdit = new QLineEdit(&dlg);
    remarkEdit->setPlaceholderText("借款用途、合同编号、质押物说明等");

    auto addField = [&](const QString& label, QWidget* w) {
        layout->addWidget(new QLabel(label, &dlg));
        layout->addWidget(w);
    };

    addField("借贷属性：", btypeCombo);
    addField("借贷方向：", dirCombo);
    addField("金融机构名称 (若选银行/网银)：", instEdit);
    addField("关联联系人 (若选个人借贷)：", contactCombo);
    addField("本金金额：", principalSpin);
    addField("年化利率：", rateSpin);
    addField("还款方式：", repayCombo);
    addField("借款起始日：", startDateEdit);
    addField("约定到期日 (关键提醒日)：", dueDateEdit);
    addField("每月固定还款日 (0表示无每月扣款)：", monthlyPayDaySpin);
    addField("每月应还本息金额 (可选)：", monthlyPayAmtSpin);
    addField("备注与约定事项：", remarkEdit);

    auto updateVisibility = [&]() {
        bool isInst = (btypeCombo->currentData().toString() == "INSTITUTION");
        instEdit->setEnabled(isInst);
        contactCombo->setEnabled(!isInst);
    };
    connect(btypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), updateVisibility);
    updateVisibility();

    auto* btnLayout = new QHBoxLayout();
    auto* cancelBtn = new QPushButton("取消", &dlg);
    auto* saveBtn = new QPushButton("保存契约", &dlg);
    saveBtn->setStyleSheet("background-color: #28c840; color: #ffffff; font-weight: bold; padding: 6px 18px;");
    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(saveBtn);
    layout->addLayout(btnLayout);

    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(saveBtn, &QPushButton::clicked, [&]() {
        bool isInst = (btypeCombo->currentData().toString() == "INSTITUTION");
        QString instName = instEdit->text().trimmed();
        int cid = contactCombo->currentData().toInt();

        if (isInst && instName.isEmpty()) {
            CustomDialog::markFieldInvalid(instEdit);
            return;
        }
        if (!isInst && cid == 0) {
            CustomDialog::markFieldInvalid(contactCombo);
            return;
        }

        bool ok = DatabaseManager::instance().addDebt(
            btypeCombo->currentData().toString(),
            dirCombo->currentData().toString(),
            cid,
            instName,
            principalSpin->value(),
            rateSpin->value(),
            repayCombo->currentText(),
            startDateEdit->date(),
            dueDateEdit->date(),
            monthlyPayDaySpin->value(),
            monthlyPayAmtSpin->value(),
            remarkEdit->text().trimmed()
        );

        if (ok) {
            dlg.accept();
        } else {
            QMessageBox::critical(&dlg, "错误", "保存借贷记录失败！");
        }
    });

    if (dlg.exec() == QDialog::Accepted) {
        refreshData();
    }
}
