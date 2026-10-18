#pragma once

#include "ILlmEngine.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <memory>

class OllamaHttpEngine : public ILlmEngine {
    Q_OBJECT
public:
    explicit OllamaHttpEngine(const QString& endpoint = "http://127.0.0.1:11434/v1",
                              const QString& model = "qwen9b-vl",
                              QObject* parent = nullptr);
    ~OllamaHttpEngine() override;

    static std::shared_ptr<OllamaHttpEngine> createFromConfig(QObject* parent = nullptr);
    static QString findLlamaServerBinary(bool preferCuda = true);
    static bool isCudaSupportedOnSystem();
    static bool isVulkanSupportedOnSystem();
    static void shutdownLocalServer();
    static void killLlamaServerSilently();
    static void prewarmLocalServer(const QString& ggufPath, int numCtx, int gpuLayers = 99, int threads = 0);

    void setApiKey(const QString& apiKey) { m_apiKey = apiKey.trimmed(); }
    void setLocalGgufPath(const QString& ggufPath) { m_localGgufPath = ggufPath.trimmed(); }
    void setTokenLimits(int numCtx, int numPredict) {
        m_numCtx = numCtx;
        m_numPredict = numPredict;
    }
    void setSamplingParams(double temperature, double topP, double repeatPenalty) {
        m_temperature = temperature;
        m_topP = topP;
        m_repeatPenalty = repeatPenalty;
    }
    void setLocalHardwareParams(int gpuLayers, int threads, int port = 18089) {
        m_gpuLayers = gpuLayers;
        m_threads = threads;
        m_localPort = port;
    }

    using AvailabilityCallback = std::function<void(bool available, const QString& detail)>;
    void checkAvailability(AvailabilityCallback callback);

    void chatStream(const QString& systemPrompt,
                    const QString& userPrompt,
                    TokenCallback onToken,
                    ErrorCallback onError) override;

    void chatStreamMessages(const QList<EngineChatMessage>& messages,
                            TokenCallback onToken,
                            ErrorCallback onError) override;

    QString engineName() const override;
    bool isBusy() const override { return m_currentReply != nullptr || m_waitingForLocalServer; }
    void cancel() override;
    bool isLastResponseIncomplete() const { return m_lastResponseIncomplete; }

private slots:
    void onReadyRead();
    void onFinished();
    void onErrorOccurred(QNetworkReply::NetworkError code);

private:
    void sendChatCompletionRequest(const QString& baseEndpoint,
                                   const QString& modelName,
                                   const QList<EngineChatMessage>& messages);

    QString m_endpoint;
    QString m_model;
    QString m_apiKey;
    QString m_localGgufPath;
    int m_numCtx{8192};
    int m_numPredict{4096};
    double m_temperature{0.60};
    double m_topP{0.90};
    double m_repeatPenalty{1.10};
    int m_gpuLayers{99};
    int m_threads{4};
    int m_localPort{18089};
    bool m_waitingForLocalServer{false};
    bool m_responseFinished{false};
    bool m_lastResponseIncomplete{false};

    QNetworkAccessManager* m_nam{nullptr};
    QNetworkReply* m_currentReply{nullptr};
    QNetworkReply* m_healthReply{nullptr};

    TokenCallback m_tokenCallback;
    ErrorCallback m_errorCallback;

    QByteArray m_buffer;
    QString m_accumulatedText;
};
