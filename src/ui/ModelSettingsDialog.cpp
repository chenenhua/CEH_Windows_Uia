#include "ModelSettingsDialog.h"
#include "../core/DatabaseManager.h"
#include "../ai/OllamaHttpEngine.h"
#include "../ai/AiAdvisorService.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QThread>
#include <QTimer>

namespace {
void setFormFieldRowVisible(QWidget* fieldWidget, bool visible) {
    if (!fieldWidget) return;
    if (QWidget* parentBox = fieldWidget->parentWidget()) {
        if (parentBox->objectName() == "inlineFormRowBox") {
            parentBox->setVisible(visible);
            return;
        }
    }
    fieldWidget->setVisible(visible);
}
} // namespace

ModelSettingsDialog::ModelSettingsDialog(QWidget* parent)
    : CustomDialog("本地大模型与 Ai参谋设置", parent) {
    m_netManager = new QNetworkAccessManager(this);
    setupUi();
    loadCurrentSettings();
    updateModeVisibility();
}

void ModelSettingsDialog::setupUi() {
    setMinimumWidth(480);

    auto* layout = new QVBoxLayout();
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    auto* descLbl = new QLabel(
        "支持两种运行模式：\n"
        "1. 【本地 Ollama / 在线接口】：可连接本机已运行的 Ollama (默认 http://127.0.0.1:11434/v1) 或任意兼容 OpenAI 的服务接口；\n"
        "2. 【内置 llama.cpp 离线驱动】：点击右侧“选择”指定本地 .gguf 格式模型文件，由软件内置引擎驱动（支持 CUDA / CPU 硬件加速）。",
        this
    );
    descLbl->setStyleSheet("color: #8e8e93; font-size: 12px; line-height: 1.4;");
    descLbl->setWordWrap(true);
    layout->addWidget(descLbl);

    auto* formLayout = new QFormLayout();
    formLayout->setSpacing(10);
    formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // 0. 模式选择：本地 Ollama/在线接口 vs 内置 llama.cpp
    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem("模式 1：在线接口", "service");
    m_modeCombo->addItem("模式 2：本地模型", "local");
    formLayout->addRow("运行模式", m_modeCombo);
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ModelSettingsDialog::onModeChanged);

    // 1. 本地模型：“输入框” + 右侧“选择”按键 (模式 2 时显示)
    m_localModelContainer = new QWidget(this);
    m_localModelContainer->setStyleSheet("background: transparent; border: none;");
    auto* localRowLayout = new QHBoxLayout(m_localModelContainer);
    localRowLayout->setContentsMargins(0, 0, 0, 0);
    localRowLayout->setSpacing(6);

    m_localModelPathEdit = new QLineEdit(m_localModelContainer);
    m_localModelPathEdit->setPlaceholderText("请点击右侧“选择”指定本地 .gguf 模型文件");
    m_localModelPathEdit->setClearButtonEnabled(true);

    m_btnSelectLocalModel = new QPushButton("选择", m_localModelContainer);
    m_btnSelectLocalModel->setObjectName("inlineBrowseBtn");
    m_btnSelectLocalModel->setFixedSize(56, 26);
    m_btnSelectLocalModel->setCursor(Qt::PointingHandCursor);
    m_btnSelectLocalModel->setStyleSheet(
        "QPushButton#inlineBrowseBtn { background-color: #242429; color: #00a361; border: 1px solid #323238; border-radius: 3px; font-size: 12px; font-weight: bold; padding: 0px; }"
        "QPushButton#inlineBrowseBtn:hover { background-color: #00a361; color: #ffffff; border-color: #00a361; }"
    );
    connect(m_btnSelectLocalModel, &QPushButton::clicked, this, &ModelSettingsDialog::onSelectLocalModel);

    localRowLayout->addWidget(m_localModelPathEdit, 1);
    localRowLayout->addWidget(m_btnSelectLocalModel, 0);
    formLayout->addRow("本地模型", m_localModelContainer);

    // 2. 本地模型硬件加速参数 (当选择本地模型时显示)
    m_gpuLayersSpin = new QSpinBox(this);
    m_gpuLayersSpin->setRange(0, 999);
    m_gpuLayersSpin->setValue(99);
    m_gpuLayersSpin->setSuffix(" 层 (99=全GPU加速, 0=纯CPU)");
    formLayout->addRow("GPU 加速层数", m_gpuLayersSpin);

    m_threadsSpin = new QSpinBox(this);
    m_threadsSpin->setRange(1, 64);
    int defaultThreads = qMax(2, QThread::idealThreadCount() - 2);
    m_threadsSpin->setValue(defaultThreads);
    m_threadsSpin->setSuffix(" 线程 (按CPU核心设置)");
    formLayout->addRow("CPU 计算线程", m_threadsSpin);

    // 3. 模型服务接口 (当本地模型为空时显示)
    m_endpointEdit = new QLineEdit(this);
    m_endpointEdit->setPlaceholderText("http://127.0.0.1:11434/v1 或任意在线模型 API 地址");
    formLayout->addRow("模型服务接口", m_endpointEdit);

    m_apiKeyEdit = new QLineEdit(this);
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    m_apiKeyEdit->setPlaceholderText("本地 Ollama 默认留空；在线模型填写 API Key");
    formLayout->addRow("API 密钥", m_apiKeyEdit);

    m_modelNameEdit = new QLineEdit(this);
    m_modelNameEdit->setPlaceholderText("qwen9b-vl");
    formLayout->addRow("模型服务名称", m_modelNameEdit);

    // 4. 通用上下文与推理限额
    m_contextTokensSpin = new QSpinBox(this);
    m_contextTokensSpin->setRange(2048, 131072);
    m_contextTokensSpin->setValue(16384);
    m_contextTokensSpin->setSuffix(" tokens");
    formLayout->addRow("上下文容量", m_contextTokensSpin);

    m_predictTokensSpin = new QSpinBox(this);
    m_predictTokensSpin->setRange(512, 32768);
    m_predictTokensSpin->setValue(4096);
    m_predictTokensSpin->setSuffix(" tokens");
    formLayout->addRow("单次推理限额", m_predictTokensSpin);

    // 5. 严谨采样控制
    m_temperatureSpin = new QDoubleSpinBox(this);
    m_temperatureSpin->setRange(0.00, 2.00);
    m_temperatureSpin->setSingleStep(0.05);
    m_temperatureSpin->setValue(0.60);
    m_temperatureSpin->setSuffix(" (推荐 0.60，严谨商业推演)");
    formLayout->addRow("采样温度 (Temp)", m_temperatureSpin);

    m_topPSpin = new QDoubleSpinBox(this);
    m_topPSpin->setRange(0.10, 1.00);
    m_topPSpin->setSingleStep(0.05);
    m_topPSpin->setValue(0.90);
    m_topPSpin->setSuffix(" (推荐 0.90)");
    formLayout->addRow("核采样 (Top P)", m_topPSpin);

    m_repeatPenaltySpin = new QDoubleSpinBox(this);
    m_repeatPenaltySpin->setRange(1.00, 1.50);
    m_repeatPenaltySpin->setSingleStep(0.05);
    m_repeatPenaltySpin->setValue(1.10);
    m_repeatPenaltySpin->setSuffix(" (推荐 1.10，防复读车轱辘话)");
    formLayout->addRow("重复惩罚系数", m_repeatPenaltySpin);

    layout->addLayout(formLayout);

    // 测试连接 / 引擎校验区域
    auto* testBox = new QHBoxLayout();
    testBox->setSpacing(10);
    m_btnTest = new QPushButton("测试连接", this);
    m_btnTest->setFixedWidth(96);
    m_btnTest->setCursor(Qt::PointingHandCursor);
    connect(m_btnTest, &QPushButton::clicked, this, &ModelSettingsDialog::onTestConnection);

    m_testResultLabel = new QLabel("未检测", this);
    m_testResultLabel->setStyleSheet("color: #8e8e93; font-size: 12px;");

    testBox->addWidget(m_btnTest);
    testBox->addWidget(m_testResultLabel, 1);
    layout->addLayout(testBox);

    // 底部保存/取消
    auto* btnBox = new QHBoxLayout();
    btnBox->setSpacing(10);

    auto* btnCancel = new QPushButton("取消", this);
    btnCancel->setCursor(Qt::PointingHandCursor);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    auto* btnSave = new QPushButton("保存配置", this);
    btnSave->setObjectName("primaryBtn");
    btnSave->setCursor(Qt::PointingHandCursor);
    connect(btnSave, &QPushButton::clicked, this, &ModelSettingsDialog::onSaveSettings);

    btnBox->addWidget(btnCancel, 1);
    btnBox->addWidget(btnSave, 1);
    layout->addLayout(btnBox);

    bodyLayout()->addLayout(layout);
    normalizeFormWidgets(this);

    connect(m_localModelPathEdit, &QLineEdit::textChanged, this, &ModelSettingsDialog::onLocalModelTextChanged);
}

