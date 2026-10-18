#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QListWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QTextBrowser>
#include <QPushButton>
#include <QStackedWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QMap>
#include <QSet>
#include <functional>
#include "../ai/AiAdvisorService.h"

class InteractiveBusinessGraphView;

class BusinessNetWidget : public QWidget {
    Q_OBJECT
public:
    struct CompanyInfo {
        int id{0};
        QString name;
        QString taxNo;
        QString industry;
        QString roleType;
        double capital{0.0};
        double shareRatio{100.0};
        QString notes;
        int legalRepContactId{0};
        QString legalRepName;
        int memberCount{0};
        bool isMyCompany{false};
    };

    struct MultiLevelNode {
        int id{0};
        int level{0}; // 0: 公司, 1: 员工, 2: 朋友, 3: 朋友的朋友, 4: ...
        QString name;
        QString industry;
        QString jobTitle;      // 职务 (在公司的职务，Level 1 员工专用)
        QString relationName;  // 关系 (与父节点的人脉关系名称)
        QString relationType;
        int parentId{0};
        int parentLevel{0};
        int closenessScore{3};
        QString phone;
    };

    struct VisualEdge {
        int fromLevel{0};
        int fromId{0};
        int toLevel{1};
        int toId{0};
        QGraphicsPathItem* pathItem{nullptr};
        QGraphicsItem* labelItem{nullptr};
        QColor baseColor;
        bool isDashed{false};
    };

    explicit BusinessNetWidget(QWidget* parent = nullptr);
    ~BusinessNetWidget() override = default;

    void refreshData();

protected:
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onCompanySearchChanged(const QString& text);
    void onCompanyItemClicked(QListWidgetItem* item);
    void onCompanyContextMenu(const QPoint& pos);
    void onEditCompanyClicked();
    void onDeleteCompanyClicked();
    void onGenerateAiClicked();
    void onSendConsultQuestion();
    void onResetGraphZoom();

private:
    void setupUi();
    void loadCompanyList();
    void updateCompanyListSelectionVisuals();
    void loadCompanyTopology(int companyId, const QString& companyName);
    void resetAiPromptPage(const QString& companyName);
    void appendAiChatMessage(const QString& role, const QString& content);

    // 节点高亮辅助
    void handleNodeClicked(int level, int id);
    void clearNodeHighlight();

    // 左侧面板
    QLineEdit* m_searchEdit{nullptr};
    QListWidget* m_companyList{nullptr};
    QList<CompanyInfo> m_allCompanies;

    // 中间拓扑图面板
    InteractiveBusinessGraphView* m_graphView{nullptr};
    QGraphicsScene* m_graphScene{nullptr};
    QLabel* m_topologyTitle{nullptr};
    QLabel* m_topologyStats{nullptr};
    QPushButton* m_btnResetZoom{nullptr};
    QPushButton* m_btnZoomIn{nullptr};
    QPushButton* m_btnZoomOut{nullptr};

    // 右侧 AI 商业策划面板 (完全对齐联系人详情 AI 点评 UI)
    QLabel* m_aiTopTitle{nullptr};
    QPushButton* m_btnTopGenerate{nullptr};
    QTextBrowser* m_txtAiReview{nullptr};
    QPlainTextEdit* m_aiQuestionEdit{nullptr}; // 支持多行 (至少可见5行)
    QPushButton* m_btnSendQuestion{nullptr};

    // 拓扑图数据与视觉映射
    int m_selectedCompanyId{-1};
    QString m_selectedCompanyName;
    QList<MultiLevelNode> m_allGraphNodes;

    QGraphicsItem* m_centerCompanyItem{nullptr};
    QMap<int, QGraphicsItem*> m_nodeItemMap; // contactId -> item
    QList<VisualEdge> m_visualEdges;

    // AI 对话历史与状态
    QList<AiAdvisorService::ChatMessage> m_aiHistory;
    bool m_isAiGenerating{false};
    bool m_isInitialCompanyGeneration{false};
    bool m_isLastResponseIncomplete{false};
    QString m_accumulatedResponse;
    QTimer* m_throttleRenderTimer{nullptr};
    bool m_renderDirty{false};

    QString formatAiReportToHtml(const QString& rawMarkdown) const;
    void renderFullConversation();
    bool loadCompanyAiCache(int compId);
    void saveCurrentCompanyAiCache();
};
