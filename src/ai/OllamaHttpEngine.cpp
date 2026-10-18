#include "OllamaHttpEngine.h"
#include "../core/DatabaseManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPointer>
#include <QProcess>
#include <QTimer>
#include <QThread>
#include <QDebug>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

// 负责在后台托管 llama-server.exe 进程，避免每次对话重新加载 GGUF 权重
class LlamaServerManager {
public:
    static LlamaServerManager& instance() {
        static LlamaServerManager s_mgr;
        return s_mgr;
    }

    QString localEndpoint() const {
        return QString("http://127.0.0.1:%1/v1").arg(m_port);
    }

    void stopServer(bool resetFallback = true) {
        if (m_process) {
            m_process->disconnect();
            if (m_process->state() != QProcess::NotRunning) {
                m_process->kill();
                m_process->waitForFinished(1000);
            }
            delete m_process;
            m_process = nullptr;
        }
#ifdef _WIN32
        OllamaHttpEngine::killLlamaServerSilently();
#endif
        m_activeGgufPath.clear();
        if (resetFallback) {
            m_triedCpuFallback = false;
            m_forceCpu = false;
        }
    }

    bool ensureRunning(const QString& ggufPath, int numCtx, QString& errMsg, int gpuLayers = 99, int threads = 0, int port = 18089) {
        QString resolvedGguf = DatabaseManager::instance().resolveAssetPath(ggufPath);
        QFileInfo ggufInfo(resolvedGguf);
        if (!ggufInfo.exists() || !ggufInfo.isFile()) {
            errMsg = QString("本地 GGUF 模型文件不存在: %1").arg(resolvedGguf);
            return false;
        }
        if (!resolvedGguf.endsWith(".gguf", Qt::CaseInsensitive)) {
            errMsg = "本地模型文件必须为 .gguf 格式！";
            return false;
        }

        if (port > 0) m_port = port;

        if (m_process && (m_process->state() == QProcess::Running || m_process->state() == QProcess::Starting) &&
            m_activeGgufPath == ggufInfo.absoluteFilePath()) {
            return true;
        }

        // 停止当前服务，但不重置 fallback 状态以防死循环
        stopServer(false);

        bool canUseCuda = !m_forceCpu && !m_triedCpuFallback && OllamaHttpEngine::isCudaSupportedOnSystem();
        QString serverBin = OllamaHttpEngine::findLlamaServerBinary(canUseCuda);
        if (serverBin.isEmpty()) {
            errMsg = "未找到内置 llama.cpp 驱动组件 (llama-server.exe)，请检查 runtime/Windows 目录。";
            return false;
        }

        m_process = new QProcess();
        QFileInfo binInfo(serverBin);
        m_process->setWorkingDirectory(binInfo.absolutePath());
        m_process->setProgram(binInfo.absoluteFilePath());

        // 将当前 runtime 目录优先插入系统 PATH 环境变量最前，彻底根除跨电脑丢失依赖 DLL 无法启动的问题
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        QString oldPath = env.value("PATH");
        env.insert("PATH", QDir::toNativeSeparators(binInfo.absolutePath()) + ";" + oldPath);
        m_process->setProcessEnvironment(env);

        // 本地桌面模式使用 -np 1 单槽位，并将本地 GGUF 上下文放宽到充足空间 (16384 ~ 32768)
        int targetCtx = (numCtx >= 16384) ? numCtx : 16384;
        int safeCtx = qBound(16384, targetCtx, 32768);
        QStringList args;
        args << "-m" << ggufInfo.absoluteFilePath()
             << "--host" << "127.0.0.1"
             << "--port" << QString::number(m_port)
             << "-np" << "1"
             << "-c" << QString::number(safeCtx);

        int idealThreads = (threads > 0) ? threads : qMax(2, QThread::idealThreadCount() - 2);
        args << "-t" << QString::number(idealThreads);

        bool isGpuBin = serverBin.contains("cuda", Qt::CaseInsensitive) || serverBin.contains("vulkan", Qt::CaseInsensitive);
        if (isGpuBin) {
            int offload = (gpuLayers >= 0) ? gpuLayers : 99;
            args << "-ngl" << QString::number(offload);
        }

        m_activeGgufPath = ggufInfo.absoluteFilePath();
        m_process->setArguments(args);

        // 捕获标准错误与输出日志，便于故障诊断
        QObject::connect(m_process, &QProcess::readyReadStandardError, [this]() {
            if (m_process) {
                QByteArray errData = m_process->readAllStandardError();
                qWarning().noquote() << "[llama-server:stderr]" << QString::fromUtf8(errData).trimmed();
            }
        });

        m_process->start();
        if (!m_process->waitForStarted(1500)) {
            if (m_process->state() == QProcess::NotRunning) {
                if (isGpuBin && !m_triedCpuFallback) {
                    m_triedCpuFallback = true;
                    m_forceCpu = true;
                    qWarning() << "[llama.cpp] GPU 驱动启动失败，自动无缝降级切换至 CPU 推理引擎...";
                    return ensureRunning(ggufPath, numCtx, errMsg, 0, threads, port);
                }
                errMsg = QString("启动 llama.cpp 引擎失败: %1").arg(m_process->errorString());
                stopServer(true);
                return false;
            }
        }
        return true;
    }

