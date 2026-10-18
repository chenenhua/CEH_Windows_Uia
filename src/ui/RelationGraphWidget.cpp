#include "RelationGraphWidget.h"
#include "UiIcons.h"
#include "../core/DatabaseManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QProgressBar>
#include <QLabel>
#include <QGraphicsItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QPen>
#include <QBrush>
#include <QtMath>
#include <QDate>
#include <QMap>
#include <QSet>
#include <functional>

namespace {

// 自定义圆形人脉节点：圆内居中绘制【名字 + 行业】两行文本，支持高亮与变灰变淡状态
class ContactNodeItem : public QGraphicsItem {
public:
    enum { Type = UserType + 101 };

    ContactNodeItem(int nodeId,
                    const QString& name,
                    const QString& industry,
                    double radius,
                    const QColor& borderColor,
                    const QColor& fillColor,
                    bool isCenter = false)
        : m_nodeId(nodeId),
          m_name(name),
          m_industry(industry.isEmpty() ? "IT" : industry),
          m_radius(radius),
          m_borderColor(borderColor),
          m_fillColor(fillColor),
          m_isCenter(isCenter) {
        setZValue(isCenter ? 4 : 2);
        setAcceptedMouseButtons(Qt::LeftButton);
    }

    int type() const override { return Type; }
    int nodeId() const { return m_nodeId; }

    void setVisualState(bool isPrimaryFocus, bool isDimmed) {
        m_isPrimaryFocus = isPrimaryFocus;
        m_isDimmed = isDimmed;
        if (isPrimaryFocus) {
            setZValue(10);
        } else if (isDimmed) {
            setZValue(1);
        } else {
            setZValue(m_isCenter ? 6 : 4);
        }
        update();
    }

    QRectF boundingRect() const override {
        return QRectF(-m_radius - 5, -m_radius - 5, (m_radius + 5) * 2, (m_radius + 5) * 2);
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override {
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::TextAntialiasing, true);

        QRectF circleRect(-m_radius, -m_radius, m_radius * 2, m_radius * 2);

        QColor drawBorder = m_borderColor;
        QColor drawFill = m_fillColor;
        QColor nameColor("#e1e1e5");
        QColor indColor = m_isCenter ? QColor("#e1e1e5") : QColor("#8e8e93");
        double penWidth = m_isCenter ? 2.2 : 1.6;

        if (m_isDimmed) {
            // 无关节点：变灰、变淡
            drawBorder = QColor(62, 62, 68, 85);
            drawFill = QColor(20, 20, 23, 95);
            nameColor = QColor(115, 115, 122, 80);
            indColor = QColor(90, 90, 96, 70);
            penWidth = 1.1;
        } else if (m_isPrimaryFocus) {
            // 当前被点击的高亮焦点节点：外发光环 + 高亮边框
            QRectF glowRect = circleRect.adjusted(-3.5, -3.5, 3.5, 3.5);
            QColor glowColor = m_borderColor;
            glowColor.setAlpha(95);
            painter->setPen(QPen(glowColor, 3.0));
            painter->setBrush(Qt::NoBrush);
            painter->drawEllipse(glowRect);

            drawBorder = m_borderColor.lighter(125);
            drawFill = (m_nodeId == 0) ? QColor("#1f4627") : QColor("#252830");
            nameColor = QColor("#ffffff");
            indColor = QColor("#d8d8de");
            penWidth = 2.6;
        }

        painter->setPen(QPen(drawBorder, penWidth));
        painter->setBrush(QBrush(drawFill));
        painter->drawEllipse(circleRect);

        // 上半部绘制名字 (12px/10px 清晰对比)
        QFont nameFont("Microsoft YaHei UI");
        nameFont.setPixelSize(m_radius >= 30 ? 12 : 10);
        nameFont.setBold(true);
        painter->setFont(nameFont);
        painter->setPen(nameColor);
        QRectF topRect(-m_radius + 3, -m_radius * 0.62, (m_radius - 3) * 2, m_radius * 0.68);
        painter->drawText(topRect, Qt::AlignHCenter | Qt::AlignBottom | Qt::TextSingleLine, m_name);

        // 下半部绘制行业 (10px 说明字号)
        QFont indFont("Microsoft YaHei UI");
        indFont.setPixelSize(m_radius >= 30 ? 10 : 9);
        painter->setFont(indFont);
        painter->setPen(indColor);
        QRectF bottomRect(-m_radius + 3, 1, (m_radius - 3) * 2, m_radius * 0.65);
        QString shortInd = m_industry;
        if (shortInd.length() > 4) shortInd = shortInd.left(4);
        painter->drawText(bottomRect, Qt::AlignHCenter | Qt::AlignTop | Qt::TextSingleLine, shortInd);
    }

private:
    int m_nodeId{0};
    QString m_name;
    QString m_industry;
    double m_radius;
    QColor m_borderColor;
    QColor m_fillColor;
    bool m_isCenter{false};
    bool m_isPrimaryFocus{false};
    bool m_isDimmed{false};
};

} // namespace

// 支持滚轮缩放 + 左键拖拽 + 点击人名节点高亮的交互式画布视图
class InteractiveGraphView : public QGraphicsView {
public:
    explicit InteractiveGraphView(QGraphicsScene* scene, QWidget* parent = nullptr)
        : QGraphicsView(scene, parent) {
        setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        setResizeAnchor(QGraphicsView::AnchorViewCenter);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setFocusPolicy(Qt::StrongFocus);
        setDragMode(QGraphicsView::NoDrag);
        setMouseTracking(true);
        setCursor(Qt::OpenHandCursor);
    }

    void setNodeClickCallback(std::function<void(int)> cb) {
        m_onNodeClicked = std::move(cb);
    }

