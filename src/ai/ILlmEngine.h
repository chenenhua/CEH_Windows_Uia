#pragma once

#include <QObject>
#include <QString>
#include <functional>

/**
 * @brief LLM 抽象引擎接口
 * 保持业务逻辑与具体推理后端解耦。
 * 当前使用 OllamaHttpEngine；后续可无缝替换为基于本地编译的 LlamaCppEngine。
 */
class ILlmEngine : public QObject {
    Q_OBJECT
public:
    explicit ILlmEngine(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~ILlmEngine() = default;

    using TokenCallback = std::function<void(const QString& token, bool isFinished)>;
    using ErrorCallback = std::function<void(const QString& errorMsg)>;

    struct EngineChatMessage {
        QString role;    // "system", "user", "assistant"
        QString content;
    };

    /**
     * @brief 异步流式对话
     * @param systemPrompt 设定参谋人格与原则
     * @param userPrompt   当前财务与人脉现状上下文及具体问题
     * @param onToken      逐字推送回调
     * @param onError      异常回调
     */
    virtual void chatStream(const QString& systemPrompt,
                            const QString& userPrompt,
                            TokenCallback onToken,
                            ErrorCallback onError) = 0;

    /**
     * @brief 异步多轮对话流式调用
     * @param messages 上下文对话消息列表 (含 system, user, assistant)
     * @param onToken  逐字推送回调
     * @param onError  异常回调
     */
    virtual void chatStreamMessages(const QList<EngineChatMessage>& messages,
                                    TokenCallback onToken,
                                    ErrorCallback onError) {
        QString sysPrompt;
        QString userPrompt;
        for (const auto& msg : messages) {
            if (msg.role == "system") {
                if (!sysPrompt.isEmpty()) sysPrompt += "\n";
                sysPrompt += msg.content;
            } else if (msg.role == "user") {
                if (!userPrompt.isEmpty()) userPrompt += "\n\n";
                userPrompt += msg.content;
            } else if (msg.role == "assistant") {
                if (!userPrompt.isEmpty()) userPrompt += "\n\n";
                userPrompt += QString("【助手前次回答】：%1").arg(msg.content);
            }
        }
        chatStream(sysPrompt, userPrompt, onToken, onError);
    }

    virtual QString engineName() const = 0;
    virtual bool isBusy() const = 0;
    virtual void cancel() = 0;
};
