#include "BusinessNetWidget.h"
#include "CompanyDialog.h"
#include "CustomDialog.h"
#include "UiIcons.h"
#include "../core/DatabaseManager.h"
#include "../ai/OllamaHttpEngine.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>
#include <QFileInfo>
#include <QDir>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QGraphicsPathItem>
#include <QRegularExpression>
#include <QScrollBar>
#include <QMenu>
#include <QSqlQuery>
#include <QSqlError>
#include <QtMath>
#include <QDateTime>
#include <algorithm>

namespace {

// ==================== 1. 自定义圆形拓扑节点 ====================
class BusinessNodeItem : public QGraphicsItem {
public:
    enum { Type = UserType + 201 };

    BusinessNodeItem(int level, int nodeId, const QString& title, const QString& subText, double radius, const QColor& themeColor)
        : m_level(level), m_nodeId(nodeId), m_title(title), m_subText(subText), m_radius(radius), m_themeColor(themeColor) {
        setZValue(level == 0 ? 10 : (level == 1 ? 7 : (level == 2 ? 5 : 3)));
        setAcceptedMouseButtons(Qt::LeftButton);
    }

    int type() const override { return Type; }
    int level() const { return m_level; }
    int nodeId() const { return m_nodeId; }
    QString title() const { return m_title; }

    void setVisualState(bool isFocus, bool isHighlighted, bool isDimmed) {
        m_isFocus = isFocus;
        m_isHighlighted = isHighlighted;
        m_isDimmed = isDimmed;

        if (m_isFocus) {
            setZValue(25);
        } else if (m_isHighlighted) {
            setZValue(m_level == 0 ? 20 : (m_level == 1 ? 16 : 12));
        } else if (m_isDimmed) {
            setZValue(1);
        } else {
            setZValue(m_level == 0 ? 10 : (m_level == 1 ? 7 : 4));
        }
        update();
    }

    QRectF boundingRect() const override {
        return QRectF(-m_radius - 8, -m_radius - 8, (m_radius + 8) * 2, (m_radius + 8) * 2);
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::TextAntialiasing, true);

        QRectF circleRect(-m_radius, -m_radius, m_radius * 2, m_radius * 2);

        QColor borderColor = m_themeColor;
        QColor fillColor;
        QColor titleColor("#ffffff");
        QColor subColor("#8e8e93");
        double penWidth = 1.8;

        if (m_level == 0) {
            // 中心公司主体 (深墨绿底色 + 荧光青绿描边)
            fillColor = QColor(14, 46, 34);
            borderColor = QColor(0, 214, 143);
            penWidth = 2.8;
            subColor = QColor(0, 214, 143);
            titleColor = QColor("#ffffff");
        } else if (m_level == 1) {
            // Level 1 员工：根据职务/角色专属色彩高亮，法人主体专属尊贵金黄，白字 + 鲜明副字
            bool isRep = m_subText.contains("法人") || m_subText.contains("法定");
            if (isRep) {
                borderColor = QColor("#febc2f");
                fillColor = QColor(64, 46, 12, 245);
                penWidth = 2.4;
                titleColor = QColor("#ffffff");
                subColor = QColor("#ffd166");
            } else {
                borderColor = m_themeColor;
                fillColor = m_themeColor.darker(350);
                fillColor.setAlpha(245);
                penWidth = 2.0;
                titleColor = QColor("#ffffff");
                subColor = m_themeColor.lighter(125);
            }
        } else {
            // Level >= 2: 依据所属关系类型色彩系统 (合伙=金黄，亲戚=粉红，客户=翡翠绿，朋友=天蓝，引荐=橙色等)
            borderColor = m_themeColor;
            fillColor = m_themeColor.darker(380);
            fillColor.setAlpha(245);
            penWidth = 1.6;
            titleColor = m_themeColor.lighter(135);
            subColor = m_themeColor.lighter(105);
        }

        if (m_isDimmed) {
            fillColor = QColor(20, 20, 24, 60);
            borderColor = QColor(60, 60, 68, 50);
            titleColor = QColor(100, 100, 105, 60);
            subColor = QColor(70, 70, 75, 50);
            penWidth = 1.0;
        } else if (m_isFocus) {
            QRectF glowRect = circleRect.adjusted(-4.0, -4.0, 4.0, 4.0);
            QColor glow = borderColor;
            glow.setAlpha(125);
            painter->setPen(QPen(glow, 3.2));
            painter->setBrush(Qt::NoBrush);
            painter->drawEllipse(glowRect);

            borderColor = borderColor.lighter(130);
            fillColor = fillColor.lighter(130);
        } else if (m_isHighlighted) {
            borderColor = borderColor.lighter(120);
        }

        // 绘制主体圆形
        painter->setPen(QPen(borderColor, penWidth));
        painter->setBrush(fillColor);
        painter->drawEllipse(circleRect);

        // 绘制圆内文字 (100% 绝对正立，左右垂直居中)
        if (m_level == 0) {
            // 公司主体：多行自动折行
            QFont tf("Microsoft YaHei UI", 10, QFont::Bold);
            painter->setFont(tf);
            painter->setPen(titleColor);

            QString cleanName = m_title;
            QStringList lines;
            int step = 4;
            if (cleanName.length() > 8) step = 5;
            for (int i = 0; i < cleanName.length(); i += step) {
                lines.append(cleanName.mid(i, step));
                if (lines.size() >= 3) break;
            }
            QString wrapName = lines.join("\n");
            painter->drawText(circleRect.adjusted(2, 2, -2, -2), Qt::AlignCenter, wrapName);
        } else {
            // 人脉节点：名字在上半部，行业/职位在下半部，绝对正立
            QFont nameFont("Microsoft YaHei UI", 10, QFont::Bold);
            painter->setFont(nameFont);
            painter->setPen(titleColor);
            QRectF topRect(-m_radius, -m_radius * 0.65, m_radius * 2, m_radius * 0.65);
            painter->drawText(topRect, Qt::AlignCenter, m_title);

            QFont subFont("Microsoft YaHei UI", 8);
            painter->setFont(subFont);
            painter->setPen(subColor);
            QString shortSub = m_subText;
            if (shortSub.length() > 5) shortSub = shortSub.left(4) + "…";
            QRectF btmRect(-m_radius, 1, m_radius * 2, m_radius * 0.65);
            painter->drawText(btmRect, Qt::AlignCenter, shortSub);
        }

        painter->restore();
    }

private:
    int m_level{0};
    int m_nodeId{0};
    QString m_title;
    QString m_subText;
    double m_radius{28.0};
    QColor m_themeColor;
    bool m_isFocus{false};
    bool m_isHighlighted{false};
    bool m_isDimmed{false};
};

// 人脉关系语义色彩映射系统：打造丰富清晰的视觉层次感
QColor getRelationThemeColor(const QString& relType, int level = 2) {
    QString r = relType.trimmed();
    // 1. 法人 / 合伙 / 投资 / 股东 (尊贵琥珀金黄 #febc2f)
    if (r.contains("法人") || r.contains("法定") || r.contains("合伙") || r.contains("投资") || r.contains("股东") || r.contains("董事") || r.contains("资本")) {
        return QColor("#febc2f");
    }
    // 2. 亲戚 / 亲属 / 家人 / 配偶 (温润玫瑰粉红 #ff6584)
    if (r.contains("亲戚") || r.contains("亲属") || r.contains("家人") || r.contains("配偶") ||
        r.contains("父母") || r.contains("兄") || r.contains("弟") || r.contains("姐") || r.contains("妹") || r.contains("子女") || r.contains("亲情")) {
        return QColor("#ff6584");
    }
    // 3. 客户 / 合作 / 商业 / 业务 / 贸易 (清爽明亮翡翠绿 #00d68f)
    if (r.contains("客户") || r.contains("合作") || r.contains("商业") || r.contains("业务") || r.contains("订单") || r.contains("采购")) {
        return QColor("#00d68f");
    }
    // 4. 引荐 / 推荐 / 中介 / 纽带 (鲜活温暖珊瑚橙 #ff9f43)
    if (r.contains("引荐") || r.contains("推荐") || r.contains("居间") || r.contains("介绍") || r.contains("纽带")) {
        return QColor("#ff9f43");
    }
    // 5. 朋友 / 同学 / 战友 / 发小 / 闺蜜 / 知己 (清新天青蓝 #38bdf8)
    if (r.contains("朋友") || r.contains("同学") || r.contains("战友") || r.contains("发小") || r.contains("闺蜜") || r.contains("知己") || r.contains("校友")) {
        return QColor("#38bdf8");
    }
    // 6. 政务 / 领导 / 导师 / 协会 (庄重紫罗兰 #a55eea)
    if (r.contains("政务") || r.contains("领导") || r.contains("导师") || r.contains("顾问") || r.contains("协会") || r.contains("会长")) {
        return QColor("#a55eea");
    }
    // 7. 同事 / 员工 / 下属 (科技靓蓝 #54a0ff)
    if (r.contains("同事") || r.contains("员工") || r.contains("下属") || r.contains("上级") || r.contains("助理")) {
        return QColor("#54a0ff");
    }
    // 8. 借贷 / 债务 / 担保 (警戒红 #fa5151)
    if (r.contains("借贷") || r.contains("债务") || r.contains("还款") || r.contains("担保") || r.contains("债权")) {
        return QColor("#fa5151");
    }
    // 9. 邻居 / 供应商 / 普通熟人 (优雅灰蓝 #94a3b8)
    if (r.contains("邻居") || r.contains("供应商") || r.contains("熟人")) {
        return QColor("#94a3b8");
    }

    // 默认兜底按层级区分渐变，富有层次
    if (level == 1) return QColor("#00d68f");
    if (level == 2) return QColor("#38bdf8");
    if (level == 3) return QColor("#ff9f43");
    return QColor("#94a3b8");
}

void createGraphEdge(
    QGraphicsScene* scene,
    const QPointF& p1, double r1,
    const QPointF& p2, double r2,
    const QString& labelText,
    const QColor& color,
    bool showArrow,
    int fromLvl, int fromId,
    int toLvl, int toId,
    QList<BusinessNetWidget::VisualEdge>& edgeList,
    bool isDashed = false
) {
    QPointF delta = p2 - p1;
    double dist = std::hypot(delta.x(), delta.y());
    if (dist < 10.0) return;

    QPointF u = delta / dist;
    QPointF startPt = p1 + u * r1;
    QPointF endPt = p2 - u * (r2 + (showArrow ? 2.5 : 0.0));

    QPainterPath path;
    path.moveTo(startPt);
    path.lineTo(endPt);

    if (showArrow) {
        double angle = std::atan2(delta.y(), delta.x());
        double arrowLen = 9.5;
        double arrowHalfAngle = 0.40;
        QPointF w1 = endPt - QPointF(arrowLen * std::cos(angle - arrowHalfAngle), arrowLen * std::sin(angle - arrowHalfAngle));
        QPointF w2 = endPt - QPointF(arrowLen * std::cos(angle + arrowHalfAngle), arrowLen * std::sin(angle + arrowHalfAngle));

        QPolygonF arrowHead;
        arrowHead << endPt << w1 << w2 << endPt;
        path.addPolygon(arrowHead);
    }

    QPen linePen(color, isDashed ? 1.2 : 1.6, isDashed ? Qt::DashLine : Qt::SolidLine);
    auto* pathItem = scene->addPath(path, linePen, QBrush(color));
    pathItem->setZValue(isDashed ? 1 : 2);

    QGraphicsItemGroup* labelGroup = nullptr;
    if (!labelText.trimmed().isEmpty()) {
        labelGroup = new QGraphicsItemGroup();

        auto* text = new QGraphicsSimpleTextItem(labelText.trimmed(), labelGroup);
        QFont f("Microsoft YaHei UI", 8);
        f.setBold(true);
        text->setFont(f);
        text->setBrush(color.lighter(135));

        QRectF tr = text->boundingRect();
        double padX = 5.0;
        double padY = 2.0;
        QRectF bgRect(-padX, -padY, tr.width() + padX * 2.0, tr.height() + padY * 2.0);

        QPainterPath pillPath;
        pillPath.addRoundedRect(bgRect, 4.0, 4.0);

        // 半透明暗色胶囊背景防穿透，带与关系同色系的柔和微边框
        QColor pillBg(16, 17, 21, 235);
        QColor pillBorder(color.red(), color.green(), color.blue(), 130);
        auto* bgItem = new QGraphicsPathItem(pillPath, labelGroup);
        bgItem->setBrush(QBrush(pillBg));
        bgItem->setPen(QPen(pillBorder, 1.0));
        bgItem->setZValue(0);
        text->setZValue(1);

        QPointF midPt = (startPt + endPt) * 0.5;
        labelGroup->setPos(midPt.x() - tr.width() * 0.5, midPt.y() - tr.height() * 0.5);
        labelGroup->setZValue(isDashed ? 3 : 4);
        scene->addItem(labelGroup);
    }

    edgeList.append({fromLvl, fromId, toLvl, toId, pathItem, labelGroup, color, isDashed});
}

} // namespace

