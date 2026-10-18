#include "GeneralDashboardWidget.h"
#include "UnifiedTransactionDialog.h"
#include "UserProfileDialog.h"
#include "CompanyDialog.h"
#include "CustomDialog.h"
#include "UiIcons.h"
#include "../ai/AiAdvisorService.h"
#include "../ai/OllamaHttpEngine.h"

#include <QHBoxLayout>
#include <QFormLayout>
#include <QFrame>
#include <QProgressBar>
#include <QScrollBar>
#include <QMessageBox>
#include <QMenu>
#include <QAction>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPainter>
#include <QPainterPath>
#include <QGraphicsEffect>
#include <QRegularExpression>
#include <QEvent>
#include <QDate>
#include <QTimer>
#include <QPointer>

namespace {

// 文字描边渲染特效：AI 回复文字黑色描边 (#000000)，我的回复文字白色描边 (#ffffff)
class TextOutlineEffect : public QGraphicsEffect {
public:
    explicit TextOutlineEffect(const QColor& outlineColor, qreal thickness = 1.0, QObject* parent = nullptr)
        : QGraphicsEffect(parent), m_outlineColor(outlineColor), m_thickness(thickness) {}

    QRectF boundingRectFor(const QRectF& rect) const override {
        return rect.adjusted(-2, -2, 2, 2);
    }

protected:
    void draw(QPainter* painter) override {
        QPoint offset;
        const QPixmap src = sourcePixmap(Qt::LogicalCoordinates, &offset, QGraphicsEffect::PadToEffectiveBoundingRect);
        if (src.isNull()) return;

        QPixmap silhouette(src.size());
        silhouette.setDevicePixelRatio(src.devicePixelRatio());
        silhouette.fill(Qt::transparent);
        {
            QPainter sp(&silhouette);
            sp.setRenderHint(QPainter::SmoothPixmapTransform, true);
            sp.drawPixmap(0, 0, src);
            sp.setCompositionMode(QPainter::CompositionMode_SourceIn);
            sp.fillRect(0, 0, silhouette.width(), silhouette.height(), m_outlineColor);
        }

        const QPointF basePos(offset.x(), offset.y());
        const qreal d = m_thickness;
        const QPointF dirs[8] = {
            {-d, -d}, {0.0, -d}, {d, -d},
            {-d, 0.0},           {d, 0.0},
            {-d,  d}, {0.0,  d}, {d,  d}
        };
        for (const QPointF& dir : dirs) {
            painter->drawPixmap(basePos + dir, silhouette);
        }
        painter->drawPixmap(basePos, src);
    }

private:
    QColor m_outlineColor;
    qreal m_thickness;
};

// AI 结构化咨询回复排版引擎：加大字号 (+2px -> 16px)，拉大段间距与行高，支持章节标题、加粗、列表与红线/重点颜色区分
QString formatAiChatToRichHtml(const QString& rawText, int baseFontPx = 16) {
    QString text = rawText.trimmed();
    if (text.isEmpty()) return QString();

    const int lineHeightPx = baseFontPx + 10; // 16px 字号对应 26px 行高，疏朗易读
    const int titleFontPx = baseFontPx + 2;   // 章节标题 18px

    auto formatInlineRich = [](QString s) -> QString {
        s = s.toHtmlEscaped();
        // 1. Markdown **粗体** -> 醒目暖金色加粗
        static const QRegularExpression boldRe("\\*\\*(.+?)\\*\\*");
        s.replace(boldRe, "<b style='color:#febc2f; font-weight:900;'>\\1</b>");
        // 2. 中文双引号内重点词 “...” -> 暖白高亮
        static const QRegularExpression quoteRe("“([^”]{1,24})”");
        s.replace(quoteRe, "<span style='color:#ffd166; font-weight:bold;'>“\\1”</span>");
        // 3. 金额与关键数字高亮
        static const QRegularExpression moneyRe("(¥\\s*[0-9,]+(?:\\.[0-9]+)?|[0-9,]+(?:\\.[0-9]+)?(?:\\s*~\\s*[0-9,]+(?:\\.[0-9]+)?)?\\s*(?:万元|万|元))");
        s.replace(moneyRe, "<b style='color:#35d28d;'>\\1</b>");
        return s;
    };

    auto pickSectionColor = [](const QString& headingText) -> QString {
        if (headingText.contains("风控") || headingText.contains("红线") || headingText.contains("风险") ||
            headingText.contains("底线") || headingText.contains("警告") || headingText.contains("禁忌") ||
            headingText.contains("严禁") || headingText.contains("止损")) {
            return "#ff6b6b"; // 风险/红线章节：醒目珊瑚红
        }
        if (headingText.contains("总结") || headingText.contains("结论") || headingText.contains("核心") ||
            headingText.contains("概览") || headingText.contains("综述")) {
            return "#35d28d"; // 总结/核心结论：翠绿高亮
        }
        if (headingText.contains("破局") || headingText.contains("行动") || headingText.contains("方案") ||
            headingText.contains("执行") || headingText.contains("下一步")) {
            return "#38bdf8"; // 行动/破局：亮天蓝
        }
        return "#febc2f"; // 默认章节标题：暖金黄
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
            html += "<hr style='border:none; border-top:1px solid #454549; margin:12px 0;'/>";
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
            i--; // 回退一行保持外层循环步进
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

        // 1. Markdown 标题 (# / ## / ### / ####)
        QRegularExpressionMatch mdMatch = mdHeadingRe.match(trimmed);
        if (mdMatch.hasMatch()) {
            QString cleanTitle = mdMatch.captured(1).trimmed();
            cleanTitle.remove("**");
            QString hColor = pickSectionColor(cleanTitle);
            inRedlineSection = (hColor == "#ff6b6b");
            int topMargin = isFirstBlock ? 2 : 16;
            isFirstBlock = false;
            html += QString(
                "<p style='margin:%1px 0 8px 0; color:%2; font-size:%3px; font-weight:900; line-height:%4px;'>%5</p>"
            ).arg(topMargin).arg(hColor).arg(titleFontPx).arg(lineHeightPx + 2).arg(cleanTitle.toHtmlEscaped());
            continue;
        }

        // 2. 中文大章节标题 (如：“四、风控红线声明”、“一、风险识别与现状评估”)
        QRegularExpressionMatch cnMatch = cnSectionRe.match(trimmed);
        if (cnMatch.hasMatch()) {
            QString fullHeading = trimmed;
            fullHeading.remove("**");
            QString hColor = pickSectionColor(fullHeading);
            inRedlineSection = (hColor == "#ff6b6b");
            int topMargin = isFirstBlock ? 2 : 16;
            isFirstBlock = false;
            html += QString(
                "<p style='margin:%1px 0 8px 0; color:%2; font-size:%3px; font-weight:900; line-height:%4px;'>%5</p>"
            ).arg(topMargin).arg(hColor).arg(titleFontPx).arg(lineHeightPx + 2).arg(fullHeading.toHtmlEscaped());
            continue;
        }

        // 3. 独占一行的【章节标题】 (如：【AI参谋收入与业务建议】、【简要总结】)
        QRegularExpressionMatch brOnlyMatch = bracketHeaderOnlyRe.match(trimmed);
        if (brOnlyMatch.hasMatch()) {
            QString innerTitle = brOnlyMatch.captured(1).trimmed();
            QString hColor = pickSectionColor(innerTitle);
            inRedlineSection = (hColor == "#ff6b6b");
            int topMargin = isFirstBlock ? 2 : 14;
            isFirstBlock = false;
            html += QString(
                "<p style='margin:%1px 0 8px 0; color:%2; font-size:%3px; font-weight:900; line-height:%4px;'>【%5】</p>"
            ).arg(topMargin).arg(hColor).arg(titleFontPx).arg(lineHeightPx + 2).arg(innerTitle.toHtmlEscaped());
            continue;
        }

        // 4. 同行带内容的【简要总结】...
        QRegularExpressionMatch brInlineMatch = bracketInlineRe.match(trimmed);
        if (brInlineMatch.hasMatch()) {
            QString tag = brInlineMatch.captured(1).trimmed();
            QString body = brInlineMatch.captured(2).trimmed();
            QString tagColor = pickSectionColor(tag);
            int topMargin = isFirstBlock ? 2 : 12;
            isFirstBlock = false;
            html += QString(
                "<p style='margin:%1px 0 10px 0; line-height:%2px;'>"
                "<b style='color:%3; font-size:%4px; font-weight:900;'>【%5】</b>"
                "<span style='color:#ffffff; font-weight:bold;'>%6</span></p>"
            ).arg(topMargin).arg(lineHeightPx).arg(tagColor).arg(baseFontPx + 1)
             .arg(tag.toHtmlEscaped(), formatInlineRich(body));
            continue;
        }

        // 5. 有序列表 (如：“1. **不新增个人债务**：无论谁出主意...”)
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
                "<p style='margin:9px 0 9px 8px; line-height:%1px; color:#e1e1e5;'>"
                "<b style='color:%2; font-weight:900;'>%3 </b>%4</p>"
            ).arg(lineHeightPx).arg(numColor, idxStr.toHtmlEscaped(), formattedRest);
            continue;
        }

