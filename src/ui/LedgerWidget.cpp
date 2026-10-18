#include "LedgerWidget.h"
#include "../core/DatabaseManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDialog>
#include "CustomDialog.h"
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QDateTimeEdit>
#include <QMessageBox>

LedgerWidget::LedgerWidget(QWidget* parent) : QWidget(parent) {
    setupUi();
    refreshData();
}

void LedgerWidget::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 20, 24, 20);
    mainLayout->setSpacing(20);

    // 1. 顶层统计指标卡片
    auto* cardsLayout = new QHBoxLayout();
    cardsLayout->setSpacing(16);

    auto createStatCard = [](const QString& title, QLabel*& valLabel, const QString& accentColor) {
        auto* card = new QFrame();
        card->setStyleSheet(QString(
            "QFrame { background-color: #1a1a1a; border-radius: 8px; border: 1px solid #282828; padding: 12px; }"
        ));
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

    cardsLayout->addWidget(createStatCard("个人日常备用金", m_lblPersonalBal, "#34db4e"));
    cardsLayout->addWidget(createStatCard("公司对公账户余额", m_lblCompanyBal, "#00bcd4"));
    cardsLayout->addWidget(createStatCard("现金流安全天数预估", m_lblRunway, "#febc2f"));

    mainLayout->addLayout(cardsLayout);

    // 2. 操作栏 (筛选与记账按钮)
    auto* toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(12);

    auto* filterTitle = new QLabel("账本维度：", this);
    filterTitle->setStyleSheet("color: #aaaaaa; font-size: 13px;");
    toolbarLayout->addWidget(filterTitle);

    m_bookFilter = new QComboBox(this);
    m_bookFilter->addItem("全部流水 (双轨汇总)", "");
    m_bookFilter->addItem("仅个人账本", "PERSONAL");
    m_bookFilter->addItem("仅公司账本", "COMPANY");
    m_bookFilter->setStyleSheet(
        "QComboBox { background: #1e1e1e; color: #ffffff; border: 1px solid #333333; border-radius: 4px; padding: 6px 12px; min-width: 160px; }"
        "QComboBox QAbstractItemView { background: #1e1e1e; color: #ffffff; selection-background-color: #2b2b2b; }"
    );
    toolbarLayout->addWidget(m_bookFilter);

    toolbarLayout->addStretch();

    auto* addBtn = new QPushButton("+ 记一笔 (个人/公司/商务送礼)", this);
    addBtn->setStyleSheet(
        "QPushButton { background: #28c840; color: #ffffff; font-weight: bold; border-radius: 4px; padding: 8px 18px; }"
        "QPushButton:hover { background: #34db4e; }"
        "QPushButton:pressed { background: #1fa834; }"
    );
    toolbarLayout->addWidget(addBtn);

    mainLayout->addLayout(toolbarLayout);

    // 3. 流水表格
    m_table = new QTableWidget(this);
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({"发生时间", "账本归属", "类型", "分类", "金额 (元)", "关联联系人", "备注"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setStyleSheet(
        "QTableWidget { background-color: #171717; border: 1px solid #282828; border-radius: 6px; gridline-color: #222222; color: #dddddd; font-size: 13px; }"
        "QHeaderView::section { background-color: #1f1f1f; color: #888888; border: none; border-bottom: 1px solid #282828; padding: 10px; font-weight: bold; }"
        "QTableWidget::item { padding: 8px; border-bottom: 1px solid #202020; }"
        "QTableWidget::item:selected { background-color: #262626; color: #ffffff; }"
    );

    mainLayout->addWidget(m_table);

    connect(m_bookFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LedgerWidget::onFilterChanged);
    connect(addBtn, &QPushButton::clicked, this, &LedgerWidget::onAddTransactionClicked);
}

void LedgerWidget::refreshData() {
    FinancialSnapshot snap = DatabaseManager::instance().getFinancialSnapshot();
    m_lblPersonalBal->setText(QString("￥%1").arg(snap.personalBalance, 0, 'f', 2));
    m_lblCompanyBal->setText(QString("￥%1").arg(snap.companyBalance, 0, 'f', 2));
    m_lblRunway->setText(QString("%1 天").arg(snap.runwayDays));

    onFilterChanged();
}

void LedgerWidget::onFilterChanged() {
    QString bookType = m_bookFilter->currentData().toString();
    auto list = DatabaseManager::instance().getRecentTransactions(100, bookType);

    m_table->setRowCount(list.size());
    for (int i = 0; i < list.size(); ++i) {
        const auto& row = list[i];
        m_table->setItem(i, 0, new QTableWidgetItem(row["trans_time"].toString()));

        QString book = (row["book_type"].toString() == "PERSONAL") ? "个人账本" : "公司账本";
        m_table->setItem(i, 1, new QTableWidgetItem(book));

        QString type = row["trans_type"].toString();
        QString typeText = (type == "EXPENSE") ? "支出" : ((type == "INCOME") ? "收入" : "内部调拨");
        m_table->setItem(i, 2, new QTableWidgetItem(typeText));

        m_table->setItem(i, 3, new QTableWidgetItem(row["category"].toString()));

        double amt = row["amount"].toDouble();
        QString amtStr = (type == "INCOME") ? QString("+￥%1").arg(amt, 0, 'f', 2)
                                            : QString("-￥%1").arg(amt, 0, 'f', 2);
        auto* amtItem = new QTableWidgetItem(amtStr);
        if (type == "INCOME") amtItem->setForeground(QColor("#28c840"));
        else amtItem->setForeground(QColor("#ff5f57"));
        m_table->setItem(i, 4, amtItem);

        QString contactName = row["contact_name"].toString();
        m_table->setItem(i, 5, new QTableWidgetItem(contactName.isEmpty() ? "—" : contactName));
        m_table->setItem(i, 6, new QTableWidgetItem(row["remark"].toString()));
    }
}

void LedgerWidget::onAddTransactionClicked() {
    CustomDialog dlg("记录新流水", this);
    dlg.resize(460, 500);

    auto* layout = dlg.contentLayout();
    layout->setSpacing(14);

    auto* bookBox = new QComboBox(&dlg);
    bookBox->addItem("个人账本", "PERSONAL");
    bookBox->addItem("公司账本", "COMPANY");

    auto* typeBox = new QComboBox(&dlg);
    typeBox->addItem("支出 (EXPENSE)", "EXPENSE");
    typeBox->addItem("收入 (INCOME)", "INCOME");
    typeBox->addItem("内部垫资/调拨 (TRANSFER)", "TRANSFER");

    auto* amtSpin = new QDoubleSpinBox(&dlg);
    amtSpin->setRange(0.01, 10000000.0);
    amtSpin->setValue(500.0);
    amtSpin->setPrefix("￥ ");

    auto* catCombo = new QComboBox(&dlg);
    catCombo->setEditable(true);

    auto updateCategories = [&]() {
        QString t = typeBox->currentData().toString();
        QString catType = (t == "INCOME") ? "INCOME" : "EXPENSE";
        catCombo->clear();
        catCombo->addItems(DatabaseManager::instance().getCategories(catType));
    };
    connect(typeBox, QOverload<int>::of(&QComboBox::currentIndexChanged), updateCategories);
    updateCategories();

    auto* contactBox = new QComboBox(&dlg);
    contactBox->addItem("无关联联系人", 0);
    auto contacts = DatabaseManager::instance().getContacts();
    for (const auto& c : contacts) {
        contactBox->addItem(QString("%1 (%2 - %3)").arg(c.name, c.company, c.title), c.id);
    }

    auto* remarkEdit = new QLineEdit(&dlg);
    remarkEdit->setPlaceholderText("备注说明");

    auto addField = [&](const QString& label, QWidget* w) {
        layout->addWidget(new QLabel(label, &dlg));
        layout->addWidget(w);
    };

    addField("账本归属：", bookBox);
    addField("交易类型：", typeBox);
    addField("金额 (元)：", amtSpin);
    addField("收支分类 (可选生活/商务开支，或直接输入新分类)：", catCombo);
    addField("关联人脉 (如果是商务送礼/招待，建议必选)：", contactBox);
    addField("备注说明：", remarkEdit);

    auto* btnLayout = new QHBoxLayout();
    auto* cancelBtn = new QPushButton("取消", &dlg);
    auto* saveBtn = new QPushButton("保存流水", &dlg);
    saveBtn->setStyleSheet("background-color: #28c840; color: #ffffff; font-weight: bold; padding: 8px 16px;");
    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(saveBtn);
    layout->addLayout(btnLayout);

    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(saveBtn, &QPushButton::clicked, [&]() {
        QString cat = catCombo->currentText().trimmed();
        if (cat.isEmpty()) {
            CustomDialog::markFieldInvalid(catCombo);
            return;
        }

        // 默认关联第一个账户
        auto accs = DatabaseManager::instance().getAccounts(bookBox->currentData().toString());
        int accId = accs.isEmpty() ? 1 : accs[0]["id"].toInt();

        bool ok = DatabaseManager::instance().addTransaction(
            bookBox->currentData().toString(),
            accId,
            typeBox->currentData().toString(),
            amtSpin->value(),
            cat,
            QDateTime::currentDateTime(),
            contactBox->currentData().toInt(),
            remarkEdit->text().trimmed()
        );

        if (ok) {
            dlg.accept();
        } else {
            QMessageBox::critical(&dlg, "错误", "写入数据库失败！");
        }
    });

    if (dlg.exec() == QDialog::Accepted) {
        refreshData();
    }
}