    bool isProcessAlive() const {
        return m_process && (m_process->state() == QProcess::Running || m_process->state() == QProcess::Starting);
    }

    bool tryFallbackCpu(const QString& ggufPath, int numCtx, QString& errMsg, int threads = 0, int port = 18089) {
        if (m_triedCpuFallback) return false;
        m_triedCpuFallback = true;
        m_forceCpu = true;
        qWarning() << "[llama.cpp] 引擎进程异常终止，立即自动切换至 CPU 推理模式重试...";
        return ensureRunning(ggufPath, numCtx, errMsg, 0, threads, port);
    }

private:
    LlamaServerManager() {
        if (qApp) {
            QObject::connect(qApp, &QCoreApplication::aboutToQuit, []() {
                LlamaServerManager::instance().stopServer(true);
            });
        }
    }
    ~LlamaServerManager() {
        stopServer(true);
    }

    QProcess* m_process{nullptr};
    QString m_activeGgufPath;
    int m_port{18089};
    bool m_triedCpuFallback{false};
    bool m_forceCpu{false};
};

QString normalizeChatCompletionsUrl(const QString& rawEndpoint) {
    QString ep = rawEndpoint.trimmed();
    while (ep.endsWith('/')) {
        ep.chop(1);
    }
    if (ep.endsWith("/chat/completions", Qt::CaseInsensitive)) {
        return ep;
    }
    if (ep.endsWith("/v1", Qt::CaseInsensitive) || ep.endsWith("/api/v3", Qt::CaseInsensitive)) {
        return ep + "/chat/completions";
    }
    return ep + "/v1/chat/completions";
}

QString normalizeModelsUrl(const QString& rawEndpoint) {
    QString ep = rawEndpoint.trimmed();
    while (ep.endsWith('/')) {
        ep.chop(1);
    }
    if (ep.endsWith("/chat/completions", Qt::CaseInsensitive)) {
        ep.chop(QString("/chat/completions").length());
    }
    if (ep.endsWith("/models", Qt::CaseInsensitive)) {
        return ep;
    }
    if (ep.endsWith("/v1", Qt::CaseInsensitive) || ep.endsWith("/api/v3", Qt::CaseInsensitive)) {
        return ep + "/models";
    }
    return ep + "/v1/models";
}

} // namespace

OllamaHttpEngine::OllamaHttpEngine(const QString& endpoint, const QString& model, QObject* parent)
    : ILlmEngine(parent), m_endpoint(endpoint.trimmed()), m_model(model.trimmed()) {
    m_nam = new QNetworkAccessManager(this);
}

OllamaHttpEngine::~OllamaHttpEngine() {
    cancel();
}

void OllamaHttpEngine::shutdownLocalServer() {
    LlamaServerManager::instance().stopServer();
    killLlamaServerSilently();
}

void OllamaHttpEngine::prewarmLocalServer(const QString& ggufPath, int numCtx, int gpuLayers, int threads) {
    if (ggufPath.trimmed().isEmpty()) return;
    QString startErr;
    LlamaServerManager::instance().ensureRunning(ggufPath.trimmed(), numCtx, startErr, gpuLayers, threads);
}

void OllamaHttpEngine::killLlamaServerSilently() {
#if defined(Q_OS_WIN)
    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    wchar_t cmd[] = L"taskkill.exe /F /IM llama-server.exe /T";
    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 1000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
#endif
}

