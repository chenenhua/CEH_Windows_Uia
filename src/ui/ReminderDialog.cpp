#include "ReminderDialog.h"
#include "../core/DatabaseManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDate>

ReminderDialog::ReminderDialog(QWidget* parent)
    : CustomDialog("⚠️ 近期到期与人情走动强提醒", parent) {
    setupUi();
    loadReminders();
}

void ReminderDialog::setupUi() {
    resize(760, 480);

    auto* mainLayout = contentLayout();
    mainLayout->setSpacing(14);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setStyleSheet("color: #febc2f; font-size: 15px; font-weight: bold;");
    mainLayout->addWidget(m_summaryLabel);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({"类别", "关联对象", "待办/履约事项", "约定时间", "剩余天数 / 紧急程度"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setStyleSheet(
        "QTableWidget { background-color: #141414; border: 1px solid #282828; border-radius: 6px; gridline-color: #222222; color: #dddddd; font-size: 13px; }"
        "QHeaderView::section { background-color: #1f1f1f; color: #888888; border: none; padding: 8px; font-weight: bold; }"
        "QTableWidget::item { padding: 8px; border-bottom: 1px solid #202020; }"
    );
    mainLayout->addWidget(m_table);

    auto* bottomRow = new QHBoxLayout();
    bottomRow->addStretch();

    auto* okBtn = new QPushButton("已阅，进入系统", this);
    okBtn->setStyleSheet(
        "QPushButton { background: #28c840; color: #ffffff; font-weight: bold; border-radius: 4px; padding: 8px 24px; font-size: 13px; }"
        "QPushButton:hover { background: #34db4e; }"
    );
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    bottomRow->addWidget(okBtn);

    mainLayout->addLayout(bottomRow);
}

void ReminderDialog::loadReminders() {
    auto debts = DatabaseManager::instance().getUpcomingDebts(7);
    auto plans = DatabaseManager::instance().getUpcomingSocialPlans(7);
    auto pendingGifts = DatabaseManager::instance().getSocialEvents(0, true);

    int totalCount = debts.size() + plans.size() + pendingGifts.size();
    m_summaryLabel->setText(QString("检测到 %1 项即将到期或亟需走动的事务，请注意履约与安排！").arg(totalCount));

    m_table->setRowCount(totalCount);
    int row = 0;
    QDate today = QDate::currentDate();

    // 1. 债务/贷款到期
    for (const auto& d : debts) {
        auto* catItem = new QTableWidgetItem("💰 借贷到期");
        catItem->setForeground(QColor("#ff5f57"));
        m_table->setItem(row, 0, catItem);

        QString who = (d.borrowerType == "PERSON") ? d.contactName : d.institutionName;
        m_table->setItem(row, 1, new QTableWidgetItem(who));

        QString dir = (d.direction == "BORROW_IN") ? "应还款" : "应催款";
        m_table->setItem(row, 2, new QTableWidgetItem(QString("%1: ￥%2").arg(dir).arg(d.principal, 0, 'f', 2)));
        m_table->setItem(row, 3, new QTableWidgetItem(d.dueDate.toString("yyyy-MM-dd")));

        int daysLeft = today.daysTo(d.dueDate);
        QString urgency = (daysLeft < 0) ? QString("已逾期 %1 天 🚨").arg(-daysLeft)
                                         : ((daysLeft == 0) ? "今日到期 ⚠️" : QString("剩 %1 天").arg(daysLeft));
        auto* urgItem = new QTableWidgetItem(urgency);
        urgItem->setForeground(daysLeft <= 0 ? QColor("#ff5f57") : QColor("#febc2f"));
        m_table->setItem(row, 4, urgItem);
        row++;
    }

    // 2. 走动拜访/请客计划
    for (const auto& p : plans) {
        auto* catItem = new QTableWidgetItem("📅 走动计划");
        catItem->setForeground(QColor("#00bcd4"));
        m_table->setItem(row, 0, catItem);

        m_table->setItem(row, 1, new QTableWidgetItem(p.contactName.isEmpty() ? "公共事务" : p.contactName));
        m_table->setItem(row, 2, new QTableWidgetItem(p.title));
        m_table->setItem(row, 3, new QTableWidgetItem(p.plannedDate.toString("yyyy-MM-dd")));

        int daysLeft = today.daysTo(p.plannedDate);
        QString urgency = (daysLeft < 0) ? "已过约定日" : ((daysLeft == 0) ? "今日执行 🔥" : QString("剩 %1 天").arg(daysLeft));
        auto* urgItem = new QTableWidgetItem(urgency);
        urgItem->setForeground(QColor("#00bcd4"));
        m_table->setItem(row, 4, urgItem);
        row++;
    }

    // 3. 待回礼提醒
    for (const auto& g : pendingGifts) {
        auto* catItem = new QTableWidgetItem("🎁 待还人情");
        catItem->setForeground(QColor("#febc2f"));
        m_table->setItem(row, 0, catItem);

        m_table->setItem(row, 1, new QTableWidgetItem(g["contact_name"].toString()));
        m_table->setItem(row, 2, new QTableWidgetItem(QString("收礼: %1 (估值:￥%2)").arg(g["title"].toString()).arg(g["estimated_value"].toDouble(), 0, 'f', 0)));
        m_table->setItem(row, 3, new QTableWidgetItem(g["event_date"].toString()));

        auto* urgItem = new QTableWidgetItem("择机回礼 ⚠️");
        urgItem->setForeground(QColor("#febc2f"));
        m_table->setItem(row, 4, urgItem);
        row++;
    }
}

void ReminderDialog::onMarkPlanCompleted(int planId) {
    DatabaseManager::instance().updateSocialPlanStatus(planId, "COMPLETED");
    loadReminders();
}

bool ReminderDialog::checkAndShowIfNeeded(QWidget* parent) {
    auto debts = DatabaseManager::instance().getUpcomingDebts(7);
    auto plans = DatabaseManager::instance().getUpcomingSocialPlans(7);
    auto pendingGifts = DatabaseManager::instance().getSocialEvents(0, true);

    if (debts.isEmpty() && plans.isEmpty() && pendingGifts.isEmpty()) {
        return false;
    }

    ReminderDialog dlg(parent);
    dlg.exec();
    return true;
}
