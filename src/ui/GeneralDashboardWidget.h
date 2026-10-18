#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QLineEdit>
#include "../core/DatabaseManager.h"
#include "../ai/AiAdvisorService.h"

class ChatBubbleWidget;

class GeneralDashboardWidget : public QWidget {
    Q_OBJECT
public:
    explicit GeneralDashboardWidget(QWidget* parent = nullptr);

    void refreshData();
    void checkAndTriggerInitialAiState(bool force = false);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void requestAddTransaction(const QString& type);
    void requestAddPlan();

private slots:
    void onToggleExpenseIncome(bool isExpense);
    void onAddTransactionClicked();
    void onEditProfileClicked();
    void onAddCompanyClicked();
    void onAddAssetClicked();
    void onSendAiPrompt();
    void onClearAiChat();

private:
    void setupUi();
    void setupExpenseOverviewColumn(QWidget* parent);
    void setupAiChatColumn(QWidget* parent);
    void setupProfileFinanceColumn(QWidget* parent);

    void updateCategoryList();
    void updateProfileCard();
    void updateCompanyCard();
    void updateFinanceCard();
    void updateAssetCard();
    void openAssetDialog(const AssetItem* existingAsset = nullptr);
    ChatBubbleWidget* appendAiMessage(const QString& sender, const QString& content, bool isAi);
    void updateChatBubblesWidth();
    void resetSendButtonState();
    void disconnectAiSignals();
    void clearChatForUnconfiguredAi();
    void clearChatWidgetsOnly();

    bool m_initialAiCheckDone{false};

    // Column 1 成员 (收支概览)
    QPushButton* m_btnExpenseTab{nullptr};
    QPushButton* m_btnIncomeTab{nullptr};
    QPushButton* m_btnAddTrans{nullptr};
    QVBoxLayout* m_categoryListLayout{nullptr};
    QLabel* m_monthlyTotalLabel{nullptr};
    QString m_currentOverviewType{"EXPENSE"};

    // Column 2 成员 (AI 人生规划)
    QScrollArea* m_chatScroll{nullptr};
    QWidget* m_chatContentWidget{nullptr};
    QVBoxLayout* m_chatLayout{nullptr};
    QLineEdit* m_chatInput{nullptr};
    QPushButton* m_btnSendChat{nullptr};
    QList<AiAdvisorService::ChatMessage> m_chatHistory;
    QMetaObject::Connection m_connToken;
    QMetaObject::Connection m_connFinished;
    QMetaObject::Connection m_connFailed;
    QMetaObject::Connection m_connCanceled;
    ChatBubbleWidget* m_streamingAiBubble{nullptr};
    QString m_streamingAiText;
    QTimer* m_streamingThrottleTimer{nullptr};
    bool m_streamingDirty{false};

    // Column 3 成员 (个人名片、个人财务、公司财务、资产)
    QLabel* m_profileNameLabel{nullptr};
    QLabel* m_profilePhoneLabel{nullptr};
    QLabel* m_profileAddressLabel{nullptr};
    QLabel* m_profileIndustryLabel{nullptr};
    QLabel* m_profileTitleLabel{nullptr};
    QLabel* m_profileCompanyLabel{nullptr};
    QLabel* m_profileFriendsLabel{nullptr};

    // 个人财务
    QLabel* m_personalFinanceStatusPill{nullptr};
    QLabel* m_personalCurrentFundLabel{nullptr};      // 当前资金
    QLabel* m_personalLiquidFundLabel{nullptr};       // 流动资金 (当前资金 - 负债)
    QLabel* m_personalTotalIncomeLabel{nullptr};      // 总进资金
    QLabel* m_personalTotalExpenseLabel{nullptr};     // 总出资金
    QLabel* m_personalDebtLabel{nullptr};              // 负债
    QLabel* m_personalReceivableLabel{nullptr};        // 应收
    QLabel* m_personalAvgExpenseLabel{nullptr};        // 月均支出
    QLabel* m_personalAvgIncomeLabel{nullptr};         // 月均收出

    // 公司财务
    QLabel* m_companyFinanceStatusPill{nullptr};
    QLabel* m_companyCurrentFundLabel{nullptr};       // 当前资金
    QLabel* m_companyLiquidFundLabel{nullptr};        // 流动资金 (当前资金 - 负债)
    QLabel* m_companyTotalIncomeLabel{nullptr};       // 总进资金
    QLabel* m_companyTotalExpenseLabel{nullptr};      // 总出资金
    QLabel* m_companyDebtLabel{nullptr};               // 负债
    QLabel* m_companyReceivableLabel{nullptr};         // 应收
    QLabel* m_companyAvgExpenseLabel{nullptr};         // 月均支出
    QLabel* m_companyAvgIncomeLabel{nullptr};          // 月均收出

    QVBoxLayout* m_companyListLayout{nullptr};
    QVBoxLayout* m_assetListLayout{nullptr};
};