    void resetZoom() {
        resetTransform();
        m_currentScale = 1.0;
    }

protected:
    void enterEvent(QEnterEvent* event) override {
        setFocus(Qt::MouseFocusReason);
        QGraphicsView::enterEvent(event);
    }

    void wheelEvent(QWheelEvent* event) override {
        const double zoomFactor = 1.15;
        if (event->angleDelta().y() > 0) {
            if (m_currentScale < 3.5) {
                scale(zoomFactor, zoomFactor);
                m_currentScale *= zoomFactor;
            }
        } else if (event->angleDelta().y() < 0) {
            if (m_currentScale > 0.3) {
                scale(1.0 / zoomFactor, 1.0 / zoomFactor);
                m_currentScale /= zoomFactor;
            }
        }
        event->accept();
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
            m_spacePressed = true;
            setCursor(Qt::OpenHandCursor);
            event->accept();
            return;
        }
        QGraphicsView::keyPressEvent(event);
    }

    void keyReleaseEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
            m_spacePressed = false;
            if (!m_isPanning) {
                setCursor(Qt::OpenHandCursor);
            }
            event->accept();
            return;
        }
        QGraphicsView::keyReleaseEvent(event);
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            m_isPanning = true;
            m_didDrag = false;
            m_pressPos = event->pos();
            m_lastPanPos = event->pos();
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }
        QGraphicsView::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (m_isPanning) {
            if ((event->pos() - m_pressPos).manhattanLength() > 4) {
                m_didDrag = true;
            }
            QPoint delta = event->pos() - m_lastPanPos;
            m_lastPanPos = event->pos();
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
            verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
            event->accept();
            return;
        }
        QGraphicsItem* hit = itemAt(event->pos());
        if (hit && hit->type() == ContactNodeItem::Type) {
            setCursor(Qt::PointingHandCursor);
        } else {
            setCursor(Qt::OpenHandCursor);
        }
        QGraphicsView::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && m_isPanning) {
            m_isPanning = false;
            QGraphicsItem* hit = itemAt(event->pos());
            if (!m_didDrag && hit && hit->type() == ContactNodeItem::Type) {
                auto* nodeItem = static_cast<ContactNodeItem*>(hit);
                if (m_onNodeClicked) {
                    m_onNodeClicked(nodeItem->nodeId());
                }
                setCursor(Qt::PointingHandCursor);
            } else {
                setCursor(Qt::OpenHandCursor);
            }
            event->accept();
            return;
        }
        QGraphicsView::mouseReleaseEvent(event);
    }

private:
    bool m_spacePressed{false};
    bool m_isPanning{false};
    bool m_didDrag{false};
    QPoint m_pressPos;
    QPoint m_lastPanPos;
    double m_currentScale{1.0};
    std::function<void(int)> m_onNodeClicked;
};

// 节点防重叠与间隙保证算法：迭代消除所有圆与圆的交叉与挤压，确保圆和圆之间绝对不交叉，且保留清晰空隙
static void resolveNodeCollisions(QMap<int, QPointF>& posMap, int centerId, double centerR, double nodeR, double minGap) {
    const int maxIterations = 150;
    for (int iter = 0; iter < maxIterations; ++iter) {
        bool hasOverlap = false;
        QList<int> ids = posMap.keys();
        int count = ids.size();

        for (int i = 0; i < count; ++i) {
            int u = ids[i];
            double ru = (u == centerId) ? centerR : nodeR;
            for (int j = i + 1; j < count; ++j) {
                int v = ids[j];
                double rv = (v == centerId) ? centerR : nodeR;

                QPointF delta = posMap[v] - posMap[u];
                double dist = std::hypot(delta.x(), delta.y());
                double reqDist = ru + rv + minGap;

                if (dist < reqDist) {
                    hasOverlap = true;
                    QPointF dir;
                    if (dist > 0.001) {
                        dir = delta / dist;
                    } else {
                        double rndAngle = (i * 1.618 + j) * 2.0 * M_PI;
                        dir = QPointF(qCos(rndAngle), qSin(rndAngle));
                    }
                    double push = reqDist - dist;

                    if (u == centerId) {
                        posMap[v] += dir * push;
                    } else if (v == centerId) {
                        posMap[u] -= dir * push;
                    } else {
                        posMap[u] -= dir * (push * 0.5);
                        posMap[v] += dir * (push * 0.5);
                    }
                }
            }
        }

        // 严格保证所有外围节点与中心点之间的间距
        double minCenterDist = centerR + nodeR + minGap + 12.0;
        for (int id : ids) {
            if (id == centerId) continue;
            QPointF cDelta = posMap[id] - posMap[centerId];
            double distCenter = std::hypot(cDelta.x(), cDelta.y());
            if (distCenter < minCenterDist) {
                hasOverlap = true;
                QPointF dir = (distCenter > 0.001) ? (cDelta / distCenter) : QPointF(0.0, 1.0);
                posMap[id] = posMap[centerId] + dir * minCenterDist;
            }
        }

        if (!hasOverlap) break;
    }
}

RelationGraphWidget::RelationGraphWidget(QWidget* parent) : QWidget(parent) {
    setupUi();
    refreshGraph();
}