// ==================== 2. 交互式画布视图 (滚轮/漫游/点击) ====================
class InteractiveBusinessGraphView : public QGraphicsView {
public:
    explicit InteractiveBusinessGraphView(QGraphicsScene* scene, QWidget* parent = nullptr)
        : QGraphicsView(scene, parent) {
        setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        setResizeAnchor(QGraphicsView::AnchorViewCenter);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);
        setCursor(Qt::OpenHandCursor);
    }

    void setNodeClickCallback(std::function<void(int, int)> cb) {
        m_onNodeClicked = std::move(cb);
    }

    void setCanvasClickCallback(std::function<void()> cb) {
        m_onCanvasClicked = std::move(cb);
    }

    void resetZoom() {
        resetTransform();
        m_currentScale = 1.0;
    }

    void zoomIn() {
        if (m_currentScale < 3.5) {
            scale(1.15, 1.15);
            m_currentScale *= 1.15;
        }
    }

    void zoomOut() {
        if (m_currentScale > 0.35) {
            scale(1.0 / 1.15, 1.0 / 1.15);
            m_currentScale /= 1.15;
        }
    }

protected:
    void wheelEvent(QWheelEvent* event) override {
        const double factor = 1.15;
        if (event->angleDelta().y() > 0) {
            if (m_currentScale < 3.5) {
                scale(factor, factor);
                m_currentScale *= factor;
            }
        } else if (event->angleDelta().y() < 0) {
            if (m_currentScale > 0.35) {
                scale(1.0 / factor, 1.0 / factor);
                m_currentScale /= factor;
            }
        }
        event->accept();
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
        if (hit && hit->type() == BusinessNodeItem::Type) {
            setCursor(Qt::PointingHandCursor);
        } else {
            setCursor(Qt::OpenHandCursor);
        }
        QGraphicsView::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && m_isPanning) {
            m_isPanning = false;
            setCursor(Qt::OpenHandCursor);
            if (!m_didDrag) {
                QGraphicsItem* hit = itemAt(event->pos());
                if (hit && hit->type() == BusinessNodeItem::Type) {
                    auto* node = static_cast<BusinessNodeItem*>(hit);
                    if (m_onNodeClicked) {
                        m_onNodeClicked(node->level(), node->nodeId());
                    }
                } else {
                    if (m_onCanvasClicked) {
                        m_onCanvasClicked();
                    }
                }
            }
            event->accept();
            return;
        }
        QGraphicsView::mouseReleaseEvent(event);
    }

private:
    double m_currentScale{1.0};
    bool m_isPanning{false};
    bool m_didDrag{false};
    QPoint m_pressPos;
    QPoint m_lastPanPos;
    std::function<void(int, int)> m_onNodeClicked;
    std::function<void()> m_onCanvasClicked;
};

// ==================== 3. BusinessNetWidget 主界面 ====================
BusinessNetWidget::BusinessNetWidget(QWidget* parent) : QWidget(parent) {
    setupUi();

    m_throttleRenderTimer = new QTimer(this);
    m_throttleRenderTimer->setInterval(60); // 60ms 批处理更新防抖 (约16fps，彻底根除高频 setHtml 导致的 UI 卡顿)
    connect(m_throttleRenderTimer, &QTimer::timeout, this, [this]() {
        if (m_renderDirty) {
            m_renderDirty = false;
            renderFullConversation();
        }
    });

    // 监听 AI 流式响应
    connect(&AiAdvisorService::instance(), &AiAdvisorService::tokenReceived, this, [this](const QString& token) {
        if (m_isAiGenerating) {
            m_accumulatedResponse += token;
            m_renderDirty = true;
            if (m_throttleRenderTimer && !m_throttleRenderTimer->isActive()) {
                m_throttleRenderTimer->start();
            }
        }
    });

    connect(&AiAdvisorService::instance(), &AiAdvisorService::evaluationFinished, this, [this](const QString& fullText) {
        if (m_isAiGenerating) {
            if (m_throttleRenderTimer) m_throttleRenderTimer->stop();
            m_renderDirty = false;
            m_isAiGenerating = false;
            m_accumulatedResponse = fullText;
            m_aiHistory.append({"assistant", fullText});

            auto engine = std::dynamic_pointer_cast<OllamaHttpEngine>(AiAdvisorService::instance().engine());
            m_isLastResponseIncomplete = engine ? engine->isLastResponseIncomplete() : false;

            renderFullConversation();

            if (m_btnSendQuestion) {
                m_btnSendQuestion->setText("发送");
                m_btnSendQuestion->setEnabled(true);
            }
            if (m_aiQuestionEdit) m_aiQuestionEdit->setEnabled(true);
            if (m_btnTopGenerate) {
                m_btnTopGenerate->setEnabled(true);
                m_btnTopGenerate->setText("重新生成");
            }

            // 核心要求 2：仅在点击“生成/重新生成”时覆盖更新 {公司ID}.json 文件，后续继续回复不保存
            if (m_isInitialCompanyGeneration) {
                saveCurrentCompanyAiCache();
                m_isInitialCompanyGeneration = false;
            }
        }
    });

    connect(&AiAdvisorService::instance(), &AiAdvisorService::evaluationFailed, this, [this](const QString& err) {
        if (m_isAiGenerating) {
            if (m_throttleRenderTimer) m_throttleRenderTimer->stop();
            m_renderDirty = false;
            m_isAiGenerating = false;
            if (m_txtAiReview) {
                m_txtAiReview->setHtml(QString("<div style='color:#fa5151; font-size:14px; line-height:22px; padding:4px;'><b>AI 生成失败：</b>%1</div>").arg(err.toHtmlEscaped()));
            }
            if (m_btnSendQuestion) {
                m_btnSendQuestion->setText("发送");
                m_btnSendQuestion->setEnabled(true);
            }
            if (m_aiQuestionEdit) m_aiQuestionEdit->setEnabled(true);
            if (m_btnTopGenerate) {
                m_btnTopGenerate->setEnabled(true);
                m_btnTopGenerate->setText("生成");
            }
        }
    });
}

void BusinessNetWidget::setupUi() {
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(1);
    splitter->setStyleSheet("QSplitter::handle { background-color: #28282b; }");

    // =========================================================================
    // 1. 左侧面板：搜索公司 + 公司列表 (公司名称 + 右对齐 2位)
    // =========================================================================
    auto* leftWidget = new QWidget(splitter);
    leftWidget->setMinimumWidth(230);
    leftWidget->setMaximumWidth(280);
    leftWidget->setStyleSheet("background-color: #141416;");

    auto* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(10, 10, 10, 10);
    leftLayout->setSpacing(8);

    m_searchEdit = new QLineEdit(leftWidget);
    m_searchEdit->setPlaceholderText("搜索公司");
    m_searchEdit->setStyleSheet(
        "QLineEdit { background-color: #1a1a1d; color: #e1e1e5; border: 1px solid #28282b; border-radius: 4px; padding: 6px 10px; font-size: 12px; }"
        "QLineEdit:focus { border-color: #00a361; }"
    );
    connect(m_searchEdit, &QLineEdit::textChanged, this, &BusinessNetWidget::onCompanySearchChanged);
    leftLayout->addWidget(m_searchEdit);

    m_companyList = new QListWidget(leftWidget);
    m_companyList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_companyList->setStyleSheet(
        "QListWidget { background-color: transparent; border: none; outline: none; padding: 0px; }"
        "QListWidget::item { color: #e1e1e5; height: 38px; border-radius: 4px; margin-bottom: 3px; padding: 0px; background: transparent; }"
        "QListWidget::item:hover { background: transparent; }"
        "QListWidget::item:selected { background: transparent; }"
    );
    connect(m_companyList, &QListWidget::itemClicked, this, &BusinessNetWidget::onCompanyItemClicked);
    connect(m_companyList, &QListWidget::currentItemChanged, this, [this](QListWidgetItem*, QListWidgetItem*) {
        updateCompanyListSelectionVisuals();
    });
    connect(m_companyList, &QListWidget::customContextMenuRequested, this, &BusinessNetWidget::onCompanyContextMenu);
    leftLayout->addWidget(m_companyList, 1);

    splitter->addWidget(leftWidget);

    // =========================================================================
    // 2. 中间面板：公司拓扑图 (以公司为绝对中心 -> 员工 -> 朋友 -> 朋友的朋友 无限多层)
    // =========================================================================
    auto* centerWidget = new QWidget(splitter);
    centerWidget->setStyleSheet("background-color: #0f0f11;");
    auto* centerLayout = new QVBoxLayout(centerWidget);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(0);

    m_graphScene = new QGraphicsScene(this);
    m_graphView = new InteractiveBusinessGraphView(m_graphScene, centerWidget);
    m_graphView->setStyleSheet("background-color: #0b0b0d; border: none;");
    m_graphView->setNodeClickCallback([this](int level, int id) {
        handleNodeClicked(level, id);
    });
    m_graphView->setCanvasClickCallback([this]() {
        clearNodeHighlight();
    });

    centerLayout->addWidget(m_graphView, 1);
    splitter->addWidget(centerWidget);

    // =========================================================================
    // 3. 右侧面板：AI点评 (标题 + 右上角“生成” + 报告卡片 + 至少5行可见咨询输入框)
    // =========================================================================
    auto* rightWidget = new QWidget(splitter);
    rightWidget->setMinimumWidth(320);
    rightWidget->setMaximumWidth(400);
    rightWidget->setStyleSheet("background-color: #141416;");

    auto* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(12, 10, 12, 12);
    rightLayout->setSpacing(10);

    // 顶部标题栏：左侧“AI点评”，右侧绿色无边框“生成” (对齐联系人详情 UI)
    auto* topHeaderRow = new QHBoxLayout();
    topHeaderRow->setContentsMargins(0, 0, 0, 0);

    m_aiTopTitle = new QLabel("AI点评", rightWidget);
    m_aiTopTitle->setStyleSheet("color: #e1e1e5; font-size: 13px; font-weight: bold;");
    topHeaderRow->addWidget(m_aiTopTitle);

    topHeaderRow->addStretch();

    m_btnTopGenerate = new QPushButton("生成", rightWidget);
    m_btnTopGenerate->setCursor(Qt::PointingHandCursor);
    m_btnTopGenerate->setStyleSheet(
        "QPushButton { background: transparent; color: #00a361; border: none; font-size: 12px; font-weight: bold; padding: 2px 4px; }"
        "QPushButton:hover { color: #00c474; }"
        "QPushButton:disabled { color: #55555c; }"
    );
    connect(m_btnTopGenerate, &QPushButton::clicked, this, &BusinessNetWidget::onGenerateAiClicked);
    topHeaderRow->addWidget(m_btnTopGenerate);

    rightLayout->addLayout(topHeaderRow);

    m_txtAiReview = new QTextBrowser(rightWidget);
    m_txtAiReview->setOpenExternalLinks(false);
    m_txtAiReview->setFrameShape(QFrame::NoFrame);
    m_txtAiReview->setStyleSheet(
        "QTextBrowser { background-color: #2f2f30; color: #e1e1e5; border: none; border-radius: 5px; font-size: 14px; padding: 12px 14px; line-height: 24px; }"
    );
    connect(m_txtAiReview, &QTextBrowser::anchorClicked, this, [this](const QUrl& url) {
        if (url.toString() == "action:continue") {
            if (!m_isAiGenerating && m_aiQuestionEdit) {
                m_aiQuestionEdit->setPlainText("请承接你上面未阐述完毕的内容继续往下深入剖析，直接输出后续内容，条理清晰。");
                onSendConsultQuestion();
            }
        }
    });
    rightLayout->addWidget(m_txtAiReview, 1);

    // 底部输入栏：一体化圆角卡片 (内嵌输入区 + 右下角发送按键，严格对齐用户UI)
    auto* bottomInputContainer = new QWidget(rightWidget);
    bottomInputContainer->setObjectName("bottomInputCard");
    bottomInputContainer->setStyleSheet(
        "QWidget#bottomInputCard { background-color: #17171a; border: 1px solid #28282e; border-radius: 6px; }"
        "QWidget#bottomInputCard:focus-within { border-color: #383842; }"
    );
    auto* bottomInputLayout = new QVBoxLayout(bottomInputContainer);
    bottomInputLayout->setContentsMargins(12, 10, 12, 10);
    bottomInputLayout->setSpacing(6);

    m_aiQuestionEdit = new QPlainTextEdit(bottomInputContainer);
    m_aiQuestionEdit->setPlaceholderText("请输入AI咨询....");
    m_aiQuestionEdit->setMinimumHeight(96); // 保证至少 5~6 行高度可见
    m_aiQuestionEdit->setMaximumHeight(130);
    m_aiQuestionEdit->installEventFilter(this);
    m_aiQuestionEdit->setStyleSheet(
        "QPlainTextEdit { background: transparent; color: #ffffff; border: none; font-size: 13px; line-height: 1.5; padding: 0px; selection-background-color: #00a361; }"
    );
    bottomInputLayout->addWidget(m_aiQuestionEdit);

    auto* btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(0, 0, 0, 0);
    btnRow->addStretch();

    m_btnSendQuestion = new QPushButton("发送", bottomInputContainer);
    m_btnSendQuestion->setCursor(Qt::PointingHandCursor);
    m_btnSendQuestion->setFixedSize(76, 30);
    m_btnSendQuestion->setStyleSheet(
        "QPushButton { background-color: #2f2f35; color: #ffffff; border: 1px solid #3c3c44; border-radius: 4px; font-size: 12px; font-weight: 500; padding: 0px; }"
        "QPushButton:hover { background-color: #00a361; border-color: #00a361; color: #ffffff; }"
        "QPushButton:disabled { background-color: #202024; border-color: #2a2a30; color: #62626a; }"
    );
    connect(m_btnSendQuestion, &QPushButton::clicked, this, &BusinessNetWidget::onSendConsultQuestion);
    btnRow->addWidget(m_btnSendQuestion);

    bottomInputLayout->addLayout(btnRow);
    rightLayout->addWidget(bottomInputContainer);

    splitter->addWidget(rightWidget);

    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 6);
    splitter->setStretchFactor(2, 4);

    mainLayout->addWidget(splitter);
}