std::shared_ptr<OllamaHttpEngine> OllamaHttpEngine::createFromConfig(QObject* parent) {
    auto& db = DatabaseManager::instance();
    QString engineMode = db.getSecurityConfig("llm_engine_mode").trimmed();
    QString localGguf = db.getSecurityConfig("llm_local_gguf_path").trimmed();
    QString ep = db.getSecurityConfig("llm_endpoint").trimmed();
    if (ep.isEmpty()) ep = "http://127.0.0.1:11434/v1";
    QString apiKey = db.getSecurityConfig("llm_api_key").trimmed();
    QString model = db.getSecurityConfig("llm_model_name").trimmed();
    if (model.isEmpty()) model = "qwen9b-vl";

    int numCtx = db.getSecurityConfig("llm_num_ctx").toInt();
    if (numCtx < 8192) numCtx = 16384;
    int numPredict = db.getSecurityConfig("llm_num_predict").toInt();
    if (numPredict <= 0) numPredict = 4096;

    double temperature = 0.60;
    double topP = 0.90;
    double repeatPenalty = 1.10;
    int gpuLayers = 99;
    int threads = qMax(2, QThread::idealThreadCount() - 2);
    int localPort = 18089;

    QString dbTemp = db.getSecurityConfig("llm_temperature");
    if (!dbTemp.isEmpty()) temperature = dbTemp.toDouble();
    QString dbTopP = db.getSecurityConfig("llm_top_p");
    if (!dbTopP.isEmpty()) topP = dbTopP.toDouble();
    QString dbRep = db.getSecurityConfig("llm_repeat_penalty");
    if (!dbRep.isEmpty()) repeatPenalty = dbRep.toDouble();
    QString dbGpu = db.getSecurityConfig("llm_gpu_layers");
    if (!dbGpu.isEmpty()) gpuLayers = dbGpu.toInt();
    QString dbTh = db.getSecurityConfig("llm_threads");
    if (!dbTh.isEmpty()) threads = dbTh.toInt();

    // 核心要求 1：优先检查 assets/models/models.json 是否有最新模型配置覆盖
    QStringList modelJsonPaths = {
        DatabaseManager::instance().resolveAssetPath("assets/models/models.json"),
        DatabaseManager::instance().resolveAssetPath("models/models.json")
    };
    for (const auto& p : modelJsonPaths) {
        if (!p.isEmpty() && QFile::exists(p)) {
            QFile f(p);
            if (f.open(QIODevice::ReadOnly)) {
                QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
                f.close();
                if (doc.isObject() && doc.object().contains("ai_model")) {
                    QJsonObject m = doc.object()["ai_model"].toObject();
                    if (m.contains("engine_mode") && !m["engine_mode"].toString().trimmed().isEmpty()) {
                        engineMode = m["engine_mode"].toString().trimmed();
                    }
                    if (m.contains("local_gguf_path")) {
                        QString gPath = m["local_gguf_path"].toString().trimmed();
                        if (!gPath.isEmpty()) localGguf = gPath;
                    }
                    if (m.contains("endpoint") && !m["endpoint"].toString().trimmed().isEmpty()) ep = m["endpoint"].toString().trimmed();
                    if (m.contains("api_key") && !m["api_key"].toString().trimmed().isEmpty()) apiKey = m["api_key"].toString().trimmed();
                    if (m.contains("model_name") && !m["model_name"].toString().trimmed().isEmpty()) model = m["model_name"].toString().trimmed();
                    if (m.contains("num_ctx") && m["num_ctx"].toInt() > 0) numCtx = m["num_ctx"].toInt();
                    if (m.contains("num_predict") && m["num_predict"].toInt() > 0) numPredict = m["num_predict"].toInt();
                    if (m.contains("temperature") && m["temperature"].toDouble() > 0.0) temperature = m["temperature"].toDouble();
                    if (m.contains("top_p") && m["top_p"].toDouble() > 0.0) topP = m["top_p"].toDouble();
                    if (m.contains("repeat_penalty") && m["repeat_penalty"].toDouble() > 0.0) repeatPenalty = m["repeat_penalty"].toDouble();
                    if (m.contains("gpu_layers")) gpuLayers = m["gpu_layers"].toInt();
                    if (m.contains("threads") && m["threads"].toInt() > 0) threads = m["threads"].toInt();
                    if (m.contains("local_port") && m["local_port"].toInt() > 0) localPort = m["local_port"].toInt();
                    break;
                }
            }
        }
    }

    if (engineMode == "service") {
        localGguf.clear();
    }

    auto engine = std::make_shared<OllamaHttpEngine>(ep, model, parent);
    engine->setApiKey(apiKey);
    engine->setLocalGgufPath(localGguf);
    engine->setTokenLimits(numCtx, numPredict);
    engine->setSamplingParams(temperature, topP, repeatPenalty);
    engine->setLocalHardwareParams(gpuLayers, threads, localPort);

    if (localGguf.isEmpty()) {
        LlamaServerManager::instance().stopServer(true);
    }
    return engine;
}

bool OllamaHttpEngine::isCudaSupportedOnSystem() {
#if defined(Q_OS_WIN)
    HMODULE h = LoadLibraryW(L"nvcuda.dll");
    if (h) {
        FreeLibrary(h);
        return true;
    }
    return false;
#else
    return false;
#endif
}

bool OllamaHttpEngine::isVulkanSupportedOnSystem() {
#if defined(Q_OS_WIN)
    HMODULE h = LoadLibraryW(L"vulkan-1.dll");
    if (h) {
        FreeLibrary(h);
        return true;
    }
    return false;
#else
    return false;
#endif
}