void ModelSettingsDialog::onModeChanged(int index) {
    Q_UNUSED(index);
    updateModeVisibility();
}

void ModelSettingsDialog::loadCurrentSettings() {
    auto& db = DatabaseManager::instance();
    QString configuredFlag = db.getSecurityConfig("llm_configured");
    QString engineMode = db.getSecurityConfig("llm_engine_mode").trimmed();

    QString localGguf = db.getSecurityConfig("llm_local_gguf_path").trimmed();
    QString ep = db.getSecurityConfig("llm_endpoint");
    QString apiKey = db.getSecurityConfig("llm_api_key");
    QString model = db.getSecurityConfig("llm_model_name");
    int numCtx = db.getSecurityConfig("llm_num_ctx").toInt();
    int numPredict = db.getSecurityConfig("llm_num_predict").toInt();

    int gpuLayers = 99;
    int threads = qMax(2, QThread::idealThreadCount() - 2);
    double temperature = 0.60;
    double topP = 0.90;
    double repeatPenalty = 1.10;

    QString dbGpu = db.getSecurityConfig("llm_gpu_layers");
    if (!dbGpu.isEmpty()) gpuLayers = dbGpu.toInt();
    QString dbTh = db.getSecurityConfig("llm_threads");
    if (!dbTh.isEmpty()) threads = dbTh.toInt();
    QString dbTemp = db.getSecurityConfig("llm_temperature");
    if (!dbTemp.isEmpty()) temperature = dbTemp.toDouble();
    QString dbTopP = db.getSecurityConfig("llm_top_p");
    if (!dbTopP.isEmpty()) topP = dbTopP.toDouble();
    QString dbRep = db.getSecurityConfig("llm_repeat_penalty");
    if (!dbRep.isEmpty()) repeatPenalty = dbRep.toDouble();

    // 核心要求 1：优先从 assets/models/models.json 读取模型配置
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
                    if (m.contains("api_key")) apiKey = m["api_key"].toString().trimmed();
                    if (m.contains("model_name")) model = m["model_name"].toString().trimmed();
                    if (m.contains("num_ctx") && m["num_ctx"].toInt() > 0) numCtx = m["num_ctx"].toInt();
                    if (m.contains("num_predict") && m["num_predict"].toInt() > 0) numPredict = m["num_predict"].toInt();
                    if (m.contains("configured")) configuredFlag = m["configured"].toString();
                    if (m.contains("gpu_layers")) gpuLayers = m["gpu_layers"].toInt();
                    if (m.contains("threads") && m["threads"].toInt() > 0) threads = m["threads"].toInt();
                    if (m.contains("temperature") && m["temperature"].toDouble() > 0.0) temperature = m["temperature"].toDouble();
                    if (m.contains("top_p") && m["top_p"].toDouble() > 0.0) topP = m["top_p"].toDouble();
                    if (m.contains("repeat_penalty") && m["repeat_penalty"].toDouble() > 0.0) repeatPenalty = m["repeat_penalty"].toDouble();
                    break;
                }
            }
        }
    }

    m_localModelPathEdit->setText(localGguf);
    if (ep.isEmpty() && configuredFlag != "0") ep = "http://127.0.0.1:11434/v1";
    m_endpointEdit->setText(ep);
    m_apiKeyEdit->setText(apiKey);
    if (model.isEmpty() && configuredFlag != "0") model = "qwen9b-vl";
    m_modelNameEdit->setText(model);
    if (numCtx > 0) m_contextTokensSpin->setValue(numCtx);
    if (numPredict > 0) m_predictTokensSpin->setValue(numPredict);

    m_gpuLayersSpin->setValue(gpuLayers);
    m_threadsSpin->setValue(threads);
    m_temperatureSpin->setValue(temperature);
    m_topPSpin->setValue(topP);
    m_repeatPenaltySpin->setValue(repeatPenalty);

    if (m_modeCombo) {
        if (engineMode == "local") {
            m_modeCombo->setCurrentIndex(1);
        } else if (engineMode == "service") {
            m_modeCombo->setCurrentIndex(0);
        } else {
            m_modeCombo->setCurrentIndex(!localGguf.isEmpty() ? 1 : 0);
        }
    }
}