        // 6. 无序列表 (* / - / •)
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
                "<p style='margin:8px 0 8px 8px; line-height:%1px; color:#e1e1e5;'>"
                "<b style='color:#35d28d; font-weight:900;'>• </b>%2</p>"
            ).arg(lineHeightPx).arg(formattedRest);
            continue;
        }

        // 7. 普通段落
        int topMargin = isFirstBlock ? 2 : 8;
        isFirstBlock = false;
        QString paraHtml = formatInlineRich(trimmed);
        if (trimmed.endsWith("：") || trimmed.endsWith(":")) {
            html += QString(
                "<p style='margin:%1px 0 8px 0; line-height:%2px; color:#f0f0f4; font-weight:bold;'>%3</p>"
            ).arg(topMargin).arg(lineHeightPx).arg(paraHtml);
        } else {
            html += QString(
                "<p style='margin:%1px 0 8px 0; line-height:%2px; color:#e1e1e5;'>%3</p>"
            ).arg(topMargin).arg(lineHeightPx).arg(paraHtml);
        }
    }

    html += "</div>";
    return html;
}
} // namespace

// 带左右三角箭头的聊天气泡组件 (AI 左侧三角 #2f2f30 + 黑色描边, 用户 右侧三角 #35d28d + 白色描边, 字号加大2px为16px)
class ChatBubbleWidget : public QWidget {
public:
    ChatBubbleWidget(const QString& text, bool isAi, QWidget* parent = nullptr)
        : QWidget(parent), m_isAi(isAi) {
        setAttribute(Qt::WA_TranslucentBackground, true);
        auto* layout = new QVBoxLayout(this);
        const int arrowWidth = 8;
        if (m_isAi) {
            layout->setContentsMargins(arrowWidth + 16, 14, 16, 14);
        } else {
            layout->setContentsMargins(16, 12, arrowWidth + 16, 12);
        }

        QString textColor = m_isAi ? "#e1e1e5" : "#161616";
        QString html;
        if (m_isAi) {
            html = formatAiChatToRichHtml(text, 16);
        } else {
            QString escaped = text.toHtmlEscaped().replace("\n", "<br/>");
            html = QString(
                "<p style='margin: 0px; color: #161616; font-size: 16px; font-weight: 700; line-height: 24px;'>%1</p>"
            ).arg(escaped);
        }

        m_label = new QLabel(this);
        m_label->setTextFormat(Qt::RichText);
        m_label->setText(html);
        m_label->setWordWrap(true);
        m_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_label->setStyleSheet(
            QString("QLabel { background: transparent; color: %1; font-size: 16px; border: none; padding: 2px; }").arg(textColor)
        );

        // AI回复：文字黑色描边；我的回复：文字白色描边
        QColor strokeColor = m_isAi ? QColor(0, 0, 0, 235) : QColor(69, 246, 168, 240);
        m_label->setGraphicsEffect(new TextOutlineEffect(strokeColor, 1.0, m_label));

        layout->addWidget(m_label);
    }

    void updateText(const QString& rawText) {
        if (!m_label) return;
        QString html;
        if (m_isAi) {
            html = formatAiChatToRichHtml(rawText, 16);
        } else {
            QString escaped = rawText.toHtmlEscaped().replace("\n", "<br/>");
            html = QString(
                "<p style='margin: 0px; color: #161616; font-size: 16px; font-weight: 700; line-height: 24px;'>%1</p>"
            ).arg(escaped);
        }
        m_label->setText(html);
    }

    bool isAiBubble() const { return m_isAi; }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);

        const double arrowW = 8.0;
        const double arrowHalfH = 6.0;
        const double radius = 5.0;
        const double arrowCenterY = qMin(25.0, height() * 0.5);

        QColor bg = m_isAi ? QColor("#2f2f30") : QColor("#35d28d");
        painter.setBrush(bg);

        if (m_isAi) {
            QRectF bodyRect(arrowW, 0, width() - arrowW, height());
            painter.drawRoundedRect(bodyRect, radius, radius);

            QPolygonF triangle;
            triangle << QPointF(arrowW + 1.0, arrowCenterY - arrowHalfH)
                     << QPointF(0.0, arrowCenterY)
                     << QPointF(arrowW + 1.0, arrowCenterY + arrowHalfH);
            painter.drawPolygon(triangle);
        } else {
            QRectF bodyRect(0, 0, width() - arrowW, height());
            painter.drawRoundedRect(bodyRect, radius, radius);

            QPolygonF triangle;
            triangle << QPointF(width() - arrowW - 1.0, arrowCenterY - arrowHalfH)
                     << QPointF(width(), arrowCenterY)
                     << QPointF(width() - arrowW - 1.0, arrowCenterY + arrowHalfH);
            painter.drawPolygon(triangle);
        }
    }

private:
    bool m_isAi{true};
    QLabel* m_label{nullptr};
};

GeneralDashboardWidget::GeneralDashboardWidget(QWidget* parent) : QWidget(parent) {
    m_streamingThrottleTimer = new QTimer(this);
    m_streamingThrottleTimer->setInterval(60);
    connect(m_streamingThrottleTimer, &QTimer::timeout, this, [this]() {
        if (m_streamingDirty && m_streamingAiBubble) {
            m_streamingDirty = false;
            m_streamingAiBubble->updateText(m_streamingAiText);
            if (m_chatScroll && m_chatScroll->verticalScrollBar()) {
                m_chatScroll->verticalScrollBar()->setValue(m_chatScroll->verticalScrollBar()->maximum());
            }
        }
    });

    setupUi();
    refreshData();
}

void GeneralDashboardWidget::setupUi() {
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // ==================== Column 1: 收支概览 ====================
    auto* col2 = new QWidget(this);
    col2->setFixedWidth(300);
    col2->setObjectName("macCard");
    setupExpenseOverviewColumn(col2);
    mainLayout->addWidget(col2);

    // 竖分割线 1
    auto* vline1 = new QFrame(this);
    vline1->setObjectName("vDivider");
    vline1->setFixedWidth(1);
    vline1->setStyleSheet("background-color: #28282b; border: none;");
    mainLayout->addWidget(vline1);

    // ==================== Column 2: AI 人生规划 ====================
    auto* col3 = new QWidget(this);
    col3->setObjectName("macCard");
    setupAiChatColumn(col3);
    mainLayout->addWidget(col3, 1);

    // 竖分割线 2
    auto* vline2 = new QFrame(this);
    vline2->setObjectName("vDivider");
    vline2->setFixedWidth(1);
    vline2->setStyleSheet("background-color: #28282b; border: none;");
    mainLayout->addWidget(vline2);

    // ==================== Column 3: 个人名片与财务/资产看板 ====================
    auto* col4 = new QWidget(this);
    col4->setFixedWidth(330);
    col4->setObjectName("macCard");
    setupProfileFinanceColumn(col4);
    mainLayout->addWidget(col4);
}

void GeneralDashboardWidget::setupExpenseOverviewColumn(QWidget* parent) {
    auto* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    // 1. 顶部切换与添加按钮 (带明确背景色)
    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(8);

    QString tabBtnStyle =
        "QPushButton { background-color: #1c1c1f; color: #8e8e93; border: 1px solid #28282b; border-radius: 3px; padding: 5px 16px; font-size: 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #242428; color: #e1e1e5; }"
        "QPushButton:checked { background-color: #00a361; color: #ffffff; border-color: #00a361; font-weight: bold; }";

    m_btnExpenseTab = new QPushButton("支出", parent);
    m_btnExpenseTab->setCheckable(true);
    m_btnExpenseTab->setChecked(true);
    m_btnExpenseTab->setCursor(Qt::PointingHandCursor);
    m_btnExpenseTab->setStyleSheet(tabBtnStyle);

    m_btnIncomeTab = new QPushButton("收入", parent);
    m_btnIncomeTab->setCheckable(true);
    m_btnIncomeTab->setCursor(Qt::PointingHandCursor);
    m_btnIncomeTab->setStyleSheet(tabBtnStyle);

    connect(m_btnExpenseTab, &QPushButton::clicked, this, [this]() {
        m_btnExpenseTab->setChecked(true);
        m_btnIncomeTab->setChecked(false);
        onToggleExpenseIncome(true);
    });
    connect(m_btnIncomeTab, &QPushButton::clicked, this, [this]() {
        m_btnIncomeTab->setChecked(true);
        m_btnExpenseTab->setChecked(false);
        onToggleExpenseIncome(false);
    });

    m_btnAddTrans = new QPushButton("+添加", parent);
    m_btnAddTrans->setObjectName("flatTextBtn");
    m_btnAddTrans->setCursor(Qt::PointingHandCursor);
    connect(m_btnAddTrans, &QPushButton::clicked, this, &GeneralDashboardWidget::onAddTransactionClicked);

    topRow->addWidget(m_btnExpenseTab);
    topRow->addWidget(m_btnIncomeTab);
    topRow->addStretch();
    topRow->addWidget(m_btnAddTrans);
    layout->addLayout(topRow);

    // 顶部按钮下边缘分割线 (#28282b)
    auto* topDivider = new QFrame(parent);
    topDivider->setFixedHeight(1);
    topDivider->setStyleSheet("background-color: #28282b; border: none;");
    layout->addWidget(topDivider);

    // 2. 本月收支汇总标头
    m_monthlyTotalLabel = new QLabel("本月总支出: ¥ 0.00", parent);
    m_monthlyTotalLabel->setStyleSheet("color: #8e8e93; font-size: 12px;");
    layout->addWidget(m_monthlyTotalLabel);

    // 3. 分类柱状进度条列表 (可滚动)
    auto* scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("background: transparent; border: none;");

    auto* scrollContent = new QWidget(scroll);
    scrollContent->setStyleSheet("background: transparent;");
    m_categoryListLayout = new QVBoxLayout(scrollContent);
    m_categoryListLayout->setContentsMargins(0, 0, 0, 0);
    m_categoryListLayout->setSpacing(10);
    m_categoryListLayout->addStretch();

    scroll->setWidget(scrollContent);
    layout->addWidget(scroll, 1);
}