void RelationGraphWidget::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    m_headerWidget = new QWidget(this);
    auto* topRow = new QHBoxLayout(m_headerWidget);
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(10);

    auto* iconLbl = new QLabel(m_headerWidget);
    iconLbl->setPixmap(UiIcons::pixmap(UiIcons::Code::Network, 14, QColor("#8e8e93")));
    topRow->addWidget(iconLbl);

    auto* title = new QLabel("关系网", m_headerWidget);
    title->setStyleSheet("color: #e1e1e5; font-size: 14px; font-weight: bold;");
    topRow->addWidget(title);

    auto* hintLbl = new QLabel("提示：点击任意人名高亮其关系网（无关节点变淡），点击自己名字全部高亮；滚轮可缩放，左键可拖动", m_headerWidget);
    hintLbl->setStyleSheet("color: #8e8e93; font-size: 11px;");
    topRow->addWidget(hintLbl);

    topRow->addStretch();

    auto* resetBtn = new QPushButton("重置视角", m_headerWidget);
    resetBtn->setCursor(Qt::PointingHandCursor);
    resetBtn->setStyleSheet(
        "QPushButton { background: #222226; color: #be782e; padding: 4px 10px; border-radius: 3px; border: 1px solid #28282b; font-size: 12px; }"
        "QPushButton:hover { background-color: #2c2c30; }"
    );
    topRow->addWidget(resetBtn);

    auto* refreshBtn = new QPushButton("刷新", m_headerWidget);
    refreshBtn->setCursor(Qt::PointingHandCursor);
    refreshBtn->setStyleSheet(
        "QPushButton { background: #222226; color: #00a361; padding: 4px 10px; border-radius: 3px; border: 1px solid #28282b; font-size: 12px; }"
        "QPushButton:hover { background-color: #2c2c30; }"
    );
    topRow->addWidget(refreshBtn);

    layout->addWidget(m_headerWidget);

    m_scene = new QGraphicsScene(this);
    m_view = new InteractiveGraphView(m_scene, this);
    m_view->setStyleSheet("background-color: #0e0e10; border: none; border-radius: 3px;");
    m_view->setNodeClickCallback([this](int nodeId) {
        applyHighlightForNode(nodeId);
    });

    layout->addWidget(m_view, 1);

    setupSummaryCard();

    connect(refreshBtn, &QPushButton::clicked, this, [this]() {
        m_highlightedNodeId = 0;
        if (m_summaryCard) m_summaryCard->hide();
        refreshGraph();
    });
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        m_highlightedNodeId = 0;
        applyHighlightForNode(0);
        m_view->resetZoom();
        m_view->centerOn(0, 0);
    });
}

void RelationGraphWidget::setupSummaryCard() {
    m_summaryCard = new QFrame(m_view);
    m_summaryCard->setObjectName("graphSummaryCard");
    m_summaryCard->setFixedWidth(368);
    m_summaryCard->setStyleSheet(
        "QFrame#graphSummaryCard { background-color: #161618; border: 1px solid #242428; border-radius: 8px; }"
        "QFrame#graphSummaryCard QLabel { background: transparent; border: none; padding: 1px 0px; }"
    );

    auto* cardLayout = new QVBoxLayout(m_summaryCard);
    cardLayout->setContentsMargins(16, 14, 16, 16);
    cardLayout->setSpacing(10);

    // 1. 相识时间与投入回报总结
    m_lblMeetDate = new QLabel(m_summaryCard);
    m_lblMeetDate->setMinimumHeight(20);
    m_lblMeetDate->setStyleSheet("color: #e1e1e5; font-size: 13px; font-weight: bold;");
    cardLayout->addWidget(m_lblMeetDate);

    m_lblSpendVsReturn = new QLabel(m_summaryCard);
    m_lblSpendVsReturn->setMinimumHeight(20);
    m_lblSpendVsReturn->setTextFormat(Qt::RichText);
    m_lblSpendVsReturn->setStyleSheet("font-size: 12px;");
    cardLayout->addWidget(m_lblSpendVsReturn);

    // 2. 社交互动进度条区域
    m_socialStatsContainer = new QWidget(m_summaryCard);
    m_socialStatsContainer->setStyleSheet("background: transparent; border: none;");
    m_socialStatsLayout = new QVBoxLayout(m_socialStatsContainer);
    m_socialStatsLayout->setContentsMargins(0, 2, 0, 2);
    m_socialStatsLayout->setSpacing(6);
    cardLayout->addWidget(m_socialStatsContainer);

    // 分割线
    auto* divLine = new QFrame(m_summaryCard);
    divLine->setFixedHeight(1);
    divLine->setStyleSheet("background-color: #242428; border: none;");
    cardLayout->addWidget(divLine);

    // 3. 亲疏 / 能量 / 信用 星级行
    auto* starsRow = new QHBoxLayout();
    starsRow->setContentsMargins(0, 2, 0, 2);
    starsRow->setSpacing(10);

    auto makeStarItem = [this, starsRow](const QString& keyText, QLabel*& starLbl) {
        auto* h = new QHBoxLayout();
        h->setSpacing(4);
        auto* k = new QLabel(keyText, m_summaryCard);
        k->setMinimumHeight(20);
        k->setStyleSheet("color: #8e8e93; font-size: 12px;");
        starLbl = new QLabel("★★★☆☆", m_summaryCard);
        starLbl->setMinimumHeight(20);
        starLbl->setStyleSheet("color: #be782e; font-size: 12px; font-weight: bold;");
        h->addWidget(k);
        h->addWidget(starLbl);
        h->addStretch();
        starsRow->addLayout(h, 1);
    };

    makeStarItem("亲疏:", m_lblClosenessStars);
    makeStarItem("能量:", m_lblInfluenceStars);
    makeStarItem("信用:", m_lblCreditStars);
    cardLayout->addLayout(starsRow);

    // 4. 双列资料网格: 电话/公司, 地址/职务, 行业/引荐人
    auto* grid = new QGridLayout();
    grid->setContentsMargins(0, 2, 0, 2);
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(10);

    auto addInfoCell = [this, grid](int r, int c, const QString& keyText, QLabel*& valLbl) {
        auto* cell = new QHBoxLayout();
        cell->setSpacing(4);
        auto* k = new QLabel(keyText, m_summaryCard);
        k->setMinimumHeight(20);
        k->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        k->setStyleSheet("color: #8e8e93; font-size: 12px;");
        valLbl = new QLabel("—", m_summaryCard);
        valLbl->setMinimumHeight(20);
        valLbl->setWordWrap(true);
        valLbl->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        valLbl->setStyleSheet("color: #e1e1e5; font-size: 12px;");
        cell->addWidget(k);
        cell->addWidget(valLbl, 1);
        grid->addLayout(cell, r, c);
    };

    addInfoCell(0, 0, "电话:", m_lblPhone);
    addInfoCell(0, 1, "公司:", m_lblCompany);
    addInfoCell(1, 0, "地址:", m_lblAddress);
    addInfoCell(1, 1, "职务:", m_lblTitle);
    addInfoCell(2, 0, "行业:", m_lblIndustry);
    addInfoCell(2, 1, "引荐人:", m_lblIntroducer);

    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    cardLayout->addLayout(grid);

    // 5. 偏好习惯与禁忌 (左：喜好 | 右：禁忌)
    auto* prefTitle = new QLabel("偏好习惯与禁忌", m_summaryCard);
    prefTitle->setMinimumHeight(22);
    prefTitle->setStyleSheet("color: #e1e1e5; font-size: 13px; font-weight: bold; margin-top: 2px;");
    cardLayout->addWidget(prefTitle);

    auto* prefTwoCol = new QHBoxLayout();
    prefTwoCol->setContentsMargins(0, 0, 0, 0);
    prefTwoCol->setSpacing(12);

    m_lblPreferences = new QLabel(m_summaryCard);
    m_lblPreferences->setMinimumHeight(22);
    m_lblPreferences->setTextFormat(Qt::RichText);
    m_lblPreferences->setWordWrap(true);
    m_lblPreferences->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_lblPreferences->setStyleSheet("color: #8e8e93; font-size: 12px; font-weight: bold;");

    m_lblTaboos = new QLabel(m_summaryCard);
    m_lblTaboos->setMinimumHeight(22);
    m_lblTaboos->setTextFormat(Qt::RichText);
    m_lblTaboos->setWordWrap(true);
    m_lblTaboos->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_lblTaboos->setStyleSheet("color: #8e8e93; font-size: 12px; font-weight: bold;");

    prefTwoCol->addWidget(m_lblPreferences, 1);
    prefTwoCol->addWidget(m_lblTaboos, 1);
    cardLayout->addLayout(prefTwoCol);

    m_summaryCard->hide();
}