bool BusinessNetWidget::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_aiQuestionEdit && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            if (!(keyEvent->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier))) {
                onSendConsultQuestion();
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void BusinessNetWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
}

void BusinessNetWidget::refreshData() {
    loadCompanyList();
}

// =========================================================================
// 4. 加载公司列表：添加联系人时候的公司 + 我的公司，我的公司优先显示
// =========================================================================
void BusinessNetWidget::loadCompanyList() {
    m_companyList->clear();
    m_allCompanies.clear();

    auto& db = DatabaseManager::instance();
    UserProfile myProfile = db.getUserProfile();
    QString myCorp = myProfile.company.trimmed();

    QMap<QString, CompanyInfo> compMap;

    // 1. 获取“我的公司”主体 (来自 companies 表)
    auto rawCompanies = db.getCompaniesList();
    for (const auto& m : rawCompanies) {
        CompanyInfo info;
        info.id = m["id"].toInt();
        info.name = m["name"].toString().trimmed();
        info.taxNo = m["tax_no"].toString().trimmed();
        info.industry = m["industry"].toString().trimmed();
        info.roleType = m["role_type"].toString().trimmed();
        info.capital = m["registered_capital"].toDouble();
        info.shareRatio = m["share_ratio"].toDouble();
        info.notes = m["notes"].toString().trimmed();
        info.legalRepContactId = m["legal_rep_contact_id"].toInt();
        info.legalRepName = m["legal_rep_name"].toString().trimmed();

        // 严密判定是否为我的公司：必须有明确 is_my_company 标记为 1，或者名片匹配
        if (m.contains("is_my_company") && !m["is_my_company"].isNull()) {
            info.isMyCompany = (m["is_my_company"].toInt() == 1);
        } else {
            bool isRoleMine = (info.roleType != "EXTERNAL" && (info.roleType == "OWNER" || info.roleType == "PARTNER" || info.roleType == "EMPLOYEE"));
            info.isMyCompany = (!myCorp.isEmpty() && info.name == myCorp) ||
                               (isRoleMine && (info.legalRepContactId == -1 || info.legalRepName == "本人"));
        }

        if (!info.name.isEmpty()) {
            compMap[info.name] = info;
        }
    }

    // 2. 名片中配置的公司名
    if (!myCorp.isEmpty() && !compMap.contains(myCorp)) {
        CompanyInfo info;
        info.name = myCorp;
        info.industry = myProfile.industry;
        info.roleType = "OWNER";
        info.isMyCompany = true;
        compMap[myCorp] = info;
    }

    // 3. 获取“添加联系人时候的公司”（来自 contacts.unit）
    QSqlQuery qUnit(db.database());
    qUnit.exec("SELECT DISTINCT unit, company_id FROM contacts WHERE unit IS NOT NULL AND TRIM(unit) != '';");
    while (qUnit.next()) {
        QString u = qUnit.value(0).toString().trimmed();
        int cid = qUnit.value(1).toInt();
        if (!u.isEmpty()) {
            if (!compMap.contains(u)) {
                if (cid <= 0) {
                    cid = db.ensureCompanyExists(u);
                }
                CompanyInfo info;
                info.id = cid;
                info.name = u;
                info.roleType = "EXTERNAL";
                info.isMyCompany = (!myCorp.isEmpty() && u == myCorp);
                compMap[u] = info;
            } else if (compMap[u].id <= 0) {
                if (cid > 0) compMap[u].id = cid;
                else compMap[u].id = db.ensureCompanyExists(u);
            }
        }
    }

    // 4. 统计每家公司的关联人数（杜绝读取无联系人关联的孤立 dictionary 词条，防止修改或删除后产生幽灵条目）
    for (auto it = compMap.begin(); it != compMap.end(); ++it) {
        CompanyInfo& info = it.value();
        QSqlQuery qCnt(db.database());
        if (info.id > 0) {
            qCnt.prepare("SELECT COUNT(*) FROM contacts WHERE company_id = :cid OR unit = :cname;");
            qCnt.bindValue(":cid", info.id);
            qCnt.bindValue(":cname", info.name);
        } else {
            qCnt.prepare("SELECT COUNT(*) FROM contacts WHERE unit = :cname;");
            qCnt.bindValue(":cname", info.name);
        }
        if (qCnt.exec() && qCnt.next()) {
            info.memberCount = qCnt.value(0).toInt();
        }
        m_allCompanies.append(info);
    }

    // 5. 排序：我的公司优先显示在最上方；然后按关联人数降序排列
    std::sort(m_allCompanies.begin(), m_allCompanies.end(), [](const CompanyInfo& a, const CompanyInfo& b) {
        if (a.isMyCompany != b.isMyCompany) {
            return a.isMyCompany; // 我的公司排前面
        }
        if (a.memberCount != b.memberCount) {
            return a.memberCount > b.memberCount;
        }
        return a.name < b.name;
    });

    int targetSelectRow = -1;
    for (int i = 0; i < m_allCompanies.size(); ++i) {
        const auto& info = m_allCompanies[i];
        auto* item = new QListWidgetItem(m_companyList);
        item->setSizeHint(QSize(200, 36));
        item->setData(Qt::UserRole, info.id);
        item->setData(Qt::UserRole + 1, info.name);
        item->setData(Qt::UserRole + 2, info.memberCount);

        auto* itemWidget = new QWidget();
        itemWidget->setObjectName("companyItemRow");
        auto* itemLayout = new QHBoxLayout(itemWidget);
        itemLayout->setContentsMargins(10, 0, 10, 0);
        itemLayout->setSpacing(6);

        auto* nameLbl = new QLabel(info.name, itemWidget);
        nameLbl->setObjectName("compNameLbl");
        nameLbl->setStyleSheet("color: #c4c4cc; font-size: 12px; background: transparent;");
        itemLayout->addWidget(nameLbl, 1);

        if (info.isMyCompany) {
            auto* tagLbl = new QLabel("我的", itemWidget);
            tagLbl->setObjectName("compTagLbl");
            tagLbl->setStyleSheet("color: #00d68f; background: rgba(0, 214, 143, 0.15); font-size: 10px; padding: 1px 5px; border-radius: 3px; font-weight: bold;");
            itemLayout->addWidget(tagLbl, 0, Qt::AlignVCenter);
        }

        auto* countLbl = new QLabel(QString("%1位").arg(info.memberCount), itemWidget);
        countLbl->setObjectName("compCountLbl");
        countLbl->setStyleSheet("color: #7a7a82; font-size: 12px; background: transparent;");
        itemLayout->addWidget(countLbl, 0, Qt::AlignRight | Qt::AlignVCenter);

        m_companyList->setItemWidget(item, itemWidget);

        if (info.name == m_selectedCompanyName || (info.id > 0 && info.id == m_selectedCompanyId)) {
            targetSelectRow = i;
        }
    }

    if (m_companyList->count() > 0) {
        int selectRow = (targetSelectRow >= 0) ? targetSelectRow : 0;
        m_companyList->setCurrentRow(selectRow);
        onCompanyItemClicked(m_companyList->item(selectRow));
    } else {
        m_selectedCompanyId = -1;
        m_selectedCompanyName.clear();
        m_graphScene->clear();
        resetAiPromptPage("（未选择公司）");
    }
    updateCompanyListSelectionVisuals();
}

void BusinessNetWidget::updateCompanyListSelectionVisuals() {
    auto* curItem = m_companyList->currentItem();
    for (int i = 0; i < m_companyList->count(); ++i) {
        auto* item = m_companyList->item(i);
        auto* w = m_companyList->itemWidget(item);
        if (!w) continue;

        bool isSelected = (item == curItem) ||
                          (m_selectedCompanyId > 0 && item->data(Qt::UserRole).toInt() == m_selectedCompanyId) ||
                          (!m_selectedCompanyName.isEmpty() && item->data(Qt::UserRole + 1).toString() == m_selectedCompanyName);

        auto* nameLbl = w->findChild<QLabel*>("compNameLbl");
        auto* tagLbl = w->findChild<QLabel*>("compTagLbl");
        auto* countLbl = w->findChild<QLabel*>("compCountLbl");

        if (isSelected) {
            // 核心要求 2：选中的公司明显变色（深祖母绿高亮底色 + 4px 亮绿色指示条 + 纯白加粗文字）
            w->setStyleSheet(
                "QWidget#companyItemRow { "
                "  background-color: #173827; "
                "  border-left: 4px solid #00d68f; "
                "  border-radius: 4px; "
                "}"
            );
            if (nameLbl) {
                nameLbl->setStyleSheet("color: #ffffff; font-size: 13px; font-weight: bold; background: transparent;");
            }
            if (countLbl) {
                countLbl->setStyleSheet("color: #00d68f; font-size: 12px; font-weight: bold; background: transparent;");
            }
            if (tagLbl) {
                tagLbl->setStyleSheet("color: #00ffaa; background: rgba(0, 214, 143, 0.35); font-size: 10px; padding: 1px 5px; border-radius: 3px; font-weight: bold;");
            }
        } else {
            // 未选中常态：纯透明背景，鼠标悬停时微灰
            w->setStyleSheet(
                "QWidget#companyItemRow { "
                "  background-color: transparent; "
                "  border-left: 4px solid transparent; "
                "  border-radius: 4px; "
                "} "
                "QWidget#companyItemRow:hover { "
                "  background-color: #1a1a1e; "
                "}"
            );
            if (nameLbl) {
                nameLbl->setStyleSheet("color: #c4c4cc; font-size: 12px; background: transparent;");
            }
            if (countLbl) {
                countLbl->setStyleSheet("color: #7a7a82; font-size: 12px; background: transparent;");
            }
            if (tagLbl) {
                tagLbl->setStyleSheet("color: #00d68f; background: rgba(0, 214, 143, 0.15); font-size: 10px; padding: 1px 5px; border-radius: 3px; font-weight: bold;");
            }
        }
    }
}

void BusinessNetWidget::onCompanySearchChanged(const QString& text) {
    QString kw = text.trimmed();
    for (int i = 0; i < m_companyList->count(); ++i) {
        auto* item = m_companyList->item(i);
        QString compName = item->data(Qt::UserRole + 1).toString();
        bool match = kw.isEmpty() || compName.contains(kw, Qt::CaseInsensitive);
        item->setHidden(!match);
    }
}

void BusinessNetWidget::onCompanyItemClicked(QListWidgetItem* item) {
    if (!item) return;
    int compId = item->data(Qt::UserRole).toInt();
    QString compName = item->data(Qt::UserRole + 1).toString().trimmed();

    // 若 compId <= 0，确保在 companies 表中建立合法主体档案并获取唯一主键
    if (compId <= 0 && !compName.isEmpty()) {
        compId = DatabaseManager::instance().ensureCompanyExists(compName);
        item->setData(Qt::UserRole, compId);
    }

    m_selectedCompanyId = compId;
    m_selectedCompanyName = compName;

    // 选中的公司变色更新
    updateCompanyListSelectionVisuals();

    // 1. 刷新拓扑图
    loadCompanyTopology(compId, compName);

    // 2. 核心要求 3：优先检查 assets/uppic/json/{id}.json 缓存，防止每次重新生成
    if (!loadCompanyAiCache(compId)) {
        // 缓存不存在：展示防误触引导页，提示“点击生成才开始”
        resetAiPromptPage(compName);
    }
}

