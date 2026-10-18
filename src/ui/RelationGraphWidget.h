#pragma once

#include <QWidget>
#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QPushButton>
#include <QMap>
#include <QSet>
#include <QList>

class InteractiveGraphView;

class RelationGraphWidget : public QWidget {
    Q_OBJECT
public:
    explicit RelationGraphWidget(QWidget* parent = nullptr);

    void setMiniMode(bool mini);
    void setCenterContactId(int contactId);

    void refreshGraph();
    void refreshData() { refreshGraph(); }

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void setupUi();
    void setupSummaryCard();
    void updateSummaryCard(int contactId);
    void repositionSummaryCard();
    void applyHighlightForNode(int clickedNodeId);

    struct EdgeVisual {
        int u{0};
        int v{0};
        QGraphicsLineItem* line{nullptr};
        QGraphicsTextItem* label{nullptr};
        QColor origLineColor;
        QColor origTextColor;
        bool dashed{true};
        double baseWidth{1.3};
    };

    bool m_isMiniMode{false};
    int m_centerContactId{0};
    int m_highlightedNodeId{0}; // 0 表示"我"(全部高亮)，>0 表示当前点击聚焦的联系人ID

    QWidget* m_headerWidget{nullptr};
    InteractiveGraphView* m_view{nullptr};
    QGraphicsScene* m_scene{nullptr};

    // 右上角联系人总结浮窗 (点击除自己外任意联系人时显示)
    QFrame* m_summaryCard{nullptr};
    QLabel* m_lblMeetDate{nullptr};
    QLabel* m_lblSpendVsReturn{nullptr};
    QWidget* m_socialStatsContainer{nullptr};
    QVBoxLayout* m_socialStatsLayout{nullptr};
    QLabel* m_lblClosenessStars{nullptr};
    QLabel* m_lblInfluenceStars{nullptr};
    QLabel* m_lblCreditStars{nullptr};
    QLabel* m_lblPhone{nullptr};
    QLabel* m_lblCompany{nullptr};
    QLabel* m_lblAddress{nullptr};
    QLabel* m_lblTitle{nullptr};
    QLabel* m_lblIndustry{nullptr};
    QLabel* m_lblIntroducer{nullptr};
    QLabel* m_lblPreferences{nullptr};
    QLabel* m_lblTaboos{nullptr};

    QMap<int, QGraphicsItem*> m_nodeItems;
    QMap<int, QSet<int>> m_adjSet;
    QList<EdgeVisual> m_edgeVisuals;
};