QString OllamaHttpEngine::findLlamaServerBinary(bool preferCuda) {
    QString appDir = QCoreApplication::applicationDirPath();
    QString curDir = QDir::currentPath();
    QString rootDir = DatabaseManager::instance().projectRootDir();

    QStringList cudaCandidates = {
        appDir + "/runtime/Windows/cuda12/llama-server.exe",
        curDir + "/runtime/Windows/cuda12/llama-server.exe",
        rootDir + "/runtime/Windows/cuda12/llama-server.exe",
        appDir + "/../runtime/Windows/cuda12/llama-server.exe",
        curDir + "/llama.cpp/build-cuda/bin/Release/llama-server.exe",
        appDir + "/../llama.cpp/build-cuda/bin/Release/llama-server.exe"
    };

    QStringList vulkanCandidates = {
        appDir + "/runtime/Windows/vulkan/llama-server.exe",
        curDir + "/runtime/Windows/vulkan/llama-server.exe",
        rootDir + "/runtime/Windows/vulkan/llama-server.exe",
        appDir + "/../runtime/Windows/vulkan/llama-server.exe",
        curDir + "/llama.cpp/build-vulkan/bin/Release/llama-server.exe",
        appDir + "/../llama.cpp/build-vulkan/bin/Release/llama-server.exe"
    };

    QStringList cpuCandidates = {
        appDir + "/runtime/Windows/cpu/llama-server.exe",
        curDir + "/runtime/Windows/cpu/llama-server.exe",
        rootDir + "/runtime/Windows/cpu/llama-server.exe",
        appDir + "/../runtime/Windows/cpu/llama-server.exe",
        curDir + "/llama.cpp/build-cpu/bin/Release/llama-server.exe",
        appDir + "/../llama.cpp/build-cpu/bin/Release/llama-server.exe"
    };

    bool allowCuda = preferCuda && isCudaSupportedOnSystem();
    bool allowVulkan = isVulkanSupportedOnSystem();

    QStringList searchOrder;
    if (allowCuda) {
        // 优先 NVIDIA CUDA -> 次选 Vulkan (跨平台GPU) -> 保底 CPU
        searchOrder = cudaCandidates + vulkanCandidates + cpuCandidates;
    } else if (allowVulkan) {
        // 无 CUDA 时优先 Vulkan -> 保底 CPU -> 最后考虑 CUDA
        searchOrder = vulkanCandidates + cpuCandidates + cudaCandidates;
    } else {
        // 无可用 GPU 运行时，直接走通用 CPU
        searchOrder = cpuCandidates + vulkanCandidates + cudaCandidates;
    }

    for (const QString& path : searchOrder) {
        QFileInfo fi(path);
        if (fi.exists() && fi.isFile()) {
            return fi.absoluteFilePath();
        }
    }
    return QString();
}

QString OllamaHttpEngine::engineName() const {
    if (!m_localGgufPath.isEmpty()) {
        return QString("llama.cpp (%1)").arg(QFileInfo(m_localGgufPath).fileName());
    }
    return QString("Model Service (%1)").arg(m_model);
}