void BusinessNetWidget::resetAiPromptPage(const QString& companyName) {
    m_aiHistory.clear();
    m_accumulatedResponse.clear();
    m_isAiGenerating = false;
    if (m_btnTopGenerate) {
        m_btnTopGenerate->setEnabled(true);
        m_btnTopGenerate->setText("生成");
    }
    if (m_btnSendQuestion) {
        m_btnSendQuestion->setText("发送");
        m_btnSendQuestion->setEnabled(true);
    }
    if (m_aiQuestionEdit) {
        m_aiQuestionEdit->setEnabled(true);
        m_aiQuestionEdit->clear();
    }

    if (m_txtAiReview) {
        QString guideText = QString(
            "<div style='font-family:\"Microsoft YaHei UI\",\"Microsoft YaHei\",sans-serif; "
            "font-size:14px; color:#d1d1d6; line-height:24px; padding:2px;'>"
            "可以按当前【<b style='color:#ffffff;'>%1</b>】公司，关系网评价生意可能、能量....对比您的职业提供最精准的策划；在此主题上可以无限咨询"
            "</div>"
        ).arg(companyName);
        m_txtAiReview->setHtml(guideText);
    }
}

// =========================================================================
// 5. 渲染拓扑图：公司为绝对中心 -> 员工 -> 朋友 -> 朋友的朋友 无限多层
// =========================================================================
void BusinessNetWidget::loadCompanyTopology(int companyId, const QString& companyName) {
    m_graphScene->clear();
    m_nodeItemMap.clear();
    m_visualEdges.clear();
    m_allGraphNodes.clear();

    auto& db = DatabaseManager::instance();

    // 1. 查询该公司的法人主体设定
    int legalRepId = 0;
    QString legalRepName;
    QSqlQuery qComp(db.database());
    qComp.prepare("SELECT legal_rep_contact_id, legal_rep_name FROM companies WHERE id = :cid OR name = :cname;");
    qComp.bindValue(":cid", companyId);
    qComp.bindValue(":cname", companyName);
    if (qComp.exec() && qComp.next()) {
        legalRepId = qComp.value(0).toInt();
        legalRepName = qComp.value(1).toString().trimmed();
    }

    QString centerSubText = "企业主体";
    if (legalRepId == -1) {
        centerSubText = "企业主体 (法人: 本人)";
    } else if (!legalRepName.isEmpty()) {
        centerSubText = QString("企业主体 (法人: %1)").arg(legalRepName);
    }

    // Level 0 中心公司主体：牢固固定在绝对原点 (0, 0)
    auto* centerItem = new BusinessNodeItem(0, companyId, companyName, centerSubText, 48.0, QColor(0, 214, 143));
    centerItem->setPos(0, 0);
    m_graphScene->addItem(centerItem);
    m_centerCompanyItem = centerItem;

    // 2. BFS 广度优先多层遍历：挖掘员工 -> 员工朋友 -> 朋友的朋友 无限多层链条
    QSet<int> visitedContactIds;
    QList<MultiLevelNode> level1Staff;

    QSqlQuery qStaff(db.database());
    if (companyId > 0) {
        qStaff.prepare("SELECT c.id, c.name, c.title, c.industry, c.phone, c.relation_type, c.closeness_score "
                       "FROM contacts c "
                       "WHERE c.company_id = :cid OR unit = :cname "
                       "ORDER BY c.closeness_score DESC;");
        qStaff.bindValue(":cid", companyId);
        qStaff.bindValue(":cname", companyName);
    } else {
        qStaff.prepare("SELECT c.id, c.name, c.title, c.industry, c.phone, c.relation_type, c.closeness_score "
                       "FROM contacts c "
                       "WHERE unit = :cname "
                       "ORDER BY c.closeness_score DESC;");
        qStaff.bindValue(":cname", companyName);
    }

    if (qStaff.exec()) {
        while (qStaff.next()) {
            MultiLevelNode n;
            n.id = qStaff.value(0).toInt();
            n.level = 1;
            n.name = qStaff.value(1).toString().trimmed();

            // 职务：用户明确要求“公司 -(职务)-> 员工”，如“合伙人”、“投资人”、“总经理”、“员工”
            QString rawTitle = qStaff.value(2).toString().trimmed();
            QString rawRelType = qStaff.value(5).toString().trimmed();

            bool isLegalRep = (legalRepId > 0 && n.id == legalRepId);
            if (isLegalRep) {
                if (rawTitle.isEmpty() || rawTitle == "员工") {
                    n.jobTitle = "法定代表人";
                } else if (!rawTitle.contains("法人") && !rawTitle.contains("法定")) {
                    n.jobTitle = "法人 (" + rawTitle + ")";
                } else {
                    n.jobTitle = rawTitle;
                }
            } else if (!rawTitle.isEmpty()) {
                n.jobTitle = rawTitle;
            } else if (!rawRelType.isEmpty()) {
                n.jobTitle = rawRelType;
            } else {
                n.jobTitle = "员工";
            }
            n.relationName = n.jobTitle;
            n.relationType = n.jobTitle;

            // 行业：用户图示如“制造业”
            n.industry = qStaff.value(3).toString().trimmed();
            if (n.industry.isEmpty()) {
                n.industry = "制造业";
            }

            n.phone = qStaff.value(4).toString().trimmed();
            n.closenessScore = qBound(1, (qStaff.value(6).toInt() + 19) / 20, 5);
            n.parentId = companyId;
            n.parentLevel = 0;

            if (isLegalRep) {
                level1Staff.prepend(n); // 法人主体优先排在第一位
            } else {
                level1Staff.append(n);
            }
            visitedContactIds.insert(n.id);
        }
    }

    // 若设定的法人主体联系人尚未在上述列表中出现，单独将其加载并置于首位
    if (legalRepId > 0 && !visitedContactIds.contains(legalRepId)) {
        QSqlQuery qRep(db.database());
        qRep.prepare("SELECT id, name, title, industry, phone, relation_type, closeness_score FROM contacts WHERE id = :rid;");
        qRep.bindValue(":rid", legalRepId);
        if (qRep.exec() && qRep.next()) {
            MultiLevelNode n;
            n.id = qRep.value(0).toInt();
            n.level = 1;
            n.name = qRep.value(1).toString().trimmed();
            QString rawTitle = qRep.value(2).toString().trimmed();
            if (rawTitle.isEmpty() || rawTitle == "员工") {
                n.jobTitle = "法定代表人";
            } else if (!rawTitle.contains("法人") && !rawTitle.contains("法定")) {
                n.jobTitle = "法人 (" + rawTitle + ")";
            } else {
                n.jobTitle = rawTitle;
            }
            n.relationName = n.jobTitle;
            n.relationType = n.jobTitle;
            n.industry = qRep.value(3).toString().trimmed();
            if (n.industry.isEmpty()) n.industry = "制造业";
            n.phone = qRep.value(4).toString().trimmed();
            n.closenessScore = qBound(1, (qRep.value(6).toInt() + 19) / 20, 5);
            n.parentId = companyId;
            n.parentLevel = 0;

            level1Staff.prepend(n);
            visitedContactIds.insert(n.id);
        }
    }

    m_allGraphNodes = level1Staff;
    QList<MultiLevelNode> currentLayer = level1Staff;
    int currentLevel = 1;
    const int MAX_LEVEL = 20; // 充分覆盖所有可能深度，实现“无限”拓扑延展

    while (!currentLayer.isEmpty() && currentLevel < MAX_LEVEL) {
        QList<MultiLevelNode> nextLayer;
        for (const auto& parentNode : currentLayer) {
            // 查询关联朋友：
            // 1) relationships 表 (双向关系：from_contact_id = parent 或 to_contact_id = parent)
            // 2) contacts.introducer_id (双向引荐关系)
            // 注意：relationships 表中的关系字段为 relation_type (绝非 relation_name)
            QSqlQuery qFr(db.database());
            qFr.prepare(
                "SELECT c.id, c.name, c.unit, c.title, c.industry, r.relation_type, c.closeness_score, c.phone "
                "FROM relationships r "
                "JOIN contacts c ON r.to_contact_id = c.id "
                "WHERE r.from_contact_id = :p1 AND c.id != :p2 "
                "UNION "
                "SELECT c.id, c.name, c.unit, c.title, c.industry, r.relation_type, c.closeness_score, c.phone "
                "FROM relationships r "
                "JOIN contacts c ON r.from_contact_id = c.id "
                "WHERE r.to_contact_id = :p3 AND c.id != :p4 "
                "UNION "
                "SELECT c.id, c.name, c.unit, c.title, c.industry, '引荐' AS relation_type, c.closeness_score, c.phone "
                "FROM contacts c "
                "WHERE c.introducer_id = :p5 AND c.id != :p6 "
                "UNION "
                "SELECT c.id, c.name, c.unit, c.title, c.industry, '引荐人' AS relation_type, c.closeness_score, c.phone "
                "FROM contacts c "
                "WHERE c.id = (SELECT introducer_id FROM contacts WHERE id = :p7 AND introducer_id > 0) AND c.id != :p8;"
            );
            qFr.bindValue(":p1", parentNode.id);
            qFr.bindValue(":p2", parentNode.id);
            qFr.bindValue(":p3", parentNode.id);
            qFr.bindValue(":p4", parentNode.id);
            qFr.bindValue(":p5", parentNode.id);
            qFr.bindValue(":p6", parentNode.id);
            qFr.bindValue(":p7", parentNode.id);
            qFr.bindValue(":p8", parentNode.id);

            if (qFr.exec()) {
                while (qFr.next()) {
                    int fid = qFr.value(0).toInt();
                    if (fid <= 0 || visitedContactIds.contains(fid)) continue;
                    visitedContactIds.insert(fid);

                    MultiLevelNode childNode;
                    childNode.id = fid;
                    childNode.level = currentLevel + 1;
                    childNode.name = qFr.value(1).toString().trimmed();

                    // 行业：按用户图示显示，如“行业”或“制造业”
                    childNode.industry = qFr.value(4).toString().trimmed();
                    if (childNode.industry.isEmpty()) {
                        QString u = qFr.value(2).toString().trimmed();
                        if (!u.isEmpty() && u != companyName) childNode.industry = u;
                    }
                    if (childNode.industry.isEmpty()) childNode.industry = "行业";

                    // 关系：用户明确要求“员工 -(关系)-> 朋友 -(关系)-> 朋友”，如“朋友”、“战友”、“老乡”、“合伙”等
                    childNode.relationName = qFr.value(5).toString().trimmed();
                    if (childNode.relationName.isEmpty()) childNode.relationName = "朋友";
                    childNode.relationType = childNode.relationName;
                    childNode.jobTitle = "";
                    childNode.closenessScore = qBound(1, (qFr.value(6).toInt() + 19) / 20, 5);
                    childNode.phone = qFr.value(7).toString().trimmed();
                    childNode.parentId = parentNode.id;
                    childNode.parentLevel = currentLevel;

                    nextLayer.append(childNode);
                    m_allGraphNodes.append(childNode);
                }
            }
        }
        currentLayer = nextLayer;
        currentLevel++;
    }

    if (m_allGraphNodes.isEmpty()) {
        onResetGraphZoom();
        return;
    }

    // 3. 几何辐射排布算法 (以 (0, 0) 公司为绝对中心向外扩散，层层分支且互不重叠)
    QMap<int, QPointF> nodePosMap;
    QMap<int, double> nodeAngleMap;

    int staffCount = level1Staff.size();
    double R1 = qMax(190.0, (28.0 * 2.0 + 35.0) * staffCount / (2.0 * M_PI));

    struct Sector {
        double minAng;
        double maxAng;
    };
    QMap<int, Sector> nodeSectorMap;

    // Level 1 员工：严格围绕原点 (0, 0) 同心圆环均匀展开
    for (int i = 0; i < staffCount; ++i) {
        const auto& staff = level1Staff[i];
        double ang = -M_PI * 0.5 + (2.0 * M_PI * i) / qMax(1, staffCount);
        double sx = R1 * std::cos(ang);
        double sy = R1 * std::sin(ang);

        nodePosMap[staff.id] = QPointF(sx, sy);
        nodeAngleMap[staff.id] = ang;

        double span = (2.0 * M_PI / qMax(1, staffCount));
        nodeSectorMap[staff.id] = { ang - span * 0.46, ang + span * 0.46 };
    }

    // 逐层处理 Level >= 2 节点：以对应父节点扇区为基准，向外同心圆环层层扩展
    for (int lvl = 2; lvl <= currentLevel; ++lvl) {
        QList<MultiLevelNode> layerNodes;
        for (const auto& n : m_allGraphNodes) {
            if (n.level == lvl) layerNodes.append(n);
        }
        if (layerNodes.isEmpty()) continue;

        double baseRadius = R1 + 150.0 * (lvl - 1);

        // 按父节点分组
        QMap<int, QList<MultiLevelNode>> byParent;
        for (const auto& n : layerNodes) {
            byParent[n.parentId].append(n);
        }

        for (auto it = byParent.begin(); it != byParent.end(); ++it) {
            int pid = it.key();
            const auto& children = it.value();
            int k = children.size();
            Sector pSector = nodeSectorMap.value(pid, { -M_PI, M_PI });
            double parentAng = nodeAngleMap.value(pid, (pSector.minAng + pSector.maxAng) * 0.5);

            double sectorWidth = pSector.maxAng - pSector.minAng;
            double step = sectorWidth / qMax(1, k);

            for (int j = 0; j < k; ++j) {
                const auto& child = children[j];
                double sMin = pSector.minAng + j * step;
                double sMax = sMin + step;
                double childAng = (k == 1) ? parentAng : ((sMin + sMax) * 0.5);

                nodeAngleMap[child.id] = childAng;
                nodeSectorMap[child.id] = { sMin, sMax };

                // 当子节点较多时，采用3阶交错外环半径，形成梯次景深与充足呼吸空间
                double ringOffset = 0.0;
                if (k >= 5) {
                    ringOffset = (j % 3) * 52.0;
                } else if (k >= 2) {
                    ringOffset = (j % 2 == 0 ? 0.0 : 40.0);
                }
                double ringRadius = baseRadius + ringOffset;
                double cx = ringRadius * std::cos(childAng);
                double cy = ringRadius * std::sin(childAng);

                nodePosMap[child.id] = QPointF(cx, cy);
            }
        }
    }

    // 碰撞消除微调 (保留 (0, 0) 公司主体固定不移动，节点之间保持充足间隙)
    const int ITERATIONS = 35;
    for (int iter = 0; iter < ITERATIONS; ++iter) {
        for (int i = 0; i < m_allGraphNodes.size(); ++i) {
            for (int j = i + 1; j < m_allGraphNodes.size(); ++j) {
                int idA = m_allGraphNodes[i].id;
                int idB = m_allGraphNodes[j].id;
                QPointF posA = nodePosMap[idA];
                QPointF posB = nodePosMap[idB];
                double rA = (m_allGraphNodes[i].level == 1 ? 28.0 : 22.0);
                double rB = (m_allGraphNodes[j].level == 1 ? 28.0 : 22.0);
                double minDist = rA + rB + 28.0;

                QPointF delta = posB - posA;
                double dist = std::hypot(delta.x(), delta.y());
                if (dist < 1.0) {
                    delta = QPointF(1.0, 0.0);
                    dist = 1.0;
                }
                if (dist < minDist) {
                    double overlap = minDist - dist;
                    QPointF norm = delta / dist;
                    double moveRatioA = (m_allGraphNodes[i].level == 1 ? 0.05 : 0.5);
                    double moveRatioB = (m_allGraphNodes[j].level == 1 ? 0.05 : 0.5);
                    nodePosMap[idA] -= norm * (overlap * moveRatioA);
                    nodePosMap[idB] += norm * (overlap * moveRatioB);
                }
            }
        }
    }

    // 4. 创建视觉图元与树状主干连线
    // Level 1 员工
    for (const auto& staff : level1Staff) {
        QPointF pt = nodePosMap[staff.id];
        // 依据职务/角色专属色彩高亮
        QColor themeCol = getRelationThemeColor(staff.jobTitle, 1);
        if (staff.jobTitle.contains("合伙") || staff.jobTitle.contains("投资")) {
            themeCol = QColor("#febc2f"); // 金黄
        } else if (staff.jobTitle.contains("董事长") || staff.jobTitle.contains("总经理") || staff.jobTitle.contains("总监")) {
            themeCol = QColor("#00d68f"); // 翡翠绿
        }

        auto* item = new BusinessNodeItem(1, staff.id, staff.name, staff.industry, 28.0, themeCol);
        item->setPos(pt);
        m_graphScene->addItem(item);
        m_nodeItemMap[staff.id] = item;

        // 公司 -(职务)-> 员工 连线 (实线，标注职务，专属色彩胶囊标签)
        createGraphEdge(
            m_graphScene,
            QPointF(0, 0), 48.0,
            pt, 28.0,
            staff.jobTitle,
            themeCol,
            false,
            0, companyId,
            1, staff.id,
            m_visualEdges,
            false
        );
    }

    // Level >= 2 朋友与朋友的朋友 (无限延伸层级，各关系类型独立色彩系统)
    for (const auto& node : m_allGraphNodes) {
        if (node.level < 2) continue;
        QPointF pt = nodePosMap[node.id];
        double radiusSize = qMax(18.0, 24.0 - (node.level - 2) * 1.5);

        // 依据所属关系类型获得专属主题色 (合伙=金黄，亲戚=粉红，客户=翡翠绿，朋友=天蓝，引荐=橙色等)
        QColor relCol = getRelationThemeColor(node.relationName, node.level);

        auto* item = new BusinessNodeItem(node.level, node.id, node.name, node.industry, radiusSize, relCol);
        item->setPos(pt);
        m_graphScene->addItem(item);
        m_nodeItemMap[node.id] = item;

        // 员工 -(关系)-> 朋友 -(关系)-> 朋友：专属色彩主干实线 + 专属色彩实心方向箭头 + 专属色彩胶囊标签
        QPointF parentPt = (node.parentLevel == 0) ? QPointF(0, 0) : nodePosMap[node.parentId];
        double parentRadius = (node.parentLevel == 0 ? 48.0 : (node.parentLevel == 1 ? 28.0 : qMax(18.0, 24.0 - (node.parentLevel - 2) * 1.5)));

        createGraphEdge(
            m_graphScene,
            parentPt, parentRadius,
            pt, radiusSize,
            node.relationName,
            relCol,
            true, // 专属颜色实心方向箭头
            node.parentLevel, node.parentId,
            node.level, node.id,
            m_visualEdges,
            false // 主干实线
        );
    }

    // 5. 跨分支交织关系网连线 (非父子关系的已有节点之间的关系)
    int totalCount = m_allGraphNodes.size();
    for (int i = 0; i < totalCount; ++i) {
        for (int j = i + 1; j < totalCount; ++j) {
            int nid1 = m_allGraphNodes[i].id;
            int nid2 = m_allGraphNodes[j].id;
            if (!m_nodeItemMap.contains(nid1) || !m_nodeItemMap.contains(nid2)) continue;

            // 避免重复绘制树主干边
            if (m_allGraphNodes[i].parentId == nid2 || m_allGraphNodes[j].parentId == nid1) continue;

            // 检查数据库中是否存在关联 (relation_type)
            QSqlQuery qCross(db.database());
            qCross.prepare("SELECT relation_type FROM relationships "
                           "WHERE (from_contact_id = :a AND to_contact_id = :b) OR (from_contact_id = :b AND to_contact_id = :a);");
            qCross.bindValue(":a", nid1);
            qCross.bindValue(":b", nid2);
            if (qCross.exec() && qCross.next()) {
                QString relType = qCross.value(0).toString().trimmed();
                if (relType.isEmpty()) relType = "关联";
                QPointF p1 = m_nodeItemMap[nid1]->pos();
                QPointF p2 = m_nodeItemMap[nid2]->pos();
                double r1 = (m_allGraphNodes[i].level == 1 ? 28.0 : 22.0);
                double r2 = (m_allGraphNodes[j].level == 1 ? 28.0 : 22.0);

                // 跨分支网络关联采用专属色彩的虚线连接，拉开主次层次感
                QColor crossCol = getRelationThemeColor(relType, 2);
                crossCol.setAlpha(175);
                createGraphEdge(
                    m_graphScene,
                    p1, r1,
                    p2, r2,
                    relType,
                    crossCol,
                    false,
                    m_allGraphNodes[i].level, nid1,
                    m_allGraphNodes[j].level, nid2,
                    m_visualEdges,
                    true // isDashed = true，虚线表示跨分支关系
                );
            }
        }
    }

    onResetGraphZoom();
}