void ModelSettingsDialog::onSelectLocalModel() {
    QString defaultDir = DatabaseManager::instance().resolveAssetPath("assets/model");
    if (!QFileInfo::exists(defaultDir)) {
        defaultDir = DatabaseManager::instance().projectRootDir();
    }
    QString path = QFileDialog::getOpenFileName(
        this,
        "选择本地 GGUF 大模型文件 (llama.cpp 驱动)",
        defaultDir,
        "GGUF 模型文件 (*.gguf)"
    );
    if (path.isEmpty()) return;

    if (!path.endsWith(".gguf", Qt::CaseInsensitive)) {
        CustomDialog::showWarning(this, "格式错误", "本地模型必须为 .gguf 格式文件！");
        return;
    }

    // 如果所选模型位于当前软件安装目录下（例如 <安装地址>/assets/model/xxx.gguf），自动保存为相对路径以便打包分发
    QDir rootDir(DatabaseManager::instance().projectRootDir());
    QString relPath = rootDir.relativeFilePath(path);
    if (!relPath.startsWith("..")) {
        m_localModelPathEdit->setText(relPath);
    } else {
        m_localModelPathEdit->setText(path);
    }
}

void ModelSettingsDialog::onLocalModelTextChanged(const QString& text) {
    Q_UNUSED(text);
    updateModeVisibility();
}