void OllamaHttpEngine::checkAvailability(AvailabilityCallback callback) {
    QPointer<OllamaHttpEngine> self(this);

    // 1. 本地 .gguf 模型可用性检测
    if (!m_localGgufPath.isEmpty()) {
        QString startErr;
        int port = (m_localPort > 0) ? m_localPort : 18089;
        if (!LlamaServerManager::instance().ensureRunning(m_localGgufPath, m_numCtx, startErr, m_gpuLayers, m_threads, port)) {
            if (callback) callback(false, startErr);
            return;
        }

        auto pollCheck = std::make_shared<std::function<void(int)>>();
        *pollCheck = [self, callback, pollCheck, port](int attempt) {
            if (!self) return;
            if (!LlamaServerManager::instance().isProcessAlive()) {
                QString fallbackErr;
                if (LlamaServerManager::instance().tryFallbackCpu(self->m_localGgufPath, self->m_numCtx, fallbackErr, self->m_threads, port)) {
                    QTimer::singleShot(500, self, [self, pollCheck]() {
                        if (self) (*pollCheck)(1);
                    });
                    return;
                }
                if (callback) callback(false, "本地 GGUF 模型加载失败，请检查模型文件是否完整");
                return;
            }

            QNetworkRequest req(QUrl(QString("http://127.0.0.1:%1/health").arg(port)));
            req.setTransferTimeout(1500);
            QNetworkReply* reply = self->m_nam->get(req);
            connect(reply, &QNetworkReply::finished, self, [self, reply, callback, pollCheck, attempt]() {
                int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                reply->deleteLater();
                if (!self) return;

                if (code == 200) {
                    if (callback) callback(true, QString());
                } else if (attempt < 120) { // 延长至 60 秒容忍模型权重加载
                    QTimer::singleShot(500, self, [self, pollCheck, attempt]() {
                        if (self) (*pollCheck)(attempt + 1);
                    });
                } else {
                    if (callback) callback(false, "本地模型加载等待超时 (超过60秒)");
                }
            });
        };
        (*pollCheck)(0);
        return;
    }

    // 2. 远程或本地模型服务接口 (如 Ollama / OpenAI 兼容接口) 可用性检测
    if (m_endpoint.isEmpty() || m_model.isEmpty()) {
        if (callback) callback(false, "模型未配置");
        return;
    }

    QNetworkRequest req(QUrl(normalizeModelsUrl(m_endpoint)));
    req.setTransferTimeout(2500);
    if (!m_apiKey.isEmpty()) {
        req.setRawHeader("Authorization", "Bearer " + m_apiKey.toUtf8());
    }

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [self, reply, callback]() {
        if (!self) {
            reply->deleteLater();
            return;
        }
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QNetworkReply::NetworkError netErr = reply->error();
        QByteArray body = reply->readAll();
        reply->deleteLater();

        if (netErr == QNetworkReply::NoError && statusCode == 200) {
            QJsonDocument doc = QJsonDocument::fromJson(body);
            if (doc.isObject() && doc.object().contains("data") && doc.object()["data"].isArray()) {
                QJsonArray arr = doc.object()["data"].toArray();
                if (arr.isEmpty()) {
                    if (callback) callback(false, "模型列表为空");
                    return;
                }
                QString target = self->m_model.trimmed();
                bool found = false;
                for (const auto& v : arr) {
                    QString id = v.toObject()["id"].toString().trimmed();
                    if (id.compare(target, Qt::CaseInsensitive) == 0 ||
                        id.startsWith(target + ":", Qt::CaseInsensitive) ||
                        target.startsWith(id + ":", Qt::CaseInsensitive)) {
                        found = true;
                        break;
                    }
                }
                bool isLocal = self->m_endpoint.contains("127.0.0.1") || self->m_endpoint.contains("localhost");
                if (isLocal && !found) {
                    if (callback) callback(false, "未找到指定模型");
                    return;
                }
            }
            if (callback) callback(true, QString());
            return;
        }

        // 若接口不支持 /models (例如部分仅暴露 /chat/completions 的网关返回 404)，则用 1-token 探测
        if (statusCode == 404) {
            QNetworkRequest chatReq(QUrl(normalizeChatCompletionsUrl(self->m_endpoint)));
            chatReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            chatReq.setTransferTimeout(3500);
            if (!self->m_apiKey.isEmpty()) {
                chatReq.setRawHeader("Authorization", "Bearer " + self->m_apiKey.toUtf8());
            }
            QJsonObject root;
            root["model"] = self->m_model;
            root["stream"] = false;
            root["max_tokens"] = 1;
            QJsonArray msgs;
            QJsonObject m;
            m["role"] = "user";
            m["content"] = "ping";
            msgs.append(m);
            root["messages"] = msgs;

            QNetworkReply* chatReply = self->m_nam->post(chatReq, QJsonDocument(root).toJson(QJsonDocument::Compact));
            connect(chatReply, &QNetworkReply::finished, self, [self, chatReply, callback]() {
                int chatCode = chatReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                bool ok = (chatReply->error() == QNetworkReply::NoError && chatCode == 200);
                chatReply->deleteLater();
                if (!self) return;
                if (callback) callback(ok, ok ? QString() : "模型服务不可用");
            });
            return;
        }

        if (callback) callback(false, "模型服务不可用");
    });
}