// 确保以 (0, 0) 公司为对称中心居中展示
void BusinessNetWidget::onResetGraphZoom() {
    if (!m_graphView || !m_graphScene) return;
    m_graphView->resetZoom();
    QRectF bounds = m_graphScene->itemsBoundingRect();
    double maxExtent = qMax(qMax(qAbs(bounds.left()), qAbs(bounds.right())),
                            qMax(qAbs(bounds.top()), qAbs(bounds.bottom())));
    maxExtent = qMax(maxExtent + 60.0, 320.0);
    QRectF centerSquare(-maxExtent, -maxExtent, maxExtent * 2, maxExtent * 2);
    m_graphScene->setSceneRect(centerSquare);
    m_graphView->fitInView(centerSquare, Qt::KeepAspectRatio);
}

void BusinessNetWidget::handleNodeClicked(int level, int id) {
    QSet<QGraphicsItem*> highItems;
    QSet<QGraphicsItem*> highLines;

    if (level == 0) {
        clearNodeHighlight();
        return;
    }

    highItems.insert(m_centerCompanyItem);
    if (m_nodeItemMap.contains(id)) {
        highItems.insert(m_nodeItemMap[id]);
    }

    for (const auto& edge : m_visualEdges) {
        if ((edge.fromId == id && edge.fromLevel == level) || (edge.toId == id && edge.toLevel == level)) {
            if (edge.pathItem) highLines.insert(edge.pathItem);
            if (edge.labelItem) highLines.insert(edge.labelItem);
            if (m_nodeItemMap.contains(edge.fromId)) highItems.insert(m_nodeItemMap[edge.fromId]);
            if (m_nodeItemMap.contains(edge.toId)) highItems.insert(m_nodeItemMap[edge.toId]);
        }
    }

    auto applyState = [&](QGraphicsItem* itm, int itmLevel, int itmId) {
        auto* bNode = dynamic_cast<BusinessNodeItem*>(itm);
        if (!bNode) return;
        bool isFocus = (itmLevel == level && itmId == id);
        bool isHigh = highItems.contains(itm);
        bNode->setVisualState(isFocus, isHigh, !isHigh);
    };

    if (m_centerCompanyItem) {
        applyState(m_centerCompanyItem, 0, m_selectedCompanyId);
    }
    for (const auto& node : m_allGraphNodes) {
        if (m_nodeItemMap.contains(node.id)) {
            applyState(m_nodeItemMap[node.id], node.level, node.id);
        }
    }

    for (const auto& edge : m_visualEdges) {
        bool isLineHigh = highLines.contains(edge.pathItem);
        if (edge.pathItem) {
            QColor c = edge.baseColor;
            c.setAlpha(isLineHigh ? 240 : 35);
            edge.pathItem->setPen(QPen(c, isLineHigh ? 2.0 : (edge.isDashed ? 1.1 : 1.5), edge.isDashed ? Qt::DashLine : Qt::SolidLine));
            edge.pathItem->setBrush(c);
        }
        if (edge.labelItem) {
            edge.labelItem->setVisible(isLineHigh);
        }
    }
}

void BusinessNetWidget::clearNodeHighlight() {
    auto resetState = [](QGraphicsItem* itm) {
        auto* bNode = dynamic_cast<BusinessNodeItem*>(itm);
        if (bNode) {
            bNode->setVisualState(false, false, false);
        }
    };

    if (m_centerCompanyItem) resetState(m_centerCompanyItem);
    for (auto* itm : m_nodeItemMap) resetState(itm);

    for (const auto& edge : m_visualEdges) {
        if (edge.pathItem) {
            edge.pathItem->setPen(QPen(edge.baseColor, edge.isDashed ? 1.1 : 1.5, edge.isDashed ? Qt::DashLine : Qt::SolidLine));
            edge.pathItem->setBrush(edge.baseColor);
        }
        if (edge.labelItem) {
            edge.labelItem->setVisible(true);
        }
    }
}

// =========================================================================
// 6. 右键菜单与删除限制：人员关联小于2 才可以删除
// =========================================================================
void BusinessNetWidget::onCompanyContextMenu(const QPoint& pos) {
    auto* item = m_companyList->itemAt(pos);
    if (!item) return;

    // 严谨设置当前操作项为选中项，杜绝选中A公司却右键编辑了B公司的问题
    m_companyList->setCurrentItem(item);
    updateCompanyListSelectionVisuals();

    int compId = item->data(Qt::UserRole).toInt();
    QString compName = item->data(Qt::UserRole + 1).toString();
    int memberCount = item->data(Qt::UserRole + 2).toInt();

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #1c1c1f; color: #e1e1e5; border: 1px solid #28282b; padding: 4px; border-radius: 4px; }"
        "QMenu::item { padding: 6px 20px; border-radius: 3px; font-size: 12px; }"
        "QMenu::item:selected { background-color: #2e2e34; color: #ffffff; }"
    );

    auto* actEdit = menu.addAction(UiIcons::icon(UiIcons::Code::Diary, 12, QColor("#e1e1e5")), "编辑");
    auto* actDelete = menu.addAction(UiIcons::icon(UiIcons::Code::Delete, 12, QColor("#fa5151")), "删除");

    auto* selectedAction = menu.exec(m_companyList->mapToGlobal(pos));
    if (selectedAction == actEdit) {
        onEditCompanyClicked();
    } else if (selectedAction == actDelete) {
        onDeleteCompanyClicked();
    }
}

