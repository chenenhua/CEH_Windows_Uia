#pragma once

#include <QObject>
#include <QString>
#include <memory>
#include "ILlmEngine.h"

enum class AdvisorMode {
    Conservative, // 保守稳健 (防御、风控、控制开支、守住现金流)
    Aggressive    // 激进扩张 (进攻、杠杆、资源互换、重注核心贵人)
};

class AiAdvisorService : public QObject {
    Q_OBJECT
public:
    static AiAdvisorService& instance();

    void setEngine(std::shared_ptr<ILlmEngine> engine);
    std::shared_ptr<ILlmEngine> engine() const { return m_engine; }

    void setMode(AdvisorMode mode);
    AdvisorMode mode() const { return m_mode; }
    QString modeName() const;

    // 核心评估指令
    struct ChatMessage {
        QString role; // "user" or "assistant"
        QString content;
    };

    void evaluateGlobalSituation();
    void evaluateContact(int contactId);
    void askCustomQuestion(const QString& userQuestion, int contactId = 0, const QList<ChatMessage>& history = {});
    void cancel();
    void cancelCurrentTask();
    void shutdown();
    bool isBusy() const;
    QString buildSystemPrompt() const;
    static QString getRuleFromAppJson(const QString& ruleKey, const QString& defaultRule = QString());

signals:
    void evaluationStarted();
    void tokenReceived(const QString& token);
    void evaluationFinished(const QString& fullText);
    void evaluationFailed(const QString& error);
    void evaluationCanceled();
    void modeChanged(AdvisorMode newMode);

private:
    AiAdvisorService(QObject* parent = nullptr);
    ~AiAdvisorService() override = default;

    QString buildChatSystemPrompt() const;
    QString buildGlobalContext() const;
    QString buildContactContext(int contactId) const;

    std::shared_ptr<ILlmEngine> m_engine;
    AdvisorMode m_mode{AdvisorMode::Conservative};
    QString m_accumulatedResponse;
};