void OllamaHttpEngine::cancel() {
    m_waitingForLocalServer = false;
    m_responseFinished = true;
    if (m_healthReply) {
        m_healthReply->disconnect(this);
        m_healthReply->abort();
        m_healthReply->deleteLater();
        m_healthReply = nullptr;
    }
    if (m_currentReply) {
        m_currentReply->disconnect(this);
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
}

void OllamaHttpEngine::chatStream(const QString& systemPrompt,
                                  const QString& userPrompt,
                                  TokenCallback onToken,
                                  ErrorCallback onError) {
    QList<EngineChatMessage> msgs;
    if (!systemPrompt.isEmpty()) {
        msgs.append({"system", systemPrompt});
    }
    if (!userPrompt.isEmpty()) {
        msgs.append({"user", userPrompt});
    }
    chatStreamMessages(msgs, onToken, onError);
}

void OllamaHttpEngine::chatStreamMessages(const QList<EngineChatMessage>& messages,
                                          TokenCallback onToken,
                                          ErrorCallback onError) {
    cancel();

    m_responseFinished = false;
    m_tokenCallback = onToken;
    m_errorCallback = onError;
    m_buffer.clear();
    m_accumulatedText.clear();

    QPointer<OllamaHttpEngine> self(this);

    // 情况 1: 用户选择了本地 .gguf 模型，使用内置 llama.cpp 驱动
    if (!m_localGgufPath.isEmpty()) {
        QString startErr;
        if (!LlamaServerManager::instance().ensureRunning(m_localGgufPath, m_numCtx, startErr, m_gpuLayers, m_threads, m_localPort)) {
            m_responseFinished = true;
            auto errCb = m_errorCallback;
            if (errCb) errCb(startErr);
            return;
        }

        m_waitingForLocalServer = true;
        auto pollReady = std::make_shared<std::function<void(int)>>();
        *pollReady = [self, messages, pollReady](int attempt) {
            if (!self || !self->m_waitingForLocalServer) return;

            if (!LlamaServerManager::instance().isProcessAlive()) {
                QString fallbackErr;
                if (LlamaServerManager::instance().tryFallbackCpu(self->m_localGgufPath, self->m_numCtx, fallbackErr, self->m_threads, self->m_localPort)) {
                    QTimer::singleShot(500, self, [self, pollReady]() {
                        if (self && self->m_waitingForLocalServer) (*pollReady)(1);
                    });
                    return;
                }
                self->m_waitingForLocalServer = false;
                self->m_responseFinished = true;
                auto errCb = self->m_errorCallback;
                if (errCb) {
                    errCb("本地 llama.cpp 引擎加载 GGUF 模型失败，请确认模型文件完整性与显存/内存容量。");
                }
                return;
            }

            QUrl healthUrl(QString("http://127.0.0.1:%1/health").arg(self->m_localPort));
            QNetworkRequest req(healthUrl);
            QNetworkReply* healthReply = self->m_nam->get(req);
            self->m_healthReply = healthReply;
            connect(healthReply, &QNetworkReply::finished, self, [self, healthReply, messages, pollReady, attempt]() {
                if (!self) {
                    healthReply->deleteLater();
                    return;
                }
                if (self->m_healthReply == healthReply) {
                    self->m_healthReply = nullptr;
                }
                int statusCode = healthReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                healthReply->deleteLater();
                if (!self->m_waitingForLocalServer) return;

                if (statusCode == 200) {
                    self->m_waitingForLocalServer = false;
                    QString ggufName = QFileInfo(self->m_localGgufPath).fileName();
                    self->sendChatCompletionRequest(
                        LlamaServerManager::instance().localEndpoint(),
                        ggufName,
                        messages
                    );
                } else if (attempt < 200) { // 轮询等待延长至 100 秒，保证大模型权重有充足时间读入内存
                    QTimer::singleShot(500, self, [self, pollReady, attempt]() {
                        if (self && self->m_waitingForLocalServer) {
                            (*pollReady)(attempt + 1);
                        }
                    });
                } else {
                    self->m_waitingForLocalServer = false;
                    self->m_responseFinished = true;
                    auto errCb = self->m_errorCallback;
                    if (errCb) {
                        errCb("等待本地 llama.cpp 模型加载超时 (超过100秒)。");
                    }
                }
            });
        };

        (*pollReady)(0);
        return;
    }

    // 情况 2: 本地模型留空，使用模型服务接口 (支持本地 Ollama 或任意在线兼容 OpenAI 的大模型接口)
    sendChatCompletionRequest(m_endpoint, m_model, messages);
}

void OllamaHttpEngine::sendChatCompletionRequest(const QString& baseEndpoint,
                                                 const QString& modelName,
                                                 const QList<EngineChatMessage>& messages) {
    QUrl url(normalizeChatCompletionsUrl(baseEndpoint));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!m_apiKey.isEmpty() && m_localGgufPath.isEmpty()) {
        request.setRawHeader("Authorization", "Bearer " + m_apiKey.toUtf8());
    }

    QJsonObject root;
    root["model"] = modelName;
    root["stream"] = true;
    root["temperature"] = 0.7;

    m_lastResponseIncomplete = false;

    // 准确估算 Token 消耗：中文字符通常占 1.3 ~ 1.8 个 BPE Token，英文字符与空格占 ~0.3 ~ 0.5 个 Token
    auto estimateTokens = [](const QString& str) -> int {
        int chineseCount = 0;
        int otherCount = 0;
        for (const QChar& ch : str) {
            if (ch.unicode() >= 0x4E00 && ch.unicode() <= 0x9FFF) {
                chineseCount++;
            } else {
                otherCount++;
            }
        }
        return static_cast<int>(chineseCount * 1.5 + otherCount * 0.4) + 1;
    };

    // 如果历史对话过长，智能修剪中间消息，保留首条 System/背景 以及最新的追问，防止撑爆上下文引起 400 Bad Request
    QList<EngineChatMessage> prunedMessages = messages;
    int totalPromptTokens = 0;
    for (const auto& msg : prunedMessages) {
        totalPromptTokens += estimateTokens(msg.content);
    }

    int maxAllowedPromptTokens = qMax(2048, m_numCtx - 3072);
    while (totalPromptTokens > maxAllowedPromptTokens && prunedMessages.size() > 2) {
        // 保留第 0 条（通常为 system）与第 1 条（企业全量背景），从第 2 条（最早的中间问答）开始裁剪
        int removeIdx = (prunedMessages.size() > 3) ? 2 : 1;
        totalPromptTokens -= estimateTokens(prunedMessages[removeIdx].content);
        prunedMessages.removeAt(removeIdx);
    }

    int availableForOutput = m_numCtx - totalPromptTokens - 256;
    if (availableForOutput < 256) availableForOutput = 256;

    if (m_numPredict > 0) {
        if (!m_localGgufPath.isEmpty()) {
            // 本地 llama-server 严格校验 prompt_tokens + max_tokens <= n_ctx
            int safeMaxTokens = qBound(256, qMin(m_numPredict, availableForOutput), 4096);
            root["max_tokens"] = safeMaxTokens;
        } else {
            root["max_tokens"] = m_numPredict;
        }
    }

    // 严密注入采样参数，确保推演严谨与杜绝复读
    root["temperature"] = m_temperature;
    root["top_p"] = m_topP;
    if (m_repeatPenalty > 1.0) {
        root["frequency_penalty"] = m_repeatPenalty - 1.0;
        root["repeat_penalty"] = m_repeatPenalty;
    }

    // 规范化消息列表：严格遵循标准 OpenAI / Ollama 消息流格式
    // 1. 合并所有前置 system 消息为一条单一 system 消息（杜绝多条连续 system 导致 Ollama Jinja 模板崩溃）
    QString combinedSystemPrompt;
    QList<EngineChatMessage> convMessages;
    for (const auto& msg : prunedMessages) {
        if (msg.role == "system") {
            if (!combinedSystemPrompt.isEmpty()) combinedSystemPrompt += "\n\n";
            combinedSystemPrompt += msg.content.trimmed();
        } else {
            convMessages.append(msg);
        }
    }

    // 2. 确保对话主体必须以 "user" 消息开始；若第一条非 system 消息为 "assistant"，自动补充初始请求提示词
    if (!convMessages.isEmpty() && convMessages.first().role == "assistant") {
        convMessages.prepend(EngineChatMessage{"user", "请根据上下文与参谋定位，出具深度综合评价报告。"});
    }

    // 3. 严格保证对话轮次交替（user -> assistant -> user -> assistant），合并连续相同角色的消息
    QList<EngineChatMessage> sanitized;
    for (const auto& msg : convMessages) {
        QString cleanContent = msg.content.trimmed();
        if (cleanContent.isEmpty()) continue;
        if (!sanitized.isEmpty() && sanitized.last().role == msg.role) {
            sanitized.last().content += "\n\n" + cleanContent;
        } else {
            sanitized.append(EngineChatMessage{msg.role, cleanContent});
        }
    }

    // 4. 组装最终 messages 数组
    QJsonArray messagesArr;
    if (!combinedSystemPrompt.isEmpty()) {
        QJsonObject m;
        m["role"] = "system";
        m["content"] = combinedSystemPrompt;
        messagesArr.append(m);
    }
    for (const auto& msg : sanitized) {
        QJsonObject m;
        m["role"] = msg.role;
        m["content"] = msg.content;
        messagesArr.append(m);
    }
    root["messages"] = messagesArr;

    QByteArray postData = QJsonDocument(root).toJson(QJsonDocument::Compact);
    m_currentReply = m_nam->post(request, postData);

    connect(m_currentReply, &QNetworkReply::readyRead, this, &OllamaHttpEngine::onReadyRead);
    connect(m_currentReply, &QNetworkReply::finished, this, &OllamaHttpEngine::onFinished);
    connect(m_currentReply, &QNetworkReply::errorOccurred, this, &OllamaHttpEngine::onErrorOccurred);
}