void BusinessNetWidget::onEditCompanyClicked() {
    auto* currentItem = m_companyList->currentItem();
    if (!currentItem) return;

    int compId = currentItem->data(Qt::UserRole).toInt();
    QString compName = currentItem->data(Qt::UserRole + 1).toString().trimmed();

    // 核心确保：编辑前若该企业仅在联系人单位中存在，即刻在 companies 表中建立档案并获得其唯一 ID，防止修改时误判为全新公司
    if (compId <= 0 && !compName.isEmpty()) {
        compId = DatabaseManager::instance().ensureCompanyExists(compName);
        if (compId > 0) {
            currentItem->setData(Qt::UserRole, compId);
        }
    }

    auto rawList = DatabaseManager::instance().getCompaniesList();
    QVariantMap currentCompData;
    // 优先按选中的公司名称精确匹配，杜绝历史 ID 错位导致读出其他公司数据
    for (const auto& m : rawList) {
        if (!compName.isEmpty() && m["name"].toString().trimmed() == compName) {
            currentCompData = m;
            break;
        }
    }
    if (currentCompData.isEmpty() && compId > 0) {
        for (const auto& m : rawList) {
            if (m["id"].toInt() == compId) {
                currentCompData = m;
                break;
            }
        }
    }

    UserProfile myProfile = DatabaseManager::instance().getUserProfile();
    QString myCorp = myProfile.company.trimmed();
    bool isMine = (!myCorp.isEmpty() && compName == myCorp);

    if (currentCompData.isEmpty()) {
        currentCompData["id"] = compId;
        currentCompData["name"] = compName;
        currentCompData["is_my_company"] = isMine ? 1 : 0;
        currentCompData["role_type"] = isMine ? "OWNER" : "EXTERNAL";
        currentCompData["registered_capital"] = 0.0;
        currentCompData["share_ratio"] = isMine ? 100.0 : 0.0;
        currentCompData["legal_rep_contact_id"] = 0;
        currentCompData["legal_rep_name"] = "";
    } else {
        if (compId > 0) currentCompData["id"] = compId;
        if (!compName.isEmpty()) currentCompData["name"] = compName;
        if (!currentCompData.contains("is_my_company") || currentCompData["is_my_company"].isNull()) {
            QString r = currentCompData["role_type"].toString().trimmed();
            int lrid = currentCompData["legal_rep_contact_id"].toInt();
            QString lrname = currentCompData["legal_rep_name"].toString().trimmed();
            int cid = currentCompData["id"].toInt();
            bool autoMine = (compName == myCorp) ||
                            (cid <= 6 && (r == "OWNER" || r == "PARTNER" || r == "EMPLOYEE")) ||
                            ((r == "OWNER" || r == "PARTNER") && (lrid == -1 || lrname == "本人"));
            currentCompData["is_my_company"] = autoMine ? 1 : 0;
            if (!autoMine && r != "EXTERNAL") {
                currentCompData["role_type"] = "EXTERNAL";
            }
        }
    }

    CompanyDialog dlg(currentCompData, this);
    if (dlg.exec() == QDialog::Accepted) {
        QString updatedName = dlg.companyName().trimmed();
        int updatedId = dlg.companyId();
        if (!updatedName.isEmpty()) {
            m_selectedCompanyName = updatedName;
            m_selectedCompanyId = updatedId;
        }
        loadCompanyList();
    }
}

void BusinessNetWidget::onDeleteCompanyClicked() {
    auto* currentItem = m_companyList->currentItem();
    if (!currentItem) return;

    int compId = currentItem->data(Qt::UserRole).toInt();
    QString compName = currentItem->data(Qt::UserRole + 1).toString();
    int memberCount = currentItem->data(Qt::UserRole + 2).toInt();

    // 业务硬性规则：人员关联小于2 才可以删除
    if (memberCount >= 2) {
        CustomDialog::showWarning(
            this,
            "无法删除公司",
            QString("公司【%1】当前关联了 %2 位公司人员。\n\n按系统规则，人员关联需小于 2 位（即 0 位或 1 位）方可删除！\n请先在联系人中解除多余人员关联后再尝试。")
                .arg(compName)
                .arg(memberCount)
        );
        return;
    }

    if (CustomDialog::confirm(
            this,
            "删除公司",
            QString("确定要删除公司【%1】吗？\n删除后不可恢复。").arg(compName),
            "确认删除",
            "取消",
            true)) {
        auto& db = DatabaseManager::instance();
        if (compId > 0) {
            db.deleteCompany(compId);
        }
        QSqlQuery q(db.database());
        q.prepare("UPDATE contacts SET unit = NULL WHERE unit = :cname;");
        q.bindValue(":cname", compName);
        q.exec();

        QSqlQuery qDict(db.database());
        qDict.prepare("DELETE FROM dictionary WHERE category IN ('UNIT', 'COMPANY') AND value = :cname;");
        qDict.bindValue(":cname", compName);
        qDict.exec();

        CustomDialog::showInfo(this, "删除成功", QString("公司【%1】已成功删除。").arg(compName));
        m_selectedCompanyId = -1;
        m_selectedCompanyName.clear();
        loadCompanyList();
    }
}

// =========================================================================
// 7. AI 点评生成与多轮无限咨询 (带至少5行输入框)
// =========================================================================
void BusinessNetWidget::onGenerateAiClicked() {
    if (m_selectedCompanyName.isEmpty()) {
        CustomDialog::showWarning(this, "提示", "请先在左侧选择要点评的公司。");
        return;
    }

    auto engine = AiAdvisorService::instance().engine();
    if (!engine) {
        CustomDialog::showWarning(this, "AI 模型未就绪", "本地大模型推理引擎未初始化，请点击顶部状态配置模型。");
        return;
    }

    m_accumulatedResponse.clear();
    m_aiHistory.clear();
    m_isAiGenerating = true;
    m_isInitialCompanyGeneration = true;
    m_isLastResponseIncomplete = false;

    if (m_btnSendQuestion) {
        m_btnSendQuestion->setText("生成中...");
        m_btnSendQuestion->setEnabled(false);
    }
    if (m_aiQuestionEdit) m_aiQuestionEdit->setEnabled(false);
    if (m_btnTopGenerate) {
        m_btnTopGenerate->setEnabled(false);
        m_btnTopGenerate->setText("生成中...");
    }

    if (m_txtAiReview) {
        m_txtAiReview->setHtml(
            QString("<div style='color:#febc2f; font-size:14px; font-weight:bold; line-height:24px; padding:4px;'>"
                    "AI 军师正在进行多维人脉与商业博弈推演...</div>")
        );
    }

    emit AiAdvisorService::instance().evaluationStarted();

    auto& db = DatabaseManager::instance();
    UserProfile up = db.getUserProfile();
    FinancialSnapshot snap = db.getFinancialSnapshot();

    QString staffDetails;
    for (const auto& n : m_allGraphNodes) {
        if (n.level == 1) {
            staffDetails += QString("  - 核心员工: %1 | 职务: %2 | 行业: %3 | 亲密度: %4星 | 电话: %5\n")
                .arg(n.name).arg(n.jobTitle).arg(n.industry).arg(n.closenessScore).arg(n.phone);
        } else {
            staffDetails += QString("  - Level %1 延伸人脉: %2 | 关系类型: %3 | 行业: %4\n")
                .arg(n.level).arg(n.name).arg(n.relationName).arg(n.industry);
        }
    }
    if (staffDetails.isEmpty()) staffDetails = "  - （暂无录入直接成员）\n";

    QString headerInfo = QString(
        "【企业主体与无限多层商业关系网情报】\n"
        "- 目标公司：%1\n"
        "- 挖掘出的多层全网关联人员数量：%2 位\n"
        "人员与关系网详情：\n%3\n"
        "【当事人（我）的背景与财务底牌】\n"
        "- 姓名：%4 | 职业/行业：%5 | 所在公司：%6\n"
        "- 个人流动资金：¥%7 元 | 负债：¥%8 元 | 现金流安全天数预估：约 %9 天\n"
        "- 定制AI军师法则：%10\n"
    ).arg(m_selectedCompanyName)
     .arg(m_allGraphNodes.size())
     .arg(staffDetails)
     .arg(up.name.isEmpty() ? "当事人" : up.name)
     .arg(up.industry.isEmpty() ? "IT与商业投资" : up.industry)
     .arg(up.company.isEmpty() ? "我方主体" : up.company)
     .arg(snap.personalLiquidFund, 0, 'f', 2)
     .arg(snap.personalBorrowIn, 0, 'f', 2)
     .arg(snap.runwayDays)
     .arg(up.aiRules.isEmpty() ? "合情、合法、合理、可以不道德" : up.aiRules);

    QString defaultCompanyRules =
        "请严格结合《鬼谷子》、《易经》、《人性的弱点》、《影响力》、《金钱心理学》五大经世心法，"
        "并严格恪守八字核心真言：『合情、合法、合理、可以不道德』，出具针对【%1】的深度商业战略点评与实操博弈报告：\n\n"
        "【第一部分：当前企业与人员总结】\n"
        "全面总结该企业及其核心骨干人员的身份地位、职位分布、信用星级与历史往来价值，血淋淋指出这家企业及其人员对我当前事业与生意的真实价值（是赋能借力还是低效损耗？）。\n\n"
        "【第二部分：对比综合页面上文财务现状与总总结】\n"
        "严格对比综合页面一开始确立的我方财务现状与全局总总结生存红线，深度推演该公司及其人员在后续业务往来、利益捆绑或合作推进中，对我方可能构成的财务风险、吸血风险或势能差距。\n\n"
        "【第三部分：想破解和商业交往策略】\n"
        "结合五大经世心法，出具狠辣、务实、直击要害的商业实操破局解法：\n"
        "1. **关键人员突破与借力策略**：抓住核心操盘手或关键决策人的软肋与利益诉求，兵不血刃达成合作与借势；\n"
        "2. **预算控制与利益账算计**：严格制定预算控制上限，设立确定性止损红线与利益交换筹码；\n"
        "3. **防坑自保与狠辣制衡解法**：绝不搞圣母心态，提供最管用、最直击要害的商业防御与利益锁死方案！\n\n"
        "【铁律】：\n"
        "- 全文必须严格使用中文输出（若我未要求英文，严禁出现英文段落）；\n"
        "- 建议的处理方案必须恪守『合情、合法、合理、可以不道德』准则，抛开圣母心态，怎么管用怎么破局就怎么来！";

    QString companyRules = AiAdvisorService::getRuleFromAppJson("company_review", defaultCompanyRules);
    if (companyRules.contains("%1")) {
        companyRules = companyRules.arg(m_selectedCompanyName);
    }

    QString prompt =
        "【语言绝对铁律·必须全程纯中文】：从回答的第一个字开始全程严格使用纯中文输出！严禁任何英文开场白（如 \"Here is...\", \"Based on...\"）或英文思考过程！\n\n" +
        headerInfo + "\n\n【商务网·企业深度参谋与商业策略推演任务】：\n" + companyRules +
        "\n\n【再次严正提醒】：请直接使用纯中文出具报告，严禁夹杂任何英文字句！";

    QList<ILlmEngine::EngineChatMessage> msgs;
    msgs.append(ILlmEngine::EngineChatMessage{"system", AiAdvisorService::instance().buildSystemPrompt()});
    msgs.append(ILlmEngine::EngineChatMessage{"user", prompt});

    m_aiHistory.append({"user", prompt});

    engine->chatStreamMessages(
        msgs,
        [this](const QString& token, bool isFinished) {
            if (!token.isEmpty()) {
                emit AiAdvisorService::instance().tokenReceived(token);
            }
            if (isFinished) {
                emit AiAdvisorService::instance().evaluationFinished(m_accumulatedResponse);
            }
        },
        [this](const QString& err) {
            emit AiAdvisorService::instance().evaluationFailed(err);
        }
    );
}