void GeneralDashboardWidget::setupAiChatColumn(QWidget* parent) {
    auto* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    // 1. 标题头: AI 人生规划 + 清空 (#fa5151)
    auto* headerBox = new QHBoxLayout();
    auto* titleLbl = new QLabel("AI 人生规划", parent);
    titleLbl->setStyleSheet("color: #e1e1e5; font-size: 14px; font-weight: 900;");

    auto* btnClear = new QPushButton("清空", parent);
    btnClear->setCursor(Qt::PointingHandCursor);
    btnClear->setStyleSheet(
        "QPushButton { background: transparent; color: #fa5151; border: none; border-radius: 3px; padding: 2px 6px; font-size: 12px; }"
        "QPushButton:hover { background-color: #361c1f; }"
    );
    connect(btnClear, &QPushButton::clicked, this, &GeneralDashboardWidget::onClearAiChat);

    headerBox->addWidget(titleLbl);
    headerBox->addStretch();
    headerBox->addWidget(btnClear);
    layout->addLayout(headerBox);

    // AI 人生规划 与 清空 下边缘分割线 (#28282b)
    auto* headerDivider = new QFrame(parent);
    headerDivider->setFixedHeight(1);
    headerDivider->setStyleSheet("background-color: #28282b; border: none;");
    layout->addWidget(headerDivider);

    // 2. 对话流滚动区域
    m_chatScroll = new QScrollArea(parent);
    m_chatScroll->setWidgetResizable(true);
    m_chatScroll->setStyleSheet("background: transparent; border: none;");
    m_chatScroll->viewport()->installEventFilter(this);

    m_chatContentWidget = new QWidget(m_chatScroll);
    m_chatContentWidget->setStyleSheet("background: transparent;");
    m_chatLayout = new QVBoxLayout(m_chatContentWidget);
    m_chatLayout->setContentsMargins(4, 10, 4, 10);
    m_chatLayout->setSpacing(18);
    m_chatLayout->addStretch();

    m_chatScroll->setWidget(m_chatContentWidget);
    layout->addWidget(m_chatScroll, 1);

    // 3. 底部提问外框容器 (参考图1: 圆角边框容器 + 右下角带背景色的“发送”按钮)
    auto* inputFrame = new QFrame(parent);
    inputFrame->setFixedHeight(110);
    inputFrame->setStyleSheet(
        "QFrame { background-color: #141416; border: 1px solid #28282b; border-radius: 8px; }"
    );
    auto* inputBoxLayout = new QVBoxLayout(inputFrame);
    inputBoxLayout->setContentsMargins(14, 10, 14, 10);
    inputBoxLayout->setSpacing(6);

    m_chatInput = new QLineEdit(inputFrame);
    m_chatInput->setPlaceholderText("请输入AI咨询....");
    m_chatInput->setStyleSheet(
        "QLineEdit { background: transparent; border: none; color: #e1e1e5; font-size: 13px; padding: 0px; }"
    );
    connect(m_chatInput, &QLineEdit::returnPressed, this, &GeneralDashboardWidget::onSendAiPrompt);

    auto* sendBtnRow = new QHBoxLayout();
    sendBtnRow->addStretch();
    m_btnSendChat = new QPushButton("发送", inputFrame);
    m_btnSendChat->setCursor(Qt::PointingHandCursor);
    m_btnSendChat->setFixedSize(76, 30);
    m_btnSendChat->setStyleSheet(
        "QPushButton { background-color: #2f2f30; color: #e1e1e5; border: none; border-radius: 4px; font-size: 12px; font-weight: bold; padding: 0px 8px; }"
        "QPushButton:hover { background-color: #3d3d40; color: #ffffff; }"
        "QPushButton:pressed { background-color: #00a361; color: #ffffff; }"
    );
    connect(m_btnSendChat, &QPushButton::clicked, this, &GeneralDashboardWidget::onSendAiPrompt);
    sendBtnRow->addWidget(m_btnSendChat);

    inputBoxLayout->addWidget(m_chatInput);
    inputBoxLayout->addStretch();
    inputBoxLayout->addLayout(sendBtnRow);
    layout->addWidget(inputFrame);

    // 打开软件时先检查模型情况：若未启动显示提示，若正常则按实际数据库出具综合评价
    QTimer::singleShot(150, this, [this]() { checkAndTriggerInitialAiState(false); });

    connect(&AiAdvisorService::instance(), &AiAdvisorService::evaluationCanceled, this, [this]() {
        disconnectAiSignals();
        resetSendButtonState();
    });
}