void OllamaHttpEngine::onReadyRead() {
    if (!m_currentReply || m_responseFinished) return;

    QPointer<OllamaHttpEngine> self(this);
    m_buffer.append(m_currentReply->readAll());

    // 解析 SSE 行: data: { ... }\n\n
    while (self && !m_responseFinished) {
        int newlineIndex = m_buffer.indexOf('\n');
        if (newlineIndex < 0) break;

        QByteArray line = m_buffer.left(newlineIndex).trimmed();
        m_buffer.remove(0, newlineIndex + 1);

        if (line.isEmpty()) continue;

        if (line.startsWith("data:")) {
            QByteArray payload = line.mid(5).trimmed();
            if (payload == "[DONE]") {
                m_responseFinished = true;
                auto cb = m_tokenCallback;
                if (cb) {
                    cb("", true);
                }
                return;
            }

            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
            if (!doc.isNull() && doc.isObject()) {
                QJsonObject obj = doc.object();
                QJsonArray choices = obj["choices"].toArray();
                if (!choices.isEmpty()) {
                    QJsonObject choice0 = choices[0].toObject();
                    if (choice0.contains("finish_reason") && !choice0["finish_reason"].isNull()) {
                        QString fr = choice0["finish_reason"].toString().trimmed().toLower();
                        if (fr == "length") {
                            m_lastResponseIncomplete = true;
                        } else if (fr == "stop") {
                            m_lastResponseIncomplete = false;
                        }
                    }

                    QJsonObject delta = choice0["delta"].toObject();
                    QString token;
                    if (delta.contains("content")) {
                        token = delta["content"].toString();
                    } else if (delta.contains("reasoning_content")) {
                        // 兼容思考模型推理阶段
                        token = delta["reasoning_content"].toString();
                    }
                    if (!token.isEmpty()) {
                        m_accumulatedText.append(token);
                        auto cb = m_tokenCallback;
                        if (cb) {
                            cb(token, false);
                        }
                    }
                }
            }
        }
    }
}