void BusinessNetWidget::onSendConsultQuestion() {
    if (m_isAiGenerating) return;

    QString q = m_aiQuestionEdit->toPlainText().trimmed();
    if (q.isEmpty()) return;

    auto engine = AiAdvisorService::instance().engine();
    if (!engine) {
        CustomDialog::showWarning(this, "提示", "AI 引擎未初始化！");
        return;
    }

    m_aiQuestionEdit->clear();
    m_isAiGenerating = true;
    m_isInitialCompanyGeneration = false;
    m_isLastResponseIncomplete = false;
    m_accumulatedResponse.clear();

    if (m_btnSendQuestion) {
        m_btnSendQuestion->setText("生成中...");
        m_btnSendQuestion->setEnabled(false);
    }
    if (m_aiQuestionEdit) m_aiQuestionEdit->setEnabled(false);
    if (m_btnTopGenerate) {
        m_btnTopGenerate->setEnabled(false);
        m_btnTopGenerate->setText("生成中...");
    }

    QString trimmedLower = q.trimmed().toLower();
    QString promptForLlm = q;
    if (trimmedLower == "继续" || trimmedLower == "请继续" || trimmedLower == "接着写" || trimmedLower == "接着说" || trimmedLower == "继续说") {
        promptForLlm = "请承接你上面未阐述完毕的内容继续往下深入剖析，直接输出后续内容，条理清晰，严禁重复前文已说过的废话。";
    }

    m_aiHistory.append({"user", q});
    renderFullConversation();

    emit AiAdvisorService::instance().evaluationStarted();

    QList<ILlmEngine::EngineChatMessage> msgs;
    msgs.append(ILlmEngine::EngineChatMessage{"system", "你是一位深谙底层人性、商业博弈与财务策划的顶级智囊军师。所有的策划恪守八字铁律：『合情、合法、合理、可以不道德』。紧扣当前公司与推演上下文深入解答，严格使用纯中文回复。"});

    // 历史上下文智能截断与保护：首轮公司全量背景极其重要（保留），同时保留最近几轮问答，杜绝超长多轮导致上下文溢出
    int historySize = m_aiHistory.size();
    if (historySize <= 6) {
        for (int i = 0; i < historySize - 1; ++i) {
            msgs.append(ILlmEngine::EngineChatMessage{m_aiHistory[i].role, m_aiHistory[i].content});
        }
    } else {
        // 保留第 0 轮企业与人脉情报
        msgs.append(ILlmEngine::EngineChatMessage{m_aiHistory[0].role, m_aiHistory[0].content});
        // 保留最近 4 条问答
        for (int i = historySize - 5; i < historySize - 1; ++i) {
            msgs.append(ILlmEngine::EngineChatMessage{m_aiHistory[i].role, m_aiHistory[i].content});
        }
    }
    // 当前轮 prompt
    msgs.append(ILlmEngine::EngineChatMessage{"user", promptForLlm});

    engine->chatStreamMessages(
        msgs,
        [this](const QString& token, bool isFinished) {
            if (!token.isEmpty()) {
                emit AiAdvisorService::instance().tokenReceived(token);
            }
            if (isFinished) {
                emit AiAdvisorService::instance().evaluationFinished(m_accumulatedResponse);
            }
        },
        [this](const QString& err) {
            emit AiAdvisorService::instance().evaluationFailed(err);
        }
    );
}

void BusinessNetWidget::renderFullConversation() {
    if (!m_txtAiReview) return;
    QString fullHtml;

    bool isFirstReport = true;
    for (int i = 0; i < m_aiHistory.size(); ++i) {
        const auto& item = m_aiHistory[i];
        if (item.role == "user") {
            if (isFirstReport) {
                // 首轮 prompt 不显示冗长的系统提示词
                isFirstReport = false;
            } else {
                fullHtml += QString(
                    "<div style='margin-top:14px; margin-bottom:8px; padding:8px 12px; background-color:#202024; border-left:3px solid #00a361; border-radius:4px;'>"
                    "<b style='color:#00d68f; font-size:13px;'>💬 我的咨询：</b><br/>"
                    "<span style='color:#ffffff; font-size:14px; line-height:22px;'>%1</span>"
                    "</div>"
                ).arg(item.content.toHtmlEscaped());
            }
        } else if (item.role == "assistant") {
            fullHtml += formatAiReportToHtml(item.content);
        }
    }

    // 正在生成的最新回复
    if (m_isAiGenerating) {
        if (!m_accumulatedResponse.isEmpty()) {
            fullHtml += formatAiReportToHtml(m_accumulatedResponse);
        } else {
            fullHtml += "<div style='color:#febc2f; font-size:14px; font-weight:bold; line-height:24px; padding:4px;'>AI 军师正在深入分析推演中...</div>";
        }
    } else if (m_isLastResponseIncomplete) {
        fullHtml += QString(
            "<div style='margin-top:14px; margin-bottom:8px; padding:10px 14px; background-color:#2a2516; border:1px solid #febc2f; border-radius:6px;'>"
            "<span style='color:#febc2f; font-size:13px; font-weight:bold;'>⚠️ 本次军师分析已达单次输出长度上限，推演未完待续。</span>"
            "<a href='action:continue' style='color:#00d68f; font-size:13px; font-weight:bold; text-decoration:none; margin-left:12px;'>【👉 点击让军师继续回答】</a>"
            "</div>"
        );
    }

    m_txtAiReview->setHtml(fullHtml);
    m_txtAiReview->verticalScrollBar()->setValue(m_txtAiReview->verticalScrollBar()->maximum());
}

bool BusinessNetWidget::loadCompanyAiCache(int compId) {
    if (compId <= 0) return false;
    QString appDir = QCoreApplication::applicationDirPath();
    QString rootDir = DatabaseManager::instance().projectRootDir();

    QStringList paths = {
        QDir(appDir).filePath(QString("assets/uppic/json/%1.json").arg(compId)),
        QDir(appDir).filePath(QString("../assets/uppic/json/%1.json").arg(compId)),
        QDir(rootDir).filePath(QString("assets/uppic/json/%1.json").arg(compId)),
        QDir(rootDir).filePath(QString("../assets/uppic/json/%1.json").arg(compId)),
        DatabaseManager::instance().resolveAssetPath(QString("assets/uppic/json/%1.json").arg(compId)),
        DatabaseManager::instance().resolveAssetPath(QString("uppic/json/%1.json").arg(compId)),
        QDir(appDir).filePath(QString("assets/uppic/json/%1.txt").arg(compId)),
        QDir(rootDir).filePath(QString("assets/uppic/json/%1.txt").arg(compId))
    };

    QString targetPath;
    for (const auto& p : paths) {
        if (!p.isEmpty() && QFile::exists(p)) {
            targetPath = p;
            break;
        }
    }
    if (targetPath.isEmpty()) return false;

    QFile f(targetPath);
    if (!f.open(QIODevice::ReadOnly)) return false;

    QString loadedContent;
    if (targetPath.endsWith(".txt", Qt::CaseInsensitive)) {
        loadedContent = QString::fromUtf8(f.readAll()).trimmed();
        f.close();
    } else {
        QByteArray data = f.readAll();
        f.close();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("content")) {
                loadedContent = obj["content"].toString().trimmed();
            } else if (obj.contains("first_summary")) {
                loadedContent = obj["first_summary"].toString().trimmed();
            } else if (obj.contains("history") && obj["history"].isArray()) {
                QJsonArray histArr = obj["history"].toArray();
                for (const auto& v : histArr) {
                    QJsonObject o = v.toObject();
                    if (o["role"].toString() == "assistant") {
                        loadedContent = o["content"].toString().trimmed();
                        break;
                    }
                }
            }
        }
    }

    if (loadedContent.isEmpty()) return false;

    m_aiHistory.clear();
    // 核心要求 2：再次进入公司先加载上次的生成内容
    m_aiHistory.append({"assistant", loadedContent});
    m_isAiGenerating = false;
    m_isLastResponseIncomplete = false;
    m_accumulatedResponse = loadedContent;

    if (m_btnTopGenerate) {
        m_btnTopGenerate->setEnabled(true);
        m_btnTopGenerate->setText("重新生成");
    }
    if (m_btnSendQuestion) {
        m_btnSendQuestion->setText("发送");
        m_btnSendQuestion->setEnabled(true);
    }
    if (m_aiQuestionEdit) {
        m_aiQuestionEdit->setEnabled(true);
        m_aiQuestionEdit->clear();
    }

    renderFullConversation();
    return true;
}

void BusinessNetWidget::saveCurrentCompanyAiCache() {
    if (m_selectedCompanyId <= 0) {
        if (!m_selectedCompanyName.isEmpty()) {
            m_selectedCompanyId = DatabaseManager::instance().ensureCompanyExists(m_selectedCompanyName);
        }
    }
    if (m_selectedCompanyId <= 0 || m_aiHistory.isEmpty()) return;

    // 获取本次“生成/重新生成”的企业深度参谋报告
    QString reportContent;
    for (int i = m_aiHistory.size() - 1; i >= 0; --i) {
        if (m_aiHistory[i].role == "assistant") {
            reportContent = m_aiHistory[i].content;
            break;
        }
    }
    if (reportContent.isEmpty()) reportContent = m_accumulatedResponse;
    if (reportContent.trimmed().isEmpty()) return;

    QJsonObject root;
    root["company_id"] = m_selectedCompanyId;
    root["company_name"] = m_selectedCompanyName;
    root["updated_at"] = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    root["content"] = reportContent;
    root["first_summary"] = reportContent;

    QByteArray jsonBytes = QJsonDocument(root).toJson(QJsonDocument::Indented);

    QString appDir = QCoreApplication::applicationDirPath();
    QString rootDir = DatabaseManager::instance().projectRootDir();

    // 严密保存至 cmake-build-debug / cmake-build-release 以及项目 assets 对应目录
    QStringList targetDirs = {
        QDir(appDir).filePath("assets/uppic/json"),
        QDir(appDir).filePath("../assets/uppic/json"),
        QDir(rootDir).filePath("assets/uppic/json"),
        QDir(rootDir).filePath("../assets/uppic/json"),
        DatabaseManager::instance().resolveAssetPath("assets/uppic/json"),
        DatabaseManager::instance().resolveAssetPath("uppic/json")
    };

    QSet<QString> processedDirs;
    for (const auto& dPath : targetDirs) {
        if (dPath.isEmpty()) continue;
        QDir dir(dPath);
        QString absDir = dir.absolutePath();
        if (processedDirs.contains(absDir)) continue;
        processedDirs.insert(absDir);

        dir.mkpath(".");
        QString fullFilePath = dir.filePath(QString("%1.json").arg(m_selectedCompanyId));
        QFile f(fullFilePath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write(jsonBytes);
            f.flush();
            f.close();
            qDebug() << "[AI] Company AI cache successfully written to:" << fullFilePath;
        }
    }
}