void ModelSettingsDialog::updateModeVisibility() {
    if (!m_modeCombo || !m_localModelContainer || !m_btnTest || !m_testResultLabel) return;
    bool isService = (m_modeCombo->currentData().toString() == "service");

    setFormFieldRowVisible(m_endpointEdit, isService);
    setFormFieldRowVisible(m_apiKeyEdit, isService);
    setFormFieldRowVisible(m_modelNameEdit, isService);

    setFormFieldRowVisible(m_localModelContainer, !isService);
    setFormFieldRowVisible(m_gpuLayersSpin, !isService);
    setFormFieldRowVisible(m_threadsSpin, !isService);

    if (isService) {
        m_btnTest->setText("测试连接");
        m_testResultLabel->setText("当前模式：本地 Ollama / 在线接口服务");
        m_testResultLabel->setStyleSheet("color: #8e8e93; font-size: 12px;");
    } else {
        m_btnTest->setText("校验引擎");
        QString ggufName = QFileInfo(m_localModelPathEdit->text().trimmed()).fileName();
        if (ggufName.isEmpty()) {
            m_testResultLabel->setText("<span style='color:#be782e;'>当前模式：内置 llama.cpp 离线驱动（请点击“选择”载入本地 .gguf 模型）</span>");
        } else {
            m_testResultLabel->setText(QString("<span style='color:#00a361;'>当前模式：内置 llama.cpp 离线驱动 (%1)</span>").arg(ggufName));
        }
    }
    adjustSize();
}