void OllamaHttpEngine::onFinished() {
    QPointer<OllamaHttpEngine> self(this);
    QNetworkReply* reply = m_currentReply;
    m_currentReply = nullptr;
    if (!reply) return;

    bool noErr = (reply->error() == QNetworkReply::NoError);
    QByteArray remaining = reply->readAll();
    reply->disconnect(this);
    reply->deleteLater();

    if (!self) return;

    if (noErr) {
        // 容错：若服务端未走 SSE 流式而直接返回普通 JSON，进行整包兜底解析
        if (m_accumulatedText.isEmpty() && (!m_buffer.isEmpty() || !remaining.isEmpty())) {
            QByteArray fullBody = m_buffer + remaining;
            QJsonDocument doc = QJsonDocument::fromJson(fullBody.trimmed());
            if (doc.isObject()) {
                QJsonArray choices = doc.object()["choices"].toArray();
                if (!choices.isEmpty()) {
                    QJsonObject choice0 = choices[0].toObject();
                    if (choice0.contains("finish_reason") && !choice0["finish_reason"].isNull()) {
                        QString fr = choice0["finish_reason"].toString().trimmed().toLower();
                        if (fr == "length") {
                            m_lastResponseIncomplete = true;
                        }
                    }
                    QString content = choice0["message"].toObject()["content"].toString();
                    if (!content.isEmpty()) {
                        m_accumulatedText = content;
                        auto cb = m_tokenCallback;
                        if (cb) {
                            cb(content, false);
                        }
                    }
                }
            }
        }

        // 兜底检测截断：若最后字符未以常规标点闭合且生成篇幅较大
        if (!m_accumulatedText.isEmpty()) {
            QString trimmedAcc = m_accumulatedText.trimmed();
            if (!trimmedAcc.isEmpty()) {
                QChar lastCh = trimmedAcc.back();
                bool isNormalEnding = (lastCh == QChar(0x3002) || // 。
                                       lastCh == QChar(0xFF01) || // ！
                                       lastCh == QChar(0xFF1F) || // ？
                                       lastCh == '.' || lastCh == '!' || lastCh == '?' ||
                                       lastCh == QChar(0x201D) || // ”
                                       lastCh == '"' || lastCh == '`' ||
                                       lastCh == QChar(0xFF09) || // ）
                                       lastCh == ')' ||
                                       lastCh == QChar(0x3011) || // 】
                                       lastCh == '}');
                if (!isNormalEnding && trimmedAcc.length() >= 600 && m_lastResponseIncomplete) {
                    m_lastResponseIncomplete = true;
                }
            }
        }

        if (!m_responseFinished) {
            m_responseFinished = true;
            auto cb = m_tokenCallback;
            if (cb) {
                cb("", true);
            }
        }
    }
}

void OllamaHttpEngine::onErrorOccurred(QNetworkReply::NetworkError code) {
    if (code == QNetworkReply::OperationCanceledError || m_responseFinished) return;

    QPointer<OllamaHttpEngine> self(this);
    QString err = m_currentReply ? m_currentReply->errorString() : "Unknown Network Error";
    if (m_currentReply) {
        QByteArray replyBody = m_currentReply->readAll();
        if (!replyBody.isEmpty()) {
            QJsonDocument doc = QJsonDocument::fromJson(replyBody);
            if (doc.isObject() && doc.object().contains("error")) {
                QJsonObject errObj = doc.object()["error"].toObject();
                QString msg = errObj["message"].toString();
                if (!msg.isEmpty()) {
                    err = msg;
                }
            }
        }
    }
    qWarning() << "[LLM] Network error:" << err;
    m_responseFinished = true;
    auto errCb = m_errorCallback;
    if (errCb) {
        errCb(err);
    }
}