QString BusinessNetWidget::formatAiReportToHtml(const QString& rawMarkdown) const {
    QString text = rawMarkdown.trimmed();
    if (text.isEmpty()) return QString();

    if (text.startsWith("“*")) text = text.mid(2).trimmed();
    else if (text.startsWith("\"*")) text = text.mid(2).trimmed();
    else if (text.startsWith("* ")) { /* keep bullet list */ }
    else if (text.startsWith("*")) text = text.mid(1).trimmed();

    if (text.endsWith("*”")) text.chop(2);
    else if (text.endsWith("*\"")) text.chop(2);
    else if (text.endsWith("*") && !text.endsWith("**")) text.chop(1);

    text = text.trimmed();
    if (text.startsWith("【AI实时参谋点评】")) {
        text = text.mid(QString("【AI实时参谋点评】").length()).trimmed();
    }

    // 彻底过滤模型内部思维链 <think>...</think>，杜绝任何英文思考过程外泄
    static const QRegularExpression thinkBlockRe("<think>.*?</think>", QRegularExpression::DotMatchesEverythingOption);
    text.remove(thinkBlockRe);
    if (text.startsWith("<think>")) {
        int closeIdx = text.indexOf("</think>");
        if (closeIdx >= 0) {
            text = text.mid(closeIdx + 8).trimmed();
        } else {
            return "<div style='color:#febc2f; font-size:14px; font-weight:bold; line-height:24px; padding:4px;'>AI 参谋正在深度构思中...</div>";
        }
    }

    const int baseFontPx = 14;
    const int lineHeightPx = 24;
    const int titleFontPx = 16;

    auto formatInlineRich = [](QString s) -> QString {
        s = s.toHtmlEscaped();
        static const QRegularExpression boldRe("\\*\\*(.+?)\\*\\*");
        s.replace(boldRe, "<b style='color:#febc2f; font-weight:900;'>\\1</b>");
        static const QRegularExpression quoteRe("“([^”]{1,24})”");
        s.replace(quoteRe, "<span style='color:#ffd166; font-weight:bold;'>“\\1”</span>");
        static const QRegularExpression moneyRe("(¥\\s*[0-9,]+(?:\\.[0-9]+)?|[+-]?¥\\s*[0-9,]+(?:\\.[0-9]+)?|[0-9,]+(?:\\.[0-9]+)?(?:\\s*~\\s*[0-9,]+(?:\\.[0-9]+)?)?\\s*(?:万元|万|元))");
        s.replace(moneyRe, "<b style='color:#35d28d;'>\\1</b>");
        return s;
    };

    auto pickSectionColor = [](const QString& headingText) -> QString {
        if (headingText.contains("风控") || headingText.contains("红线") || headingText.contains("风险") ||
            headingText.contains("底线") || headingText.contains("警告") || headingText.contains("禁忌") ||
            headingText.contains("严禁") || headingText.contains("止损")) {
            return "#ff6b6b";
        }
        if (headingText.contains("总结") || headingText.contains("结论") || headingText.contains("核心") ||
            headingText.contains("概览") || headingText.contains("综述")) {
            return "#35d28d";
        }
        if (headingText.contains("破局") || headingText.contains("行动") || headingText.contains("方案") ||
            headingText.contains("执行") || headingText.contains("下一步")) {
            return "#38bdf8";
        }
        return "#febc2f";
    };

    QStringList lines = text.split('\n');
    QString html = QString(
        "<div style='font-family:\"Microsoft YaHei UI\",\"Microsoft YaHei\",sans-serif; "
        "font-size:%1px; color:#e1e1e5; line-height:%2px;'>"
    ).arg(baseFontPx).arg(lineHeightPx);

    bool isFirstBlock = true;
    bool inRedlineSection = false;

    static const QRegularExpression mdHeadingRe("^#+\\s*(.+)$");
    static const QRegularExpression cnSectionRe("^([一二三四五六七八九十]+[、．.]|（[一二三四五六七八九十]+）)\\s*(.+)$");
    static const QRegularExpression bracketHeaderOnlyRe("^【([^】]+)】[：:]?\\s*$");
    static const QRegularExpression bracketInlineRe("^【([^】]+)】[：:]?\\s*(.+)$");
    static const QRegularExpression numListRe("^(\\d+[.、．])\\s*(.+)$");
    static const QRegularExpression boldLeadColonRe("^\\*\\*(.+?)\\*\\*\\s*([：:])\\s*(.+)$");
    static const QRegularExpression plainLeadColonRe("^([^：:*]{2,18})([：:])\\s*(.+)$");

    auto renderMarkdownTable = [&](const QStringList& tableLines) -> QString {
        if (tableLines.size() < 2) {
            QString out;
            for (const auto& tl : tableLines) out += QString("<p style='margin:4px 0;'>%1</p>").arg(formatInlineRich(tl));
            return out;
        }
        auto splitCells = [](const QString& line) -> QStringList {
            QString clean = line.trimmed();
            if (clean.startsWith('|')) clean = clean.mid(1);
            if (clean.endsWith('|')) clean.chop(1);
            QStringList rawCols = clean.split('|');
            QStringList cols;
            for (const auto& c : rawCols) cols.append(c.trimmed());
            return cols;
        };

        QStringList headers = splitCells(tableLines[0]);
        int startRow = 1;
        if (tableLines.size() > 1 && tableLines[1].contains("---")) {
            startRow = 2;
        }

        QString tHtml = QString("<table style='width:100%; border-collapse:collapse; margin:10px 0; background-color:#202024; border-radius:4px; font-size:%1px;'>").arg(baseFontPx);
        tHtml += "<thead><tr>";
        for (const QString& h : headers) {
            tHtml += QString("<th style='background-color:#27272a; color:#febc2f; border:1px solid #3f3f46; padding:7px 10px; font-weight:bold; text-align:left;'>%1</th>").arg(formatInlineRich(h));
        }
        tHtml += "</tr></thead><tbody>";

        for (int r = startRow; r < tableLines.size(); ++r) {
            QStringList cells = splitCells(tableLines[r]);
            tHtml += "<tr>";
            for (int c = 0; c < headers.size(); ++c) {
                QString val = (c < cells.size()) ? cells[c] : "";
                tHtml += QString("<td style='border:1px solid #3f3f46; padding:6px 10px; color:#e1e1e5;'>%1</td>").arg(formatInlineRich(val));
            }
            tHtml += "</tr>";
        }
        tHtml += "</tbody></table>";
        return tHtml;
    };

    for (int i = 0; i < lines.size(); ++i) {
        QString rawLine = lines[i];
        QString trimmed = rawLine.trimmed();
        if (trimmed.isEmpty()) continue;

        if (trimmed == "---" || trimmed == "***" || trimmed == "___") {
            html += "<hr style='border:none; border-top:1px solid #454549; margin:10px 0;'/>";
            continue;
        }

        // Markdown 表格解析 (| ... |)
        if (trimmed.startsWith('|') && trimmed.endsWith('|')) {
            QStringList tableLines;
            while (i < lines.size()) {
                QString tLine = lines[i].trimmed();
                if (tLine.startsWith('|') && tLine.endsWith('|')) {
                    tableLines.append(tLine);
                    i++;
                } else {
                    break;
                }
            }
            i--;
            html += renderMarkdownTable(tableLines);
            isFirstBlock = false;
            continue;
        }

        // 引用块解析 (> ...)
        if (trimmed.startsWith('>') || trimmed.startsWith("&gt;")) {
            QString quoteContent = trimmed;
            if (quoteContent.startsWith("&gt;")) quoteContent = quoteContent.mid(4).trimmed();
            else if (quoteContent.startsWith('>')) quoteContent = quoteContent.mid(1).trimmed();
            html += QString(
                "<div style='border-left:3px solid #35d28d; background-color:rgba(53,210,141,0.1); "
                "padding:7px 12px; margin:7px 0 7px 4px; border-radius:0 4px 4px 0; color:#cbf3db; font-size:%1px; line-height:%2px;'>"
                "%3</div>"
            ).arg(baseFontPx).arg(lineHeightPx).arg(formatInlineRich(quoteContent));
            isFirstBlock = false;
            continue;
        }

        // 破折提示/引导尾行 (——...)
        if (trimmed.startsWith("——") || trimmed.startsWith("-- ")) {
            html += QString(
                "<p style='margin:10px 0 8px 4px; color:#38bdf8; font-weight:bold; font-size:%1px; line-height:%2px;'>%3</p>"
            ).arg(baseFontPx).arg(lineHeightPx).arg(formatInlineRich(trimmed));
            isFirstBlock = false;
            continue;
        }

        QRegularExpressionMatch mdMatch = mdHeadingRe.match(trimmed);
        if (mdMatch.hasMatch()) {
            QString cleanTitle = mdMatch.captured(1).trimmed();
            cleanTitle.remove("**");
            QString hColor = pickSectionColor(cleanTitle);
            inRedlineSection = (hColor == "#ff6b6b");
            int topMargin = isFirstBlock ? 2 : 14;
            isFirstBlock = false;
            html += QString(
                "<p style='margin:%1px 0 6px 0; color:%2; font-size:%3px; font-weight:900; line-height:%4px;'>%5</p>"
            ).arg(topMargin).arg(hColor).arg(titleFontPx).arg(lineHeightPx + 2).arg(cleanTitle.toHtmlEscaped());
            continue;
        }

        QRegularExpressionMatch cnMatch = cnSectionRe.match(trimmed);
        if (cnMatch.hasMatch()) {
            QString fullHeading = trimmed;
            fullHeading.remove("**");
            QString hColor = pickSectionColor(fullHeading);
            inRedlineSection = (hColor == "#ff6b6b");
            int topMargin = isFirstBlock ? 2 : 14;
            isFirstBlock = false;
            html += QString(
                "<p style='margin:%1px 0 6px 0; color:%2; font-size:%3px; font-weight:900; line-height:%4px;'>%5</p>"
            ).arg(topMargin).arg(hColor).arg(titleFontPx).arg(lineHeightPx + 2).arg(fullHeading.toHtmlEscaped());
            continue;
        }

        QRegularExpressionMatch brOnlyMatch = bracketHeaderOnlyRe.match(trimmed);
        if (brOnlyMatch.hasMatch()) {
            QString innerTitle = brOnlyMatch.captured(1).trimmed();
            QString hColor = pickSectionColor(innerTitle);
            inRedlineSection = (hColor == "#ff6b6b");
            int topMargin = isFirstBlock ? 2 : 12;
            isFirstBlock = false;
            html += QString(
                "<p style='margin:%1px 0 6px 0; color:%2; font-size:%3px; font-weight:900; line-height:%4px;'>【%5】</p>"
            ).arg(topMargin).arg(hColor).arg(titleFontPx).arg(lineHeightPx + 2).arg(innerTitle.toHtmlEscaped());
            continue;
        }

        QRegularExpressionMatch brInlineMatch = bracketInlineRe.match(trimmed);
        if (brInlineMatch.hasMatch()) {
            QString tag = brInlineMatch.captured(1).trimmed();
            QString body = brInlineMatch.captured(2).trimmed();
            QString tagColor = pickSectionColor(tag);
            int topMargin = isFirstBlock ? 2 : 10;
            isFirstBlock = false;
            html += QString(
                "<p style='margin:%1px 0 8px 0; line-height:%2px;'>"
                "<b style='color:%3; font-size:%4px; font-weight:900;'>【%5】</b>"
                "<span style='color:#ffffff; font-weight:bold;'>%6</span></p>"
            ).arg(topMargin).arg(lineHeightPx).arg(tagColor).arg(baseFontPx + 1)
             .arg(tag.toHtmlEscaped(), formatInlineRich(body));
            continue;
        }

        QRegularExpressionMatch numMatch = numListRe.match(trimmed);
        if (numMatch.hasMatch()) {
            isFirstBlock = false;
            QString idxStr = numMatch.captured(1);
            QString rest = numMatch.captured(2).trimmed();
            QString numColor = inRedlineSection ? "#ff6b6b" : "#35d28d";
            QString formattedRest;

            QRegularExpressionMatch boldLead = boldLeadColonRe.match(rest);
            if (boldLead.hasMatch()) {
                QString leadTitle = boldLead.captured(1).trimmed();
                QString colon = boldLead.captured(2);
                QString body = boldLead.captured(3).trimmed();
                bool isRedItem = inRedlineSection || leadTitle.startsWith("不") || leadTitle.contains("严禁") ||
                                 leadTitle.contains("红线") || leadTitle.contains("切勿") || leadTitle.contains("风险");
                QString leadColor = isRedItem ? "#ff7875" : "#febc2f";
                if (isRedItem) numColor = "#ff6b6b";
                formattedRest = QString("<b style='color:%1; font-weight:900;'>%2%3</b>%4")
                                    .arg(leadColor, leadTitle.toHtmlEscaped(), colon, formatInlineRich(body));
            } else {
                QRegularExpressionMatch plainLead = plainLeadColonRe.match(rest);
                if (plainLead.hasMatch()) {
                    QString leadTitle = plainLead.captured(1).trimmed();
                    QString colon = plainLead.captured(2);
                    QString body = plainLead.captured(3).trimmed();
                    bool isRedItem = inRedlineSection || leadTitle.startsWith("不") || leadTitle.contains("严禁") ||
                                     leadTitle.contains("红线") || leadTitle.contains("切勿") || leadTitle.contains("风险");
                    QString leadColor = isRedItem ? "#ff7875" : "#febc2f";
                    if (isRedItem) numColor = "#ff6b6b";
                    formattedRest = QString("<b style='color:%1; font-weight:900;'>%2%3</b>%4")
                                        .arg(leadColor, leadTitle.toHtmlEscaped(), colon, formatInlineRich(body));
                } else {
                    formattedRest = formatInlineRich(rest);
                }
            }

            html += QString(
                "<p style='margin:7px 0 7px 6px; line-height:%1px; color:#e1e1e5;'>"
                "<b style='color:%2; font-weight:900;'>%3 </b>%4</p>"
            ).arg(lineHeightPx).arg(numColor, idxStr.toHtmlEscaped(), formattedRest);
            continue;
        }

        if (trimmed.startsWith("* ") || trimmed.startsWith("- ") || trimmed.startsWith("• ")) {
            isFirstBlock = false;
            QString itemContent = trimmed.mid(2).trimmed();
            QString formattedRest;
            QRegularExpressionMatch boldLead = boldLeadColonRe.match(itemContent);
            if (boldLead.hasMatch()) {
                QString leadTitle = boldLead.captured(1).trimmed();
                QString colon = boldLead.captured(2);
                QString body = boldLead.captured(3).trimmed();
                formattedRest = QString("<b style='color:#febc2f; font-weight:900;'>%1%2</b>%3")
                                    .arg(leadTitle.toHtmlEscaped(), colon, formatInlineRich(body));
            } else {
                QRegularExpressionMatch plainLead = plainLeadColonRe.match(itemContent);
                if (plainLead.hasMatch()) {
                    QString leadTitle = plainLead.captured(1).trimmed();
                    QString colon = plainLead.captured(2);
                    QString body = plainLead.captured(3).trimmed();
                    formattedRest = QString("<b style='color:#febc2f; font-weight:900;'>%1%2</b>%3")
                                        .arg(leadTitle.toHtmlEscaped(), colon, formatInlineRich(body));
                } else {
                    formattedRest = formatInlineRich(itemContent);
                }
            }

            html += QString(
                "<p style='margin:7px 0 7px 6px; line-height:%1px; color:#e1e1e5;'>"
                "<b style='color:#35d28d; font-weight:900;'>• </b>%2</p>"
            ).arg(lineHeightPx).arg(formattedRest);
            continue;
        }

        int topMargin = isFirstBlock ? 2 : 7;
        isFirstBlock = false;
        QString paraHtml = formatInlineRich(trimmed);
        if (trimmed.endsWith("：") || trimmed.endsWith(":")) {
            html += QString(
                "<p style='margin:%1px 0 6px 0; line-height:%2px; color:#f0f0f4; font-weight:bold;'>%3</p>"
            ).arg(topMargin).arg(lineHeightPx).arg(paraHtml);
        } else {
            html += QString(
                "<p style='margin:%1px 0 6px 0; line-height:%2px; color:#e1e1e5;'>%3</p>"
            ).arg(topMargin).arg(lineHeightPx).arg(paraHtml);
        }
    }

    html += "</div>";
    return html;
}