void GeneralDashboardWidget::setupProfileFinanceColumn(QWidget* parent) {
    auto* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    auto* scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("background: transparent; border: none;");

    auto* contentWidget = new QWidget(scroll);
    contentWidget->setStyleSheet("background: transparent;");
    auto* contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(10);

    // ==================== Card 1: 个人名片 (严格按图2: 左 30% | 右 70%) ====================
    auto* profileHeader = new QHBoxLayout();
    m_profileNameLabel = new QLabel("陈恩华", contentWidget);
    m_profileNameLabel->setStyleSheet("font-size: 14px; font-weight: 900; color: #e1e1e5;");

    auto* btnEditProfile = new QPushButton("编辑", contentWidget);
    btnEditProfile->setCursor(Qt::PointingHandCursor);
    btnEditProfile->setStyleSheet(
        "QPushButton { background: transparent; color: #8e8e93; border: none; border-radius: 3px; padding: 2px 6px; font-size: 12px; }"
        "QPushButton:hover { background-color: #28282b; color: #e1e1e5; }"
    );
    connect(btnEditProfile, &QPushButton::clicked, this, &GeneralDashboardWidget::onEditProfileClicked);

    profileHeader->addWidget(m_profileNameLabel);
    profileHeader->addStretch();
    profileHeader->addWidget(btnEditProfile);
    contentLayout->addLayout(profileHeader);

    // 左 30% : 右 70% 行构造器
    auto make30_70Row = [contentWidget, contentLayout](const QString& labelText, QLabel*& valueLabel, QPushButton* rightActionBtn = nullptr) {
        auto* row = new QHBoxLayout();
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(0);

        auto* l = new QLabel(labelText, contentWidget);
        l->setStyleSheet("color: #8e8e93; font-size: 12px;");
        l->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

        auto* rightBox = new QWidget(contentWidget);
        rightBox->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        auto* rightLayout = new QHBoxLayout(rightBox);
        rightLayout->setContentsMargins(0, 0, 0, 0);
        rightLayout->setSpacing(4);

        valueLabel = new QLabel("—", rightBox);
        valueLabel->setStyleSheet("color: #e1e1e5; font-size: 12px;");
        valueLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        rightLayout->addWidget(valueLabel, 1);

        if (rightActionBtn) {
            rightLayout->addWidget(rightActionBtn);
        }

        row->addWidget(l, 3);        // 左 30%
        row->addWidget(rightBox, 7); // 右 70%
        contentLayout->addLayout(row);
    };

    make30_70Row("电话", m_profilePhoneLabel);
    make30_70Row("地址", m_profileAddressLabel);
    make30_70Row("行业", m_profileIndustryLabel);
    make30_70Row("职务", m_profileTitleLabel);

    // 关系 几位 和 公司 几家 + 上下调换位置
    make30_70Row("关系", m_profileFriendsLabel);

    auto* btnAddComp = new QPushButton("+", contentWidget);
    btnAddComp->setCursor(Qt::PointingHandCursor);
    btnAddComp->setFixedSize(20, 20);
    btnAddComp->setStyleSheet(
        "QPushButton { background: transparent; color: #00a361; border: none; font-size: 15px; font-weight: bold; padding: 0px; }"
        "QPushButton:hover { background-color: #192b22; border-radius: 3px; }"
    );
    connect(btnAddComp, &QPushButton::clicked, this, &GeneralDashboardWidget::onAddCompanyClicked);
    make30_70Row("公司", m_profileCompanyLabel, btnAddComp);

    // 在“公司”下方显示我拥有的公司排序列表
    m_companyListLayout = new QVBoxLayout();
    m_companyListLayout->setContentsMargins(0, 2, 0, 4);
    m_companyListLayout->setSpacing(6);
    contentLayout->addLayout(m_companyListLayout);

    // 分割线 1
    auto* line1 = new QFrame(contentWidget);
    line1->setFixedHeight(1);
    line1->setStyleSheet("background-color: #28282b; border: none;");
    contentLayout->addWidget(line1);

    // ==================== Card 2: 个人财务 & 公司财务 (分开总结) ====================
    auto* pFinHeader = new QHBoxLayout();
    auto* pFinTitle = new QLabel("个人财务", contentWidget);
    pFinTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #e1e1e5;");

    m_personalFinanceStatusPill = new QLabel("正向", contentWidget);
    m_personalFinanceStatusPill->setObjectName("statusPillGreen");

    pFinHeader->addWidget(pFinTitle);
    pFinHeader->addStretch();
    pFinHeader->addWidget(m_personalFinanceStatusPill);
    contentLayout->addLayout(pFinHeader);

    auto makeSingleRow = [&](const QString& labelText, QLabel*& valLabel) {
        auto* rowWidget = new QWidget(contentWidget);
        auto* rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 2, 0, 2);
        rowLayout->setSpacing(6);

        auto* titleLbl = new QLabel(labelText, rowWidget);
        titleLbl->setStyleSheet("color: #8e8e93; font-size: 12px;");

        valLabel = new QLabel("¥ 0.00", rowWidget);
        valLabel->setStyleSheet("color: #e1e1e5; font-size: 13px; font-weight: bold;");

        rowLayout->addWidget(titleLbl);
        rowLayout->addWidget(valLabel);
        rowLayout->addStretch();
        contentLayout->addWidget(rowWidget);
    };

    auto makeTwoColRow = [&](const QString& t1, QLabel*& l1, const QString& t2, QLabel*& l2) {
        auto* rowWidget = new QWidget(contentWidget);
        auto* rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 2, 0, 2);
        rowLayout->setSpacing(10);

        auto* leftWidget = new QWidget(rowWidget);
        auto* leftLayout = new QHBoxLayout(leftWidget);
        leftLayout->setContentsMargins(0, 0, 0, 0);
        leftLayout->setSpacing(6);
        auto* lbl1 = new QLabel(t1, leftWidget);
        lbl1->setStyleSheet("color: #8e8e93; font-size: 12px;");
        l1 = new QLabel("¥ 0.00", leftWidget);
        l1->setStyleSheet("color: #e1e1e5; font-size: 12px; font-weight: 500;");
        leftLayout->addWidget(lbl1);
        leftLayout->addWidget(l1);
        leftLayout->addStretch();

        auto* rightWidget = new QWidget(rowWidget);
        auto* rightLayout = new QHBoxLayout(rightWidget);
        rightLayout->setContentsMargins(0, 0, 0, 0);
        rightLayout->setSpacing(6);
        auto* lbl2 = new QLabel(t2, rightWidget);
        lbl2->setStyleSheet("color: #8e8e93; font-size: 12px;");
        l2 = new QLabel("¥ 0.00", rightWidget);
        l2->setStyleSheet("color: #e1e1e5; font-size: 12px; font-weight: 500;");
        rightLayout->addWidget(lbl2);
        rightLayout->addWidget(l2);
        rightLayout->addStretch();

        rowLayout->addWidget(leftWidget, 1);
        rowLayout->addWidget(rightWidget, 1);
        contentLayout->addWidget(rowWidget);
    };

    // 个人财务：1.当前资金/流动资金 2.总进资金/总出资金 3.月均支出/月均收入 4.负债/应收 (各50%/50%)
    makeTwoColRow("当前资金：", m_personalCurrentFundLabel, "流动资金：", m_personalLiquidFundLabel);
    makeTwoColRow("总进资金：", m_personalTotalIncomeLabel, "总出资金：", m_personalTotalExpenseLabel);
    makeTwoColRow("月均支出：", m_personalAvgExpenseLabel, "月均收入：", m_personalAvgIncomeLabel);
    makeTwoColRow("负债：", m_personalDebtLabel, "应收：", m_personalReceivableLabel);

    // 个人与公司财务分割线
    auto* lineFinMid = new QFrame(contentWidget);
    lineFinMid->setFixedHeight(1);
    lineFinMid->setStyleSheet("background-color: #28282b; border: none;");
    contentLayout->addWidget(lineFinMid);

    auto* cFinHeader = new QHBoxLayout();
    auto* cFinTitle = new QLabel("公司财务", contentWidget);
    cFinTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #e1e1e5;");

    m_companyFinanceStatusPill = new QLabel("正向", contentWidget);
    m_companyFinanceStatusPill->setObjectName("statusPillGreen");

    cFinHeader->addWidget(cFinTitle);
    cFinHeader->addStretch();
    cFinHeader->addWidget(m_companyFinanceStatusPill);
    contentLayout->addLayout(cFinHeader);

    // 公司财务：1.当前资金/流动资金 2.总进资金/总出资金 3.月均支出/月均收入 4.负债/应收 (各50%/50%)
    makeTwoColRow("当前资金：", m_companyCurrentFundLabel, "流动资金：", m_companyLiquidFundLabel);
    makeTwoColRow("总进资金：", m_companyTotalIncomeLabel, "总出资金：", m_companyTotalExpenseLabel);
    makeTwoColRow("月均支出：", m_companyAvgExpenseLabel, "月均收入：", m_companyAvgIncomeLabel);
    makeTwoColRow("负债：", m_companyDebtLabel, "应收：", m_companyReceivableLabel);

    // 分割线 2
    auto* line2 = new QFrame(contentWidget);
    line2->setFixedHeight(1);
    line2->setStyleSheet("background-color: #28282b; border: none;");
    contentLayout->addWidget(line2);

    // ==================== Card 3: 资产 (严格按图2: 左 20% | 中 50% | 右 30%) ====================
    auto* assetHeader = new QHBoxLayout();
    auto* assetTitle = new QLabel("资产", contentWidget);
    assetTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #e1e1e5;");

    auto* btnAddAsset = new QPushButton("+", contentWidget);
    btnAddAsset->setCursor(Qt::PointingHandCursor);
    btnAddAsset->setFixedSize(20, 20);
    btnAddAsset->setStyleSheet(
        "QPushButton { background: transparent; color: #00a361; border: none; font-size: 15px; font-weight: bold; padding: 0px; }"
        "QPushButton:hover { background-color: #192b22; border-radius: 3px; }"
    );
    connect(btnAddAsset, &QPushButton::clicked, this, &GeneralDashboardWidget::onAddAssetClicked);

    assetHeader->addWidget(assetTitle);
    assetHeader->addStretch();
    assetHeader->addWidget(btnAddAsset);
    contentLayout->addLayout(assetHeader);

    m_assetListLayout = new QVBoxLayout();
    m_assetListLayout->setSpacing(10);
    contentLayout->addLayout(m_assetListLayout);

    contentLayout->addStretch();
    scroll->setWidget(contentWidget);
    layout->addWidget(scroll);
}