void RelationGraphWidget::updateSummaryCard(int contactId) {
    if (!m_summaryCard) return;
    if (m_isMiniMode || contactId <= 0) {
        m_summaryCard->hide();
        return;
    }

    auto cs = DatabaseManager::instance().getContactDetails(contactId);
    if (cs.id <= 0) {
        m_summaryCard->hide();
        return;
    }

    // 1. 相识时间
    QDate meet = QDate::fromString(cs.meetDate, "yyyy-MM-dd");
    if (!meet.isValid()) meet = QDate::currentDate().addDays(-2);
    int days = meet.daysTo(QDate::currentDate());
    if (days <= 0) days = 1;
    m_lblMeetDate->setText(QString("相识:%1(至今%2天)")
                           .arg(meet.toString("yyyy年MM月dd日"))
                           .arg(days));

    // 2. 支出 VS 回报 总结
    double net = cs.totalReturn - cs.totalSpent;
    QString netColor = (net >= 0) ? "#00a361" : "#fa5151";
    m_lblSpendVsReturn->setText(
        QString("<span style='color:#8e8e93;'>支出 ¥%1 VS 回报 </span>"
                "<span style='color:#00a361;'>¥%2</span> "
                "<span style='color:#8e8e93;'>总结 </span>"
                "<span style='color:%3;'>%4¥%5</span>")
            .arg(cs.totalSpent, 0, 'f', 2)
            .arg(cs.totalReturn, 0, 'f', 2)
            .arg(netColor)
            .arg(net >= 0 ? "+" : "-")
            .arg(qAbs(net), 0, 'f', 2)
    );

    // 3. 社交互动进度条
    while (m_socialStatsLayout->count() > 0) {
        auto* item = m_socialStatsLayout->takeAt(0);
        if (item->widget()) {
            item->widget()->hide();
            item->widget()->deleteLater();
        }
        delete item;
    }
    auto socialItems = DatabaseManager::instance().getContactSocialBreakdown(cs.id);
    int maxCount = 1;
    for (const auto& si : socialItems) {
        if (si.count > maxCount) maxCount = si.count;
    }
    for (const auto& si : socialItems) {
        auto* row = new QWidget(m_socialStatsContainer);
        row->setMinimumHeight(20);
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 1, 0, 1);
        h->setSpacing(10);

        auto* t = new QLabel(si.category, row);
        t->setMinimumHeight(18);
        t->setStyleSheet("color: #8e8e93; font-size: 12px;");
        t->setFixedWidth(68);

        auto* bar = new QProgressBar(row);
        bar->setFixedHeight(6);
        bar->setTextVisible(false);
        bar->setRange(0, 100);
        bar->setValue(qMin(100, (si.count * 100) / maxCount));
        bar->setStyleSheet(
            "QProgressBar { background-color: #28282b; border: none; border-radius: 3px; }"
            "QProgressBar::chunk { background-color: #0a84ff; border-radius: 3px; }"
        );

        auto* countLabel = new QLabel(QString("%1次").arg(si.count), row);
        countLabel->setMinimumHeight(18);
        countLabel->setStyleSheet("color: #8e8e93; font-size: 12px;");
        countLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        countLabel->setFixedWidth(36);

        h->addWidget(t);
        h->addWidget(bar, 1);
        h->addWidget(countLabel);
        m_socialStatsLayout->addWidget(row);
    }
    m_socialStatsContainer->setVisible(!socialItems.isEmpty());

    // 4. 亲疏 / 能量 / 信用 星级与颜色 (3星下红 / 3-4星黄 / 5星绿)
    auto makeStars = [](int n) {
        QString s;
        for (int i = 0; i < 5; ++i) s += (i < n ? "★" : "☆");
        return s;
    };
    auto starStyle = [](int n) -> QString {
        QString c = "#be782e";
        if (n < 3) c = "#fa5151";
        else if (n >= 5) c = "#00a361";
        return QString("color: %1; font-size: 12px; font-weight: bold;").arg(c);
    };

    m_lblClosenessStars->setText(makeStars(cs.closenessLevel));
    m_lblClosenessStars->setStyleSheet(starStyle(cs.closenessLevel));

    m_lblInfluenceStars->setText(makeStars(cs.influenceLevel));
    m_lblInfluenceStars->setStyleSheet(starStyle(cs.influenceLevel));

    m_lblCreditStars->setText(makeStars(cs.creditRating));
    m_lblCreditStars->setStyleSheet(starStyle(cs.creditRating));

    // 5. 详细资料
    m_lblPhone->setText(cs.phone.isEmpty() ? "未填写" : cs.phone);
    m_lblCompany->setText(cs.company.isEmpty() ? "暂无" : cs.company);
    m_lblAddress->setText(cs.address.isEmpty() ? "暂无" : cs.address);
    m_lblTitle->setText(cs.title.isEmpty() ? "暂无" : cs.title);
    m_lblIndustry->setText(cs.industry.isEmpty() ? "IT" : cs.industry);
    m_lblIntroducer->setText(cs.introducerName.isEmpty() ? "本人直识" : cs.introducerName);

    QString prefText = cs.preferences.trimmed().isEmpty() ? "暂无" : cs.preferences.toHtmlEscaped();
    QString tabooText = cs.taboos.trimmed().isEmpty() ? "暂无" : cs.taboos.toHtmlEscaped();
    m_lblPreferences->setText(
        QString("<p style='margin:0px; color:#8e8e93; font-size:12px; font-weight:bold; line-height:18px;'>"
                "<span style='color:#00a361;'>喜好：</span>%1</p>").arg(prefText)
    );
    if (m_lblTaboos) {
        m_lblTaboos->setText(
            QString("<p style='margin:0px; color:#8e8e93; font-size:12px; font-weight:bold; line-height:18px;'>"
                    "<span style='color:#fa5151;'>禁忌：</span>%1</p>").arg(tabooText)
        );
    }

    // 先显示浮窗并激活内部布局，确保所有动态子控件与自动换行文本完成精确高度计算，避免文字被裁切
    m_summaryCard->show();
    if (m_socialStatsLayout) m_socialStatsLayout->activate();
    if (m_summaryCard->layout()) {
        m_summaryCard->layout()->activate();
        int reqH = qMax(m_summaryCard->sizeHint().height(),
                        m_summaryCard->layout()->totalSizeHint().height());
        m_summaryCard->setFixedHeight(reqH + 6);
    } else {
        m_summaryCard->adjustSize();
    }
    repositionSummaryCard();
    m_summaryCard->raise();
}