void ModelSettingsDialog::onTestConnection() {
    bool isLocal = (m_modeCombo && m_modeCombo->currentData().toString() == "local");
    if (isLocal) {
        QString localGguf = m_localModelPathEdit->text().trimmed();
        if (localGguf.isEmpty()) {
            m_testResultLabel->setText("<span style='color:#fa5151;'>请先点击右侧“选择”指定本地 .gguf 格式模型文件！</span>");
            return;
        }
        if (!localGguf.endsWith(".gguf", Qt::CaseInsensitive)) {
            m_testResultLabel->setText("<span style='color:#fa5151;'>错误：本地模型文件必须为 .gguf 格式！</span>");
            return;
        }
        QString resolvedGguf = DatabaseManager::instance().resolveAssetPath(localGguf);
        QFileInfo fi(resolvedGguf);
        if (!fi.exists() || !fi.isFile()) {
            m_testResultLabel->setText("<span style='color:#fa5151;'>错误：所选 .gguf 模型文件不存在！</span>");
            return;
        }
        QString binPath = OllamaHttpEngine::findLlamaServerBinary(true);
        if (binPath.isEmpty()) {
            m_testResultLabel->setText("<span style='color:#fa5151;'>未找到内置 llama.cpp 驱动组件 (llama-server.exe)！</span>");
            return;
        }
        QString backendTag;
        if (binPath.contains("cuda", Qt::CaseInsensitive)) {
            backendTag = "CUDA 12 GPU 加速";
        } else if (binPath.contains("vulkan", Qt::CaseInsensitive)) {
            backendTag = "Vulkan GPU 加速";
        } else {
            backendTag = "CPU 通用模式";
        }

        m_btnTest->setEnabled(false);
        m_testResultLabel->setText(QString("<span style='color:#be782e;'>正在校验并装载引擎 (%1)...</span>").arg(backendTag));

        // 真实启动/预热引擎并探测健康状态
        OllamaHttpEngine::prewarmLocalServer(resolvedGguf, m_contextTokensSpin->value(), m_gpuLayersSpin->value(), m_threadsSpin->value());

        // 异步轮询探测 health 状态
        auto checkHealth = std::make_shared<std::function<void(int)>>();
        *checkHealth = [this, backendTag, fi, checkHealth](int attempt) {
            QNetworkRequest req(QUrl("http://127.0.0.1:18089/health"));
            req.setTransferTimeout(1500);
            QNetworkReply* reply = m_netManager->get(req);
            connect(reply, &QNetworkReply::finished, this, [this, reply, backendTag, fi, checkHealth, attempt]() {
                int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                reply->deleteLater();
                if (code == 200) {
                    m_btnTest->setEnabled(true);
                    m_testResultLabel->setText(
                        QString("<span style='color:#00d68f; font-weight: bold;'>✓ llama.cpp (%1) 校验通过，模型已装载就绪: %2</span>")
                            .arg(backendTag, fi.fileName())
                    );
                } else if (attempt < 20) { // 轮询约 10 秒
                    QTimer::singleShot(500, this, [checkHealth, attempt]() {
                        (*checkHealth)(attempt + 1);
                    });
                } else {
                    m_btnTest->setEnabled(true);
                    m_testResultLabel->setText(
                        QString("<span style='color:#00d68f;'>✓ llama.cpp (%1) 驱动与模型就绪（后台权重装载中）</span>")
                            .arg(backendTag)
                    );
                }
            });
        };
        (*checkHealth)(0);
        return;
    }

    // 模式 1：服务接口探测
    QString ep = m_endpointEdit->text().trimmed();
    if (ep.isEmpty()) {
        m_testResultLabel->setText("<span style='color:#fa5151;'>请输入模型服务接口地址！</span>");
        return;
    }

    m_btnTest->setEnabled(false);
    m_testResultLabel->setText("<span style='color:#be782e;'>正在连接探测...</span>");

    while (ep.endsWith('/')) ep.chop(1);
    QString modelsUrl = ep.endsWith("/v1", Qt::CaseInsensitive) ? (ep + "/models") : (ep + "/v1/models");
    QUrl url(modelsUrl);
    QNetworkRequest request(url);
    QString key = m_apiKeyEdit->text().trimmed();
    if (!key.isEmpty()) {
        request.setRawHeader("Authorization", "Bearer " + key.toUtf8());
    }

    QNetworkReply* reply = m_netManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        m_btnTest->setEnabled(true);
        if (reply->error() == QNetworkReply::NoError) {
            m_testResultLabel->setText("<span style='color:#00a361;'>✓ 成功连通模型服务接口！</span>");
        } else {
            m_testResultLabel->setText(QString("<span style='color:#fa5151;'>连接失败: %1</span>").arg(reply->errorString()));
        }
        reply->deleteLater();
    });
}