bool GeneralDashboardWidget::eventFilter(QObject* watched, QEvent* event) {
    if (m_chatScroll && watched == m_chatScroll->viewport() && event->type() == QEvent::Resize) {
        updateChatBubblesWidth();
    }
    if (watched && event->type() == QEvent::MouseButtonDblClick) {
        QVariant compProp = watched->property("companyData");
        if (compProp.isValid() && compProp.canConvert<QVariantMap>()) {
            QVariantMap compMap = compProp.toMap();
            CompanyDialog dlg(compMap, this);
            if (dlg.exec() == QDialog::Accepted) {
                refreshData();
            }
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void GeneralDashboardWidget::updateChatBubblesWidth() {
    if (!m_chatScroll || !m_chatContentWidget) return;
    int viewportW = m_chatScroll->viewport()->width();
    if (viewportW <= 100) return;

    int target70Width = qMax(220, static_cast<int>(viewportW * 0.70));
    const auto children = m_chatContentWidget->findChildren<QWidget*>();
    for (auto* w : children) {
        if (auto* b = dynamic_cast<ChatBubbleWidget*>(w)) {
            if (b->isAiBubble()) {
                b->setFixedWidth(target70Width);
            } else {
                b->setMaximumWidth(target70Width);
            }
        }
    }
}

void GeneralDashboardWidget::refreshData() {
    updateCategoryList();
    updateProfileCard();
    updateCompanyCard();
    updateFinanceCard();
    updateAssetCard();
}

void GeneralDashboardWidget::onToggleExpenseIncome(bool isExpense) {
    m_currentOverviewType = isExpense ? "EXPENSE" : "INCOME";
    updateCategoryList();
}

void GeneralDashboardWidget::updateCategoryList() {
    while (m_categoryListLayout->count() > 1) {
        auto* item = m_categoryListLayout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    QDate today = QDate::currentDate();
    auto breakdowns = DatabaseManager::instance().getMonthlyCategoryBreakdown(m_currentOverviewType, today.year(), today.month());
    double totalAmt = 0.0;
    for (const auto& b : breakdowns) totalAmt += b.amount;

    m_monthlyTotalLabel->setText(QString("本月总%1: ¥ %2")
                                    .arg(m_currentOverviewType == "EXPENSE" ? "支出" : "收入")
                                    .arg(totalAmt, 0, 'f', 2));

    for (const auto& b : breakdowns) {
        auto* rowWidget = new QWidget();
        auto* rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 2, 0, 2);
        rowLayout->setSpacing(10);

        auto* nameLbl = new QLabel(b.category, rowWidget);
        nameLbl->setStyleSheet("color: #8e8e93; font-size: 12px;");
        nameLbl->setFixedWidth(64);

        auto* bar = new QProgressBar(rowWidget);
        bar->setFixedHeight(10);
        bar->setTextVisible(false);
        bar->setRange(0, 100);
        bar->setValue(static_cast<int>(b.percentage));
        bar->setStyleSheet(
            "QProgressBar { background-color: #28282b; border: none; border-radius: 2px; }"
            "QProgressBar::chunk { background-color: #00a361; border-radius: 2px; }"
        );

        auto* amtLbl = new QLabel(QString("¥ %1").arg(b.amount, 0, 'f', 2), rowWidget);
        amtLbl->setStyleSheet("color: #e1e1e5; font-size: 12px;");
        amtLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        amtLbl->setFixedWidth(84);

        rowLayout->addWidget(nameLbl);
        rowLayout->addWidget(bar, 1);
        rowLayout->addWidget(amtLbl);

        m_categoryListLayout->insertWidget(m_categoryListLayout->count() - 1, rowWidget);
    }
}

void GeneralDashboardWidget::updateProfileCard() {
    auto profile = DatabaseManager::instance().getUserProfile();
    m_profileNameLabel->setText(profile.name);
    m_profilePhoneLabel->setText(profile.phone);
    m_profileAddressLabel->setText(profile.address);
    m_profileIndustryLabel->setText(profile.industry);
    m_profileTitleLabel->setText(profile.title);
    m_profileCompanyLabel->setText(QString("%1家").arg(profile.companyCount));
    m_profileFriendsLabel->setText(QString("%1位").arg(profile.contactCount));
}

void GeneralDashboardWidget::updateCompanyCard() {
    if (!m_companyListLayout) return;

    while (m_companyListLayout->count() > 0) {
        auto* item = m_companyListLayout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    auto allCompanies = DatabaseManager::instance().getCompaniesList();
    if (allCompanies.isEmpty()) return;

    // 严密过滤：只展示属于“我的公司”，绝不混入外部/别人的公司
    QList<QVariantMap> companies;
    for (const auto& c : allCompanies) {
        int isMine = c.contains("is_my_company") ? c["is_my_company"].toInt() : -1;
        QString r = c["role_type"].toString().trimmed();
        bool isMyCorp = (isMine == 1) || (isMine != 0 && r != "EXTERNAL" && (r == "OWNER" || r == "PARTNER" || r == "EMPLOYEE"));
        if (isMyCorp) {
            companies.append(c);
        }
    }
    if (companies.isEmpty()) return;

    // 排序：持股比例从大到小，若相同按ID排序
    std::sort(companies.begin(), companies.end(), [](const QVariantMap& a, const QVariantMap& b) {
        double ratioA = a["share_ratio"].toDouble();
        double ratioB = b["share_ratio"].toDouble();
        if (qAbs(ratioA - ratioB) > 0.001) {
            return ratioA > ratioB;
        }
        return a["id"].toInt() < b["id"].toInt();
    });

    for (int i = 0; i < companies.size(); ++i) {
        const auto& c = companies[i];
        int compId = c["id"].toInt();
        QString name = c["name"].toString().trimmed();
        QString taxNo = c["tax_no"].toString().trimmed();
        double shareRatio = c["share_ratio"].toDouble();
        double capital = c["registered_capital"].toDouble();
        QString roleType = c["role_type"].toString();

        QString roleText = "控股";
        if (roleType == "PARTNER") roleText = "合伙";
        else if (roleType == "EMPLOYEE") roleText = "任职";
        else if (roleType == "OWNER") roleText = "法人/控股";

        auto* itemWidget = new QWidget(this);
        itemWidget->setCursor(Qt::PointingHandCursor);
        itemWidget->setProperty("companyData", c);
        itemWidget->installEventFilter(this);
        itemWidget->setStyleSheet(
            "QWidget { background-color: #1a1a1e; border: 1px solid #28282c; border-radius: 4px; }"
            "QWidget:hover { background-color: #242428; border: 1px solid #3f3f46; }"
        );

        auto* itemLayout = new QVBoxLayout(itemWidget);
        itemLayout->setContentsMargins(8, 6, 8, 6);
        itemLayout->setSpacing(3);

        // 第一行：公司名称 (左) + 角色与股比 (右)
        auto* topRow = new QHBoxLayout();
        topRow->setContentsMargins(0, 0, 0, 0);
        topRow->setSpacing(6);

        auto* nameLbl = new QLabel(name, itemWidget);
        nameLbl->setStyleSheet("color: #e1e1e5; font-size: 12px; font-weight: bold; border: none; background: transparent;");
        nameLbl->setAttribute(Qt::WA_TransparentForMouseEvents);

        QString ratioStr = QString("%1 %2%").arg(roleText).arg(shareRatio, 0, 'f', (shareRatio == (int)shareRatio ? 0 : 1));
        auto* ratioLbl = new QLabel(ratioStr, itemWidget);
        ratioLbl->setStyleSheet("color: #00a361; font-size: 11px; font-weight: bold; background: rgba(0, 163, 97, 0.12); padding: 1px 5px; border-radius: 3px; border: none;");
        ratioLbl->setAttribute(Qt::WA_TransparentForMouseEvents);

        topRow->addWidget(nameLbl, 1);
        topRow->addWidget(ratioLbl);
        itemLayout->addLayout(topRow);

        // 第二行：税号 (左) + 注册资本 (右)
        auto* subRow = new QHBoxLayout();
        subRow->setContentsMargins(0, 0, 0, 0);
        subRow->setSpacing(6);

        QString taxDisplay = taxNo.isEmpty() ? "税号未登记" : QString("税号: %1").arg(taxNo);
        auto* taxLbl = new QLabel(taxDisplay, itemWidget);
        taxLbl->setStyleSheet("color: #8e8e93; font-size: 11px; border: none; background: transparent;");
        taxLbl->setAttribute(Qt::WA_TransparentForMouseEvents);

        QString capDisplay = "";
        if (capital > 0) {
            if (capital >= 10000) {
                capDisplay = QString("注册资本: ¥%1万").arg(capital / 10000.0, 0, 'f', 0);
            } else {
                capDisplay = QString("注册资本: ¥%1").arg(capital, 0, 'f', 0);
            }
        }
        auto* capLbl = new QLabel(capDisplay, itemWidget);
        capLbl->setStyleSheet("color: #71717a; font-size: 11px; border: none; background: transparent;");
        capLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        capLbl->setAttribute(Qt::WA_TransparentForMouseEvents);

        subRow->addWidget(taxLbl, 1);
        if (!capDisplay.isEmpty()) {
            subRow->addWidget(capLbl);
        }
        itemLayout->addLayout(subRow);

        // 鼠标右键上下文菜单：编辑与删除
        itemWidget->setContextMenuPolicy(Qt::CustomContextMenu);
        QVariantMap capturedComp = c;
        connect(itemWidget, &QWidget::customContextMenuRequested, this, [this, capturedComp, itemWidget](const QPoint& pos) {
            QMenu menu(this);
            menu.setStyleSheet(
                "QMenu { background-color: #202024; border: 1px solid #3f3f46; padding: 4px; border-radius: 4px; }"
                "QMenu::item { color: #e1e1e5; padding: 6px 18px; border-radius: 3px; font-size: 13px; }"
                "QMenu::item:selected { background-color: #2b2b30; color: #ffffff; }"
            );
            QAction* actEdit = menu.addAction(UiIcons::icon(UiIcons::Code::Diary, 12, QColor("#be782e")), "编辑");
            QAction* actDelete = menu.addAction(UiIcons::icon(UiIcons::Code::Delete, 12, QColor("#fa5151")), "删除");
            QAction* selected = menu.exec(itemWidget->mapToGlobal(pos));

            int cId = capturedComp["id"].toInt();
            QString cName = capturedComp["name"].toString();

            if (selected == actEdit) {
                CompanyDialog dlg(capturedComp, this);
                if (dlg.exec() == QDialog::Accepted) {
                    refreshData();
                }
            } else if (selected == actDelete) {
                if (CustomDialog::confirm(this, "删除公司主体",
                        QString("确定要彻底删除公司主体【%1】吗？\n警告：删除后该公司及其关联的所有流水、资产与走动数据将彻底清除且不可恢复！").arg(cName),
                        "删除", "取消", true)) {
                    DatabaseManager::instance().deleteCompany(cId);
                    refreshData();
                }
            }
        });

        m_companyListLayout->addWidget(itemWidget);
    }
}

void GeneralDashboardWidget::updateFinanceCard() {
    auto snap = DatabaseManager::instance().getFinancialSnapshot();

    // 1. 个人财务总结
    // 当前资金 = 总进资金 - 总出资金；流动资金 = 当前资金 - 负债
    if (m_personalCurrentFundLabel) {
        m_personalCurrentFundLabel->setText(QString("¥ %1").arg(snap.personalBalance, 0, 'f', 2));
        m_personalCurrentFundLabel->setStyleSheet("color: #febc2f; font-size: 12px; font-weight: bold;");
    }
    if (m_personalLiquidFundLabel) {
        m_personalLiquidFundLabel->setText(QString("¥ %1").arg(snap.personalLiquidFund, 0, 'f', 2));
        m_personalLiquidFundLabel->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: bold;").arg(snap.personalLiquidFund >= 0 ? "#00a361" : "#fa5151"));
    }
    if (m_personalTotalIncomeLabel) {
        m_personalTotalIncomeLabel->setText(QString("¥ %1").arg(snap.personalTotalIncome, 0, 'f', 2));
        m_personalTotalIncomeLabel->setStyleSheet("color: #00a361; font-size: 12px; font-weight: 500;");
    }
    if (m_personalTotalExpenseLabel) {
        m_personalTotalExpenseLabel->setText(QString("¥ %1").arg(snap.personalTotalExpense, 0, 'f', 2));
        m_personalTotalExpenseLabel->setStyleSheet("color: #fa5151; font-size: 12px; font-weight: 500;");
    }
    if (m_personalAvgExpenseLabel) {
        m_personalAvgExpenseLabel->setText(QString("¥ %1").arg(snap.monthlyPersonalExpense, 0, 'f', 2));
        m_personalAvgExpenseLabel->setStyleSheet("color: #fa5151; font-size: 12px;");
    }
    if (m_personalAvgIncomeLabel) {
        m_personalAvgIncomeLabel->setText(QString("¥ %1").arg(snap.monthlyPersonalIncome, 0, 'f', 2));
        m_personalAvgIncomeLabel->setStyleSheet("color: #00a361; font-size: 12px;");
    }
    if (m_personalDebtLabel) {
        m_personalDebtLabel->setText(QString("¥ %1").arg(snap.personalBorrowIn, 0, 'f', 2));
        m_personalDebtLabel->setStyleSheet("color: #fa5151; font-size: 12px;");
    }
    if (m_personalReceivableLabel) {
        m_personalReceivableLabel->setText(QString("¥ %1").arg(snap.personalLendOut, 0, 'f', 2));
        m_personalReceivableLabel->setStyleSheet("color: #be782e; font-size: 12px;");
    }

    if (m_personalFinanceStatusPill) {
        if (snap.personalLiquidFund >= 0 && snap.monthlyPersonalIncome >= snap.monthlyPersonalExpense) {
            m_personalFinanceStatusPill->setText("正向");
            m_personalFinanceStatusPill->setStyleSheet("color: #00a361; font-size: 12px; font-weight: bold;");
        } else {
            m_personalFinanceStatusPill->setText("承压");
            m_personalFinanceStatusPill->setStyleSheet("color: #be782e; font-size: 12px; font-weight: bold;");
        }
    }

    // 2. 公司财务总结
    // 当前资金 = 总进资金 - 总出资金；流动资金 = 当前资金 - 负债
    if (m_companyCurrentFundLabel) {
        m_companyCurrentFundLabel->setText(QString("¥ %1").arg(snap.companyBalance, 0, 'f', 2));
        m_companyCurrentFundLabel->setStyleSheet("color: #febc2f; font-size: 12px; font-weight: bold;");
    }
    if (m_companyLiquidFundLabel) {
        m_companyLiquidFundLabel->setText(QString("¥ %1").arg(snap.companyLiquidFund, 0, 'f', 2));
        m_companyLiquidFundLabel->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: bold;").arg(snap.companyLiquidFund >= 0 ? "#00a361" : "#fa5151"));
    }
    if (m_companyTotalIncomeLabel) {
        m_companyTotalIncomeLabel->setText(QString("¥ %1").arg(snap.companyTotalIncome, 0, 'f', 2));
        m_companyTotalIncomeLabel->setStyleSheet("color: #00a361; font-size: 12px; font-weight: 500;");
    }
    if (m_companyTotalExpenseLabel) {
        m_companyTotalExpenseLabel->setText(QString("¥ %1").arg(snap.companyTotalExpense, 0, 'f', 2));
        m_companyTotalExpenseLabel->setStyleSheet("color: #fa5151; font-size: 12px; font-weight: 500;");
    }
    if (m_companyAvgExpenseLabel) {
        m_companyAvgExpenseLabel->setText(QString("¥ %1").arg(snap.monthlyCompanyExpense, 0, 'f', 2));
        m_companyAvgExpenseLabel->setStyleSheet("color: #fa5151; font-size: 12px;");
    }
    if (m_companyAvgIncomeLabel) {
        m_companyAvgIncomeLabel->setText(QString("¥ %1").arg(snap.monthlyCompanyIncome, 0, 'f', 2));
        m_companyAvgIncomeLabel->setStyleSheet("color: #00a361; font-size: 12px;");
    }
    if (m_companyDebtLabel) {
        m_companyDebtLabel->setText(QString("¥ %1").arg(snap.companyBorrowIn, 0, 'f', 2));
        m_companyDebtLabel->setStyleSheet("color: #fa5151; font-size: 12px;");
    }
    if (m_companyReceivableLabel) {
        m_companyReceivableLabel->setText(QString("¥ %1").arg(snap.companyLendOut, 0, 'f', 2));
        m_companyReceivableLabel->setStyleSheet("color: #be782e; font-size: 12px;");
    }

    if (m_companyFinanceStatusPill) {
        if (snap.companyLiquidFund >= 0 && snap.monthlyCompanyIncome >= snap.monthlyCompanyExpense) {
            m_companyFinanceStatusPill->setText("正向");
            m_companyFinanceStatusPill->setStyleSheet("color: #00a361; font-size: 12px; font-weight: bold;");
        } else {
            m_companyFinanceStatusPill->setText("承压");
            m_companyFinanceStatusPill->setStyleSheet("color: #be782e; font-size: 12px; font-weight: bold;");
        }
    }
}

void GeneralDashboardWidget::updateAssetCard() {
    while (m_assetListLayout->count() > 0) {
        auto* item = m_assetListLayout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    auto assets = DatabaseManager::instance().getAssets();
    for (const auto& a : assets) {
        auto* w = new QWidget(this);
        auto* row = new QHBoxLayout(w);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(4);

        QString typePrefix = a.assetType;
        if (a.assetType == "REAL_ESTATE") typePrefix = "房产";
        else if (a.assetType == "VEHICLE") typePrefix = "车辆";
        else if (a.assetType == "COLLECTION") typePrefix = "藏品";
        else if (a.assetType == "OTHER" || a.assetType.isEmpty()) typePrefix = "资产";

        // 左 20%
        auto* typeLbl = new QLabel(typePrefix, w);
        typeLbl->setStyleSheet("color: #8e8e93; font-size: 12px;");
        typeLbl->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

        // 中 50% (点击名称弹出修改/出售弹窗)
        auto* nameBtn = new QPushButton(a.name, w);
        nameBtn->setCursor(Qt::PointingHandCursor);
        nameBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        nameBtn->setStyleSheet(
            "QPushButton { background: transparent; color: #e1e1e5; border: none; text-align: left; font-size: 12px; padding: 0px; }"
            "QPushButton:hover { color: #00a361; }"
        );
        AssetItem capturedAsset = a;
        connect(nameBtn, &QPushButton::clicked, this, [this, capturedAsset]() {
            openAssetDialog(&capturedAsset);
        });

        // 右 30%
        auto* valLbl = new QLabel(QString("¥ %1").arg(a.estimatedValue, 0, 'f', 0), w);
        valLbl->setStyleSheet("color: #e1e1e5; font-size: 12px;");
        valLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        valLbl->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

        row->addWidget(typeLbl, 2); // 20%
        row->addWidget(nameBtn, 5); // 50%
        row->addWidget(valLbl, 3);  // 30%

        m_assetListLayout->addWidget(w);
    }
}

void GeneralDashboardWidget::resetSendButtonState() {
    const QString normalBtnStyle =
        "QPushButton { background-color: #2f2f30; color: #e1e1e5; border: none; border-radius: 4px; font-size: 12px; font-weight: bold; padding: 0px 8px; }"
        "QPushButton:hover { background-color: #3d3d40; color: #ffffff; }"
        "QPushButton:pressed { background-color: #00a361; color: #ffffff; }";
    if (m_btnSendChat) {
        m_btnSendChat->setEnabled(true);
        m_btnSendChat->setText("发送");
        m_btnSendChat->setStyleSheet(normalBtnStyle);
    }
    if (m_chatInput) {
        m_chatInput->setEnabled(true);
    }
}

void GeneralDashboardWidget::disconnectAiSignals() {
    if (m_connToken) {
        disconnect(m_connToken);
    }
    if (m_connFinished) {
        disconnect(m_connFinished);
    }
    if (m_connFailed) {
        disconnect(m_connFailed);
    }
    if (m_connCanceled) {
        disconnect(m_connCanceled);
    }
}

ChatBubbleWidget* GeneralDashboardWidget::appendAiMessage(const QString& sender, const QString& content, bool isAi) {
    Q_UNUSED(sender);

    // 记录对话历史（限制最近 10 条问答，流式占位符暂不入库）
    if (!content.trimmed().isEmpty() && content != "..." && !content.startsWith("本地模型连接提示：")) {
        m_chatHistory.append({isAi ? "assistant" : "user", content.trimmed()});
        while (m_chatHistory.size() > 10) {
            m_chatHistory.removeFirst();
        }
    }

    auto* msgCard = new QWidget(m_chatContentWidget);
    auto* rowLayout = new QHBoxLayout(msgCard);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(10);

    // 50px x 50px 圆角背景色头像 (参考图1)
    auto* avatar = new QLabel(isAi ? "AI" : "我", msgCard);
    avatar->setFixedSize(50, 50);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setStyleSheet(
        "QLabel { background-color: #1e1e22; color: #0a84ff; border-radius: 6px; font-size: 16px; font-weight: 900; border: none; }"
    );

    auto* bubble = new ChatBubbleWidget(content, isAi, msgCard);
    int vpW = (m_chatScroll && m_chatScroll->viewport()->width() > 100) ? m_chatScroll->viewport()->width() : 650;
    int target70W = qMax(240, static_cast<int>(vpW * 0.70));
    if (isAi) {
        bubble->setFixedWidth(target70W);
        rowLayout->addWidget(avatar, 0, Qt::AlignTop);
        rowLayout->addWidget(bubble, 0, Qt::AlignTop);
        rowLayout->addStretch(1);
    } else {
        bubble->setMaximumWidth(target70W);
        rowLayout->addStretch(1);
        rowLayout->addWidget(bubble, 0, Qt::AlignVCenter);
        rowLayout->addWidget(avatar, 0, Qt::AlignTop);
    }

    m_chatLayout->insertWidget(m_chatLayout->count() - 1, msgCard);

    QTimer::singleShot(10, this, [this]() {
        updateChatBubblesWidth();
        if (m_chatScroll && m_chatScroll->verticalScrollBar()) {
            m_chatScroll->verticalScrollBar()->setValue(m_chatScroll->verticalScrollBar()->maximum());
        }
    });

    return bubble;
}

void GeneralDashboardWidget::onSendAiPrompt() {
    QString text = m_chatInput->text().trimmed();
    if (text.isEmpty()) return;

    if (AiAdvisorService::instance().isBusy()) {
        return;
    }

    appendAiMessage("我", text, false);
    m_chatInput->clear();

    const QString generatingBtnStyle =
        "QPushButton { background-color: #be782e; color: #ffffff; border: none; border-radius: 4px; font-size: 12px; font-weight: bold; padding: 0px 8px; }";

    m_btnSendChat->setEnabled(false);
    m_btnSendChat->setText("生成中...");
    m_btnSendChat->setStyleSheet(generatingBtnStyle);

    // 每次发送前必须先断开任何旧的悬挂连接，杜绝信号串号
    disconnectAiSignals();

    m_streamingAiBubble = appendAiMessage("AI", "...", true);
    m_streamingAiText.clear();
    m_streamingDirty = false;

    m_connToken = connect(&AiAdvisorService::instance(), &AiAdvisorService::tokenReceived, this,
        [this](const QString& token) {
            m_streamingAiText += token;
            m_streamingDirty = true;
            if (m_streamingThrottleTimer && !m_streamingThrottleTimer->isActive()) {
                m_streamingThrottleTimer->start();
            }
        });

    m_connFinished = connect(&AiAdvisorService::instance(), &AiAdvisorService::evaluationFinished, this,
        [this](const QString& fullResponse) {
            if (m_streamingThrottleTimer) m_streamingThrottleTimer->stop();
            m_streamingDirty = false;
            disconnectAiSignals();
            resetSendButtonState();
            if (m_streamingAiBubble) {
                m_streamingAiBubble->updateText(fullResponse);
            }
            m_chatHistory.append({"assistant", fullResponse});
            m_streamingAiBubble = nullptr;
            m_streamingAiText.clear();
        });

    m_connFailed = connect(&AiAdvisorService::instance(), &AiAdvisorService::evaluationFailed, this,
        [this](const QString& err) {
            if (m_streamingThrottleTimer) m_streamingThrottleTimer->stop();
            m_streamingDirty = false;
            disconnectAiSignals();
            resetSendButtonState();
            if (m_streamingAiBubble) {
                m_streamingAiBubble->updateText(QString("本地模型连接提示：%1\n(请点击顶部栏 [Ai参谋] 检查模型配置)").arg(err));
            }
            m_streamingAiBubble = nullptr;
            m_streamingAiText.clear();
        });

    m_connCanceled = connect(&AiAdvisorService::instance(), &AiAdvisorService::evaluationCanceled, this,
        [this]() {
            if (m_streamingThrottleTimer) m_streamingThrottleTimer->stop();
            m_streamingDirty = false;
            m_streamingAiBubble = nullptr;
            m_streamingAiText.clear();
            disconnectAiSignals();
            resetSendButtonState();
        });

    // 传入包含上下文历史的多轮对话
    AiAdvisorService::instance().askCustomQuestion(text, 0, m_chatHistory);
}

void GeneralDashboardWidget::clearChatWidgetsOnly() {
    while (m_chatLayout && m_chatLayout->count() > 1) {
        auto* item = m_chatLayout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
}

void GeneralDashboardWidget::clearChatForUnconfiguredAi() {
    disconnectAiSignals();
    clearChatWidgetsOnly();
    m_chatHistory.clear();
    const QString notice =
        "您好，AI 功能未启动！\n"
        "请设置好AI 可以设置本地私有知识库，对您的收支流水、应收欠款以及人脉往来开展实时参谋。";
    appendAiMessage("AI", notice, true);
}

void GeneralDashboardWidget::checkAndTriggerInitialAiState(bool force) {
    if (force) {
        m_initialAiCheckDone = false;
    }
    if (m_initialAiCheckDone) return;
    m_initialAiCheckDone = true;

    auto& db = DatabaseManager::instance();
    QString configuredFlag = db.getSecurityConfig("llm_configured");
    if (configuredFlag == "0") {
        clearChatForUnconfiguredAi();
        return;
    }

    auto engine = std::dynamic_pointer_cast<OllamaHttpEngine>(AiAdvisorService::instance().engine());
    if (!engine) {
        clearChatForUnconfiguredAi();
        return;
    }

    if (m_btnSendChat) {
        m_btnSendChat->setEnabled(false);
        m_btnSendChat->setText("检测中...");
    }

    QPointer<GeneralDashboardWidget> guard(this);
    engine->checkAvailability([guard](bool available, const QString&) {
        if (!guard) return;
        if (!available) {
            guard->clearChatForUnconfiguredAi();
            guard->resetSendButtonState();
        } else {
            // AI正常，立刻按当前数据库实际情况全面综合评价现状
            if (guard->m_btnSendChat) {
                guard->m_btnSendChat->setEnabled(false);
                guard->m_btnSendChat->setText("评价中...");
                guard->m_btnSendChat->setStyleSheet(
                    "QPushButton { background-color: #be782e; color: #ffffff; border: none; border-radius: 4px; font-size: 12px; font-weight: bold; padding: 0px 8px; }"
                );
            }
            guard->disconnectAiSignals();

            guard->clearChatWidgetsOnly();
            guard->m_chatHistory.clear();
            guard->m_streamingAiBubble = guard->appendAiMessage("AI", "...", true);
            guard->m_streamingAiText.clear();
            guard->m_streamingDirty = false;

            guard->m_connToken = connect(&AiAdvisorService::instance(), &AiAdvisorService::tokenReceived, guard.data(),
                [guard](const QString& token) {
                    if (!guard) return;
                    guard->m_streamingAiText += token;
                    guard->m_streamingDirty = true;
                    if (guard->m_streamingThrottleTimer && !guard->m_streamingThrottleTimer->isActive()) {
                        guard->m_streamingThrottleTimer->start();
                    }
                });

            guard->m_connFinished = connect(&AiAdvisorService::instance(), &AiAdvisorService::evaluationFinished, guard.data(),
                [guard](const QString& fullResponse) {
                    if (!guard) return;
                    if (guard->m_streamingThrottleTimer) guard->m_streamingThrottleTimer->stop();
                    guard->m_streamingDirty = false;
                    guard->disconnectAiSignals();
                    guard->resetSendButtonState();
                    if (guard->m_streamingAiBubble) {
                        guard->m_streamingAiBubble->updateText(fullResponse);
                    }
                    guard->m_chatHistory.append({"assistant", fullResponse});
                    guard->m_streamingAiBubble = nullptr;
                    guard->m_streamingAiText.clear();
                });

            guard->m_connFailed = connect(&AiAdvisorService::instance(), &AiAdvisorService::evaluationFailed, guard.data(),
                [guard](const QString&) {
                    if (!guard) return;
                    if (guard->m_streamingThrottleTimer) guard->m_streamingThrottleTimer->stop();
                    guard->m_streamingDirty = false;
                    guard->m_streamingAiBubble = nullptr;
                    guard->m_streamingAiText.clear();
                    guard->disconnectAiSignals();
                    guard->clearChatForUnconfiguredAi();
                    guard->resetSendButtonState();
                });

            guard->m_connCanceled = connect(&AiAdvisorService::instance(), &AiAdvisorService::evaluationCanceled, guard.data(),
                [guard]() {
                    if (!guard) return;
                    if (guard->m_streamingThrottleTimer) guard->m_streamingThrottleTimer->stop();
                    guard->m_streamingDirty = false;
                    guard->m_streamingAiBubble = nullptr;
                    guard->m_streamingAiText.clear();
                    guard->disconnectAiSignals();
                    guard->resetSendButtonState();
                });

            AiAdvisorService::instance().evaluateGlobalSituation();
        }
    });
}

void GeneralDashboardWidget::onClearAiChat() {
    disconnectAiSignals();
    resetSendButtonState();
    m_chatHistory.clear();
    clearChatWidgetsOnly();
}

void GeneralDashboardWidget::onAddTransactionClicked() {
    // 综合页面左栏点击 +添加：隐藏顶部 支出/收入 选择行，直接默认当前选中的 m_currentOverviewType
    UnifiedTransactionDialog dlg(m_currentOverviewType, this, true);
    if (dlg.exec() == QDialog::Accepted) {
        refreshData();
    }
}

void GeneralDashboardWidget::onEditProfileClicked() {
    UserProfileDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        refreshData();
    }
}

void GeneralDashboardWidget::onAddCompanyClicked() {
    CompanyDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        refreshData();
    }
}

void GeneralDashboardWidget::onAddAssetClicked() {
    openAssetDialog(nullptr);
}

void GeneralDashboardWidget::openAssetDialog(const AssetItem* existingAsset) {
    const bool isEdit = (existingAsset != nullptr);
    CustomDialog dlg(isEdit ? "修改资产" : "添加资产", this);
    dlg.setFixedWidth(480);

    auto* layout = new QVBoxLayout();
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(10);

    // 1. 顶部先选择：个人 | 工作 (添加资产时显示，修改资产时默认保持原属性无需显示选择)
    auto* scopeWidget = new QWidget(&dlg);
    auto* scopeRow = new QHBoxLayout(scopeWidget);
    scopeRow->setContentsMargins(0, 0, 0, 0);
    scopeRow->setSpacing(10);
    auto* btnPersonal = new QPushButton("个人", scopeWidget);
    auto* btnWork = new QPushButton("工作", scopeWidget);
    btnPersonal->setCheckable(true);
    btnWork->setCheckable(true);
    btnPersonal->setCursor(Qt::PointingHandCursor);
    btnWork->setCursor(Qt::PointingHandCursor);

    QString toggleStyle =
        "QPushButton { background-color: #1c1c1f; color: #8e8e93; border: 1px solid #28282b; border-radius: 3px; padding: 6px 16px; font-weight: bold; }"
        "QPushButton:hover { background-color: #242428; color: #e1e1e5; }"
        "QPushButton:checked { background-color: #00a361; color: #ffffff; border-color: #00a361; }";
    btnPersonal->setStyleSheet(toggleStyle);
    btnWork->setStyleSheet(toggleStyle);

    bool initPersonal = true;
    if (isEdit) {
        initPersonal = (existingAsset->ownership != "COMPANY" && existingAsset->ownership != "WORK");
    }
    btnPersonal->setChecked(initPersonal);
    btnWork->setChecked(!initPersonal);

    scopeRow->addWidget(btnPersonal, 1);
    scopeRow->addWidget(btnWork, 1);
    scopeWidget->setVisible(!isEdit);
    layout->addWidget(scopeWidget);

    // 2. 如果是修改弹窗，显示“持有中”与“出售”按键（删除左侧“出售”两个字标签，左右平均宽度 1:1）
    QPushButton* btnHold = nullptr;
    QPushButton* btnSell = nullptr;
    if (isEdit) {
        auto* sellRow = new QHBoxLayout();
        sellRow->setSpacing(10);
        btnHold = new QPushButton("持有中", &dlg);
        btnSell = new QPushButton("出售", &dlg);
        btnHold->setCheckable(true);
        btnSell->setCheckable(true);
        btnHold->setChecked(true);
        btnHold->setCursor(Qt::PointingHandCursor);
        btnSell->setCursor(Qt::PointingHandCursor);
        btnHold->setStyleSheet(toggleStyle);
        btnSell->setStyleSheet(
            "QPushButton { background-color: #1c1c1f; color: #8e8e93; border: 1px solid #28282b; border-radius: 3px; padding: 6px 16px; font-weight: bold; }"
            "QPushButton:hover { background-color: #2a181b; color: #fa5151; }"
            "QPushButton:checked { background-color: #be782e; color: #ffffff; border-color: #be782e; }"
        );
        sellRow->addWidget(btnHold, 1);
        sellRow->addWidget(btnSell, 1);
        layout->addLayout(sellRow);

        connect(btnHold, &QPushButton::clicked, &dlg, [btnHold, btnSell]() {
            btnHold->setChecked(true);
            btnSell->setChecked(false);
        });
        connect(btnSell, &QPushButton::clicked, &dlg, [btnHold, btnSell]() {
            btnSell->setChecked(true);
            btnHold->setChecked(false);
        });
    }

    auto* form = new QFormLayout();
    form->setSpacing(10);

    // 2.5 选择工作时：选择我拥有的公司 (company_id)
    auto* companyLbl = new QLabel("所属公司", &dlg);
    auto* companyCombo = new QComboBox(&dlg);
    auto companies = DatabaseManager::instance().getCompaniesList();
    for (const auto& comp : companies) {
        int isMine = comp.contains("is_my_company") ? comp["is_my_company"].toInt() : -1;
        QString r = comp["role_type"].toString().trimmed();
        bool isMyCorp = (isMine == 1) || (isMine != 0 && r != "EXTERNAL" && (r == "OWNER" || r == "PARTNER" || r == "EMPLOYEE"));
        if (!isMyCorp) continue;

        int cid = comp["id"].toInt();
        companyCombo->addItem(comp["name"].toString(), cid);
        if (isEdit && existingAsset && existingAsset->companyId == cid) {
            companyCombo->setCurrentIndex(companyCombo->count() - 1);
        }
    }
    if (companyCombo->count() == 0) {
        companyCombo->addItem("默认工作主体", 0);
    }
    form->addRow(companyLbl, companyCombo);
    auto setCompanyRowVisible = [companyLbl, companyCombo](bool vis) {
        companyLbl->setVisible(vis);
        companyCombo->setVisible(vis);
        QWidget* p = companyCombo->parentWidget();
        while (p && p->objectName() != "dialogFrame" && p->objectName() != "contentWidget") {
            if (p->objectName() == "inlineFormRowBox") {
                p->setVisible(vis);
                break;
            }
            p = p->parentWidget();
        }
    };

    setCompanyRowVisible(!initPersonal);

    // 3. 资产类别：左下拉 (联动 支出->个人->资产购置 或 支出->工作->投资理财) + 右自定义输入
    auto* catRow = new QHBoxLayout();
    catRow->setSpacing(8);
    auto* typeCombo = new QComboBox(&dlg);
    auto* customTypeEdit = new QLineEdit(&dlg);
    customTypeEdit->setPlaceholderText("自定义新增类别...");
    catRow->addWidget(typeCombo, 1);
    catRow->addWidget(customTypeEdit, 1);
    form->addRow("资产类别", catRow);

    auto refreshAssetCategories = [typeCombo, setCompanyRowVisible, isEdit, existingAsset](bool isPers) {
        setCompanyRowVisible(!isPers);

        typeCombo->clear();
        QStringList subs;
        if (isPers) {
            subs = DatabaseManager::instance().getSubCategoriesByParent("PERSONAL", "EXPENSE", "资产购置");
            if (subs.isEmpty()) subs = {"房产", "车辆", "藏品", "首饰", "贵金属", "设备"};
        } else {
            subs = DatabaseManager::instance().getSubCategoriesByParent("COMPANY", "EXPENSE", "投资理财");
            if (subs.isEmpty()) subs = { "房产", "贵金属", "车辆", "股权","债券"};
        }
        typeCombo->addItems(subs);

        if (isEdit && existingAsset) {
            QString curType = existingAsset->assetType;
            if (curType == "REAL_ESTATE") curType = "房产";
            else if (curType == "VEHICLE") curType = "车辆";
            else if (curType == "COLLECTION") curType = "藏品";
            int idx = typeCombo->findText(curType);
            if (idx >= 0) {
                typeCombo->setCurrentIndex(idx);
            } else if (!curType.isEmpty()) {
                typeCombo->addItem(curType);
                typeCombo->setCurrentText(curType);
            }
        }
    };
    refreshAssetCategories(initPersonal);

    connect(btnPersonal, &QPushButton::clicked, &dlg, [btnPersonal, btnWork, refreshAssetCategories]() {
        btnPersonal->setChecked(true);
        btnWork->setChecked(false);
        refreshAssetCategories(true);
    });
    connect(btnWork, &QPushButton::clicked, &dlg, [btnPersonal, btnWork, refreshAssetCategories]() {
        btnWork->setChecked(true);
        btnPersonal->setChecked(false);
        refreshAssetCategories(false);
    });

    // 4. 资产名称
    auto* nameEdit = new QLineEdit(&dlg);
    nameEdit->setPlaceholderText("例如: 台州椒江万达商住两用公寓");
    if (isEdit) nameEdit->setText(existingAsset->name);
    form->addRow("资产名称", nameEdit);

    // 5. 预估价值 / 出售金额
    auto* valSpin = new QDoubleSpinBox(&dlg);
    valSpin->setRange(0.0, 1000000000.0);
    valSpin->setDecimals(2);
    valSpin->setValue(isEdit ? existingAsset->estimatedValue : 200000.0);
    form->addRow("预估价值", valSpin);

    // 6. 补充说明 (默认 3 行可见多行输入框)
    auto* descEdit = CustomDialog::createMultiLineEdit("选填: 补充说明...", &dlg, 3);
    if (isEdit) descEdit->setPlainText(existingAsset->remark);
    form->addRow("补充说明", descEdit);

    layout->addLayout(form);

    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);

    auto* btnCancel = new QPushButton("取消", &dlg);
    btnCancel->setCursor(Qt::PointingHandCursor);
    auto* btnSave = new QPushButton("保存", &dlg);
    btnSave->setObjectName("primaryBtn");
    btnSave->setCursor(Qt::PointingHandCursor);

    btnRow->addWidget(btnCancel, 1);
    btnRow->addWidget(btnSave, 1);
    layout->addLayout(btnRow);

    dlg.bodyLayout()->addLayout(layout);
    CustomDialog::normalizeFormWidgets(&dlg);
    setCompanyRowVisible(!initPersonal);

    connect(btnCancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(btnSave, &QPushButton::clicked, &dlg, [&]() {
        QString name = nameEdit->text().trimmed();
        if (name.isEmpty()) {
            CustomDialog::markFieldInvalid(nameEdit);
            return;
        }

        const bool isPers = btnPersonal->isChecked();
        QString ownerType = isPers ? "PERSONAL" : "COMPANY";
        QString finalCategory = customTypeEdit->text().trimmed();
        if (!finalCategory.isEmpty()) {
            DatabaseManager::instance().addCustomCategory(
                "EXPENSE",
                isPers ? "资产购置" : "投资理财",
                finalCategory,
                ownerType
            );
        } else {
            finalCategory = typeCombo->currentText().trimmed();
        }
        if (finalCategory.isEmpty()) finalCategory = "房产";

        int companyId = 0;
        if (!isPers) {
            companyId = companyCombo->currentData().toInt();
            if (companyId <= 0 && isEdit && existingAsset) {
                companyId = existingAsset->companyId;
            }
        }
        QString desc = descEdit->toPlainText().trimmed();

        if (isEdit) {
            if (btnSell && btnSell->isChecked()) {
                // 选择了“出售”，个人资产变个人收入，工作资产变公司收入
                DatabaseManager::instance().sellAsset(
                    existingAsset->id,
                    finalCategory,
                    name,
                    valSpin->value(),
                    ownerType,
                    companyId,
                    desc
                );
            } else {
                DatabaseManager::instance().updateAsset(
                    existingAsset->id,
                    finalCategory,
                    name,
                    valSpin->value(),
                    ownerType,
                    companyId,
                    desc
                );
            }
        } else {
            DatabaseManager::instance().addAsset(
                finalCategory,
                name,
                valSpin->value(),
                ownerType,
                QDate::currentDate(),
                0,
                0,
                desc,
                companyId
            );
        }
        dlg.accept();
    });

    if (dlg.exec() == QDialog::Accepted) {
        refreshData();
    }
}

