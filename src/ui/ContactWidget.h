#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QProgressBar>
#include <QFrame>
#include <QTextBrowser>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include "../core/DatabaseManager.h"

class RelationGraphWidget;

class ContactWidget : public QWidget {
    Q_OBJECT
public:
    explicit ContactWidget(QWidget* parent = nullptr);

    void refreshData();
    int currentSelectedContactId() const { return m_selectedContactId; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void plansChanged();

private slots:
    void onSearchTextChanged(const QString& text);
    void onContactTableRowClicked(int row, int col);
    void onAddContactClicked();
    void onEditContactClicked();
    void onDeleteContactClicked();
    void onImportVcfClicked();
    void onUpdateAvatarClicked();
    void onBottomTabChanged(int index);
    void onAddBottomRecordClicked();
    void onConnectRelationClicked();
    void onEvaluateWithAiClicked();

    void onRelationsContextMenu(const QPoint& pos);
    void onTimelineContextMenu(const QPoint& pos);
    void onEventsContextMenu(const QPoint& pos);
    void onPlansContextMenu(const QPoint& pos);
    void onBusinessContextMenu(const QPoint& pos);

private:
    void setupUi();
    void setupLeftColumn(QWidget* parent);
    void setupMiddleColumn(QWidget* parent);
    void setupRightColumn(QWidget* parent);

    void loadContactDetails(int contactId);
    void loadBottomTabData(int contactId);
    void updateRightColumnStats(const ContactSummary& cs);
    void updateTabButtonsStyle();
    void applyTableColumnRatios();
    QString formatAiReportToHtml(const QString& rawMarkdown) const;

    // 左栏 (好友检索、导入、+添加与列表: 20% 20% 20% 15% 25%)
    QLineEdit* m_searchEdit{nullptr};
    QPushButton* m_btnImport{nullptr};
    QPushButton* m_btnAddContact{nullptr};
    QTableWidget* m_contactsTable{nullptr};

    // 中栏 (好友个人档案、习惯禁忌、亲疏轨迹/礼尚往来/走动计划)
    QPushButton* m_avatarBtn{nullptr};
    QLabel* m_lblName{nullptr};
    QPushButton* m_btnEditContact{nullptr};
    QPushButton* m_btnDeleteContact{nullptr};
    QLabel* m_lblPhone{nullptr};
    QLabel* m_lblAddress{nullptr};
    QLabel* m_lblIntroducer{nullptr};
    QLabel* m_lblCompany{nullptr};
    QLabel* m_lblTitle{nullptr};
    QLabel* m_lblIndustry{nullptr};
    QLabel* m_lblClosenessStars{nullptr};
    QLabel* m_lblInfluenceStars{nullptr};
    QLabel* m_lblCreditStars{nullptr};
    QLabel* m_lblPreferences{nullptr};
    QLabel* m_lblTaboos{nullptr};

    QPushButton* m_btnTabRelations{nullptr};
    QPushButton* m_btnTabTimeline{nullptr};
    QPushButton* m_btnTabEvents{nullptr};
    QPushButton* m_btnTabPlans{nullptr};
    QPushButton* m_btnTabBusiness{nullptr};
    QPushButton* m_btnAddRecord{nullptr};
    QStackedWidget* m_bottomTabStack{nullptr};
    QTableWidget* m_relationsTable{nullptr};
    QTableWidget* m_timelineTable{nullptr};
    QTableWidget* m_eventsTable{nullptr};
    QTableWidget* m_plansTable{nullptr};
    QTableWidget* m_businessTable{nullptr};
    int m_currentBottomTab{0};

    // 右栏 (相识天数、投入回报、动态人脉/商务社交统计条、关系网小窗口、AI点评)
    QLabel* m_lblMeetDate{nullptr};
    QLabel* m_lblSpendVsReturn{nullptr};
    QVBoxLayout* m_socialStatsLayout{nullptr};

    QPushButton* m_btnRefreshGraph{nullptr};
    QPushButton* m_btnConnectRelation{nullptr};
    RelationGraphWidget* m_miniGraphWidget{nullptr};
    QLabel* m_lblGraphFooterName{nullptr};
    QLabel* m_lblGraphFooterCount{nullptr};

    QTextBrowser* m_txtAiReview{nullptr};
    QPushButton* m_btnAiEvaluate{nullptr};
    QString m_accumulatedContactAiReview;
    QTimer* m_contactAiThrottleTimer{nullptr};
    bool m_contactAiDirty{false};

    struct ContactAiMsg {
        QString role;
        QString content;
    };
    QList<ContactAiMsg> m_contactAiHistory;
    QPlainTextEdit* m_contactAiQuestionEdit{nullptr};
    QPushButton* m_btnSendContactQuestion{nullptr};
    bool m_isContactAiGenerating{false};

    void onSendContactConsultQuestion();
    void renderContactAiConversation();

    int m_selectedContactId{0};
    QList<ContactSummary> m_currentContactList;
};