void RelationGraphWidget::repositionSummaryCard() {
    if (!m_summaryCard || !m_view) return;
    int x = qMax(16, m_view->width() - m_summaryCard->width() - 20);
    int y = 20;
    m_summaryCard->move(x, y);
}

void RelationGraphWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    repositionSummaryCard();
}

void RelationGraphWidget::setMiniMode(bool mini) {
    m_isMiniMode = mini;
    if (m_headerWidget) {
        m_headerWidget->setVisible(!mini);
    }
    if (m_summaryCard && mini) {
        m_summaryCard->hide();
    }
    if (auto* l = qobject_cast<QVBoxLayout*>(layout())) {
        if (mini) {
            l->setContentsMargins(0, 0, 0, 0);
            l->setSpacing(0);
        } else {
            l->setContentsMargins(10, 10, 10, 10);
            l->setSpacing(10);
        }
    }
}

void RelationGraphWidget::setCenterContactId(int contactId) {
    m_centerContactId = contactId;
    m_highlightedNodeId = 0;
    refreshGraph();
}

void RelationGraphWidget::applyHighlightForNode(int clickedNodeId) {
    m_highlightedNodeId = clickedNodeId;

    // 点击“我”(nodeId == 0) 或在局部中心模式下点击当前中心节点：全部高亮恢复，隐藏右上角总结框
    if (clickedNodeId == 0 || (m_centerContactId > 0 && clickedNodeId == m_centerContactId)) {
        if (m_summaryCard) m_summaryCard->hide();
        for (auto it = m_nodeItems.begin(); it != m_nodeItems.end(); ++it) {
            auto* item = static_cast<ContactNodeItem*>(it.value());
            item->setVisualState(it.key() == clickedNodeId && clickedNodeId == 0, false);
        }
        for (const auto& ev : m_edgeVisuals) {
            if (ev.line) {
                QPen pen(ev.origLineColor, ev.baseWidth, ev.dashed ? Qt::DashLine : Qt::SolidLine);
                ev.line->setPen(pen);
                ev.line->setZValue(0);
            }
            if (ev.label) {
                ev.label->setDefaultTextColor(ev.origTextColor);
                ev.label->setZValue(1);
            }
        }
        return;
    }

    // 点击除自己外的任意联系人 (clickedNodeId > 0)：
    // 1. 右上角弹出该联系人的总结框
    updateSummaryCard(clickedNodeId);

    // 2. 优先高亮该联系人及其有直接关系的节点与连线；无关系的节点和连线变灰、变淡
    QSet<int> relatedNodes = m_adjSet.value(clickedNodeId);
    relatedNodes.insert(clickedNodeId);

    for (auto it = m_nodeItems.begin(); it != m_nodeItems.end(); ++it) {
        int nid = it.key();
        auto* item = static_cast<ContactNodeItem*>(it.value());
        bool isPrimary = (nid == clickedNodeId);
        bool isRelated = relatedNodes.contains(nid);
        item->setVisualState(isPrimary, !isRelated);
    }

    for (const auto& ev : m_edgeVisuals) {
        bool isIncident = (ev.u == clickedNodeId || ev.v == clickedNodeId);
        if (ev.line) {
            if (isIncident) {
                QColor hlColor = (ev.u == 0 || ev.v == 0) ? QColor("#00a361") : ev.origLineColor.lighter(135);
                if (hlColor.red() < 60 && hlColor.green() < 60 && hlColor.blue() < 65) {
                    hlColor = QColor("#00a361");
                }
                QPen pen(hlColor, ev.baseWidth + 0.9, Qt::SolidLine);
                ev.line->setPen(pen);
                ev.line->setZValue(3);
            } else {
                QPen dimPen(QColor(52, 52, 58, 45), 1.0, ev.dashed ? Qt::DashLine : Qt::SolidLine);
                ev.line->setPen(dimPen);
                ev.line->setZValue(0);
            }
        }
        if (ev.label) {
            if (isIncident) {
                ev.label->setDefaultTextColor(QColor("#e1e1e5"));
                ev.label->setZValue(5);
            } else {
                ev.label->setDefaultTextColor(QColor(95, 95, 102, 50));
                ev.label->setZValue(1);
            }
        }
    }
}

