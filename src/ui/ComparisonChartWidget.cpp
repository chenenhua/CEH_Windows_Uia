#include "ComparisonChartWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>

ComparisonChartWidget::ComparisonChartWidget(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void ComparisonChartWidget::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    m_lblTitle = new QLabel("请在左侧选择联系人查看深度交往图表", this);
    m_lblTitle->setStyleSheet("font-size: 16px; font-weight: bold; color: #ffffff;");
    layout->addWidget(m_lblTitle);

    m_lblMeetDate = new QLabel("相识起点：—", this);
    m_lblMeetDate->setStyleSheet("font-size: 12px; color: #00bcd4;");
    layout->addWidget(m_lblMeetDate);

    m_lblCostVsValue = new QLabel("财务往来净值：—", this);
    m_lblCostVsValue->setStyleSheet("font-size: 13px; font-weight: bold; color: #febc2f;");
    layout->addWidget(m_lblCostVsValue);

    layout->addStretch();
}

void ComparisonChartWidget::setStats(const ContactInteractionStats& stats) {
    m_stats = stats;
    m_lblTitle->setText(QString("我和【%1】的交往全景对比图表").arg(stats.contactName));

    if (stats.meetDate.isValid()) {
        m_lblMeetDate->setText(QString("🗓️ 认识时间起点：%1 (相识至今 %2 天)")
                                   .arg(stats.meetDate.toString("yyyy-MM-dd"))
                                   .arg(stats.meetDate.daysTo(QDate::currentDate())));
    } else {
        m_lblMeetDate->setText("🗓️ 认识时间起点：早期自主结识");
    }

    double diff = stats.theirTotalValue - stats.myTotalCost;
    QString diffStr = (diff >= 0) ? QString("净盈余 +￥%1").arg(diff, 0, 'f', 2)
                                  : QString("净投入 -￥%1").arg(-diff, 0, 'f', 2);
    m_lblCostVsValue->setText(QString("💰 财务双向对比：我付出 ￥%1  VS  TA回报 ￥%2  (%3)")
                                  .arg(stats.myTotalCost, 0, 'f', 2)
                                  .arg(stats.theirTotalValue, 0, 'f', 2)
                                  .arg(diffStr));
    update();
}

void ComparisonChartWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int startY = 110;
    int w = width() - 32;

    // 1. 绘制四大交往频次对比柱
    struct Item {
        QString name;
        int count;
        QColor color;
    };
    QList<Item> items = {
        {"🍸 商务应酬", m_stats.entertainCount, QColor("#9c27b0")},
        {"🍽️ 请客吃饭", m_stats.dinnerCount,    QColor("#2196f3")},
        {"💼 生意合作", m_stats.businessCount,  QColor("#ff9800")},
        {"🤝 互相帮扶", m_stats.helpCount,      QColor("#4caf50")}
    };

    int maxCount = 1;
    for (const auto& it : items) {
        if (it.count > maxCount) maxCount = it.count;
    }

    p.setPen(QColor("#888888"));
    p.setFont(QFont("Microsoft YaHei UI", 10, QFont::Bold));
    p.drawText(16, startY, "【社交频次雷达对比】");
    startY += 20;

    for (int i = 0; i < items.size(); ++i) {
        int y = startY + i * 36;
        p.setPen(QColor("#cccccc"));
        p.setFont(QFont("Microsoft YaHei UI", 9));
        p.drawText(16, y + 16, items[i].name);

        int barX = 120;
        int maxBarW = w - barX - 60;
        int barW = qMax(8, (items[i].count * maxBarW) / maxCount);

        // 背景槽
        p.setBrush(QColor("#222222"));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(barX, y + 4, maxBarW, 16, 4, 4);

        // 实际进度柱
        p.setBrush(items[i].color);
        p.drawRoundedRect(barX, y + 4, barW, 16, 4, 4);

        // 次数文字
        p.setPen(QColor("#ffffff"));
        p.drawText(barX + maxBarW + 12, y + 16, QString("%1 次").arg(items[i].count));
    }

    // 2. 绘制双向财务金额对比 (我付出的 VS TA回报的)
    int financeY = startY + 4 * 36 + 30;
    p.setPen(QColor("#888888"));
    p.setFont(QFont("Microsoft YaHei UI", 10, QFont::Bold));
    p.drawText(16, financeY, "【财务利益双向对比 (我和TA)】");
    financeY += 24;

    double maxVal = qMax(1000.0, qMax(m_stats.myTotalCost, m_stats.theirTotalValue));
    int finBarX = 140;
    int maxFinW = w - finBarX - 100;

    // 我付出的成本 (红柱)
    p.setPen(QColor("#ff5f57"));
    p.setFont(QFont("Microsoft YaHei UI", 9));
    p.drawText(16, financeY + 16, "我付出的总成本");

    int costW = static_cast<int>((m_stats.myTotalCost / maxVal) * maxFinW);
    p.setBrush(QColor("#222222"));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(finBarX, financeY + 4, maxFinW, 18, 4, 4);
    p.setBrush(QColor("#ff5f57"));
    p.drawRoundedRect(finBarX, financeY + 4, qMax(4, costW), 18, 4, 4);
    p.setPen(QColor("#ff8a80"));
    p.drawText(finBarX + maxFinW + 10, financeY + 18, QString("￥%1").arg(m_stats.myTotalCost, 0, 'f', 0));

    // TA为我带来的回报 (绿柱)
    financeY += 36;
    p.setPen(QColor("#28c840"));
    p.drawText(16, financeY + 16, "TA带来的商业回报");

    int valW = static_cast<int>((m_stats.theirTotalValue / maxVal) * maxFinW);
    p.setBrush(QColor("#222222"));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(finBarX, financeY + 4, maxFinW, 18, 4, 4);
    p.setBrush(QColor("#28c840"));
    p.drawRoundedRect(finBarX, financeY + 4, qMax(4, valW), 18, 4, 4);
    p.setPen(QColor("#69f0ae"));
    p.drawText(finBarX + maxFinW + 10, financeY + 18, QString("￥%1").arg(m_stats.theirTotalValue, 0, 'f', 0));
}