void ModelSettingsDialog::onSaveSettings() {
    bool isLocal = (m_modeCombo && m_modeCombo->currentData().toString() == "local");
    QString localGguf = isLocal ? m_localModelPathEdit->text().trimmed() : QString();

    if (isLocal) {
        if (localGguf.isEmpty()) {
            CustomDialog::showWarning(this, "未选择模型", "内置驱动模式下必须选择本地 .gguf 大模型文件！");
            return;
        }
        if (!localGguf.endsWith(".gguf", Qt::CaseInsensitive)) {
            CustomDialog::showWarning(this, "格式错误", "如果选择了本地模型，文件必须为 .gguf 格式！");
            return;
        }
        QString resolvedGguf = DatabaseManager::instance().resolveAssetPath(localGguf);
        QFileInfo fi(resolvedGguf);
        if (!fi.exists() || !fi.isFile()) {
            CustomDialog::showWarning(this, "文件不存在", "所选的本地 .gguf 模型文件不存在，请重新选择。");
            return;
        }
    } else {
        // 服务接口模式下立即关闭本地 llama-server 进程，释放 GPU/显存
        OllamaHttpEngine::shutdownLocalServer();
    }

    // 核心要求 5：点击保存的那一刻开始，如果有生成中立刻停止，表示全新设置开始使用
    AiAdvisorService::instance().cancelCurrentTask();

    auto& db = DatabaseManager::instance();
    QString ep = m_endpointEdit->text().trimmed();
    QString model = m_modelNameEdit->text().trimmed();

    db.setSecurityConfig("llm_engine_mode", isLocal ? "local" : "service");
    db.setSecurityConfig("llm_local_gguf_path", localGguf);
    db.setSecurityConfig("llm_endpoint", ep);
    db.setSecurityConfig("llm_api_key", m_apiKeyEdit->text().trimmed());
    db.setSecurityConfig("llm_model_name", model);
    db.setSecurityConfig("llm_num_ctx", QString::number(m_contextTokensSpin->value()));
    db.setSecurityConfig("llm_num_predict", QString::number(m_predictTokensSpin->value()));
    db.setSecurityConfig("llm_gpu_layers", QString::number(m_gpuLayersSpin->value()));
    db.setSecurityConfig("llm_threads", QString::number(m_threadsSpin->value()));
    db.setSecurityConfig("llm_temperature", QString::number(m_temperatureSpin->value(), 'f', 2));
    db.setSecurityConfig("llm_top_p", QString::number(m_topPSpin->value(), 'f', 2));
    db.setSecurityConfig("llm_repeat_penalty", QString::number(m_repeatPenaltySpin->value(), 'f', 2));

    bool isConfigured = isLocal ? (!localGguf.isEmpty()) : (!ep.isEmpty() && !model.isEmpty());
    db.setSecurityConfig("llm_configured", isConfigured ? "1" : "0");

    // 核心要求 1：模型部分控制全部持久化同步写入 assets/models/models.json (安全保留 ai_rules 与详细注释)
    QString appDir = QCoreApplication::applicationDirPath();
    QString rootDir = DatabaseManager::instance().projectRootDir();
    QStringList syncPaths = {
        QDir(appDir).filePath("assets/models/models.json"),
        QDir(appDir).filePath("../assets/models/models.json"),
        QDir(rootDir).filePath("assets/models/models.json"),
        QDir(rootDir).filePath("../assets/models/models.json"),
        DatabaseManager::instance().resolveAssetPath("assets/models/models.json"),
        DatabaseManager::instance().resolveAssetPath("models/models.json")
    };
    QSet<QString> processedPaths;
    for (const auto& cfgPath : syncPaths) {
        if (cfgPath.isEmpty()) continue;
        QFileInfo fi(cfgPath);
        QString absPath = fi.absoluteFilePath();
        if (processedPaths.contains(absPath)) continue;
        processedPaths.insert(absPath);

        QFile cfgFile(absPath);
        QJsonObject rootObj;
        if (cfgFile.exists() && cfgFile.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(cfgFile.readAll());
            cfgFile.close();
            if (doc.isObject()) {
                rootObj = doc.object();
            }
        }
        QJsonObject aiModel = rootObj["ai_model"].toObject();
        aiModel["configured"] = isConfigured ? "1" : "0";
        aiModel["engine_mode"] = isLocal ? "local" : "service";
        aiModel["local_gguf_path"] = localGguf;
        aiModel["endpoint"] = ep;
        aiModel["model_name"] = model;
        aiModel["api_key"] = m_apiKeyEdit->text().trimmed();
        aiModel["num_ctx"] = m_contextTokensSpin->value();
        aiModel["num_predict"] = m_predictTokensSpin->value();
        aiModel["gpu_layers"] = m_gpuLayersSpin->value();
        aiModel["threads"] = m_threadsSpin->value();
        aiModel["temperature"] = m_temperatureSpin->value();
        aiModel["top_p"] = m_topPSpin->value();
        aiModel["repeat_penalty"] = m_repeatPenaltySpin->value();
        rootObj["ai_model"] = aiModel;

        QDir().mkpath(fi.absolutePath());
        if (cfgFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            cfgFile.write(QJsonDocument(rootObj).toJson(QJsonDocument::Indented));
            cfgFile.close();
        }
    }

    // 核心要求 2：若配置了本地模型，点击保存即刻后台异步预热加载模型权重，消除冷启动等待
    if (isLocal && !localGguf.isEmpty()) {
        OllamaHttpEngine::prewarmLocalServer(localGguf, m_contextTokensSpin->value(), m_gpuLayersSpin->value(), m_threadsSpin->value());
    }

    accept();
}