void RelationGraphWidget::refreshGraph() {
    m_scene->clear();
    m_nodeItems.clear();
    m_adjSet.clear();
    m_edgeVisuals.clear();

    auto contacts = DatabaseManager::instance().getContacts();
    auto userProf = DatabaseManager::instance().getUserProfile();
    auto edges = DatabaseManager::instance().getRelationshipEdges();

    if (contacts.isEmpty()) {
        auto* text = m_scene->addText("暂无人脉数据，请先添加联系人");
        text->setDefaultTextColor(QColor("#8e8e93"));
        text->setPos(-90, -15);
        return;
    }

    const double nodeRadius = m_isMiniMode ? 21.0 : 34.0;
    const double centerRadius = m_isMiniMode ? 24.0 : 38.0;

    QSet<QString> drawnPairs;
    auto makePairKey = [](int a, int b) {
        return QString("%1_%2").arg(qMin(a, b)).arg(qMax(a, b));
    };

    auto drawEdgeWithLabel = [this](int u, int v, const QPointF& p1, const QPointF& p2,
                                    const QColor& lineColor, const QString& labelText, bool dashed = true) {
        double bw = m_isMiniMode ? 1.1 : 1.3;
        QPen pen(lineColor, bw, dashed ? Qt::DashLine : Qt::SolidLine);
        auto* line = m_scene->addLine(QLineF(p1, p2), pen);
        line->setZValue(0);

        QGraphicsTextItem* t = nullptr;
        QColor textColor("#8e8e93");
        if (!labelText.isEmpty() && !m_isMiniMode) {
            QFont f("Microsoft YaHei UI");
            f.setPixelSize(10);
            t = m_scene->addText(labelText, f);
            t->setDefaultTextColor(textColor);
            QRectF br = t->boundingRect();
            QPointF mid = (p1 + p2) * 0.5;
            t->setPos(mid.x() - br.width() * 0.5, mid.y() - br.height() * 0.5);
            t->setZValue(1);
        }

        m_adjSet[u].insert(v);
        m_adjSet[v].insert(u);
        m_edgeVisuals.append({u, v, line, t, lineColor, textColor, dashed, bw});
    };

    QMap<int, ContactSummary> contactMap;
    for (const auto& c : contacts) {
        contactMap[c.id] = c;
    }

    // 构建真实无向邻接表 (仅包含 relationships 表交叉关联 + introducer_id 真实引荐关联)
    QMap<int, QList<QPair<int, QString>>> adj;
    auto addAdjEdge = [&adj](int u, int v, const QString& label) {
        if (u <= 0 || v <= 0 || u == v) return;
        for (const auto& kv : adj[u]) {
            if (kv.first == v) return;
        }
        adj[u].append({v, label});
        adj[v].append({u, label});
    };

    for (const auto& c : contacts) {
        if (c.introducerId > 0 && contactMap.contains(c.introducerId)) {
            addAdjEdge(c.introducerId, c.id, "引荐");
        }
    }
    for (const auto& e : edges) {
        int u = e["from_id"].toInt();
        int v = e["to_id"].toInt();
        QString rel = e["relation_type"].toString();
        if (contactMap.contains(u) && contactMap.contains(v)) {
            addAdjEdge(u, v, rel);
        }
    }

    QString myName = userProf.name.isEmpty() ? "我" : userProf.name;
    QString myIndustry = userProf.industry.isEmpty() ? "核心" : userProf.industry;

    // =========================================================================
    // 模式 A: 以当前选中的联系人为中心 (关系页右侧拓扑图)
    // 严格只显示真实关联：必含“我”(nodeId=0) + 该联系人在数据库中的真实直接关联人脉，绝不拿我的其他无关好友凑数
    // =========================================================================
    if (m_centerContactId > 0 && contactMap.contains(m_centerContactId)) {
        ContactSummary centerContact = contactMap.value(m_centerContactId);
        int centerId = centerContact.id;

        QMap<int, QPointF> posMap;
        QPointF centerPos(0, 0);
        posMap[centerId] = centerPos;

        auto* centerItem = new ContactNodeItem(
            centerId,
            centerContact.name,
            centerContact.industry.isEmpty() ? "IT" : centerContact.industry,
            centerRadius,
            QColor("#00a361"),
            QColor("#1b3820"),
            true
        );
        centerItem->setPos(centerPos);
        m_scene->addItem(centerItem);
        m_nodeItems[centerId] = centerItem;

        // 收集该联系人的真实直接关联节点：第 0 个固定为“我”(nodeId = 0)，随后为 adj[centerId] 中的真实好友
        struct DirectNeighbor {
            int id;
            QString name;
            QString industry;
            QString relLabel;
            QColor borderColor;
            QColor fillColor;
        };
        QList<DirectNeighbor> realNeighbors;

        // 1. “我”与该联系人的关系
        realNeighbors.append({
            0,
            myName,
            myIndustry,
            centerContact.relationType.isEmpty() ? "朋友" : centerContact.relationType,
            QColor("#00a361"),
            QColor("#1c2d22")
        });

        // 2. 该联系人在数据库中的真实直接关联人脉
        for (const auto& nb : adj.value(centerId)) {
            int nbId = nb.first;
            if (!contactMap.contains(nbId)) continue;
            const auto& cNb = contactMap[nbId];
            realNeighbors.append({
                nbId,
                cNb.name,
                cNb.industry.isEmpty() ? "IT" : cNb.industry,
                nb.second.isEmpty() ? "互识" : nb.second,
                QColor("#fa5151"),
                QColor("#1c1c1e")
            });
        }

        double minGapA = m_isMiniMode ? 10.0 : 20.0;
        int n1 = realNeighbors.size();
        double minChordA = 2.0 * nodeRadius + minGapA + 6.0;
        double r1 = m_isMiniMode ? 75.0 : qMax(180.0, (n1 * minChordA) / (2.0 * M_PI));
        for (int i = 0; i < n1; ++i) {
            const auto& nb = realNeighbors[i];
            double angle = (n1 == 1)
                               ? 0.0
                               : (-M_PI / 2.0 + (2.0 * M_PI * i) / n1);
            QPointF p1 = centerPos + QPointF(r1 * qCos(angle), r1 * qSin(angle));
            posMap[nb.id] = p1;
        }

        // 执行防重叠与空隙保证松弛
        resolveNodeCollisions(posMap, centerId, centerRadius, nodeRadius, minGapA);

        for (int i = 0; i < n1; ++i) {
            const auto& nb = realNeighbors[i];
            QPointF p1 = posMap[nb.id];

            auto* item1 = new ContactNodeItem(
                nb.id,
                nb.name,
                nb.industry,
                nodeRadius,
                nb.borderColor,
                nb.fillColor,
                false
            );
            item1->setPos(p1);
            m_scene->addItem(item1);
            m_nodeItems[nb.id] = item1;

            drawEdgeWithLabel(centerId, nb.id, centerPos, p1, QColor("#3a3a40"), nb.relLabel, true);
            drawnPairs.insert(makePairKey(centerId, nb.id));
        }

        // 3. 绘制这些真实邻居之间实际存在的交叉认识连线
        QList<int> allVisibleIds = posMap.keys();
        for (int i = 0; i < allVisibleIds.size(); ++i) {
            for (int j = i + 1; j < allVisibleIds.size(); ++j) {
                int u = allVisibleIds[i];
                int v = allVisibleIds[j];
                QString pk = makePairKey(u, v);
                if (drawnPairs.contains(pk)) continue;

                if (u == 0 && v > 0) {
                    // 如果该直接关联人也是“我”通讯录里的联系人，绘制其与“我”的连线
                    drawEdgeWithLabel(0, v, posMap[0], posMap[v], QColor("#2e2e32"), contactMap[v].relationType, true);
                    drawnPairs.insert(pk);
                } else if (v == 0 && u > 0) {
                    drawEdgeWithLabel(0, u, posMap[0], posMap[u], QColor("#2e2e32"), contactMap[u].relationType, true);
                    drawnPairs.insert(pk);
                } else {
                    for (const auto& kv : adj.value(u)) {
                        if (kv.first == v) {
                            drawEdgeWithLabel(u, v, posMap[u], posMap[v], QColor("#3a3a40"), kv.second, true);
                            drawnPairs.insert(pk);
                            break;
                        }
                    }
                }
            }
        }

        QRectF bounds = m_scene->itemsBoundingRect().adjusted(-35, -35, 35, 35);
        m_view->setSceneRect(bounds);
        m_view->centerOn(bounds.center());
        return;
    }

    // =========================================================================
    // 模式 B: 全局关系网 (以“我”为中心 + 一级核心人脉圈 + 二级引荐分支圈 + 全局交叉互识连线)
    // 支持点击任意联系人高亮其直接关系网、无关节点与连线变灰变淡；点击“我”全部高亮
    // =========================================================================
    QPointF myPos(0, 0);
    auto* myNode = new ContactNodeItem(
        0,
        myName,
        myIndustry,
        centerRadius,
        QColor("#00a361"),
        QColor("#1b3820"),
        true
    );
    myNode->setPos(myPos);
    m_scene->addItem(myNode);
    m_nodeItems[0] = myNode;

    QList<ContactSummary> tier1Contacts;
    QList<ContactSummary> tier2Contacts;
    for (const auto& c : contacts) {
        if (c.introducerId > 0 && contactMap.contains(c.introducerId)) {
            tier2Contacts.append(c);
        } else {
            tier1Contacts.append(c);
        }
    }

    QMap<int, QPointF> nodePositions;
    QMap<int, double> nodeAngles;
    nodePositions[0] = myPos;

    const double minGapB = m_isMiniMode ? 14.0 : 24.0;

    // 1. 布局一级核心人脉 (内圈)
    int n1 = tier1Contacts.size();
    double minChord1 = 2.0 * nodeRadius + minGapB + 6.0;
    double r1 = m_isMiniMode ? 95.0 : qMax(220.0, (n1 * minChord1) / (2.0 * M_PI));
    for (int i = 0; i < n1; ++i) {
        const auto& c = tier1Contacts[i];
        double angle = -M_PI / 2.0 + (2.0 * M_PI * i) / qMax(1, n1);
        QPointF pos(r1 * qCos(angle), r1 * qSin(angle));
        nodePositions[c.id] = pos;
        nodeAngles[c.id] = angle;
    }

    // 2. 布局二级引荐/外延人脉 (外圈，围绕各自引荐人扇形展开)
    double r2 = r1 + 2.0 * nodeRadius + minGapB + 55.0;
    QMap<int, int> introChildCount;
    QMap<int, int> introTotalChildren;
    for (const auto& c : tier2Contacts) {
        introTotalChildren[c.introducerId]++;
    }

    for (int i = 0; i < tier2Contacts.size(); ++i) {
        const auto& c = tier2Contacts[i];
        int introId = c.introducerId;
        int childIdx = introChildCount[introId]++;
        int totalChildren = introTotalChildren.value(introId, 1);
        double baseAngle = nodeAngles.value(introId, -M_PI / 3.0 + i * 0.8);

        double angleStep = (2.0 * nodeRadius + minGapB + 6.0) / r2;
        double offset = (totalChildren <= 1) ? 0.0 : ((childIdx - (totalChildren - 1) / 2.0) * angleStep);
        double angle = baseAngle + offset;

        QPointF pos(r2 * qCos(angle), r2 * qSin(angle));
        nodePositions[c.id] = pos;
        nodeAngles[c.id] = angle;
    }

    // 执行防重叠与空隙保证算法：消除所有圆圈之间的重叠，确保每个圆与圆之间有至少 minGapB 的清晰空隙
    resolveNodeCollisions(nodePositions, 0, centerRadius, nodeRadius, minGapB);

    // 3. 渲染所有节点 (使用已消除重叠并保留清晰空隙的物理坐标)
    myNode->setPos(nodePositions[0]);

    for (int i = 0; i < n1; ++i) {
        const auto& c = tier1Contacts[i];
        QPointF pos = nodePositions[c.id];
        QColor borderColor = (c.closenessLevel >= 5) ? QColor("#fa5151") :
                             ((c.closenessLevel == 4) ? QColor("#be782e") : QColor("#00a361"));

        auto* item = new ContactNodeItem(
            c.id,
            c.name,
            c.industry.isEmpty() ? c.relationType : c.industry,
            nodeRadius,
            borderColor,
            QColor("#1c1c1e"),
            false
        );
        item->setPos(pos);
        m_scene->addItem(item);
        m_nodeItems[c.id] = item;

        drawEdgeWithLabel(0, c.id, nodePositions[0], pos, QColor("#2e2e32"), c.relationType, true);
        drawnPairs.insert(makePairKey(0, c.id));
    }

    for (int i = 0; i < tier2Contacts.size(); ++i) {
        const auto& c = tier2Contacts[i];
        int introId = c.introducerId;
        QPointF pos = nodePositions[c.id];

        QColor borderColor = (c.closenessLevel >= 4) ? QColor("#be782e") : QColor("#fa5151");
        auto* item = new ContactNodeItem(
            c.id,
            c.name,
            c.industry.isEmpty() ? c.relationType : c.industry,
            nodeRadius,
            borderColor,
            QColor("#1c1c1e"),
            false
        );
        item->setPos(pos);
        m_scene->addItem(item);
        m_nodeItems[c.id] = item;

        // 引荐连线
        if (nodePositions.contains(introId)) {
            drawEdgeWithLabel(introId, c.id, nodePositions[introId], pos, QColor("#be782e"), "引荐", false);
            drawnPairs.insert(makePairKey(introId, c.id));
        }
        // 同时与“我”保持虚线连接
        drawEdgeWithLabel(0, c.id, nodePositions[0], pos, QColor("#232327"), "", true);
        drawnPairs.insert(makePairKey(0, c.id));
    }

    // 4. 绘制所有人脉之间的交叉互识连线 (relationships 表)
    for (const auto& edge : edges) {
        int u = edge["from_id"].toInt();
        int v = edge["to_id"].toInt();
        QString relType = edge["relation_type"].toString();
        if (nodePositions.contains(u) && nodePositions.contains(v)) {
            QString pk = makePairKey(u, v);
            if (!drawnPairs.contains(pk)) {
                drawEdgeWithLabel(u, v, nodePositions[u], nodePositions[v], QColor("#00a361"), relType, false);
                drawnPairs.insert(pk);
            }
        }
    }

    if (m_highlightedNodeId > 0 && m_nodeItems.contains(m_highlightedNodeId)) {
        applyHighlightForNode(m_highlightedNodeId);
    } else if (m_summaryCard) {
        m_summaryCard->hide();
    }

    m_view->setSceneRect(m_scene->itemsBoundingRect().adjusted(-120, -120, 120, 120));
    m_view->centerOn(myPos);
}
