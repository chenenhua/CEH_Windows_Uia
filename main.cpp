#include <QApplication>
#include <QWidget>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QLineEdit>
#include <QTextBrowser>
#include <QTextDocument>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QPainterPath>
#include <QKeyEvent>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QIntValidator>
#include <QDoubleValidator>
#include <QListWidget>
#include <QFrame>
#include <QMap>
#include <QSet>
#include <QVariant>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTabBar>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSystemTrayIcon>
#include <QStyle>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDate>
#include <QDateTime>
#include <QTimer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QEventLoop>
#include <QUrl>
#include <QPixmap>
#include <QImage>
#include <QPainter>
#include <QLinearGradient>
#include <QShowEvent>
#include <QPen>
#include <QCursor>
#include <QCloseEvent>
#include <QMouseEvent>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QThread>
#include <QRegularExpression>
#include <QDesktopServices>
#include <QElapsedTimer>
#include <QSizePolicy>
#include <QMutex>
#include <QMutexLocker>
#include <QSemaphore>
#include <QPointer>
#include <QDirIterator>
#include <QSettings>
#include <QStandardPaths>
#include <QMetaObject>
#include <QSaveFile>
#include <QtEndian>
#include <QCryptographicHash>
#include <io.h>
#include <QBuffer>
#include <QImageReader>
#include <QUuid>
#include <thread>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <windows.h>
#include <psapi.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <UIAutomation.h>
#include <atomic>
#include <functional>
#include <cmath>
#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <iterator>

#ifndef CEH_APP_NAME
#define CEH_APP_NAME "木头自动系统控制"
#endif
#ifndef CEH_APP_VERSION
#define CEH_APP_VERSION "1.0.0"
#endif
#ifndef CEH_PROJECT_ROOT
#define CEH_PROJECT_ROOT "."
#endif

static QString g_lightFontFamily = "Microsoft YaHei UI";
static QString g_boldFontFamily = "Microsoft YaHei UI";

static QString nowTime() { return QDateTime::currentDateTime().toString("HH:mm:ss.zzz"); }
static QString nowFileTime() { return QDateTime::currentDateTime().toString("yyyy_MM_dd_HH_mm_ss_zzz"); }
static QString appDir() { return QCoreApplication::applicationDirPath(); }
static QString sourceRoot() { return QString::fromUtf8(CEH_PROJECT_ROOT); }

static QString resourcePath(const QString& relative) {
    const QString p1 = QDir(appDir()).filePath(relative);
    if (QFileInfo::exists(p1)) return QDir::cleanPath(p1);
    return QDir::cleanPath(QDir(sourceRoot()).filePath(relative));
}
static QString writablePath(const QString& relative) { return QDir::cleanPath(QDir(appDir()).filePath(relative)); }
static bool ensureDir(const QString& path) { QDir d; return d.exists(path) || d.mkpath(path); }

static QString readText(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return QString::fromUtf8(f.readAll());
}
static bool writeText(const QString& path, const QString& text) {
    ensureDir(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return false;
    f.write(text.toUtf8());
    return true;
}
static QJsonObject readJson(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QJsonParseError e;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &e);
    if (e.error != QJsonParseError::NoError || !doc.isObject()) return {};
    return doc.object();
}
static bool writeJson(const QString& path, const QJsonObject& obj) {
    ensureDir(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    return true;
}
static QJsonObject loadConfig(const QString& relative) {
    QJsonObject obj = readJson(writablePath(relative));
    if (!obj.isEmpty()) return obj;
    return readJson(resourcePath(relative));
}

// 文件日志与模型上下文互相独立：完整记录落盘，不把运行日志全文发给模型。
class Logger {
public:
    static QString root() {
        static const QString path = [] {
            const QString project = QDir(sourceRoot()).absolutePath();
            QStringList candidates;
            if (QDir(project + "/assets").exists()) candidates << project + "/assets/uppic/log";
            candidates << writablePath("assets/uppic/log")
                       << QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/log";
            for (const QString& dir : candidates) {
                if (!ensureDir(dir)) continue;
                QSaveFile probe(QDir(dir).filePath(".write_probe_" + QString::number(GetCurrentProcessId())));
                if (probe.open(QIODevice::WriteOnly)) { probe.cancelWriting(); return QDir::cleanPath(dir); }
            }
            return QDir::cleanPath(candidates.last());
        }();
        return path;
    }
    static QString filePath() {
        static const QString path = [] {
            const QString dir = QDir(root()).filePath(QDate::currentDate().toString("yyyy/MM/dd"));
            ensureDir(dir);
            return QDir(dir).filePath(nowFileTime() + "_" + QString::number(GetCurrentProcessId()) + ".txt");
        }();
        return path;
    }
    static QString previousPath() { return previousStorage(); }
    static void beginRun() {
        const QString manifest = QDir(root()).filePath("last_run.json");
        const QJsonObject old = readJson(manifest);
        previousStorage() = old.value("log_file").toString();
        writeRunState(false);
        write("SYSTEM", "日志文件：" + filePath());
        if (!previousStorage().isEmpty()) {
            write(old.value("clean_exit").toBool(false) ? "SYSTEM" : "WARNING",
                  QString("上次运行%1，日志：%2").arg(old.value("clean_exit").toBool(false) ? "正常退出" : "未记录正常退出（崩溃、强退或断电均有可能）", previousStorage()));
        }
    }
    static void endRun() { writeRunState(true); }
    static bool atomicJson(const QString& path, const QJsonObject& obj) {
        if (!ensureDir(QFileInfo(path).absolutePath())) return false;
        QSaveFile f(path);
        if (!f.open(QIODevice::WriteOnly)) return false;
        const QByteArray bytes = QJsonDocument(obj).toJson(QJsonDocument::Indented);
        if (f.write(bytes) != bytes.size() || !f.flush()) { f.cancelWriting(); return false; }
        const intptr_t native = f.handle() == -1 ? -1 : _get_osfhandle(f.handle());
        if (native != -1) FlushFileBuffers(reinterpret_cast<HANDLE>(native));
        return f.commit();
    }
    static void write(const QString& level, const QString& msg) {
        QMutexLocker locker(&mutex());
        const QByteArray bytes = QString("[%1] [%2] %3\n").arg(nowTime(), level, msg).toUtf8();
        QFile f(filePath());
        if (!f.open(QIODevice::WriteOnly | QIODevice::Append)) { OutputDebugStringW(L"CEH: 日志文件写入失败\n"); return; }
        if (f.write(bytes) != bytes.size() || !f.flush()) OutputDebugStringW(L"CEH: 日志写入不完整\n");
        if (level == "ERROR" || level == "WATCHDOG" || level == "EXIT") {
            if (f.handle() != -1) FlushFileBuffers(reinterpret_cast<HANDLE>(_get_osfhandle(f.handle())));
        }
#ifdef QT_DEBUG
        fwrite(bytes.constData(), 1, size_t(bytes.size()), stdout);
        fflush(stdout);
#endif
    }
private:
    static QMutex& mutex() { static QMutex m; return m; }
    static QString& previousStorage() { static QString p; return p; }
    static void writeRunState(bool clean) {
        atomicJson(QDir(root()).filePath("last_run.json"), QJsonObject{
            {"log_file", filePath()}, {"clean_exit", clean},
            {"pid", int(GetCurrentProcessId())}, {"updated_at", QDateTime::currentDateTime().toString(Qt::ISODateWithMs)}});
    }
};

// 看门狗在独立线程采样，不依赖 UI/Model Worker 的事件循环。
// GPU 利用率满载不等于故障；只对持续的内存余量不足熔断。
class RuntimeSafety {
public:
    struct Snapshot {
        quint64 ramFree = 0, ramTotal = 0, commitFree = 0, commitTotal = 0, gpuFree = 0, gpuTotal = 0;
        qint64 sampledAt = 0;
        int gpuIndex = -1;
        QString gpuName, gpuUuid;
        double cpuPercent = -1;
        unsigned gpuPercent = 0, temperature = 0;
        bool ramKnown = false, gpuKnown = false;
    };
    std::atomic_int modelOperations{0};
    static RuntimeSafety& instance() { static RuntimeSafety v; return v; }
    static QJsonObject config() { return loadConfig("assets/config/agent.json").value("resource_safety").toObject(); }
    static int contextLimit(const QString& role) {
        QMutexLocker lock(&instance().planMutex);
        const int actual = instance().effectiveContexts.value(role, 0);
        return actual > 0 ? actual : qBound(2048, config().value(role == "vlm" ? "vlm_context_limit" : "llm_context_limit").toInt(role == "vlm" ? 4096 : 8192), 131072);
    }
    static double fraction(const QJsonObject& c, const QString& key, double fallback) { return qBound(0.10, c.value(key).toDouble(fallback) / 100.0, 0.95); }
    void setEffectiveContext(const QString& role, int context) { QMutexLocker lock(&planMutex); effectiveContexts[role] = context; }
    void selectGpu(int index) { activeGpu = index; }
    Snapshot planningSnapshot() const {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        QElapsedTimer timer; timer.start();
        Snapshot v;
        do { v = snapshot(); if (v.sampledAt >= now || timer.elapsed() >= 1200) break; QThread::msleep(20); } while (true);
        MEMORYSTATUSEX ms{}; ms.dwLength = sizeof(ms);
        if (GlobalMemoryStatusEx(&ms)) { v.ramKnown = true; v.ramTotal = ms.ullTotalPhys; v.ramFree = ms.ullAvailPhys; }
        PERFORMANCE_INFORMATION pi{}; pi.cb = sizeof(pi);
        if (GetPerformanceInfo(&pi, sizeof(pi))) { v.commitTotal = quint64(pi.CommitLimit) * pi.PageSize; v.commitFree = quint64(pi.CommitLimit > pi.CommitTotal ? pi.CommitLimit-pi.CommitTotal : 0) * pi.PageSize; }
        return v;
    }
    static int outputLimit(const QString& role) { return qBound(128, config().value(role == "vlm" ? "vlm_max_tokens" : "llm_max_tokens").toInt(1024), qMin(2048, contextLimit(role)/4)); }
    static int requestTimeout() { return qBound(5000, config().value("request_timeout_ms").toInt(120000), 180000); }
    void start() {
        if (worker.joinable()) return;
        const QJsonObject c = config();
        minRam = quint64(qBound(256, c.value("min_available_ram_mb").toInt(768), 4096)) << 20;
        minCommit = quint64(qBound(256, c.value("min_commit_headroom_mb").toInt(1024), 4096)) << 20;
        minGpu = quint64(qBound(64, c.value("min_free_vram_mb").toInt(256), 2048)) << 20;
        worker = std::thread([this] { monitor(); });
    }
    void stop() {
        quitting = true;
        wake.notify_all();
        if (worker.joinable()) worker.join();
    }
    ~RuntimeSafety() { stop(); }
    bool beginTask(QString* error = nullptr) {
        if (pressure.load()) { if (error) *error = "资源保护中：请关闭占用资源的程序，等待内存/显存恢复后重试。"; return false; }
        cancelled = false;
        return true;
    }
    void cancel(const QString& reason = "用户停止或程序退出") {
        { QMutexLocker lock(&reasonMutex); cancelReason = reason; }
        cancelled = true;
        epoch.fetch_add(1);
    }
    QString abortDescription(quint64 captured) const {
        if (pressure.load()) return "资源保护已触发：" + description();
        if (cancelled.load() || captured != epoch.load()) {
            QMutexLocker lock(&reasonMutex);
            return "请求已取消：" + cancelReason;
        }
        return {};
    }
    bool shouldAbort(quint64 captured) const { return cancelled || pressure || captured != epoch.load(); }
    bool blocked() const { return pressure.load(); }
    quint64 generation() const { return epoch.load(); }
    Snapshot snapshot() const { QMutexLocker lock(&sampleMutex); return latest; }
    QString description() const {
        const Snapshot v = snapshot();
        return QString("RAM余量=%1MB 提交余量=%2MB CPU利用率=%3% GPU=%4 温度=%5")
            .arg(v.ramFree >> 20).arg(v.commitFree >> 20).arg(v.cpuPercent, 0, 'f', 0)
            .arg(v.gpuKnown ? QString("%1/%2MB余量 使用率%3%").arg(v.gpuFree >> 20).arg(v.gpuTotal >> 20).arg(v.gpuPercent) : "未知(NVML不可用)")
            .arg(v.gpuKnown ? QString::number(v.temperature) : "未知");
    }
    void trackModel(qint64 pid) {
        HANDLE h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, DWORD(pid));
        if (h) { QMutexLocker lock(&processMutex); owned.insert(pid, h); }
    }
    void untrackModel(qint64 pid) {
        QMutexLocker lock(&processMutex);
        const auto it = owned.find(pid);
        if (it != owned.end()) { CloseHandle(it.value()); owned.erase(it); }
    }
private:
    mutable QMutex sampleMutex, processMutex, reasonMutex, planMutex;
    QMap<QString,int> effectiveContexts;
    std::atomic_int activeGpu{-1};
    QString cancelReason = "用户停止或程序退出";
    Snapshot latest;
    QMap<qint64,HANDLE> owned;
    std::atomic_bool quitting{false}, cancelled{false}, pressure{false};
    std::atomic<quint64> epoch{0};
    std::thread worker;
    std::mutex waitMutex;
    std::condition_variable wake;
    quint64 minRam = 0, minCommit = 0, minGpu = 0;
    void releaseModelsInEmergency() {
        QMutexLocker lock(&processMutex);
        for (auto it = owned.cbegin(); it != owned.cend(); ++it) {
            if (WaitForSingleObject(it.value(), 0) == WAIT_TIMEOUT) {
                Logger::write("WATCHDOG", QString("持续资源不足，结束本程序模型服务 PID=%1").arg(it.key()));
                TerminateProcess(it.value(), 90);
            }
        }
    }
    void monitor() {
        typedef int (*InitFn)(); typedef int (*CountFn)(unsigned*);
        typedef int (*HandleFn)(unsigned, void**);
        struct Memory { unsigned long long total, free, used; };
        struct Util { unsigned gpu, memory; };
        typedef int (*MemoryFn)(void*, Memory*); typedef int (*UtilFn)(void*, Util*);
        typedef int (*TempFn)(void*, unsigned, unsigned*);
        typedef int (*NameFn)(void*, char*, unsigned);
        wchar_t systemPath[MAX_PATH]{}; GetSystemDirectoryW(systemPath, MAX_PATH);
        HMODULE nvml = LoadLibraryW(reinterpret_cast<LPCWSTR>((QString::fromWCharArray(systemPath) + "/nvml.dll").utf16()));
        if (!nvml) {
            const QString programFiles = qEnvironmentVariable("ProgramW6432", "C:/Program Files");
            nvml = LoadLibraryW(reinterpret_cast<LPCWSTR>((programFiles + "/NVIDIA Corporation/NVSMI/nvml.dll").utf16()));
        }
        auto init = nvml ? reinterpret_cast<InitFn>(GetProcAddress(nvml, "nvmlInit_v2")) : nullptr;
        auto shutdown = nvml ? reinterpret_cast<InitFn>(GetProcAddress(nvml, "nvmlShutdown")) : nullptr;
        auto count = nvml ? reinterpret_cast<CountFn>(GetProcAddress(nvml, "nvmlDeviceGetCount_v2")) : nullptr;
        auto handle = nvml ? reinterpret_cast<HandleFn>(GetProcAddress(nvml, "nvmlDeviceGetHandleByIndex_v2")) : nullptr;
        auto memory = nvml ? reinterpret_cast<MemoryFn>(GetProcAddress(nvml, "nvmlDeviceGetMemoryInfo")) : nullptr;
        auto util = nvml ? reinterpret_cast<UtilFn>(GetProcAddress(nvml, "nvmlDeviceGetUtilizationRates")) : nullptr;
        auto temp = nvml ? reinterpret_cast<TempFn>(GetProcAddress(nvml, "nvmlDeviceGetTemperature")) : nullptr;
        auto name = nvml ? reinterpret_cast<NameFn>(GetProcAddress(nvml, "nvmlDeviceGetName")) : nullptr;
        auto uuid = nvml ? reinterpret_cast<NameFn>(GetProcAddress(nvml, "nvmlDeviceGetUUID")) : nullptr;
        const bool nvReady = init && init() == 0;
        unsigned devices = 0; if (nvReady && count) count(&devices);
        quint64 oldIdle = 0, oldTotal = 0;
        int lowSamples = 0, goodSamples = 0, ticks = 0, tripTicks = 0;
        while (!quitting) {
            Snapshot v;
            MEMORYSTATUSEX ms{}; ms.dwLength = sizeof(ms);
            if (GlobalMemoryStatusEx(&ms)) {
                v.ramKnown = true; v.ramTotal = ms.ullTotalPhys; v.ramFree = ms.ullAvailPhys;
                PERFORMANCE_INFORMATION pi{}; pi.cb = sizeof(pi);
                if (GetPerformanceInfo(&pi, sizeof(pi))) { v.commitTotal = quint64(pi.CommitLimit) * pi.PageSize; v.commitFree = quint64(pi.CommitLimit > pi.CommitTotal ? pi.CommitLimit - pi.CommitTotal : 0) * pi.PageSize; }
                else v.commitFree = ms.ullAvailPageFile;
            }
            FILETIME idle{}, kernel{}, user{};
            auto number = [](FILETIME f) { return (quint64(f.dwHighDateTime) << 32) | f.dwLowDateTime; };
            if (GetSystemTimes(&idle, &kernel, &user)) {
                const quint64 total = number(kernel) + number(user), idleNow = number(idle);
                if (oldTotal && total > oldTotal) v.cpuPercent = qBound(0.0, 100.0 * (1.0 - double(idleNow - oldIdle) / double(total - oldTotal)), 100.0);
                oldTotal = total; oldIdle = idleNow;
            }
            for (unsigned i = 0; i < devices && handle && memory; ++i) {
                void* device = nullptr; Memory mem{};
                if (handle(i, &device) != 0 || memory(device, &mem) != 0) continue;
                const int selected = activeGpu.load();
                if (selected >= 0 && int(i) != selected) continue;
                if (!v.gpuKnown || mem.free > v.gpuFree) {
                    v.gpuKnown = true; v.gpuFree = mem.free; v.gpuTotal = mem.total; v.gpuIndex = int(i);
                    char label[128]{}; if (name && name(device, label, sizeof(label)) == 0) v.gpuName = QString::fromUtf8(label);
                    char identifier[128]{}; if (uuid && uuid(device,identifier,sizeof(identifier)) == 0) v.gpuUuid = QString::fromUtf8(identifier);
                    Util u{}; if (util && util(device, &u) == 0) v.gpuPercent = u.gpu;
                    if (temp) temp(device, 0, &v.temperature);
                }
            }
            v.sampledAt = QDateTime::currentMSecsSinceEpoch();
            { QMutexLocker lock(&sampleMutex); latest = v; }
            const QJsonObject policy = config();
            const quint64 ramReserve = qMax(minRam, quint64(v.ramTotal * (1.0 - fraction(policy, "ram_emergency_percent", 92))));
            const quint64 commitReserve = qMax(minCommit, quint64(v.commitTotal * (1.0 - fraction(policy, "commit_emergency_percent", 95))));
            const quint64 gpuReserve = qMax(minGpu, quint64(v.gpuTotal * (1.0 - fraction(policy, "vram_emergency_percent", 95))));
            const bool gpuInUse = activeGpu.load() >= 0;
            const bool low = (v.ramKnown && (v.ramFree < ramReserve || v.commitFree < commitReserve)) || (gpuInUse && v.gpuKnown && v.gpuFree < gpuReserve);
            lowSamples = low ? lowSamples + 1 : 0;
            if (lowSamples >= 4 && !pressure.exchange(true)) {
                cancel("持续内存或显存余量不足"); tripTicks = 0;
                Logger::write("WATCHDOG", "资源保护触发，取消推理和自动动作；" + description());
            }
            if (pressure) {
                if (++tripTicks == 10) releaseModelsInEmergency();
                const bool good = v.ramKnown && v.ramFree >= ramReserve + (256ull << 20) && v.commitFree >= commitReserve + (256ull << 20) && (!gpuInUse || !v.gpuKnown || v.gpuFree >= gpuReserve + (128ull << 20));
                goodSamples = good ? goodSamples + 1 : 0;
                if (goodSamples >= 20) { pressure = false; goodSamples = 0; Logger::write("WATCHDOG", "资源余量已恢复，旧任务保持停止，可手动发起新任务。"); }
            }
            if (++ticks == 10) { ticks = 0; Logger::write("RESOURCE", description()); }
            std::unique_lock<std::mutex> lock(waitMutex);
            wake.wait_for(lock, std::chrono::milliseconds(500), [this] { return quitting.load(); });
        }
        if (nvReady && shutdown) shutdown();
        if (nvml) FreeLibrary(nvml);
        QMutexLocker lock(&processMutex);
        for (HANDLE h : owned) CloseHandle(h);
        owned.clear();
    }
};

static QJsonObject defaultApp() {
    return {
        {"app", QJsonObject{{"name", QString::fromUtf8(CEH_APP_NAME)},{"version", QString::fromUtf8(CEH_APP_VERSION)}}},
        {"window", QJsonObject{{"width", 500},{"height", 1000},{"start_hidden", false}}}
    };
}
static QJsonObject defaultAgent() {
    return {
        {"max_steps", 30},{"max_retries_per_step", 3},{"step_timeout_ms", 30000},{"tool_timeout_ms", 120000},
        {"verify_after_action", true},{"stop_on_user_input", true},{"vision_fallback", true}
    };
}
static QJsonObject defaultCamera() {
    return {
        {"capture_cursor_monitor", true},{"capture_virtual_desktop", true},{"save_screenshot", true},{"format", "png"}
    };
}
static QJsonObject defaultModels() {
    const QString model = "D:/桌面/project/models/gemma-4/gemma-4-E4B-it-Q4_0.gguf";
    const QString mmproj = "D:/桌面/project/models/gemma-4/mmproj-gemma-4-E4B-it-Q8_0.gguf";
    QJsonObject llmLlama{
        {"runtime", "cuda13"},{"model", model},{"mmproj", ""},{"server_executable", "llama-server.exe"},
        {"host", "127.0.0.1"},{"port", 8888},{"context", 32768},{"gpu_layers", 999},{"threads", 8},
        {"temperature", 0.2},{"max_tokens", 4096},{"timeout_ms", 120000}
    };
    QJsonObject vlmLlama = llmLlama;
    vlmLlama["mmproj"] = mmproj;
    vlmLlama["port"] = 8889;
    vlmLlama["context"] = 16384;
    vlmLlama["temperature"] = 0.1;
    vlmLlama["max_tokens"] = 2048;
    QJsonObject profile{
        {"llm", QJsonObject{
            {"provider", "llama_cpp"},{"enabled", true},{"llama_cpp", llmLlama},
            {"ollama", QJsonObject{{"base_url", "http://127.0.0.1:11434"},{"model", "gemma4:e4b"},{"context", 32768},{"temperature", 0.2},{"max_tokens", 4096}}},
            {"online", QJsonObject{{"base_url", "https://api.openai.com/v1"},{"api_key", ""},{"model", ""},{"temperature", 0.2},{"max_tokens", 4096}}}
        }},
        {"vlm", QJsonObject{
            {"provider", "llama_cpp"},{"enabled", true},{"llama_cpp", vlmLlama},
            {"ollama", QJsonObject{{"base_url", "http://127.0.0.1:11434"},{"model", ""},{"context", 16384},{"temperature", 0.1}}},
            {"online", QJsonObject{{"base_url", "https://api.openai.com/v1"},{"api_key", ""},{"model", ""},{"temperature", 0.1},{"max_tokens", 2048}}}
        }},
        {"verifier", QJsonObject{{"provider", "same_as_llm"},{"enabled", true}}}
    };
    return {
        {"active_profile", "local_gemma"},
        {"profiles", QJsonObject{{"local_gemma", profile}}},
        {"runtime", QJsonObject{
            {"auto_select", true},
            {"priority", QJsonArray{"cuda13", "vulkan", "cpu"}},
            {"paths", QJsonObject{{"cpu", "runtime/Windows/cpu"},{"cuda13", "runtime/Windows/cuda13"},{"vulkan", "runtime/Windows/vulkan"}}}
        }}
    };
}
static QJsonObject defaultPermissions() {
    return {
        {"default_policy", "confirm"},
        {"safe_prefixes", QJsonArray{"dir","type","where","echo","cd ","whoami","hostname","ipconfig","git status","git diff","cmake --build"}},
        {"confirm_contains", QJsonArray{"del ","erase ","rmdir","rd ","taskkill","reg add","reg delete","shutdown","remove-item","start-process"}},
        {"forbidden_contains", QJsonArray{"format ","diskpart","clean all","bcdedit /delete","vssadmin delete shadows","cipher /w:"}}
    };
}
static QJsonObject defaultTools() {
    return {
        {"tools", QJsonArray{
            QJsonObject{{"name","uia_scan"},{"enabled",true},{"risk","safe"}},
            QJsonObject{{"name","uia_invoke"},{"enabled",true},{"risk","safe"}},
            QJsonObject{{"name","uia_set_value"},{"enabled",true},{"risk","safe"}},
            QJsonObject{{"name","screen_capture"},{"enabled",true},{"risk","safe"}},
            QJsonObject{{"name","vision_analyze"},{"enabled",true},{"risk","safe"}},
            QJsonObject{{"name","cmd_exec"},{"enabled",true},{"risk","confirm"}},
            QJsonObject{{"name","powershell_exec"},{"enabled",true},{"risk","confirm"}},
            QJsonObject{{"name","wait"},{"enabled",true},{"risk","safe"}},
            QJsonObject{{"name","done"},{"enabled",true},{"risk","safe"}}
        }}
    };
}
static void ensureJsonFile(const QString& relative, const QJsonObject& defaults) {
    const QString dst = writablePath(relative);
    if (QFileInfo::exists(dst) && QFileInfo(dst).size() > 0 && !readJson(dst).isEmpty()) return;
    const QString src = resourcePath(relative);
    if (QFileInfo::exists(src) && QFileInfo(src).size() > 0 && !readJson(src).isEmpty() && QDir::cleanPath(src) != QDir::cleanPath(dst)) {
        ensureDir(QFileInfo(dst).absolutePath());
        QFile::remove(dst);
        QFile::copy(src, dst);
        return;
    }
    writeJson(dst, defaults);
}
static void ensurePromptFile(const QString& relative, const QString& fallback) {
    const QString dst = writablePath(relative);
    if (QFileInfo::exists(dst) && QFileInfo(dst).size() > 0) return;
    const QString src = resourcePath(relative);
    if (QFileInfo::exists(src) && QFileInfo(src).size() > 0 && QDir::cleanPath(src) != QDir::cleanPath(dst)) {
        ensureDir(QFileInfo(dst).absolutePath());
        QFile::remove(dst);
        QFile::copy(src, dst);
        return;
    }
    writeText(dst, fallback + "\n");
}
static void ensureApplicationFiles() {
    ensureJsonFile("assets/config/app.json", defaultApp());
    ensureJsonFile("assets/config/agent.json", defaultAgent());
    {
        const QString path=writablePath("assets/config/agent.json");
        QJsonObject root=readJson(path), policy=root.value("resource_safety").toObject();
        const QJsonObject defaults{{"auto_allocate",true},{"ram_budget_percent",75},{"vram_budget_percent",80},
            {"commit_budget_percent",85},{"cpu_thread_percent",75},{"ram_emergency_percent",92},
            {"vram_emergency_percent",95},{"commit_emergency_percent",95},{"single_model_resident",true},
            {"llm_target_context",8192},{"vlm_target_context",4096}};
        bool changed=false;
        for(auto it=defaults.begin();it!=defaults.end();++it)if(!policy.contains(it.key())){policy[it.key()]=it.value();changed=true;}
        if(changed){root["resource_safety"]=policy;if(!Logger::atomicJson(path,root))Logger::write("ERROR","无法保存自动资源预算配置");}
    }
    ensureJsonFile("assets/config/camera.json", defaultCamera());
    ensureJsonFile("assets/config/models.json", defaultModels());
    ensureJsonFile("assets/config/permissions.json", defaultPermissions());
    ensureJsonFile("assets/config/tools.json", defaultTools());
    ensurePromptFile("assets/models/prompts/system.txt","你是木头自动系统控制的本地人工智能核心。必须服从 CURRENT_MODE；CHAT 只正常聊天，DESKTOP_CONTROL/WINDOW_CONTROL 才允许工具控制；内部 JSON 禁止直接展示给用户。");
    ensurePromptFile("assets/models/prompts/planner.txt","你只服务 DESKTOP_CONTROL/WINDOW_CONTROL。每次只输出一个 JSON 动作；UIA 优先；WINDOW_CONTROL 必须限制 selected_window。");
    ensurePromptFile("assets/models/prompts/vision.txt","必须服从 VISION_MODE。USER_IMAGE_CHAT/USER_SCREENSHOT_CHAT 输出自然语言；AGENT_DESKTOP_OBSERVATION/AGENT_WINDOW_OBSERVATION 输出结构化 JSON。");
    ensurePromptFile("assets/models/prompts/verifier.txt","验证动作步骤与用户最终目标是否真正完成，只输出 JSON；WINDOW_CONTROL 只能使用指定 HWND 状态作为主要证据。");
    ensurePromptFile("assets/models/prompts/context_summary.txt","你是Agent上下文压缩器。只总结旧历史，保留用户目标、已验证事实、失败原因、当前进度和待完成目标；旧UIA ID必须标记为过期，不得把上下文压缩视为任务失败。只输出JSON。");
}
static QString defaultSql() {
    return QString::fromUtf8(R"SQL(
PRAGMA foreign_keys = ON;
PRAGMA journal_mode = WAL;
PRAGMA synchronous = NORMAL;
PRAGMA busy_timeout = 5000;
CREATE TABLE IF NOT EXISTS database_meta (id INTEGER PRIMARY KEY AUTOINCREMENT,schema_version TEXT NOT NULL,app_version TEXT,created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP);
CREATE TABLE IF NOT EXISTS agent_session (id INTEGER PRIMARY KEY AUTOINCREMENT,session_uuid TEXT,title TEXT,goal TEXT NOT NULL,mode TEXT DEFAULT 'assist',status TEXT DEFAULT 'created',provider TEXT,model_name TEXT,started_at TEXT,finished_at TEXT,created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP);
CREATE TABLE IF NOT EXISTS agent_step (id INTEGER PRIMARY KEY AUTOINCREMENT,session_id INTEGER NOT NULL,step_index INTEGER NOT NULL,action_name TEXT,action_json TEXT,reason TEXT,result_text TEXT,success INTEGER DEFAULT 0,verified INTEGER DEFAULT 0,duration_ms INTEGER DEFAULT 0,error_message TEXT,created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,FOREIGN KEY(session_id) REFERENCES agent_session(id) ON DELETE CASCADE);
CREATE TABLE IF NOT EXISTS screenshot_record (id INTEGER PRIMARY KEY AUTOINCREMENT,session_id INTEGER,step_id INTEGER,file_path TEXT NOT NULL,screenshot_type TEXT,reason TEXT,created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP);
CREATE TABLE IF NOT EXISTS application_log (id INTEGER PRIMARY KEY AUTOINCREMENT,level TEXT NOT NULL,module TEXT,message TEXT NOT NULL,created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP);
)SQL");
}
static bool executeSqlScript(QSqlDatabase& db, const QString& script) {
    QString current; bool inSingle = false; bool inDouble = false;
    auto execOne = [&](const QString& raw) -> bool {
        const QString sql = raw.trimmed();
        if (sql.isEmpty()) return true;
        QSqlQuery q(db);
        if (!q.exec(sql)) { Logger::write("ERROR","SQLite执行失败：" + q.lastError().text() + " | " + sql.left(400)); return false; }
        return true;
    };
    for (int i = 0; i < script.size(); ++i) {
        const QChar ch = script.at(i);
        if (ch == '\'' && !inDouble) inSingle = !inSingle;
        else if (ch == '"' && !inSingle) inDouble = !inDouble;
        if (ch == ';' && !inSingle && !inDouble) { if (!execOne(current)) return false; current.clear(); }
        else current += ch;
    }
    return execOne(current);
}
static bool initDatabase() {
    const QString dbPath = writablePath("assets/db/wood.db");
    const QString sqlPath = writablePath("assets/db/SQLite.sql");
    ensureDir(QFileInfo(dbPath).absolutePath());
    if (!QFileInfo::exists(sqlPath) || QFileInfo(sqlPath).size() == 0) {
        const QString sourceSql = resourcePath("assets/db/SQLite.sql");
        if (QFileInfo::exists(sourceSql) && QFileInfo(sourceSql).size() > 0 && QDir::cleanPath(sourceSql) != QDir::cleanPath(sqlPath)) { QFile::remove(sqlPath); QFile::copy(sourceSql, sqlPath); }
        else writeText(sqlPath, defaultSql());
    }
    if (QSqlDatabase::contains("ceh")) QSqlDatabase::removeDatabase("ceh");
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "ceh");
    db.setDatabaseName(dbPath);
    if (!db.open()) { Logger::write("ERROR","SQLite打开失败：" + db.lastError().text()); return false; }
    if (!executeSqlScript(db, readText(sqlPath))) return false;
    Logger::write("INFO","wood.db 已初始化：" + dbPath);
    return true;
}
struct CaptureResult { QImage image; QRect rect; bool valid = false; };

static CaptureResult captureRect(const QRect& rect) {
    CaptureResult result;
    if (rect.width() <= 0 || rect.height() <= 0) return result;
    HDC screenDC = GetDC(nullptr);
    if (!screenDC) return result;
    HDC memDC = CreateCompatibleDC(screenDC);
    HBITMAP bitmap = CreateCompatibleBitmap(screenDC, rect.width(), rect.height());
    if (!memDC || !bitmap) {
        if (bitmap) DeleteObject(bitmap);
        if (memDC) DeleteDC(memDC);
        ReleaseDC(nullptr, screenDC);
        return result;
    }
    HGDIOBJ old = SelectObject(memDC, bitmap);
    BitBlt(memDC, 0, 0, rect.width(), rect.height(), screenDC, rect.x(), rect.y(), SRCCOPY | CAPTUREBLT);
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = rect.width();
    bmi.bmiHeader.biHeight = -rect.height();
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    QImage image(rect.width(), rect.height(), QImage::Format_ARGB32);
    if (GetDIBits(memDC, bitmap, 0, rect.height(), image.bits(), &bmi, DIB_RGB_COLORS)) { result.image = image; result.rect = rect; result.valid = true; }
    SelectObject(memDC, old);
    DeleteObject(bitmap);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);
    return result;
}

static CaptureResult captureWindow(HWND hwnd) {
    CaptureResult result;
    if (!hwnd || !IsWindow(hwnd)) return result;
    RECT wr{};
    if (!GetWindowRect(hwnd, &wr)) return result;
    const int width = wr.right - wr.left;
    const int height = wr.bottom - wr.top;
    if (width <= 1 || height <= 1) return result;
    HDC windowDC = GetWindowDC(hwnd);
    if (!windowDC) return captureRect(QRect(wr.left, wr.top, width, height));
    HDC memDC = CreateCompatibleDC(windowDC);
    HBITMAP bitmap = CreateCompatibleBitmap(windowDC, width, height);
    if (!memDC || !bitmap) {
        if (bitmap) DeleteObject(bitmap);
        if (memDC) DeleteDC(memDC);
        ReleaseDC(hwnd, windowDC);
        return captureRect(QRect(wr.left, wr.top, width, height));
    }
    HGDIOBJ old = SelectObject(memDC, bitmap);
#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif
    BOOL printed = PrintWindow(hwnd, memDC, PW_RENDERFULLCONTENT);
    if (!printed) printed = PrintWindow(hwnd, memDC, 0);
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    QImage image(width, height, QImage::Format_ARGB32);
    bool gotBits = GetDIBits(memDC, bitmap, 0, height, image.bits(), &bmi, DIB_RGB_COLORS) != 0;
    SelectObject(memDC, old);
    DeleteObject(bitmap);
    DeleteDC(memDC);
    ReleaseDC(hwnd, windowDC);
    bool suspiciousBlack = true;
    if (printed && gotBits && !image.isNull()) {
        const int stepX = qMax(1, width / 16);
        const int stepY = qMax(1, height / 16);
        int nonBlack = 0; int samples = 0;
        for (int y = 0; y < height; y += stepY) {
            for (int x = 0; x < width; x += stepX) {
                const QColor c = image.pixelColor(x, y); ++samples;
                if (c.red() + c.green() + c.blue() > 18) ++nonBlack;
            }
        }
        suspiciousBlack = samples == 0 || nonBlack < qMax(2, samples / 50);
    }
    if (!printed || !gotBits || image.isNull() || suspiciousBlack) return captureRect(QRect(wr.left, wr.top, width, height));
    result.image = image;
    result.rect = QRect(wr.left, wr.top, width, height);
    result.valid = true;
    return result;
}

static CaptureResult captureVirtualDesktop() {
    return captureRect(QRect(GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN), GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN)));
}

static CaptureResult captureCursorMonitor() {
    POINT p{};
    GetCursorPos(&p);
    HMONITOR monitor = MonitorFromPoint(p, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfo(monitor, &info)) return {};
    const RECT r = info.rcMonitor;
    return captureRect(QRect(r.left, r.top, r.right - r.left, r.bottom - r.top));
}

static QString saveScreenshot(const QImage& image, const QString& suffix) {
    const QDate d = QDate::currentDate();
    const QString dir = writablePath(QString("assets/uppic/%1/%2/%3")
        .arg(d.year(), 4, 10, QChar('0')).arg(d.month(), 2, 10, QChar('0')).arg(d.day(), 2, 10, QChar('0')));
    ensureDir(dir);
    const QString path = QDir(dir).filePath(nowFileTime() + "_" + suffix + ".png");
    image.save(path, "PNG");
    return path;
}

// Agent视觉审计截图与文本日志放在同一个日期目录，便于后续复盘。
// 普通用户截图仍保留原assets/uppic目录，不改变现有UI聊天行为。
static QString saveAgentAuditScreenshot(const QImage& image, const QString& taskId, int step, const QString& suffix) {
    const QDate d = QDate::currentDate();
    const QString dir = QDir(Logger::root()).filePath(QString("%1/%2/%3")
        .arg(d.year(), 4, 10, QChar('0')).arg(d.month(), 2, 10, QChar('0')).arg(d.day(), 2, 10, QChar('0')));
    if (!ensureDir(dir)) return {};
    const QString safeTask = taskId.isEmpty() ? "no_task" : taskId;
    const QString fileName = QString("%1_task_%2_step_%3_%4.png").arg(nowFileTime()).arg(safeTask).arg(step, 3, 10, QChar('0')).arg(suffix);
    const QString path = QDir(dir).filePath(fileName);
    if (!image.save(path, "PNG")) return {};
    return QDir::cleanPath(path);
}

struct UIAItem { int id = 0; QString type; QString name; QString automationId; QRect rect; bool clickable = false; };

static QString bstrString(BSTR s) { if (!s) return {}; return QString::fromWCharArray(s, SysStringLen(s)); }

static QString controlTypeName(CONTROLTYPEID id) {
    switch (id) {
        case UIA_ButtonControlTypeId: return "Button";
        case UIA_CheckBoxControlTypeId: return "CheckBox";
        case UIA_ComboBoxControlTypeId: return "ComboBox";
        case UIA_EditControlTypeId: return "Edit";
        case UIA_HyperlinkControlTypeId: return "Hyperlink";
        case UIA_ListItemControlTypeId: return "ListItem";
        case UIA_ListControlTypeId: return "List";
        case UIA_MenuItemControlTypeId: return "MenuItem";
        case UIA_RadioButtonControlTypeId: return "RadioButton";
        case UIA_TabItemControlTypeId: return "TabItem";
        case UIA_TextControlTypeId: return "Text";
        case UIA_TreeItemControlTypeId: return "TreeItem";
        case UIA_WindowControlTypeId: return "Window";
        case UIA_PaneControlTypeId: return "Pane";
        default: return QString("Control(%1)").arg(id);
    }
}

static const ULONG_PTR CEH_AI_INPUT_MAGIC = static_cast<ULONG_PTR>(0x43454841u);

class UIAutomationEngine {
public:
    UIAutomationEngine() {
        const HRESULT hr = CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_uia));
        if (FAILED(hr)) { m_uia = nullptr; Logger::write("ERROR","UIAutomation初始化失败"); }
        else {
            IUIAutomation2* options=nullptr;
            if(SUCCEEDED(m_uia->QueryInterface(IID_PPV_ARGS(&options)))&&options){options->put_ConnectionTimeout(2000);options->put_TransactionTimeout(5000);options->Release();}
            Logger::write("INFO","UIAutomation初始化成功");
        }
    }
    ~UIAutomationEngine() { clearElements(); if (m_uia) m_uia->Release(); }
    bool valid() const { return m_uia != nullptr; }
    QVector<UIAItem> scanDesktop() {
        clearElements();m_items.clear();m_nextId=1;m_scanClock.start();
        if (!m_uia) return m_items;
        IUIAutomationElement* root = nullptr;
        if (FAILED(m_uia->GetRootElement(&root)) || !root) return m_items;
        walk(root, 0, 10, 2500); root->Release();
        return m_items;
    }
    QVector<UIAItem> scanDesktopOverview() {
        clearElements();m_items.clear();m_nextId=1;m_scanClock.start();
        if (!m_uia) return m_items;
        IUIAutomationElement* root = nullptr;
        if (FAILED(m_uia->GetRootElement(&root)) || !root) return m_items;
        walk(root, 0, 2, 500); root->Release();
        return m_items;
    }
    QVector<UIAItem> scanWindow(HWND hwnd) {
        clearElements();m_items.clear();m_nextId=1;m_scanClock.start();
        if (!m_uia || !hwnd) return m_items;
        IUIAutomationElement* root = nullptr;
        if (FAILED(m_uia->ElementFromHandle(hwnd, &root)) || !root) return m_items;
        walk(root, 0, 15, 2500); root->Release();
        return m_items;
    }
    bool invoke(int id, QString* error = nullptr) {
        const UIAItem* item = find(id);
        if (!item) { if (error) *error = "UIA ID不存在"; return false; }
        IUIAutomationElement* element=elementForId(id,error);if(!element)return false;
        bool ok = false; IUnknown* unknown = nullptr;
        if (SUCCEEDED(element->GetCurrentPattern(UIA_InvokePatternId, &unknown)) && unknown) {
            IUIAutomationInvokePattern* p = nullptr;
            if (SUCCEEDED(unknown->QueryInterface(IID_PPV_ARGS(&p))) && p) { ok = SUCCEEDED(p->Invoke()); p->Release(); }
            unknown->Release();
        }
        element->Release();
        if (!ok && error) *error = "控件调用失败";
        return ok;
    }
    bool focus(int id, QString* error = nullptr) {
        const UIAItem* item = find(id);
        if (!item) { if (error) *error = "UIA ID不存在"; return false; }
        IUIAutomationElement* element=elementForId(id,error);if(!element)return false;
        const bool ok = SUCCEEDED(element->SetFocus());
        element->Release();
        if (!ok && error) *error = "SetFocus失败";
        return ok;
    }
    bool setValue(int id,const QString& text,QString* error=nullptr) {
        auto* element=elementForId(id,error);if(!element)return false;
        IUIAutomationValuePattern* pattern=nullptr;
        const HRESULT query=element->GetCurrentPatternAs(UIA_ValuePatternId,IID_PPV_ARGS(&pattern));bool ok=false;
        if(SUCCEEDED(query)&&pattern){BSTR value=SysAllocStringLen(reinterpret_cast<const OLECHAR*>(text.utf16()),UINT(text.size()));if(value){ok=SUCCEEDED(pattern->SetValue(value));SysFreeString(value);}pattern->Release();}
        element->Release();
        if(!ok&&error)*error="控件不支持ValuePattern或写入失败，请通过视觉定位输入框后使用keyboard_type";
        return ok;
    }
    bool operate(int id,const QString& action,const QJsonObject& options,QString* error=nullptr) {
        auto* element=elementForId(id,error);if(!element)return false;HRESULT hr=E_FAIL;
        if(action=="uia_select"){IUIAutomationSelectionItemPattern* p=nullptr;if(SUCCEEDED(element->GetCurrentPatternAs(UIA_SelectionItemPatternId,IID_PPV_ARGS(&p)))&&p){hr=p->Select();p->Release();}}
        if(action=="uia_toggle"){IUIAutomationTogglePattern* p=nullptr;if(SUCCEEDED(element->GetCurrentPatternAs(UIA_TogglePatternId,IID_PPV_ARGS(&p)))&&p){hr=p->Toggle();p->Release();}}
        if(action=="uia_scroll"){IUIAutomationScrollPattern* p=nullptr;if(SUCCEEDED(element->GetCurrentPatternAs(UIA_ScrollPatternId,IID_PPV_ARGS(&p)))&&p){const QString direction=options.value("direction").toString("down");const bool page=options.value("page").toBool(false);const auto increase=page?ScrollAmount_LargeIncrement:ScrollAmount_SmallIncrement;const auto decrease=page?ScrollAmount_LargeDecrement:ScrollAmount_SmallDecrement;hr=p->Scroll(direction=="right"?increase:direction=="left"?decrease:ScrollAmount_NoAmount,direction=="down"?increase:direction=="up"?decrease:ScrollAmount_NoAmount);p->Release();}}
        element->Release();
        if(FAILED(hr)&&error)*error="UIA模式不受支持或操作失败，HRESULT="+QString::number(quint32(hr),16);
        return SUCCEEDED(hr);
    }
private:
    QMap<int,IUIAutomationElement*> m_elements;
    QElapsedTimer m_scanClock;
    void clearElements(){for(auto* element:m_elements)if(element)element->Release();m_elements.clear();}
    IUIAutomationElement* elementForId(int id,QString* error){
        auto* element=m_elements.value(id,nullptr);const auto* item=find(id);
        if(!element||!item){if(error)*error="UIA ID已失效，必须重新扫描";return nullptr;}
        RECT r{};if(FAILED(element->get_CurrentBoundingRectangle(&r))){if(error)*error="UIA控件不可用";return nullptr;}
        const QRect current(r.left,r.top,r.right-r.left,r.bottom-r.top);
        if(current.isEmpty()||!current.intersects(item->rect)){if(error)*error="UIA控件位置已变化，必须重新扫描";return nullptr;}
        element->AddRef();return element;
    }
    IUIAutomation* m_uia = nullptr;
    QVector<UIAItem> m_items;
    int m_nextId = 1;
    const UIAItem* find(int id) const { for (const auto& item : m_items) if (item.id == id) return &item; return nullptr; }
    bool clickable(IUIAutomationElement* element, CONTROLTYPEID type) {
        switch (type) {
            case UIA_ButtonControlTypeId: case UIA_CheckBoxControlTypeId: case UIA_ComboBoxControlTypeId:
            case UIA_EditControlTypeId: case UIA_HyperlinkControlTypeId: case UIA_ListItemControlTypeId:
            case UIA_MenuItemControlTypeId: case UIA_RadioButtonControlTypeId: case UIA_TabItemControlTypeId: return true;
            default: break;
        }
        IUnknown* p = nullptr;
        if (SUCCEEDED(element->GetCurrentPattern(UIA_InvokePatternId, &p)) && p) { p->Release(); return true; }
        return false;
    }
    void walk(IUIAutomationElement* element, int depth, int maxDepth, int maxItems) {
        if (!element || depth > maxDepth || m_items.size() >= maxItems || (m_scanClock.isValid()&&m_scanClock.elapsed()>5000)) return;
        CONTROLTYPEID type = 0;
        element->get_CurrentControlType(&type);
        BSTR nameRaw = nullptr; BSTR idRaw = nullptr;
        element->get_CurrentName(&nameRaw); element->get_CurrentAutomationId(&idRaw);
        RECT r{};
        element->get_CurrentBoundingRectangle(&r);
        const QRect rect(r.left, r.top, r.right - r.left, r.bottom - r.top);
        const QString name = bstrString(nameRaw);
        const QString automationId = bstrString(idRaw);
        if (nameRaw) SysFreeString(nameRaw);
        if (idRaw) SysFreeString(idRaw);
        const bool canClick = clickable(element, type);
        if (rect.width() > 1 && rect.height() > 1 && (canClick || !name.trimmed().isEmpty() || type == UIA_WindowControlTypeId)) {
            UIAItem item;
            item.id = m_nextId++; item.type = controlTypeName(type); item.name = name;
            item.automationId = automationId; item.rect = rect; item.clickable = canClick;
            m_items.push_back(item);element->AddRef();m_elements.insert(item.id,element);
        }
        IUIAutomationTreeWalker* walker = nullptr;
        if (FAILED(m_uia->get_ControlViewWalker(&walker)) || !walker) return;
        IUIAutomationElement* child = nullptr;
        HRESULT hr = walker->GetFirstChildElement(element, &child);
        while (SUCCEEDED(hr) && child && m_items.size() < maxItems && (!m_scanClock.isValid()||m_scanClock.elapsed()<=5000)) {
            walk(child, depth + 1, maxDepth, maxItems);
            IUIAutomationElement* next = nullptr;
            hr = walker->GetNextSiblingElement(child, &next);
            child->Release();
            child = next;
        }
        if (child) child->Release();
        walker->Release();
    }
};

static QImage drawOverlay(const CaptureResult& capture, const QVector<UIAItem>& items) {
    if (!capture.valid) return {};
    QImage image = capture.image.copy();
    QPainter p(&image);
    QFont f(g_lightFontFamily);
    f.setPixelSize(13); f.setWeight(QFont::Normal); p.setFont(f);
    for (const UIAItem& item : items) {
        if (!item.clickable) continue;
        QRect r = item.rect.translated(-capture.rect.x(), -capture.rect.y());
        if (!r.intersects(image.rect())) continue;
        p.setPen(QPen(QColor("#40c977"), 2)); p.drawRect(r);
        QString label = QString("#%1 %2 %3").arg(item.id).arg(item.type).arg(item.name.left(20));
        QRect tr = p.boundingRect(QRect(r.x(), qMax(0, r.y() - 23), 400, 20), Qt::AlignLeft | Qt::AlignVCenter, label);
        p.fillRect(tr.adjusted(-3,-2,3,2), QColor(20,20,20,220));
        p.setPen(Qt::white);
        p.drawText(tr, Qt::AlignLeft | Qt::AlignVCenter, label);
    }
    return image;
}

struct HttpResult { bool ok = false; int code = 0; QByteArray raw; QString error; };

static HttpResult waitHttp(QNetworkReply* reply, int timeoutMs, bool cancellable) {
    const auto generation = RuntimeSafety::instance().generation();
    QEventLoop loop;
    QElapsedTimer elapsed; elapsed.start();
    QTimer poll; poll.setInterval(100);
    QString stopped;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if (cancellable && RuntimeSafety::instance().shouldAbort(generation)) stopped = RuntimeSafety::instance().abortDescription(generation);
        else if (elapsed.elapsed() >= timeoutMs) stopped = "请求超过总时限";
        else if (reply->bytesAvailable() > 4 * 1024 * 1024) stopped = "模型响应过大，已停止接收";
        if (!stopped.isEmpty()) { reply->abort(); loop.quit(); }
    });
    poll.start();
    if (cancellable && RuntimeSafety::instance().shouldAbort(generation)) { stopped = RuntimeSafety::instance().abortDescription(generation); reply->abort(); }
    if (!reply->isFinished()) loop.exec();
    poll.stop();
    HttpResult r;
    r.code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    r.raw = reply->readAll();
    r.ok = stopped.isEmpty() && reply->error() == QNetworkReply::NoError && r.code >= 200 && r.code < 300;
    if (!r.ok) r.error = !stopped.isEmpty() ? stopped : reply->errorString() + "\n" + QString::fromUtf8(r.raw).left(4096);
    reply->deleteLater();
    return r;
}
static HttpResult httpGet(const QUrl& url) {
    QNetworkAccessManager manager;
    QNetworkRequest request(url); request.setTransferTimeout(2000);
    return waitHttp(manager.get(request), 2000, true);
}
static HttpResult httpPostJson(const QUrl& url, const QJsonObject& body, const QMap<QByteArray,QByteArray>& headers = {}) {
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setTransferTimeout(RuntimeSafety::requestTimeout());
    for (auto it = headers.begin(); it != headers.end(); ++it) request.setRawHeader(it.key(), it.value());
    return waitHttp(manager.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact)), RuntimeSafety::requestTimeout(), true);
}

// 防止 Qt 嵌套事件循环重新进入同一模型管理器，Model Worker 一次只接受一个推理。
class ModelActivity {
public:
    ModelActivity() { RuntimeSafety::instance().modelOperations.fetch_add(1); }
    ~ModelActivity() { RuntimeSafety::instance().modelOperations.fetch_sub(1); }
};
class ModelOperation {
public:
    ModelOperation(bool& busy, QString* error) : flag(busy), accepted(!busy) {
        if (busy) { if (error) *error = "已有推理正在执行，拒绝重入请求"; }
        else if (RuntimeSafety::instance().shouldAbort(RuntimeSafety::instance().generation())) { accepted = false; if (error) *error = RuntimeSafety::instance().abortDescription(RuntimeSafety::instance().generation()); }
        if (accepted) { flag = true; RuntimeSafety::instance().modelOperations.fetch_add(1); }
    }
    ~ModelOperation() { if (accepted) { flag = false; RuntimeSafety::instance().modelOperations.fetch_sub(1); } }
    explicit operator bool() const { return accepted; }
private:
    bool& flag; bool accepted;
};

// GGUF只读取元数据和张量索引，不把整个模型读入内存。
// 根据张量偏移统计每个实际block的权重；文件大小与运行内存占用明确分开。
struct ModelFootprint {
    QString path, architecture, name, warning;
    quint64 fileBytes = 0, tensorBytes = 0, nonLayerBytes = 0;
    int layers = 0, trainedContext = 0, embedding = 0, heads = 0, kvHeads = 0, keyLength = 0, valueLength = 0;
    QVector<quint64> layerBytes;
    bool indexed = false, kvKnown = false;
};
class GgufInspector {
public:
    static ModelFootprint inspect(const QString& path, QString* error) {
        ModelFootprint m; m.path = path; m.fileBytes = quint64(qMax<qint64>(0, QFileInfo(path).size()));
        GgufInspector r(path);
        if (!r.f.open(QIODevice::ReadOnly) || r.f.read(4) != "GGUF") return fail(m, error, "不是可读取的GGUF文件：" + path);
        const quint32 version = r.u32(); const quint64 tensors = r.u64(), metadata = r.u64();
        if ((version != 2 && version != 3) || tensors == 0 || tensors > 1000000 || metadata > 100000) return fail(m,error,"GGUF头部版本或索引数量不合法");
        QMap<QString,quint64> numbers; QMap<QString,QString> strings;
        for (quint64 i=0; i<metadata && r.ok; ++i) {
            const QString key = r.text(); const quint32 type = r.u32();
            if (type == 8) strings[key] = r.text();
            else if (type == 4) numbers[key] = r.u32();
            else if (type == 10) numbers[key] = r.u64();
            else r.skip(type,0);
        }
        m.architecture = strings.value("general.architecture"); m.name = strings.value("general.name", QFileInfo(path).baseName());
        const QString a=m.architecture + ".";
        auto integer=[&](const QString& key){ return int(qMin<quint64>(numbers.value(a+key), 1000000)); };
        m.layers=integer("block_count"); m.trainedContext=integer("context_length"); m.embedding=integer("embedding_length");
        m.heads=integer("attention.head_count"); m.kvHeads=integer("attention.head_count_kv");
        m.keyLength=integer("attention.key_length"); m.valueLength=integer("attention.value_length");
        struct Tensor { quint64 offset; int layer; };
        QVector<Tensor> list; list.reserve(int(tensors));
        const QRegularExpression block("^blk\\.(\\d+)\\.");
        for (quint64 i=0;i<tensors && r.ok;++i) {
            const QString label=r.text(); const quint32 dims=r.u32();
            if (dims==0 || dims>8) { r.ok=false; break; }
            for (quint32 d=0;d<dims;++d) if (r.u64()==0) r.ok=false;
            r.u32(); const quint64 offset=r.u64();
            const auto match=block.match(label);
            list.append({offset,match.hasMatch()?match.captured(1).toInt():-1});
        }
        const quint64 alignment=numbers.value("general.alignment",32);
        if (!r.ok || alignment==0 || alignment>4096 || (alignment&(alignment-1))) return fail(m,error,"GGUF元数据/张量索引损坏或不支持");
        const quint64 data=(quint64(r.f.pos())+alignment-1)/alignment*alignment;
        if (data>=m.fileBytes || m.layers>4096) return fail(m,error,"GGUF数据偏移或层数不合法");
        std::sort(list.begin(),list.end(),[](const Tensor& x,const Tensor& y){return x.offset<y.offset;});
        m.layerBytes.fill(0,m.layers); m.tensorBytes=m.fileBytes-data;
        for (int i=0;i<list.size();++i) {
            const quint64 end=i+1<list.size()?list.at(i+1).offset:m.tensorBytes;
            if (list.at(i).offset>=end || end>m.tensorBytes) return fail(m,error,"GGUF张量偏移重叠或越界");
            const quint64 bytes=end-list.at(i).offset;
            if (list.at(i).layer>=0 && list.at(i).layer<m.layers) m.layerBytes[list.at(i).layer]+=bytes; else m.nonLayerBytes+=bytes;
        }
        m.indexed=m.layers>0 && std::all_of(m.layerBytes.cbegin(),m.layerBytes.cend(),[](quint64 n){return n>0;});
        if (!m.keyLength && m.heads) m.keyLength=m.embedding/m.heads;
        if (!m.valueLength) m.valueLength=m.keyLength;
        const QStringList transformer{"llama","qwen2","qwen3","qwen3moe","gemma","gemma2","gemma3","gemma4","mistral","phi3","deepseek2","cohere","falcon"};
        m.kvKnown=transformer.contains(m.architecture) && m.layers>0 && m.kvHeads>0 && m.keyLength>0 && m.valueLength>0;
        if (!m.indexed || !m.kvKnown) m.warning="未知/混合缓存布局，禁用GPU权重估算，采用小上下文CPU方案；以服务实际分配为准。";
        if (error) error->clear(); return m;
    }
private:
    QFile f; bool ok=true;
    explicit GgufInspector(const QString& path):f(path){}
    quint32 u32(){ char b[4]{}; if(f.read(b,4)!=4)ok=false; return qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(b)); }
    quint64 u64(){ char b[8]{}; if(f.read(b,8)!=8)ok=false; return qFromLittleEndian<quint64>(reinterpret_cast<const uchar*>(b)); }
    QString text(){const quint64 n=u64(); if(n>1048576 || n>quint64(qMax<qint64>(0,f.size()-f.pos()))){ok=false;return {}; } const QByteArray b=f.read(qint64(n)); if(quint64(b.size())!=n)ok=false; return QString::fromUtf8(b);}
    void advance(quint64 n){if(n>quint64(qMax<qint64>(0,f.size()-f.pos())) || !f.seek(f.pos()+qint64(n)))ok=false;}
    void skip(quint32 type,int depth){
        static const int widths[]={1,1,2,2,4,4,4,1,0,0,8,8,8};
        if(depth>8 || type>12){ok=false;return;}
        if(type==8){const quint64 n=u64();advance(n);return;}
        if(type==9){const quint32 t=u32();const quint64 n=u64();if(n>10000000 || t>12){ok=false;return;} if(widths[t])advance(n*quint64(widths[t]));else for(quint64 i=0;i<n && ok;++i)skip(t,depth+1);return;}
        advance(widths[type]);
    }
    static ModelFootprint fail(ModelFootprint m,QString* e,const QString& text){ if(e)*e=text;m.indexed=false; m.warning=text;return m; }
};
struct AdaptivePlan {
    bool valid=false, cpuOnly=true;
    int context=2048, gpuLayers=0, threads=1, batch=128, microBatch=32, gpuIndex=-1;
    quint64 ramBudget=0,gpuBudget=0,ramEstimate=0,gpuEstimate=0;
    QString reason;
};
class ResourcePlanner {
public:
    static quint64 bytes(double value) {
        if (!std::isfinite(value) || value < 0 || value >= double(std::numeric_limits<quint64>::max())) return std::numeric_limits<quint64>::max();
        return quint64(value);
    }
    static quint64 headroom(quint64 total,quint64 free,double cap,quint64 reserve){
        const quint64 allowed=quint64(double(total)*cap),used=total>free?total-free:0;
        return qMin(allowed>used?allowed-used:0,free>reserve?free-reserve:0);
    }
    static AdaptivePlan make(const ModelFootprint& model,quint64 projector,const RuntimeSafety::Snapshot& hw,const QJsonObject& cfg,const QString& role,bool gpuAllowed,int attempt=0){
        AdaptivePlan p; const QJsonObject c=RuntimeSafety::config();
        const double ramFraction=RuntimeSafety::fraction(c,"ram_budget_percent",75),gpuFraction=RuntimeSafety::fraction(c,"vram_budget_percent",80);
        const quint64 ramReserve=qMax<quint64>(768ull<<20,quint64(hw.ramTotal*0.10));
        p.ramBudget=headroom(hw.ramTotal,hw.ramFree,ramFraction,ramReserve);
        if(hw.commitTotal) p.ramBudget=qMin(p.ramBudget,headroom(hw.commitTotal,hw.commitFree,RuntimeSafety::fraction(c,"commit_budget_percent",85),1024ull<<20));
        p.gpuBudget=gpuAllowed && hw.gpuKnown?headroom(hw.gpuTotal,hw.gpuFree,gpuFraction,qMax<quint64>(512ull<<20,quint64(hw.gpuTotal*0.10))):0;
        const double retryFactor=std::pow(0.80,attempt); p.gpuBudget=quint64(p.gpuBudget*retryFactor); p.ramBudget=quint64(p.ramBudget*retryFactor);
        const int logical=qMax(1,QThread::idealThreadCount());
        p.threads=qBound(1,int(std::floor(logical*RuntimeSafety::fraction(c,"cpu_thread_percent",75))),logical);
        const bool automatic=c.value("auto_allocate").toBool(true);
        if(!automatic)p.threads=qBound(1,cfg.value("threads").toInt(p.threads),logical);
        const int preferred=c.value(role=="vlm"?"vlm_target_context":"llm_target_context").toInt(role=="vlm"?4096:8192);
        int desired=qBound(2048,cfg.value("context").toInt(preferred),131072);
        if(automatic)desired=qMin(desired,qBound(2048,preferred,131072));
        if(model.trainedContext>0 && model.trainedContext<2048){p.reason="模型声明的上下文窗口小于当前智能体最低需求，拒绝超出模型窗口启动。";return p;}
        if(model.trainedContext>0)desired=qMin(desired,model.trainedContext);
        desired=qMax(2048,desired>>attempt);
        if(!model.kvKnown || !model.indexed)desired=2048;
        const quint64 stage=qMin<quint64>(model.tensorBytes/8,512ull<<20);
        for(int context=desired;context>=2048;context=context>2048?qMax(2048,context/2):0){
            const double kvPerLayer=model.kvKnown?double(context)*model.kvHeads*(model.keyLength+model.valueLength)*2.0*1.20:double(context)*qMax(4096,model.embedding)*8.0;
            int maxLayers=model.indexed && model.kvKnown && gpuAllowed && p.gpuBudget?model.layers:0;
            if(!automatic){const int requested=cfg.value("gpu_layers").toInt(maxLayers);if(requested>=0)maxLayers=qMin(maxLayers,requested);}
            for(int n=maxLayers;n>=0;--n){
                quint64 weights=0;
                for(int i=model.layers-n;i<model.layers && n>0;++i)weights+=model.layerBytes.at(i);
                const bool gpu=n>0;
                const quint64 gpuNeed=gpu?bytes(weights*1.10+kvPerLayer*n+double(projector)*1.50+(role=="vlm"?512ull:256ull)*1024*1024):0;
                const quint64 cpuWeights=model.tensorBytes>weights?model.tensorBytes-weights:0;
                const quint64 cpuNeed=bytes(cpuWeights*1.10+kvPerLayer*qMax(1,model.layers-n)+(!gpu?double(projector)*1.50:0.0)+stage+(768ull<<20));
                if(cpuNeed<=p.ramBudget && (!gpu || gpuNeed<=p.gpuBudget)){
                    p.valid=true;p.context=context;p.gpuLayers=n;p.cpuOnly=!gpu;p.gpuIndex=gpu?hw.gpuIndex:-1;p.ramEstimate=cpuNeed;p.gpuEstimate=gpuNeed;
                    p.batch=context>=8192?256:128;p.microBatch=p.gpuBudget>=(4ull<<30)?64:32;
                    p.reason=model.warning;
                    if(p.reason.isEmpty() && p.cpuOnly)p.reason=gpuAllowed?"显存预算不足，安全分配为CPU推理":"显卡监控或架构信息不可用/未选择CUDA后端，采用CPU推理";
                    return p;
                }
            }
            if(context==2048)break;
        }
        p.reason="模型权重、缓存、视觉投影及运行缓冲区无法放入当前RAM/显存百分比预算，请关闭其他程序或选择更小模型。";return p;
    }
    static QString summary(const ModelFootprint& m,quint64 projector,const RuntimeSafety::Snapshot& h,const AdaptivePlan& p){
        QSettings cpu("HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",QSettings::NativeFormat);
        return QString("模型=%1 架构=%2 层数=%3 主文件=%4MiB mmproj=%5MiB；CPU=%6 逻辑线程=%7；RAM=%8MiB 可用=%9MiB；GPU[%10]=%11 总显存=%12MiB 可用=%13MiB；计划 GPU层=%14 CPU线程=%15 ctx=%16 batch=%17/%18 RAM估计/预算=%19/%20MiB GPU估计/预算=%21/%22MiB；%23")
            .arg(m.name,m.architecture).arg(m.layers).arg(m.fileBytes>>20).arg(projector>>20)
            .arg(cpu.value("ProcessorNameString","未知CPU").toString()).arg(QThread::idealThreadCount()).arg(h.ramTotal>>20).arg(h.ramFree>>20)
            .arg(h.gpuIndex).arg(h.gpuName).arg(h.gpuTotal>>20).arg(h.gpuFree>>20).arg(p.gpuLayers).arg(p.threads).arg(p.context).arg(p.batch).arg(p.microBatch)
            .arg(p.ramEstimate>>20).arg(p.ramBudget>>20).arg(p.gpuEstimate>>20).arg(p.gpuBudget>>20).arg(p.reason);
    }
};

class ModelJob {
public:
    ModelJob() {
        handle = CreateJobObjectW(nullptr, nullptr);
        if (handle) {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
            info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            if (!SetInformationJobObject(handle, JobObjectExtendedLimitInformation, &info, sizeof(info))) { CloseHandle(handle); handle = nullptr; }
        }
    }
    ~ModelJob() { if (handle) CloseHandle(handle); }
    void attach(qint64 pid) {
        if (!handle) { Logger::write("WARNING", "模型Job保护不可用，退出时仍会按所属进程清理"); return; }
        HANDLE process = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, DWORD(pid));
        if (process) {
            if (!AssignProcessToJobObject(handle, process)) Logger::write("WARNING", "模型进程加入Job失败：" + QString::number(GetLastError()));
            CloseHandle(process);
        }
    }
    ModelJob(const ModelJob&) = delete;
    ModelJob& operator=(const ModelJob&) = delete;
private:
    HANDLE handle = nullptr;
};

class ModelManager {
public:
    ModelManager() { reload(); }
    ~ModelManager() { stopAll(); }
    void reload() {
        m_models = loadConfig("assets/config/models.json");
        m_activeName = m_models.value("active_profile").toString("local_gemma");
        m_profile = m_models.value("profiles").toObject().value(m_activeName).toObject();
    }
    QJsonObject role(const QString& name) const { return m_profile.value(name).toObject(); }
    QString provider(const QString& name) const { return role(name).value("provider").toString("llama_cpp"); }
    bool ensureReady(const QString& roleName, QString* error) {
        ModelActivity loadingActivity;
        if (RuntimeSafety::instance().shouldAbort(RuntimeSafety::instance().generation())) { if (error) *error = RuntimeSafety::instance().abortDescription(RuntimeSafety::instance().generation()); return false; }
        if (provider(roleName) != "llama_cpp") return true;
        if(roleName=="llm"&&canReuseMultimodal()){
            RuntimeSafety::instance().setEffectiveContext("llm",RuntimeSafety::contextLimit("vlm"));
            Logger::write("MODEL_REUSE","文本规划复用已加载的同一多模态模型，不重复加载权重");return true;
        }
        if (RuntimeSafety::config().value("single_model_resident").toBool(true)) {
            QProcess*& other = roleName == "vlm" ? m_llm : m_vlm;
            if (other) { Logger::write("MODEL", "按需切换至" + roleName + "，卸载另一模型服务"); stopProcess(other); }
        }
        QProcess*& proc = roleName == "vlm" ? m_vlm : m_llm;
        const QJsonObject requestedCfg = role(roleName).value("llama_cpp").toObject();
        const QString signature = QString::fromUtf8(QJsonDocument(requestedCfg).toJson(QJsonDocument::Compact)) + QString::fromUtf8(QJsonDocument(RuntimeSafety::config()).toJson(QJsonDocument::Compact));
        const QString modelFile = requestedCfg.value("model").toString();
        const QString projectorFile = requestedCfg.value("mmproj").toString();
        const QString fileSignature = signature + QString::number(QFileInfo(modelFile).size()) + QFileInfo(modelFile).lastModified().toString(Qt::ISODateWithMs)
            + QString::number(QFileInfo(projectorFile).size()) + QFileInfo(projectorFile).lastModified().toString(Qt::ISODateWithMs);
        if (proc && proc->state() != QProcess::NotRunning && proc->property("config_signature").toString() == fileSignature) return true;
        if (proc) stopProcess(proc);
        const QJsonObject cfg = role(roleName).value("llama_cpp").toObject();
        const QString model = cfg.value("model").toString();
        const QString mmproj = roleName=="vlm"?cfg.value("mmproj").toString():QString();
        const QString host = cfg.value("host").toString("127.0.0.1");
        const int port = cfg.value("port").toInt(roleName == "vlm" ? 8889 : 8888);
        if (!QFileInfo::exists(model)) { if(error)*error="GGUF模型不存在："+model; return false; }
        if (!mmproj.isEmpty() && !QFileInfo::exists(mmproj)) { if(error)*error="mmproj不存在："+mmproj;return false; }
        QString inspectError;
        const ModelFootprint footprint=GgufInspector::inspect(model,&inspectError);
        if(!inspectError.isEmpty()){if(error)*error=inspectError;return false;}
        if(!mmproj.isEmpty()){
            GgufInspector::inspect(mmproj,&inspectError);
            if(!inspectError.isEmpty()){if(error)*error="视觉投影文件无效："+inspectError;return false;}
        }
        const quint64 projector=quint64(qMax<qint64>(0,QFileInfo(mmproj).size()));
        const auto generation=RuntimeSafety::instance().generation();
        QString lastError;
        for(int attempt=0;attempt<3;++attempt){
            if(RuntimeSafety::instance().shouldAbort(generation)){if(error)*error=RuntimeSafety::instance().abortDescription(generation);return false;}
            lastError.clear();
            RuntimeSafety::instance().selectGpu(-1);
            const auto hw=RuntimeSafety::instance().planningSnapshot();
            QString runtime=cfg.value("runtime").toString("cuda13");
            const bool gpuAllowed=runtime.startsWith("cuda") && hw.gpuKnown && footprint.kvKnown && footprint.indexed;
            const AdaptivePlan plan=ResourcePlanner::make(footprint,projector,hw,cfg,roleName,gpuAllowed,attempt);
            Logger::write("AUTO_PLAN",ResourcePlanner::summary(footprint,projector,hw,plan));
            if(!plan.valid){if(error)*error=plan.reason;return false;}
            if(plan.cpuOnly)runtime="cpu";
            const QString relative=m_models.value("runtime").toObject().value("paths").toObject().value(runtime).toString("runtime/Windows/"+runtime);
            const QString runtimeDir=writablePath(relative);
            const QString exe=QDir(runtimeDir).filePath(cfg.value("server_executable").toString("llama-server.exe"));
            if(!QFileInfo::exists(exe)){if(error)*error="安全加载方案需要对应运行时，但文件不存在："+exe;return false;}
            QStringList args{"-m",model,"--host",host,"--port",QString::number(port),"-c",QString::number(plan.context),"-t",QString::number(plan.threads),
                "--n-gpu-layers",QString::number(plan.gpuLayers),"--parallel","1","--batch-size",QString::number(plan.batch),"--ubatch-size",QString::number(plan.microBatch)};
            if(!mmproj.isEmpty())args<<"--mmproj"<<mmproj;
            proc=new QProcess();
            proc->setProperty("config_signature",fileSignature);
            proc->setProperty("model_path",QFileInfo(model).canonicalFilePath());
            proc->setProperty("model_size",QFileInfo(model).size());
            proc->setProperty("model_mtime",QFileInfo(model).lastModified());
            proc->setProperty("server_host",host);proc->setProperty("server_port",port);
            proc->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* a){a->flags|=CREATE_NO_WINDOW;});
            QProcess* logProcess=proc;
            QObject::connect(proc,&QProcess::readyReadStandardError,proc,[logProcess]{
                const QByteArray bytes=logProcess->readAllStandardError();
                logProcess->setProperty("stderr_tail",(logProcess->property("stderr_tail").toByteArray()+bytes).right(32768));
                Logger::write("MODEL_STDERR",QString::fromUtf8(bytes));
            });
            QObject::connect(proc,&QProcess::readyReadStandardOutput,proc,[logProcess]{Logger::write("MODEL_STDOUT",QString::fromUtf8(logProcess->readAllStandardOutput()));});
            proc->setProgram(exe);proc->setArguments(args);proc->setWorkingDirectory(runtimeDir);
            QProcessEnvironment env=QProcessEnvironment::systemEnvironment();env.insert("PATH",runtimeDir+";"+env.value("PATH"));
            if(!plan.cpuOnly)env.insert("CUDA_VISIBLE_DEVICES",hw.gpuUuid.isEmpty()?QString::number(plan.gpuIndex):hw.gpuUuid);
            proc->setProcessEnvironment(env);
            RuntimeSafety::instance().selectGpu(plan.cpuOnly?-2:plan.gpuIndex);
            RuntimeSafety::instance().setEffectiveContext(roleName,plan.context);
            Logger::write("MODEL",QString("第%1/3次自适应启动：").arg(attempt+1)+args.join(' '));
            proc->start();
            if(!proc->waitForStarted(8000)){if(error)*error=proc->errorString();stopProcess(proc);return false;}
            m_job.attach(proc->processId());RuntimeSafety::instance().trackModel(proc->processId());proc->setProperty("owned_pid",proc->processId());
            QElapsedTimer timer;timer.start();bool retry=false;
            const int timeout=qBound(10000,cfg.value("timeout_ms").toInt(120000),180000);
            while(timer.elapsed()<timeout){
                if(RuntimeSafety::instance().shouldAbort(generation)){if(error)*error=RuntimeSafety::instance().abortDescription(generation);stopProcess(proc);return false;}
                QCoreApplication::processEvents();
                if(proc->state()==QProcess::NotRunning){
                    lastError=QString::fromUtf8(proc->property("stderr_tail").toByteArray()+proc->readAllStandardError());
                    const QString lower=lastError.toLower();
                    retry=lower.contains("out of memory")||lower.contains("failed to allocate")||lower.contains("cannot allocate")||lower.contains("cuda error: memory")||lower.contains("buffer allocation failed");
                    break;
                }
                if(httpGet(QUrl(QString("http://%1:%2/health").arg(host).arg(port))).ok){
                    const auto actual=RuntimeSafety::instance().planningSnapshot();
                    const QJsonObject policy=RuntimeSafety::config();
                    const bool ramOver=actual.ramKnown && actual.ramTotal-actual.ramFree>quint64(actual.ramTotal*RuntimeSafety::fraction(policy,"ram_budget_percent",75));
                    const bool gpuOver=!plan.cpuOnly && actual.gpuKnown && actual.gpuTotal-actual.gpuFree>quint64(actual.gpuTotal*RuntimeSafety::fraction(policy,"vram_budget_percent",80));
                    const bool commitOver=actual.commitTotal && actual.commitTotal-actual.commitFree>quint64(actual.commitTotal*RuntimeSafety::fraction(policy,"commit_budget_percent",85));
                    Logger::write("AUTO_ACTUAL","加载后实测："+RuntimeSafety::instance().description());
                    if(!ramOver&&!gpuOver&&!commitOver){if(roleName=="vlm"&&canReuseMultimodal())RuntimeSafety::instance().setEffectiveContext("llm",plan.context);if(error)error->clear();return true;}
                    lastError="实际加载占用超过百分比预算，卸载后降低配置重试";retry=true;break;
                }
                QThread::msleep(200);
            }
            if(lastError.isEmpty())lastError="模型加载超时";
            stopProcess(proc);
            if(!retry || attempt==2){if(error)*error=lastError;return false;}
            Logger::write("AUTO_RETRY",lastError+"；降低上下文及资源预算后重新规划，不无限重启。");
        }
        if(error)*error=lastError;return false;
    }
    QString chat(const QString& roleName, const QJsonArray& messages, QString* error) {
        ModelOperation operation(m_busy, error);
        if (!operation) return {};
        reload();
        const QString p = provider(roleName);
        if (p == "llama_cpp") { if (!ensureReady(roleName, error)) return {}; return openAiChat(roleName, messages, error); }
        if (p == "ollama") return ollamaChat(roleName, messages, error);
        if (p == "online") return openAiChat(roleName, messages, error);
        if (error) *error = "未知Provider：" + p;
        return {};
    }
    QString chatStream(const QString& roleName, const QJsonArray& messages, const std::function<void(const QString&)>& onDelta, QString* error) {
        ModelOperation operation(m_busy, error);
        if (!operation) return {};
        reload();
        const QString p = provider(roleName);
        if (p == "llama_cpp") { if (!ensureReady(roleName, error)) return {}; return openAiChatStream(roleName, messages, onDelta, error); }
        if (p == "online") return openAiChatStream(roleName, messages, onDelta, error);
        if (p == "ollama") return ollamaChatStream(roleName, messages, onDelta, error);
        if (error) *error = "未知Provider：" + p;
        return {};
    }
    QString vision(const QString& imagePath, const QString& prompt, QString* error) {
        ModelOperation operation(m_busy, error);
        if (!operation) return {};
        reload();
        QImageReader reader(imagePath);
        const QSize original = reader.size();
        if (!original.isValid() || qint64(original.width()) * original.height() > 100000000) { if (error) *error = "图片尺寸无效或超过安全上限"; return {}; }
        const int edge = qBound(512, RuntimeSafety::config().value("vision_max_edge").toInt(1280), 2048);
        if (qMax(original.width(), original.height()) > edge) reader.setScaledSize(original.scaled(edge, edge, Qt::KeepAspectRatio));
        QImage image = reader.read();
        if (image.isNull()) { if (error) *error = "图片读取失败：" + reader.errorString(); return {}; }
        if (qMax(image.width(), image.height()) > edge) image = image.scaled(edge, edge, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QByteArray png; QBuffer buffer(&png); buffer.open(QIODevice::WriteOnly);
        if (!image.save(&buffer, "PNG")) { if (error) *error = "图片编码失败"; return {}; }
        const QByteArray base64 = png.toBase64();
        const QString p = provider("vlm");
        if (p == "llama_cpp") { if (!ensureReady("vlm", error)) return {}; return openAiVision(prompt + QString("\n实际传入图像为%1x%2，原图为%3x%4。目标norm_x/norm_y按实际图像归一化，像素坐标请换算回原图尺寸。不要猜测不可见内容。").arg(image.width()).arg(image.height()).arg(original.width()).arg(original.height()), base64, error); }
        if (p == "ollama") return ollamaVision(prompt + QString("\n实际图像%1x%2，原图%3x%4。请输出归一化坐标，像素坐标换算回原图。").arg(image.width()).arg(image.height()).arg(original.width()).arg(original.height()), base64, error);
        if (p == "online") return openAiVision(prompt + QString("\n实际传入图像为%1x%2，原图为%3x%4。目标norm_x/norm_y按实际图像归一化，像素坐标请换算回原图尺寸。不要猜测不可见内容。").arg(image.width()).arg(image.height()).arg(original.width()).arg(original.height()), base64, error);
        if (error) *error = "未知VLM Provider";
        return {};
    }
    void stopAll() { stopProcess(m_llm); stopProcess(m_vlm); RuntimeSafety::instance().selectGpu(-2); }
private:
    bool canReuseMultimodal() const {
        if(!RuntimeSafety::config().value("reuse_multimodal_for_text").toBool(true)||!m_vlm||m_vlm->state()==QProcess::NotRunning||provider("llm")!="llama_cpp")return false;
        const QFileInfo model(role("llm").value("llama_cpp").toObject().value("model").toString());
        return !model.canonicalFilePath().isEmpty()&&model.canonicalFilePath().compare(m_vlm->property("model_path").toString(),Qt::CaseInsensitive)==0
            &&model.size()==m_vlm->property("model_size").toLongLong()&&model.lastModified()==m_vlm->property("model_mtime").toDateTime();
    }
    QString localBase(const QString& roleName,const QJsonObject& cfg) const {
        if(roleName=="llm"&&canReuseMultimodal())return QString("http://%1:%2/v1").arg(m_vlm->property("server_host").toString()).arg(m_vlm->property("server_port").toInt());
        return QString("http://%1:%2/v1").arg(cfg.value("host").toString("127.0.0.1")).arg(cfg.value("port").toInt(roleName=="vlm"?8889:8888));
    }
    QJsonObject m_models;
    QJsonObject m_profile;
    QString m_activeName;
    QProcess* m_llm = nullptr;
    QProcess* m_vlm = nullptr;
    bool m_busy = false;
    ModelJob m_job;
    static void stopProcess(QProcess*& p) {
        if (!p) return;
        RuntimeSafety::instance().untrackModel(p->property("owned_pid").toLongLong());
        if (p->state() != QProcess::NotRunning) { p->terminate(); if (!p->waitForFinished(3000)) { p->kill(); p->waitForFinished(2000); } }
        delete p;
        p = nullptr;
    }
    QString openAiChatStream(const QString& roleName, const QJsonArray& messages, const std::function<void(const QString&)>& onDelta, QString* error) {
        const QJsonObject roleObj = role(roleName);
        const QString p = roleObj.value("provider").toString();
        const QJsonObject cfg = roleObj.value(p).toObject();
        QString base, key, model;
        if (p == "llama_cpp") { base = localBase(roleName,cfg); model = QFileInfo(cfg.value("model").toString()).baseName(); }
        else { base = cfg.value("base_url").toString(); key = cfg.value("api_key").toString(); model = cfg.value("model").toString(); }
        QJsonObject body{{"model", model},{"messages", messages},{"stream", true},{"temperature", cfg.value("temperature").toDouble(0.2)},{"max_tokens", qBound(128, cfg.value("max_tokens").toInt(1024), RuntimeSafety::outputLimit(roleName))}};
        QNetworkAccessManager manager;
        QNetworkRequest request(QUrl(base + "/chat/completions"));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        if (!key.isEmpty()) request.setRawHeader("Authorization", "Bearer " + key.toUtf8());
        QNetworkReply* reply = manager.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
        QEventLoop loop;
        QElapsedTimer requestClock; requestClock.start();
        const auto generation = RuntimeSafety::instance().generation();
        QTimer requestGuard; requestGuard.setInterval(100);
        QString abortReason; QByteArray pending; QString full;
        QObject::connect(&requestGuard, &QTimer::timeout, &loop, [&] {
            if (RuntimeSafety::instance().shouldAbort(generation)) abortReason = RuntimeSafety::instance().abortDescription(generation);
            else if (requestClock.elapsed() >= RuntimeSafety::requestTimeout()) abortReason = "流式请求超过总时限";
            else if (pending.size() > 1024 * 1024 || full.size() > 256000) abortReason = "流式输出超过安全上限";
            if (!abortReason.isEmpty()) { reply->abort(); loop.quit(); }
        });
        requestGuard.start();
        auto consumeLine = [&](QByteArray line) {
            line = line.trimmed();
            if (line.isEmpty()) return;
            if (line.startsWith("data:")) line = line.mid(5).trimmed();
            if (line == "[DONE]") return;
            const QJsonDocument doc = QJsonDocument::fromJson(line);
            if (!doc.isObject()) return;
            const QJsonArray choices = doc.object().value("choices").toArray();
            if (choices.isEmpty()) return;
            const QJsonObject choice = choices.first().toObject();
            QString delta = choice.value("delta").toObject().value("content").toString();
            if (delta.isEmpty()) delta = choice.value("message").toObject().value("content").toString();
            if (!delta.isEmpty()) { full += delta; if (onDelta) onDelta(delta); }
        };
        QObject::connect(reply, &QNetworkReply::readyRead, &loop, [&]() {
            pending += reply->readAll();
            int nl = -1;
            while ((nl = pending.indexOf('\n')) >= 0) { QByteArray line = pending.left(nl); pending.remove(0, nl + 1); consumeLine(line); }
        });
        QObject::connect(reply, &QNetworkReply::finished, &loop, [&]() { pending += reply->readAll(); if (!pending.trimmed().isEmpty()) consumeLine(pending); loop.quit(); });
        if (!reply->isFinished()) loop.exec();
        requestGuard.stop();
        if (!abortReason.isEmpty()) { if (error) *error = abortReason; reply->deleteLater(); return {}; }
        if (reply->error() != QNetworkReply::NoError) { if (error) *error = reply->errorString(); reply->deleteLater(); return {}; }
        reply->deleteLater();
        return full;
    }
    QString ollamaChatStream(const QString& roleName, const QJsonArray& messages, const std::function<void(const QString&)>& onDelta, QString* error) {
        const QJsonObject cfg = role(roleName).value("ollama").toObject();
        QJsonObject body{{"model", cfg.value("model").toString()},{"messages", messages},{"stream", true},{"options", QJsonObject{{"temperature", cfg.value("temperature").toDouble(0.2)},{"num_ctx", qBound(2048, cfg.value("context").toInt(8192), RuntimeSafety::contextLimit(roleName))},{"num_predict", RuntimeSafety::outputLimit(roleName)}}}};
        QNetworkAccessManager manager;
        QNetworkRequest request(QUrl(cfg.value("base_url").toString("http://127.0.0.1:11434") + "/api/chat"));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        QNetworkReply* reply = manager.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
        QEventLoop loop; QElapsedTimer requestClock; requestClock.start();
        const auto generation = RuntimeSafety::instance().generation();
        QTimer requestGuard; requestGuard.setInterval(100);
        QString abortReason; QByteArray pending; QString full;
        QObject::connect(&requestGuard, &QTimer::timeout, &loop, [&] {
            if (RuntimeSafety::instance().shouldAbort(generation)) abortReason = RuntimeSafety::instance().abortDescription(generation);
            else if (requestClock.elapsed() >= RuntimeSafety::requestTimeout()) abortReason = "流式请求超过总时限";
            else if (pending.size() > 1024 * 1024 || full.size() > 256000) abortReason = "流式输出超过安全上限";
            if (!abortReason.isEmpty()) { reply->abort(); loop.quit(); }
        });
        requestGuard.start();
        auto consumeLine = [&](const QByteArray& lineRaw) {
            const QByteArray line = lineRaw.trimmed();
            if (line.isEmpty()) return;
            const QJsonDocument doc = QJsonDocument::fromJson(line);
            if (!doc.isObject()) return;
            const QString delta = doc.object().value("message").toObject().value("content").toString();
            if (!delta.isEmpty()) { full += delta; if (onDelta) onDelta(delta); }
        };
        QObject::connect(reply, &QNetworkReply::readyRead, &loop, [&]() {
            pending += reply->readAll();
            int nl = -1;
            while ((nl = pending.indexOf('\n')) >= 0) { const QByteArray line = pending.left(nl); pending.remove(0, nl + 1); consumeLine(line); }
        });
        QObject::connect(reply, &QNetworkReply::finished, &loop, [&]() { pending += reply->readAll(); if (!pending.trimmed().isEmpty()) consumeLine(pending); loop.quit(); });
        if (!reply->isFinished()) loop.exec();
        requestGuard.stop();
        if (!abortReason.isEmpty()) { if (error) *error = abortReason; reply->deleteLater(); return {}; }
        if (reply->error() != QNetworkReply::NoError) { if (error) *error = reply->errorString(); reply->deleteLater(); return {}; }
        reply->deleteLater();
        return full;
    }
    QString openAiChat(const QString& roleName, const QJsonArray& messages, QString* error) {
        const QJsonObject roleObj = role(roleName);
        const QString p = roleObj.value("provider").toString();
        const QJsonObject cfg = roleObj.value(p).toObject();
        QString base, key, model;
        if (p == "llama_cpp") { base = localBase(roleName,cfg); model = QFileInfo(cfg.value("model").toString()).baseName(); }
        else { base = cfg.value("base_url").toString(); key = cfg.value("api_key").toString(); model = cfg.value("model").toString(); }
        QJsonObject body{{"model", model},{"messages", messages},{"stream", false},{"temperature", cfg.value("temperature").toDouble(0.2)},{"max_tokens", qBound(128, cfg.value("max_tokens").toInt(1024), RuntimeSafety::outputLimit(roleName))}};
        if(p=="llama_cpp")body["chat_template_kwargs"]=QJsonObject{{"enable_thinking",false}};
        QMap<QByteArray,QByteArray> headers;
        if (!key.isEmpty()) headers.insert("Authorization", "Bearer " + key.toUtf8());
        const HttpResult r = httpPostJson(QUrl(base + "/chat/completions"), body, headers);
        if (!r.ok) { if (error) *error = r.error; return {}; }
        const QJsonDocument doc = QJsonDocument::fromJson(r.raw);
        const QJsonArray choices = doc.object().value("choices").toArray();
        if (choices.isEmpty()) { if (error) *error = "模型没有返回choices"; return {}; }
        return choices.first().toObject().value("message").toObject().value("content").toString();
    }
    QString ollamaChat(const QString& roleName, const QJsonArray& messages, QString* error) {
        const QJsonObject cfg = role(roleName).value("ollama").toObject();
        QJsonObject body{{"model", cfg.value("model").toString()},{"messages", messages},{"stream", false},{"options", QJsonObject{{"temperature", cfg.value("temperature").toDouble(0.2)},{"num_ctx", qBound(2048, cfg.value("context").toInt(8192), RuntimeSafety::contextLimit(roleName))},{"num_predict", RuntimeSafety::outputLimit(roleName)}}}};
        const HttpResult r = httpPostJson(QUrl(cfg.value("base_url").toString("http://127.0.0.1:11434") + "/api/chat"), body);
        if (!r.ok) { if (error) *error = r.error; return {}; }
        return QJsonDocument::fromJson(r.raw).object().value("message").toObject().value("content").toString();
    }
    QString openAiVision(const QString& prompt, const QByteArray& base64, QString* error) {
        const QJsonObject roleObj = role("vlm");
        const QString p = roleObj.value("provider").toString();
        const QJsonObject cfg = roleObj.value(p).toObject();
        QString base, key, model;
        if (p == "llama_cpp") { base = QString("http://%1:%2/v1").arg(cfg.value("host").toString("127.0.0.1")).arg(cfg.value("port").toInt(8889)); model = QFileInfo(cfg.value("model").toString()).baseName(); }
        else { base = cfg.value("base_url").toString(); key = cfg.value("api_key").toString(); model = cfg.value("model").toString(); }
        QJsonArray content{QJsonObject{{"type","text"},{"text",prompt}},QJsonObject{{"type","image_url"},{"image_url",QJsonObject{{"url","data:image/png;base64," + QString::fromLatin1(base64)}}}}};
        QJsonArray messages{QJsonObject{{"role","user"},{"content",content}}};
        QJsonObject body{{"model",model},{"messages",messages},{"stream",false},{"temperature",cfg.value("temperature").toDouble(0.1)},{"max_tokens",qBound(128, cfg.value("max_tokens").toInt(768), RuntimeSafety::outputLimit("vlm"))}};
        const bool internal=prompt.contains("VISION_MODE=AGENT_");
        if(internal&&cfg.value("structured_vision").toBool(true))body["response_format"]=QJsonObject{{"type","json_object"}};
        if(p=="llama_cpp")body["chat_template_kwargs"]=QJsonObject{{"enable_thinking",false}};
        QMap<QByteArray,QByteArray> headers;
        if (!key.isEmpty()) headers.insert("Authorization","Bearer " + key.toUtf8());
        HttpResult r = httpPostJson(QUrl(base + "/chat/completions"), body, headers);
        if(!r.ok&&internal&&r.code==400&&(r.error.contains("response_format",Qt::CaseInsensitive)||r.error.contains("grammar",Qt::CaseInsensitive)||r.error.contains("chat_template_kwargs",Qt::CaseInsensitive))){
            body.remove("response_format");body.remove("chat_template_kwargs");Logger::write("VISION_COMPAT","服务拒绝可选结构化参数，使用短JSON协议重试一次");r=httpPostJson(QUrl(base+"/chat/completions"),body,headers);
        }
        if (!r.ok) { if (error) *error = r.error; return {}; }
        const auto first=QJsonDocument::fromJson(r.raw).object().value("choices").toArray();
        if(internal&&!first.isEmpty()&&first.first().toObject().value("finish_reason")=="length"){
            const int retryTokens=qMin(2048,RuntimeSafety::contextLimit("vlm")/3);
            if(retryTokens>body.value("max_tokens").toInt()){ body["max_tokens"]=retryTokens;Logger::write("VISION_RETRY","输出截断，按当前上下文预算增加视觉输出并重试一次");r=httpPostJson(QUrl(base+"/chat/completions"),body,headers);if(!r.ok){if(error)*error=r.error;return {};} }
        }
        const QJsonObject payload=QJsonDocument::fromJson(r.raw).object();
        const QJsonArray choices=payload.value("choices").toArray();
        if(choices.isEmpty()){if(error)*error="视觉服务没有返回choices";return {};}
        const auto choice=choices.first().toObject();const QString finish=choice.value("finish_reason").toString();
        Logger::write("VISION_USAGE",QString::fromUtf8(QJsonDocument(payload.value("usage").toObject()).toJson(QJsonDocument::Compact))+" finish_reason="+finish);
        if(finish=="length"){if(error)*error="视觉输出达到长度上限，结果不完整；应缩短观察目标或调整视觉输出预算";return {};}
        const QString responseContent=choice.value("message").toObject().value("content").toString();
        if(responseContent.trimmed().isEmpty()&&error)*error="视觉服务返回空content";
        return responseContent;
    }
    QString ollamaVision(const QString& prompt, const QByteArray& base64, QString* error) {
        const QJsonObject cfg = role("vlm").value("ollama").toObject();
        QJsonArray messages{QJsonObject{{"role","user"},{"content",prompt},{"images",QJsonArray{QString::fromLatin1(base64)}}}};
        QJsonObject body{{"model",cfg.value("model").toString()},{"messages",messages},{"stream",false},{"keep_alive",0},{"options", QJsonObject{{"num_ctx",RuntimeSafety::contextLimit("vlm")},{"num_predict",RuntimeSafety::outputLimit("vlm")}}}};
        const HttpResult r = httpPostJson(QUrl(cfg.value("base_url").toString("http://127.0.0.1:11434") + "/api/chat"), body);
        if (!r.ok) { if (error) *error = r.error; return {}; }
        return QJsonDocument::fromJson(r.raw).object().value("message").toObject().value("content").toString();
    }
};
enum class CommandRisk { Safe, Confirm, Forbidden };

static CommandRisk commandRisk(const QString& command) {
    const QJsonObject cfg = loadConfig("assets/config/permissions.json");
    const QString lower = command.toLower();
    for (const QJsonValue& v : cfg.value("forbidden_contains").toArray()) if (lower.contains(v.toString().toLower())) return CommandRisk::Forbidden;
    for (const QJsonValue& v : cfg.value("confirm_contains").toArray()) if (lower.contains(v.toString().toLower())) return CommandRisk::Confirm;
    for (const QJsonValue& v : cfg.value("safe_prefixes").toArray()) if (lower.startsWith(v.toString().toLower())) return CommandRisk::Safe;
    return CommandRisk::Confirm;
}

static QString execProcess(const QString& program, const QStringList& args, int timeoutMs, bool* ok) {
    QProcess p;
    p.setProgram(program); p.setArguments(args); p.start();
    if (!p.waitForStarted(5000)) { if (ok) *ok = false; return "启动失败：" + p.errorString(); }
    QElapsedTimer timer; timer.start();
    while (p.state() != QProcess::NotRunning && timer.elapsed() < timeoutMs) {
        if (RuntimeSafety::instance().shouldAbort(RuntimeSafety::instance().generation())) { p.kill(); p.waitForFinished(1500); if (ok) *ok = false; return "命令已取消或资源保护已触发"; }
        p.waitForFinished(40);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 20);
    }
    if (p.state() != QProcess::NotRunning) { p.kill(); p.waitForFinished(1500); if (ok) *ok = false; return "命令执行超时"; }
    if (ok) *ok = p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
    QString text = QString::fromLocal8Bit(p.readAllStandardOutput());
    const QString err = QString::fromLocal8Bit(p.readAllStandardError());
    if (!err.isEmpty()) text += "\n[STDERR]\n" + err;
    return text;
}

static bool customConfirm(QWidget* parent, const QString& title, const QString& body) {
    QDialog d(parent);
    d.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    d.resize(420,220);
    d.setStyleSheet("QDialog{background:#212121;color:#fff;border:1px solid #3c3c3c;}" "QLabel{color:#fff;font-size:12px;}" "QPushButton{min-height:32px;background:#363636;color:#fff;border:0;border-radius:5px;padding:0 18px;}" "QPushButton#yes{background:#fb6a22;}");
    auto* root = new QVBoxLayout(&d);
    root->setContentsMargins(10,10,10,10); root->setSpacing(10);
    auto* t = new QLabel(title);
    QFont tf(g_lightFontFamily); tf.setPixelSize(16); tf.setWeight(QFont::Normal); t->setFont(tf);
    auto* b = new QLabel(body); b->setWordWrap(true);
    auto* row = new QHBoxLayout(); row->addStretch();
    auto* no = new QPushButton("取消");
    auto* yes = new QPushButton("继续执行"); yes->setObjectName("yes");
    row->addWidget(no); row->addWidget(yes);
    root->addWidget(t); root->addWidget(b,1); root->addLayout(row);
    bool accepted = false;
    QObject::connect(no,&QPushButton::clicked,&d,&QDialog::reject);
    QObject::connect(yes,&QPushButton::clicked,[&](){accepted=true;d.accept();});
    d.exec();
    return accepted;
}

static bool confirmExitProgram(QWidget* parent) {
    QDialog dialog(parent);
    dialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dialog.setModal(true); dialog.setObjectName("exitDialog"); dialog.setFixedSize(420, 220);
    auto* root = new QVBoxLayout(&dialog);
    root->setContentsMargins(10, 10, 10, 10); root->setSpacing(0);
    auto* top = new QWidget(); top->setObjectName("exitTopBar"); top->setFixedHeight(42);
    auto* topLayout = new QHBoxLayout(top);
    topLayout->setContentsMargins(8, 0, 4, 0); topLayout->setSpacing(8);
    auto* title = new QLabel("退出当前软件"); title->setObjectName("exitTitle");
    QFont titleFont(g_boldFontFamily); titleFont.setPixelSize(14); title->setFont(titleFont);
    auto* close = new QPushButton(); close->setObjectName("exitClose"); close->setFixedSize(20, 20);
    topLayout->addWidget(title); topLayout->addStretch(); topLayout->addWidget(close);
    root->addWidget(top);
    auto* body = new QWidget(); body->setObjectName("exitBody");
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(10, 12, 10, 10); bodyLayout->setSpacing(10);
    auto* description = new QLabel("确定关闭“木头自动系统控制”？\n" "程序会按顺序停止当前任务、模型服务、数据库、系统托盘，" "最后退出当前软件。");
    description->setObjectName("exitDescription"); description->setWordWrap(true);
    bodyLayout->addWidget(description); bodyLayout->addStretch();
    auto* buttonRow = new QHBoxLayout();
    buttonRow->setContentsMargins(0, 0, 0, 0); buttonRow->addStretch();
    auto* continueExit = new QPushButton("继续退出"); continueExit->setObjectName("continueExitButton"); continueExit->setFixedSize(92, 36);
    buttonRow->addWidget(continueExit);
    bodyLayout->addLayout(buttonRow);
    root->addWidget(body, 1);
    bool accepted = false;
    QObject::connect(close, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(continueExit, &QPushButton::clicked, [&]() { accepted = true; dialog.accept(); });
    dialog.exec();
    return accepted;
}

struct JsonFieldBinding { QString path; QJsonValue originalValue; QWidget* editor = nullptr; };

static QString jsonKeyChinese(const QString& key) {
    static const QMap<QString, QString> names = {
        {"app", "软件信息"},{"window", "窗口设置"},{"paths", "路径设置"},{"colors", "颜色设置"},{"theme", "主题设置"},
        {"app_user_model_id", "应用模型 ID"},{"appusermodelid", "应用模型 ID"},{"organization", "开发单位"},{"company", "公司名称"},{"description", "软件说明"},{"product_name", "产品名称"},{"product", "产品名称"},
        {"name", "名称"},{"version", "版本"},{"language", "语言"},{"width", "窗口宽度"},{"height", "窗口高度"},{"min_width", "最小宽度"},{"min_height", "最小高度"},{"start_hidden", "启动时隐藏"},{"allow_maximize", "允许最大化"},{"always_on_top", "窗口始终置顶"},{"close_button_exit_program", "关闭按钮退出程序"},{"frameless", "无边框窗口"},{"hide_to_tray_on_green_button", "绿色按钮隐藏到托盘"},{"remember_position", "记住窗口位置"},{"resizable", "允许调整窗口大小"},{"save_position", "保存窗口位置"},{"save_window_position", "保存窗口位置"},
        {"auto_database", "自动创建数据库"},{"auto_db", "自动创建数据库"},{"auto_log", "自动创建日志目录"},{"auto_logs", "自动创建日志目录"},{"auto_assets", "自动同步资源目录"},{"auto_copy_assets", "自动同步资源目录"},{"auto_config", "自动创建配置文件"},{"auto_configs", "自动创建配置文件"},{"auto_prompt", "自动创建提示词文件"},{"auto_prompts", "自动创建提示词文件"},{"auto_runtime", "自动检查运行时"},{"auto_model", "自动加载模型"},{"auto_load_model", "自动加载模型"},{"auto_database_init", "自动初始化数据库"},
        {"title_separator", "标题分隔线颜色"},{"warning", "警告颜色"},{"background", "背景颜色"},{"panel", "面板颜色"},{"text", "文字颜色"},{"secondary_text", "次要文字颜色"},{"success", "成功颜色"},{"error", "错误颜色"},{"primary", "主色"},{"accent", "强调色"},
        {"database", "数据库文件"},{"database_schema", "数据库结构文件"},{"style", "样式文件"},{"upload", "上传目录"},{"log", "日志目录"},
        {"max_steps", "最大执行步骤"},{"max_retries_per_step", "单步最大重试次数"},{"step_timeout_ms", "步骤超时（毫秒）"},{"tool_timeout_ms", "工具超时（毫秒）"},{"verify_after_action", "动作后自动验证"},{"stop_on_user_input", "用户输入时停止自动控制"},{"vision_fallback", "启用视觉回退"},{"coordinate_fallback", "启用坐标回退"},{"step_delay_ms", "步骤延迟（毫秒）"},
        {"desktop_capture", "启用桌面截图"},{"capture_cursor_monitor", "截取鼠标所在显示器"},{"capture_virtual_desktop", "截取完整虚拟桌面"},{"save_screenshot", "自动保存截图"},{"format", "截图格式"},{"vlm_max_width", "视觉输入最大宽度"},{"vlm_max_height", "视觉输入最大高度"},
        {"resource_safety","资源预算与保护"},{"auto_allocate","自动分配资源"},
        {"ram_budget_percent","全机内存占用预算（%）"},{"vram_budget_percent","所选显卡显存占用预算（%）"},
        {"commit_budget_percent","系统提交内存预算（%）"},{"cpu_thread_percent","CPU推理线程比例（%）"},
        {"ram_emergency_percent","全机内存熔断占用（%）"},{"vram_emergency_percent","显存熔断占用（%）"},
        {"commit_emergency_percent","提交内存熔断占用（%）"},{"single_model_resident","单模型按需驻留"},
        {"llm_target_context","语言模型目标上下文"},{"vlm_target_context","视觉模型目标上下文"},
        {"active_profile", "当前模型配置方案"},{"profiles", "模型配置方案"},{"runtime", "运行时"},{"auto_select", "自动选择运行时"},{"priority", "运行时优先级"},{"llm", "语言模型"},{"vlm", "视觉模型"},{"verifier", "结果验证器"},{"provider", "模型提供方式"},{"enabled", "启用"},{"llama_cpp", "llama.cpp 本地服务"},{"ollama", "Ollama 本地服务"},{"online", "在线 API"},{"model", "模型文件 / 模型名称"},{"mmproj", "视觉 mmproj 文件"},{"server_executable", "模型服务程序"},{"host", "服务主机地址"},{"port", "服务端口"},{"context", "上下文长度"},{"gpu_layers", "GPU 加载层数"},{"threads", "CPU 线程数"},{"temperature", "生成温度"},{"max_tokens", "最大输出 Token"},{"timeout_ms", "超时（毫秒）"},{"base_url", "API 地址"},{"api_key", "API Key"},{"role", "模型角色"},
        {"default_policy", "默认权限策略"},{"safe_prefixes", "安全命令前缀"},{"confirm_contains", "执行前需要确认的命令"},{"forbidden_contains", "禁止执行的命令"},
        {"tools", "工具列表"},{"risk", "风险等级"},{"work_mode", "工作模式"},{"control_scope", "控制范围"},{"desktop_control", "桌面控制"},{"window_control", "指定窗口控制"},{"region_selector", "截图选区"},{"uia_refresh_interval_ms", "UIA刷新间隔（毫秒）"},{"window_rect_refresh_interval_ms", "窗口位置刷新间隔（毫秒）"},{"overlay_refresh_interval_ms", "覆盖层刷新间隔（毫秒）"},{"restore_last_control_mode_on_startup", "启动时恢复上次控制模式"},{"restrict_to_selected_hwnd", "限制为指定HWND"},{"show_overlay", "显示覆盖层"},{"show_clickable_only", "只显示可操作控件"},{"vlm_refresh_mode", "VLM刷新模式"},{"stop_when_window_closed", "目标窗口关闭时停止"},{"hide_internal_json_from_chat", "聊天隐藏内部JSON"},{"write_internal_json_to_log", "内部JSON写入日志"},{"border_color", "边框颜色"},{"border_width", "边框宽度"},{"fill_color", "填充颜色"},{"fill_opacity", "填充透明度"},{"dim_background", "压暗选区外背景"},{"dim_opacity", "背景压暗透明度"},{"window_glow_enabled", "指定窗口绿色闪烁"},{"window_glow_color", "指定窗口闪烁颜色"},{"window_glow_opacity", "指定窗口闪烁透明度"},{"window_glow_pulse_ms", "指定窗口闪烁周期（毫秒）"}
    };
    const QString lowerKey = key.toLower();
    if (names.contains(lowerKey)) return names.value(lowerKey);
    static const QMap<QString, QString> tokenMap = {
        {"auto", "自动"},{"create", "创建"},{"copy", "复制"},{"sync", "同步"},{"save", "保存"},{"load", "加载"},{"check", "检查"},{"enable", "启用"},{"enabled", "启用"},{"allow", "允许"},{"always", "始终"},{"default", "默认"},{"max", "最大"},{"min", "最小"},{"window", "窗口"},{"button", "按钮"},{"close", "关闭"},{"exit", "退出"},{"program", "程序"},{"show", "显示"},{"hide", "隐藏"},{"tray", "托盘"},{"title", "标题"},{"separator", "分隔线"},{"color", "颜色"},{"background", "背景"},{"width", "宽度"},{"height", "高度"},{"path", "路径"},{"file", "文件"},{"files", "文件"},{"model", "模型"},{"server", "服务"},{"timeout", "超时"},{"step", "步骤"},{"retry", "重试"},{"capture", "截图"},{"screen", "屏幕"},{"desktop", "桌面"},{"mouse", "鼠标"},{"keyboard", "键盘"},{"input", "输入"},{"output", "输出"},{"remember", "记住"},{"position", "位置"},{"frame", "边框"},{"frameless", "无边框"},{"warning", "警告"},{"error", "错误"},{"success", "成功"},{"text", "文字"},{"font", "字体"},{"size", "大小"},{"language", "语言"},{"log", "日志"},{"logs", "日志"},{"database", "数据库"},{"db", "数据库"},{"camera", "相机"},{"image", "图像"},{"vision", "视觉"},{"agent", "智能体"},{"permission", "权限"},{"tool", "工具"},{"risk", "风险"},{"config", "配置"},{"configs", "配置"},{"prompt", "提示词"},{"prompts", "提示词"},{"runtime", "运行时"},{"assets", "资源"},{"organization", "开发单位"},{"company", "公司"},{"description", "说明"},{"id", "ID"}
    };
    const QStringList tokens = lowerKey.split('_', Qt::SkipEmptyParts);
    QString result;
    for (const QString& token : tokens) { const QString translated = tokenMap.value(token); if (!translated.isEmpty()) result += translated; }
    if (!result.isEmpty()) return result;
    return "其他设置";
}

static QString jsonSectionChinese(const QString& key) { return jsonKeyChinese(key); }

static QStringList enumChoicesForPath(const QString& path) {
    if (path.endsWith(".provider") || path == "provider") return {"llama_cpp", "ollama", "online", "same_as_llm"};
    if (path.endsWith(".runtime") || path == "runtime") return {"cuda13", "vulkan", "cpu"};
    if (path.endsWith(".default_policy") || path == "default_policy") return {"confirm", "safe", "forbidden"};
    if (path.endsWith(".risk") || path == "risk") return {"safe", "confirm", "forbidden"};
    if (path.endsWith(".format") || path == "format") return {"png", "jpg", "jpeg", "bmp", "webp"};
    if (path.endsWith(".language") || path == "language") return {"zh-CN", "en-US"};
    return {};
}

static QJsonValue setJsonValueByPath(const QJsonValue& root, const QStringList& parts, int index, const QJsonValue& value) {
    if (index >= parts.size()) return value;
    QJsonObject obj = root.isObject() ? root.toObject() : QJsonObject{};
    const QString key = parts.at(index);
    const QJsonValue child = obj.value(key);
    obj.insert(key, setJsonValueByPath(child, parts, index + 1, value));
    return obj;
}

class JsonSettingsPage : public QWidget {
public:
    JsonSettingsPage(const QString& fileRelative, QWidget* parent = nullptr) : QWidget(parent), m_fileRelative(fileRelative) {
        m_json = loadConfig(fileRelative);
        auto* root = new QVBoxLayout(this); root->setContentsMargins(0, 0, 0, 0); root->setSpacing(0);
        auto* scroll = new QScrollArea();
        scroll->setObjectName("settingsScroll"); scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); scroll->setFrameShape(QFrame::NoFrame);
        auto* content = new QWidget(); content->setObjectName("settingsPageContent");
        m_layout = new QVBoxLayout(content);
        m_layout->setContentsMargins(0, 10, 0, 14); m_layout->setSpacing(10);
        buildObject(m_json, QString()); m_layout->addStretch(1);
        scroll->setWidget(content); root->addWidget(scroll);
    }
    bool save(QString* error = nullptr) {
        QJsonObject updated = m_json;
        for (const JsonFieldBinding& binding : m_bindings) {
            bool ok = true;
            const QJsonValue value = editorValue(binding, &ok);
            if (!ok) { if (error) *error = QString("设置项“%1”输入格式不正确。").arg(binding.editor ? binding.editor->property("ceh_title").toString() : QString()); return false; }
            const QStringList parts = binding.path.split('.', Qt::SkipEmptyParts);
            const QJsonValue result = setJsonValueByPath(updated, parts, 0, value);
            if (result.isObject()) updated = result.toObject();
        }
        const QString path = writablePath(m_fileRelative);
        if (!writeJson(path, updated)) { if (error) *error = "无法保存：" + path; return false; }
        m_json = updated; return true;
    }
private:
    QString m_fileRelative; QJsonObject m_json; QVBoxLayout* m_layout = nullptr; QVector<JsonFieldBinding> m_bindings;
    void addSection(const QString& key) {
        auto* label = new QLabel(jsonSectionChinese(key));
        label->setObjectName("settingsSection"); label->setFont(QFont(g_lightFontFamily));
        m_layout->addWidget(label);
    }
    void addSingleLineCard(const QString& title, QWidget* editor) {
        auto* card = new QWidget(); card->setObjectName("settingsCard"); card->setMinimumHeight(50); card->setMaximumHeight(50);
        auto* row = new QHBoxLayout(card); row->setContentsMargins(14, 0, 12, 0); row->setSpacing(12);
        auto* label = new QLabel(title);
        label->setObjectName("settingsFieldLabel"); label->setFont(QFont(g_lightFontFamily));
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); label->setWordWrap(false);
        label->setMinimumWidth(138); label->setMaximumWidth(170);
        editor->setProperty("ceh_title", title);
        row->addWidget(label, 0); row->addWidget(editor, 1);
        m_layout->addWidget(card);
    }
    void addMultiLineCard(const QString& title, QWidget* editor) {
        auto* card = new QWidget(); card->setObjectName("settingsCardLarge");
        auto* column = new QVBoxLayout(card); column->setContentsMargins(14, 10, 14, 12); column->setSpacing(8);
        auto* label = new QLabel(title);
        label->setObjectName("settingsFieldLabelLarge"); label->setFont(QFont(g_lightFontFamily));
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); label->setWordWrap(false);
        editor->setProperty("ceh_title", title);
        column->addWidget(label); column->addWidget(editor, 1);
        m_layout->addWidget(card);
    }
    void buildObject(const QJsonObject& object, const QString& prefix) {
        for (auto it = object.begin(); it != object.end(); ++it) {
            const QString key = it.key(); const QJsonValue value = it.value();
            const QString path = prefix.isEmpty() ? key : prefix + "." + key;
            if (value.isObject()) { addSection(key); buildObject(value.toObject(), path); continue; }
            QWidget* editor = makeEditor(path, value);
            if (!editor) continue;
            const QString title = jsonKeyChinese(key);
            if (value.isArray()) addMultiLineCard(title, editor); else addSingleLineCard(title, editor);
            m_bindings.push_back(JsonFieldBinding{path, value, editor});
        }
    }
    QWidget* makeEditor(const QString& path, const QJsonValue& value) {
        if (value.isBool()) {
            auto* combo = new QComboBox();
            combo->setObjectName("settingsCombo"); combo->setFont(QFont(g_lightFontFamily));
            combo->addItem("开启", true); combo->addItem("关闭", false);
            combo->setCurrentIndex(value.toBool() ? 0 : 1); return combo;
        }
        if (value.isDouble()) {
            const double number = value.toDouble();
            const bool integerLike = qAbs(number - qRound64(number)) < 0.0000001;
            auto* edit = new QLineEdit();
            edit->setObjectName(integerLike ? "settingsIntegerEdit" : "settingsNumberEdit");
            QFont valueFont("Microsoft YaHei UI"); valueFont.setPixelSize(12); edit->setFont(valueFont);
            if (integerLike) {
                edit->setText(QString::number(static_cast<qint64>(qRound64(number))));
                auto* validator = new QIntValidator(-1000000000, 1000000000, edit);
                edit->setValidator(validator); edit->setProperty("ceh_number_type", "int");
            } else {
                edit->setText(QString::number(number, 'f', 6).remove(QRegularExpression("0+$")).remove(QRegularExpression("\\.$")));
                auto* validator = new QDoubleValidator(-1000000.0, 1000000.0, 6, edit);
                validator->setNotation(QDoubleValidator::StandardNotation);
                edit->setValidator(validator); edit->setProperty("ceh_number_type", "double");
            }
            return edit;
        }
        if (value.isArray()) {
            auto* edit = new QTextEdit();
            edit->setObjectName("settingsArrayEdit"); edit->setAcceptRichText(false);
            edit->setMinimumHeight(150); edit->setFont(QFont("Microsoft YaHei UI"));
            const QJsonArray array = value.toArray(); bool allStrings = true;
            for (const QJsonValue& item : array) { if (!item.isString()) { allStrings = false; break; } }
            if (allStrings) {
                QStringList lines; for (const QJsonValue& item : array) lines << item.toString();
                edit->setPlainText(lines.join('\n')); edit->setProperty("arrayMode", "strings");
            } else {
                edit->setPlainText(QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Indented)));
                edit->setProperty("arrayMode", "json");
            }
            return edit;
        }
        if (value.isString()) {
            const QStringList choices = enumChoicesForPath(path);
            if (!choices.isEmpty()) {
                auto* combo = new QComboBox();
                combo->setObjectName("settingsCombo");
                QFont valueFont("Microsoft YaHei UI"); valueFont.setPixelSize(12); combo->setFont(valueFont);
                combo->addItems(choices);
                const QString current = value.toString();
                if (!current.isEmpty() && combo->findText(current) < 0) combo->addItem(current);
                combo->setCurrentText(current); return combo;
            }
            auto* edit = new QLineEdit(value.toString());
            edit->setObjectName("settingsLineEdit");
            QFont valueFont("Microsoft YaHei UI"); valueFont.setPixelSize(12); edit->setFont(valueFont);
            if (path.endsWith(".api_key") || path == "api_key") edit->setEchoMode(QLineEdit::PasswordEchoOnEdit);
            return edit;
        }
        return nullptr;
    }
    QJsonValue editorValue(const JsonFieldBinding& binding, bool* ok) const {
        if (ok) *ok = true;
        if (auto* combo = qobject_cast<QComboBox*>(binding.editor)) {
            if (binding.originalValue.isBool()) return combo->currentData().toBool();
            return combo->currentText();
        }
        if (auto* line = qobject_cast<QLineEdit*>(binding.editor)) {
            const QString numberType = line->property("ceh_number_type").toString();
            if (numberType == "int") { bool localOk = false; const qlonglong value = line->text().trimmed().toLongLong(&localOk); if (ok) *ok = localOk; return static_cast<double>(value); }
            if (numberType == "double") { bool localOk = false; const double value = line->text().trimmed().toDouble(&localOk); if (ok) *ok = localOk; return value; }
            return line->text();
        }
        if (auto* edit = qobject_cast<QTextEdit*>(binding.editor)) {
            const QString mode = edit->property("arrayMode").toString();
            if (mode == "strings") {
                QJsonArray array;
                const QStringList lines = edit->toPlainText().split('\n', Qt::SkipEmptyParts);
                for (const QString& line : lines) array.append(line.trimmed());
                return array;
            }
            if (mode == "json") {
                QJsonParseError parseError;
                const QJsonDocument doc = QJsonDocument::fromJson(edit->toPlainText().toUtf8(), &parseError);
                if (parseError.error != QJsonParseError::NoError || !doc.isArray()) { if (ok) *ok = false; return binding.originalValue; }
                return doc.array();
            }
        }
        return binding.originalValue;
    }
};

class SettingsDialog : public QDialog {
public:
    explicit SettingsDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        setModal(true); setObjectName("settingsDialog");
        const int targetWidth = parent ? parent->width() : 500;
        resize(targetWidth, 760); setFixedWidth(targetWidth); setMinimumHeight(620);
        buildUi();
    }
protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == m_topBar) {
            if (event->type() == QEvent::MouseButtonPress) {
                auto* mouse = static_cast<QMouseEvent*>(event);
                if (mouse->button() == Qt::LeftButton) { m_dragging = true; m_dragOffset = mouse->globalPosition().toPoint() - frameGeometry().topLeft(); return true; }
            } else if (event->type() == QEvent::MouseMove && m_dragging) { auto* mouse = static_cast<QMouseEvent*>(event); move(mouse->globalPosition().toPoint() - m_dragOffset); return true; }
            else if (event->type() == QEvent::MouseButtonRelease) { m_dragging = false; return true; }
        }
        return QDialog::eventFilter(watched, event);
    }
private:
    QWidget* m_topBar = nullptr; QTabBar* m_tabs = nullptr; QStackedWidget* m_stack = nullptr;
    QVector<JsonSettingsPage*> m_pages; bool m_dragging = false; QPoint m_dragOffset;
    void buildUi() {
        auto* root = new QVBoxLayout(this); root->setContentsMargins(10, 10, 10, 10); root->setSpacing(0);
        m_topBar = new QWidget(); m_topBar->setObjectName("settingsTopBar"); m_topBar->setFixedHeight(42); m_topBar->installEventFilter(this);
        auto* topLayout = new QHBoxLayout(m_topBar); topLayout->setContentsMargins(8, 0, 0, 0); topLayout->setSpacing(8);
        auto* title = new QLabel("系统设置"); title->setObjectName("settingsTitle");
        QFont titleFont(g_boldFontFamily); titleFont.setPixelSize(14); title->setFont(titleFont);
        auto* close = new QPushButton(); close->setObjectName("settingsClose"); close->setFixedSize(20, 20);
        topLayout->addWidget(title); topLayout->addStretch(); topLayout->addWidget(close);
        root->addWidget(m_topBar);
        m_tabs = new QTabBar(); m_tabs->setObjectName("settingsTabs");
        QFont tabsFont(g_boldFontFamily); tabsFont.setPixelSize(11); m_tabs->setFont(tabsFont);
        m_tabs->setMovable(true); m_tabs->setExpanding(true); m_tabs->setDrawBase(false);
        m_tabs->setUsesScrollButtons(false); m_tabs->setElideMode(Qt::ElideRight); m_tabs->setFixedHeight(46);
        m_stack = new QStackedWidget(); m_stack->setObjectName("settingsStack");
        struct Entry { QString title; QString file; };
        const QVector<Entry> entries = {
            {"软件设置", "assets/config/app.json"},{"智能体设置", "assets/config/agent.json"},
            {"截图与图像", "assets/config/camera.json"},{"模型设置", "assets/config/models.json"},
            {"权限与安全", "assets/config/permissions.json"},{"工具设置", "assets/config/tools.json"}
        };
        for (const Entry& entry : entries) {
            m_tabs->addTab(entry.title);
            auto* page = new JsonSettingsPage(entry.file);
            m_pages.push_back(page); m_stack->addWidget(page);
        }
        root->addWidget(m_tabs); root->addWidget(m_stack, 1);
        auto* footer = new QWidget(); footer->setObjectName("settingsFooter"); footer->setFixedHeight(54);
        auto* footerLayout = new QHBoxLayout(footer); footerLayout->setContentsMargins(4, 8, 4, 0); footerLayout->setSpacing(8);
        auto* hint = new QLabel("修改后保存，直接写入 assets/config/*.json"); hint->setObjectName("settingsHint");
        auto* save = new QPushButton("保存配置"); save->setObjectName("settingsSave"); save->setFixedSize(82, 36);
        footerLayout->addWidget(hint, 1); footerLayout->addWidget(save);
        root->addWidget(footer);
        connect(close, &QPushButton::clicked, this, &QDialog::reject);
        connect(m_tabs, &QTabBar::currentChanged, m_stack, &QStackedWidget::setCurrentIndex);
        connect(m_tabs, &QTabBar::tabMoved, [this](int from, int to) {
            QWidget* page = m_stack->widget(from); if (!page) return;
            m_stack->removeWidget(page); m_stack->insertWidget(to, page);
            if (from >= 0 && from < m_pages.size()) { auto* moved = m_pages.takeAt(from); m_pages.insert(to, moved); }
            m_stack->setCurrentIndex(m_tabs->currentIndex());
        });
        connect(save, &QPushButton::clicked, [this]() {
            for (JsonSettingsPage* page : m_pages) { QString error; if (!page->save(&error)) { customConfirm(this, "保存失败", error); return; } }
            accept();
        });
        m_tabs->setCurrentIndex(0); m_stack->setCurrentIndex(0);
    }
};

enum class MessageRole { Ai, User, System };

class BubbleWidget : public QWidget {
public:
    BubbleWidget(MessageRole role, const QString& text, int maxBubbleWidth, QWidget* parent = nullptr)
        : QWidget(parent), m_role(role), m_maxBubbleWidth(maxBubbleWidth) {
        setAttribute(Qt::WA_TranslucentBackground, true);
        setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Minimum);
        m_layout = new QVBoxLayout(this);
        if (m_role == MessageRole::Ai) m_layout->setContentsMargins(16, 8, 10, 8);
        else m_layout->setContentsMargins(10, 8, 16, 8);
        m_layout->setSpacing(6);
        m_view = new QTextBrowser(this);
        m_view->setObjectName("bubbleText"); m_view->setFrameShape(QFrame::NoFrame);
        m_view->setReadOnly(true); m_view->setOpenExternalLinks(false);
        m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_view->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
        QFont normal(g_lightFontFamily); normal.setPixelSize(14);
        m_view->setFont(normal); m_view->document()->setDefaultFont(normal);
        m_view->document()->setDefaultStyleSheet(
            QString("body{font-family:'%1';font-size:14px;color:#ffffff;line-height:1.45;}"
                "p{margin:2px 0 6px 0;}"
                "h1{font-family:'%2';font-size:20px;font-weight:700;margin:5px 0 8px 0;}"
                "h2{font-family:'%2';font-size:17px;font-weight:700;margin:5px 0 7px 0;}"
                "h3{font-family:'%2';font-size:15px;font-weight:700;margin:4px 0 6px 0;}"
                "strong,b{font-family:'%2';font-weight:700;}"
                "code{font-family:Consolas,'Microsoft YaHei UI';background:#111111;color:#e6e6e6;padding:1px 3px;}"
                "pre{font-family:Consolas,'Microsoft YaHei UI';background:#111111;color:#e6e6e6;"
                "margin:5px 0;padding:8px;border-radius:4px;white-space:pre-wrap;}"
                "blockquote{margin:5px 0;padding:4px 8px;border-left:3px solid #543b27;color:#c8c8c8;}"
                "table{border-collapse:collapse;margin:6px 0;width:100%;}"
                "th{font-family:'%2';font-weight:700;background:#2a2a2a;padding:5px;border:1px solid #444444;}"
                "td{padding:5px;border:1px solid #3a3a3a;}"
                "ul,ol{margin:3px 0 6px 18px;}"
                "li{margin:2px 0;}"
                "a{color:#7cb7ff;text-decoration:none;}").arg(g_lightFontFamily, g_boldFontFamily));
        m_layout->addWidget(m_view);
        setMaximumWidth(m_maxBubbleWidth);
        setMarkdownText(text);
    }
    void setImagePath(const QString& path) {
        if (path.isEmpty()) return;
        if (!m_imageLabel) {
            m_imageLabel = new QLabel(this);
            m_imageLabel->setObjectName("chatImage");
            m_imageLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
            m_imageLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
            m_layout->insertWidget(0, m_imageLabel, 0, m_role == MessageRole::User ? Qt::AlignRight : Qt::AlignLeft);
        }
        QPixmap pix(path); if (pix.isNull()) return;
        const int imageMaxWidth = qMax(140, m_maxBubbleWidth - 34);
        const int imageMaxHeight = 280;
        m_imageLabel->setPixmap(pix.scaled(imageMaxWidth, imageMaxHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        m_imageLabel->adjustSize(); updateBubbleGeometry();
    }
    void setMarkdownText(const QString& text) {
        m_text = text; if (!m_view) return;
        m_view->document()->setMarkdown(m_text, QTextDocument::MarkdownDialectGitHub);
        updateBubbleGeometry();
    }
    void appendStreamDelta(const QString& delta) { m_text += delta; setMarkdownText(m_text); }
    QString text() const { return m_text; }
private:
    MessageRole m_role; int m_maxBubbleWidth = 320;
    QVBoxLayout* m_layout = nullptr; QTextBrowser* m_view = nullptr; QLabel* m_imageLabel = nullptr;
    QString m_text;
    void updateBubbleGeometry() {
        if (!m_view) return;
        const bool longText = m_text.size() >= 18 || m_text.contains('\n') || m_text.contains('|') || m_text.contains("```");
        const int maximumTextWidth = qMax(160, m_maxBubbleWidth - 36);
        int textWidth = maximumTextWidth;
        if (!longText && !m_text.isEmpty()) { m_view->document()->setTextWidth(-1); const int natural = qCeil(m_view->document()->idealWidth()) + 18; textWidth = qBound(110, natural, maximumTextWidth); }
        m_view->document()->setTextWidth(textWidth);
        const QSizeF docSize = m_view->document()->size();
        const int documentHeight = qMax(22, qCeil(docSize.height()) + 4);
        m_view->setFixedWidth(textWidth); m_view->setFixedHeight(documentHeight);
        int totalHeight = documentHeight + 16;
        if (m_imageLabel && !m_imageLabel->pixmap().isNull()) totalHeight += m_imageLabel->height() + 6;
        setMinimumHeight(totalHeight); setMaximumWidth(m_maxBubbleWidth);
        updateGeometry(); update();
    }
protected:
    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);
        QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing, true);
        const bool ai = m_role == MessageRole::Ai;
        const QColor fill = ai ? QColor("#202020") : QColor("#543b27");
        QRectF bubbleRect = rect().adjusted(ai ? 8 : 0, 0, ai ? 0 : -8, 0);
        QPainterPath path; path.addRoundedRect(bubbleRect, 5, 5);
        QPolygonF triangle;
        if (ai) {
            triangle << QPointF(bubbleRect.left(), bubbleRect.top() + 12)
                     << QPointF(bubbleRect.left() - 8, bubbleRect.top() + 17)
                     << QPointF(bubbleRect.left(), bubbleRect.top() + 22);
        } else {
            triangle << QPointF(bubbleRect.right(), bubbleRect.top() + 12)
                     << QPointF(bubbleRect.right() + 8, bubbleRect.top() + 17)
                     << QPointF(bubbleRect.right(), bubbleRect.top() + 22);
        }
        path.addPolygon(triangle);
        painter.setPen(Qt::NoPen);
        painter.fillPath(path, fill);
    }
};

class ChatMessageWidget : public QWidget {
public:
    ChatMessageWidget(MessageRole role, const QString& text, int bubbleWidth, QWidget* parent = nullptr)
        : QWidget(parent), m_role(role) {
        setObjectName("chatMessage");
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        auto* row = new QHBoxLayout(this);
        row->setContentsMargins(0, 5, 0, 5); row->setSpacing(7);
        if (role == MessageRole::System) {
            row->addStretch(1);
            m_systemLabel = new QLabel(text);
            m_systemLabel->setObjectName("systemMessage"); m_systemLabel->setWordWrap(true);
            m_systemLabel->setAlignment(Qt::AlignCenter); m_systemLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
            m_systemLabel->setFixedWidth(qMax(200, bubbleWidth));
            row->addWidget(m_systemLabel, 0, Qt::AlignHCenter);
            row->addStretch(1);
            return;
        }
        const bool ai = role == MessageRole::Ai;
        auto* avatar = new QLabel();
        avatar->setObjectName(ai ? "aiAvatar" : "userAvatar");
        avatar->setFixedSize(38, 38); avatar->setAlignment(Qt::AlignCenter);
        const QString avatarPath = ai ? ":/icons/assets/img/icon/min_logo.png" : ":/icons/assets/img/no_avatar.png";
        QPixmap avatarPixmap(avatarPath);
        if (!avatarPixmap.isNull()) avatar->setPixmap(avatarPixmap.scaled(34, 34, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        m_bubble = new BubbleWidget(role, text, bubbleWidth, this);
        if (ai) { row->addWidget(avatar, 0, Qt::AlignTop); row->addWidget(m_bubble, 0, Qt::AlignTop); row->addStretch(1); }
        else { row->addStretch(1); row->addWidget(m_bubble, 0, Qt::AlignTop); row->addWidget(avatar, 0, Qt::AlignTop); }
    }
    void setText(const QString& text) { if (m_bubble) m_bubble->setMarkdownText(text); else if (m_systemLabel) m_systemLabel->setText(text); }
    void appendDelta(const QString& delta) { if (m_bubble) m_bubble->appendStreamDelta(delta); }
    void setImagePath(const QString& path) { if (m_bubble) m_bubble->setImagePath(path); }
private:
    MessageRole m_role; BubbleWidget* m_bubble = nullptr; QLabel* m_systemLabel = nullptr;
};
enum class AppState { Checking, LoadingModel, Ready, Generating, Failed };
enum class WorkMode { Chat, DesktopControl, WindowControl, ScreenshotSelecting };
enum class AgentPolicyPhase { ObserveOrVerify, ActionReady, NeedGrounding, WaitForChange, VerifyEffect };

// 视觉用途严格分流：用户视觉=自然语言；Agent视觉=结构化Grounding JSON。
enum class VisionPurpose { UserImageChat, UserScreenshotChat, AgentDesktopGrounding, AgentWindowGrounding };
enum class AgentScope { Desktop, SelectedWindow };

static QRect virtualDesktopRect() {
    return QRect(GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN),
                 GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN));
}
static QRect cursorMonitorRect() {
    POINT p{}; GetCursorPos(&p);
    HMONITOR monitor = MonitorFromPoint(p, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{}; info.cbSize = sizeof(info);
    if (!GetMonitorInfo(monitor, &info)) return {};
    return QRect(info.rcMonitor.left, info.rcMonitor.top, info.rcMonitor.right - info.rcMonitor.left, info.rcMonitor.bottom - info.rcMonitor.top);
}
static bool validWindowHandle(HWND hwnd) { return hwnd && IsWindow(hwnd) && IsWindowVisible(hwnd); }
static QString windowTitleOf(HWND hwnd) {
    if (!validWindowHandle(hwnd)) return {};
    wchar_t buffer[2048]{}; const int len = GetWindowTextW(hwnd, buffer, 2047);
    return len > 0 ? QString::fromWCharArray(buffer, len).trimmed() : QString();
}
static QString windowClassOf(HWND hwnd) {
    if (!hwnd) return {};
    wchar_t buffer[512]{}; const int len = GetClassNameW(hwnd, buffer, 511);
    return len > 0 ? QString::fromWCharArray(buffer, len) : QString();
}
static QRect windowRectOf(HWND hwnd) {
    if (!validWindowHandle(hwnd)) return {};
    RECT r{}; if (!GetWindowRect(hwnd, &r)) return {};
    return QRect(r.left, r.top, r.right-r.left, r.bottom-r.top);
}
static QString processExecutablePath(HWND hwnd) {
    if (!hwnd) return {};
    DWORD pid = 0; GetWindowThreadProcessId(hwnd, &pid); if (!pid) return {};
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid); if (!process) return {};
    wchar_t buffer[32768]{}; DWORD size = static_cast<DWORD>(std::size(buffer)); QString result;
    if (QueryFullProcessImageNameW(process, 0, buffer, &size) && size > 0) result = QString::fromWCharArray(buffer, static_cast<int>(size));
    CloseHandle(process); return result;
}
static QString processExecutableName(HWND hwnd) {
    const QString path = processExecutablePath(hwnd);
    return path.isEmpty() ? QString() : QFileInfo(path).fileName();
}
static bool isShellOrDesktopWindow(HWND hwnd) {
    const QString cls = windowClassOf(hwnd);
    return cls.compare("Shell_TrayWnd", Qt::CaseInsensitive) == 0 || cls.compare("Shell_SecondaryTrayWnd", Qt::CaseInsensitive) == 0 ||
           cls.compare("Progman", Qt::CaseInsensitive) == 0 || cls.compare("WorkerW", Qt::CaseInsensitive) == 0;
}
static LONG normalizeMouseAxis(int value, int origin, int length) {
    if (length <= 1) return 0;
    const double v = (static_cast<double>(value-origin) * 65535.0) / static_cast<double>(length-1);
    return static_cast<LONG>(qBound(0.0, v, 65535.0));
}
static bool sendAiMouseMove(int x, int y) {
    const QRect vr = virtualDesktopRect();
    INPUT input{}; input.type = INPUT_MOUSE;
    input.mi.dx = normalizeMouseAxis(x, vr.left(), vr.width());
    input.mi.dy = normalizeMouseAxis(y, vr.top(), vr.height());
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    input.mi.dwExtraInfo = CEH_AI_INPUT_MAGIC;
    return SendInput(1, &input, sizeof(INPUT)) == 1;
}
static bool sendAiMouseClick(int x, int y, const QString& button) {
    if (!sendAiMouseMove(x,y)) return false;
    DWORD downFlag=MOUSEEVENTF_LEFTDOWN, upFlag=MOUSEEVENTF_LEFTUP;
    if (button.compare("right",Qt::CaseInsensitive)==0) { downFlag=MOUSEEVENTF_RIGHTDOWN; upFlag=MOUSEEVENTF_RIGHTUP; }
    else if (button.compare("middle",Qt::CaseInsensitive)==0) { downFlag=MOUSEEVENTF_MIDDLEDOWN; upFlag=MOUSEEVENTF_MIDDLEUP; }
    INPUT in[2]{};
    in[0].type=INPUT_MOUSE; in[0].mi.dwFlags=downFlag; in[0].mi.dwExtraInfo=CEH_AI_INPUT_MAGIC;
    in[1].type=INPUT_MOUSE; in[1].mi.dwFlags=upFlag; in[1].mi.dwExtraInfo=CEH_AI_INPUT_MAGIC;
    return SendInput(2,in,sizeof(INPUT))==2;
}
static bool sendAiMouseDrag(int x1,int y1,int x2,int y2,int durationMs) {
    const auto generation = RuntimeSafety::instance().generation();
    if (RuntimeSafety::instance().shouldAbort(generation)) return false;
    durationMs=qBound(50,durationMs,5000);
    if (!sendAiMouseMove(x1,y1)) return false;
    INPUT down{}; down.type=INPUT_MOUSE; down.mi.dwFlags=MOUSEEVENTF_LEFTDOWN; down.mi.dwExtraInfo=CEH_AI_INPUT_MAGIC;
    if (SendInput(1,&down,sizeof(INPUT))!=1) return false;
    const int steps=qBound(2,durationMs/16,300);
    for(int i=1;i<=steps;++i){
        if (RuntimeSafety::instance().shouldAbort(generation)) break;
        const double t=static_cast<double>(i)/steps;
        sendAiMouseMove(qRound(x1+(x2-x1)*t),qRound(y1+(y2-y1)*t));
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        QThread::msleep(qMax(1,durationMs/steps));
    }
    INPUT up{}; up.type=INPUT_MOUSE; up.mi.dwFlags=MOUSEEVENTF_LEFTUP; up.mi.dwExtraInfo=CEH_AI_INPUT_MAGIC;
    const bool released = SendInput(1,&up,sizeof(INPUT))==1;
    return released && !RuntimeSafety::instance().shouldAbort(generation);
}
static bool sendAiUnicodeText(const QString& text) {
    const auto generation = RuntimeSafety::instance().generation();
    for (QChar ch : text) {
        if (RuntimeSafety::instance().shouldAbort(generation)) return false;
        INPUT in[2]{};
        in[0].type=INPUT_KEYBOARD; in[0].ki.wScan=ch.unicode(); in[0].ki.dwFlags=KEYEVENTF_UNICODE; in[0].ki.dwExtraInfo=CEH_AI_INPUT_MAGIC;
        in[1].type=INPUT_KEYBOARD; in[1].ki.wScan=ch.unicode(); in[1].ki.dwFlags=KEYEVENTF_UNICODE|KEYEVENTF_KEYUP; in[1].ki.dwExtraInfo=CEH_AI_INPUT_MAGIC;
        if (SendInput(2,in,sizeof(INPUT))!=2) return false;
    }
    return true;
}
static WORD virtualKeyFromName(QString name) {
    name=name.trimmed().toUpper();
    if(name=="CTRL"||name=="CONTROL") return VK_CONTROL;
    if(name=="SHIFT") return VK_SHIFT;
    if(name=="ALT") return VK_MENU;
    if(name=="WIN"||name=="WINDOWS") return VK_LWIN;
    if(name=="ENTER"||name=="RETURN") return VK_RETURN;
    if(name=="ESC"||name=="ESCAPE") return VK_ESCAPE;
    if(name=="TAB") return VK_TAB;
    if(name=="BACKSPACE") return VK_BACK;
    if(name=="DELETE"||name=="DEL") return VK_DELETE;
    if(name=="SPACE") return VK_SPACE;
    if(name=="UP") return VK_UP;
    if(name=="DOWN") return VK_DOWN;
    if(name=="LEFT") return VK_LEFT;
    if(name=="RIGHT") return VK_RIGHT;
    if(name=="HOME") return VK_HOME;
    if(name=="END") return VK_END;
    if(name=="PAGEUP") return VK_PRIOR;
    if(name=="PAGEDOWN") return VK_NEXT;
    if(name.size()==1){ SHORT vk=VkKeyScanW(name.at(0).unicode()); if(vk!=-1) return LOBYTE(vk); }
    if(name.startsWith("F")){ bool ok=false; int n=name.mid(1).toInt(&ok); if(ok&&n>=1&&n<=24) return static_cast<WORD>(VK_F1+n-1); }
    return 0;
}
static bool sendAiHotkey(const QString& hotkey) {
    const QStringList parts=hotkey.split('+',Qt::SkipEmptyParts); if(parts.isEmpty()) return false;
    QVector<WORD> keys;
    for(const QString& p:parts){ WORD vk=virtualKeyFromName(p); if(!vk) return false; keys.push_back(vk); }
    QVector<INPUT> inputs; inputs.reserve(keys.size()*2);
    for(WORD vk:keys){ INPUT in{}; in.type=INPUT_KEYBOARD; in.ki.wVk=vk; in.ki.dwExtraInfo=CEH_AI_INPUT_MAGIC; inputs.push_back(in); }
    for(int i=keys.size()-1;i>=0;--i){ INPUT in{}; in.type=INPUT_KEYBOARD; in.ki.wVk=keys.at(i); in.ki.dwFlags=KEYEVENTF_KEYUP; in.ki.dwExtraInfo=CEH_AI_INPUT_MAGIC; inputs.push_back(in); }
    return SendInput(static_cast<UINT>(inputs.size()),inputs.data(),sizeof(INPUT))==static_cast<UINT>(inputs.size());
}

class WorkerExecutor {
public:
    WorkerExecutor() = default;
    ~WorkerExecutor() = default;
    bool start(const QString& name) {
        if (m_thread && m_thread->isRunning()) return true;
        m_thread = new QThread(); m_thread->setObjectName(name);
        m_context = new QObject(); m_context->moveToThread(m_thread);
        m_thread->start(); return m_thread->isRunning();
    }
    bool isRunning() const { return m_thread && m_thread->isRunning(); }
    QThread* thread() const { return m_thread; }
    template <typename Func>
    auto call(Func&& func) -> decltype(func()) {
        using Result = decltype(func());
        if (!m_context || !m_thread || !m_thread->isRunning()) {
            if constexpr (std::is_void_v<Result>) { func(); return; } else { return func(); }
        }
        if (QThread::currentThread() == m_thread) {
            if constexpr (std::is_void_v<Result>) { func(); return; } else { return func(); }
        }
        QSemaphore semaphore(0);
        if constexpr (std::is_void_v<Result>) {
            QMetaObject::invokeMethod(m_context, [fn = std::forward<Func>(func), &semaphore]() mutable { fn(); semaphore.release(); }, Qt::QueuedConnection);
            semaphore.acquire(); return;
        } else {
            std::optional<Result> result;
            QMetaObject::invokeMethod(m_context, [fn = std::forward<Func>(func), &semaphore, &result]() mutable { result.emplace(fn()); semaphore.release(); }, Qt::QueuedConnection);
            semaphore.acquire(); return std::move(result.value());
        }
    }
    template <typename Func>
    void post(Func&& func) {
        if (!m_context || !m_thread || !m_thread->isRunning()) return;
        QMetaObject::invokeMethod(m_context, [fn = std::forward<Func>(func)]() mutable { fn(); }, Qt::QueuedConnection);
    }
    bool stop(int timeoutMs) {
        if (!m_thread) return true;
        if (m_context) { m_context->deleteLater(); m_context = nullptr; }
        m_thread->quit();
        const bool stopped = m_thread->wait(qMax(100, timeoutMs));
        if (stopped) { delete m_thread; m_thread = nullptr; return true; }
        m_thread->requestInterruption();
        Logger::write("WARN", QString("Worker线程 %1 在 %2ms 内未结束，交由进程退出兜底。").arg(m_thread->objectName()).arg(timeoutMs));
        return false;
    }
private:
    QThread* m_thread = nullptr; QObject* m_context = nullptr;
};

class UserInputMonitor {
public:
    ~UserInputMonitor() { stop(); }
    void setInterruptFlag(std::atomic_bool* flag) { m_flag = flag; }
    // 只有自动控制任务执行中才累计真人输入；聊天、看图、启动、空闲不计入接管。
    void setTaskActiveFlag(const std::atomic_bool* active) { m_taskActive = active; }
    void configurePolicy(const QJsonObject& cfg){m_enabled=cfg.value("enabled").toBool(true)&&cfg.value("pause_agent_on_user_input").toBool(true);m_watchMove=cfg.value("mouse_move").toBool(true);m_watchClick=cfg.value("mouse_click").toBool(true);m_watchKeyboard=cfg.value("keyboard_input").toBool(true);}
    void configure(int sustainMs = 5000, int continuityGapMs = 700) { m_sustainMs = qMax(1000, sustainMs); m_continuityGapMs = qBound(150, continuityGapMs, 2000); }
    void resetActivity() { m_activityStart = 0; m_lastActivity = 0; if (m_flag) *m_flag = false; }
    bool start() {
        if (m_mouseHook || m_keyboardHook) return true;
        s_instance = this;
        m_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, &UserInputMonitor::mouseProc, GetModuleHandleW(nullptr), 0);
        m_keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, &UserInputMonitor::keyboardProc, GetModuleHandleW(nullptr), 0);
        if (!m_mouseHook || !m_keyboardHook) { stop(); return false; }
        return true;
    }
    void stop() {
        if (m_mouseHook) { UnhookWindowsHookEx(m_mouseHook); m_mouseHook = nullptr; }
        if (m_keyboardHook) { UnhookWindowsHookEx(m_keyboardHook); m_keyboardHook = nullptr; }
        if (s_instance == this) s_instance = nullptr;
    }
private:
    HHOOK m_mouseHook = nullptr; HHOOK m_keyboardHook = nullptr;
    std::atomic_bool* m_flag = nullptr; const std::atomic_bool* m_taskActive = nullptr;
    bool m_enabled=true,m_watchMove=true,m_watchClick=true,m_watchKeyboard=true;
    ULONGLONG m_activityStart = 0; ULONGLONG m_lastActivity = 0;
    int m_sustainMs = 5000; int m_continuityGapMs = 700;
    inline static UserInputMonitor* s_instance = nullptr;
    void humanActivity() {
        if (!m_enabled || !m_taskActive || !m_taskActive->load()) return;
        const ULONGLONG now = GetTickCount64();
        if (m_lastActivity == 0 || now - m_lastActivity > static_cast<ULONGLONG>(m_continuityGapMs)) m_activityStart = now;
        m_lastActivity = now;
        if (m_flag && m_activityStart > 0 && now - m_activityStart >= static_cast<ULONGLONG>(m_sustainMs)) *m_flag = true;
    }
    static LRESULT CALLBACK mouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
        if (nCode >= 0 && s_instance) {
            auto* d = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
            if (d && d->dwExtraInfo != CEH_AI_INPUT_MAGIC) {
                if ((wParam==WM_MOUSEMOVE&&s_instance->m_watchMove)||((wParam==WM_LBUTTONDOWN||wParam==WM_RBUTTONDOWN||wParam==WM_MBUTTONDOWN||wParam==WM_MOUSEWHEEL)&&s_instance->m_watchClick)) s_instance->humanActivity();
            }
        }
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }
    static LRESULT CALLBACK keyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
        if (nCode >= 0 && s_instance) {
            auto* d = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
            if (d && d->dwExtraInfo != CEH_AI_INPUT_MAGIC) {
                if (s_instance->m_watchKeyboard&&(wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) s_instance->humanActivity();
            }
        }
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }
};

class ScreenshotOverlay final : public QWidget {
public:
    using FinishedCallback=std::function<void(const QRect&,bool)>;
    ScreenshotOverlay(const QRect& globalRect,const QJsonObject& cfg,FinishedCallback cb)
        :m_globalRect(globalRect),m_cfg(cfg),m_cb(std::move(cb)){
        setWindowFlags(Qt::FramelessWindowHint|Qt::Tool|Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_TranslucentBackground,true);
        setAttribute(Qt::WA_DeleteOnClose,true);
        setMouseTracking(true); setFocusPolicy(Qt::StrongFocus); setGeometry(globalRect); setCursor(Qt::CrossCursor);
    }
protected:
    void showEvent(QShowEvent* e) override { QWidget::showEvent(e); raise(); activateWindow(); setFocus(); }
    void keyPressEvent(QKeyEvent* e) override { if(e->key()==Qt::Key_Escape){ finish({},false); return; } QWidget::keyPressEvent(e); }
    void mousePressEvent(QMouseEvent* e) override {
        if(e->button()==Qt::RightButton){ finish({},false); return; }
        if(e->button()!=Qt::LeftButton) return;
        m_selecting=true; m_origin=e->position().toPoint(); m_current=m_origin; update();
    }
    void mouseMoveEvent(QMouseEvent* e) override { if(m_selecting){ m_current=e->position().toPoint(); update(); } }
    void mouseReleaseEvent(QMouseEvent* e) override {
        if(!m_selecting||e->button()!=Qt::LeftButton) return;
        m_current=e->position().toPoint(); m_selecting=false;
        QRect local=selection();
        if(local.width()<m_cfg.value("minimum_width").toInt(10)||local.height()<m_cfg.value("minimum_height").toInt(10)){ update(); return; }
        finish(QRect(m_globalRect.x()+local.x(),m_globalRect.y()+local.y(),local.width(),local.height()),true);
    }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing,true);
        if(m_cfg.value("dim_background").toBool(true)){
            const int a=qBound(0,qRound(m_cfg.value("dim_opacity").toDouble(0.22)*255.0),255);
            p.fillRect(rect(),QColor(0,0,0,a));
        }
        QRect r=selection(); if(r.isEmpty()) return;
        QColor fill(m_cfg.value("fill_color").toString("#40c977")); fill.setAlpha(qBound(0,qRound(m_cfg.value("fill_opacity").toDouble(0.08)*255.0),255));
        p.fillRect(r,fill); p.setBrush(Qt::NoBrush); p.setPen(QPen(QColor(m_cfg.value("border_color").toString("#40c977")),m_cfg.value("border_width").toInt(2))); p.drawRect(r.adjusted(1,1,-1,-1));
        if(m_cfg.value("show_size_text").toBool(true)){
            QString t=QString("%1 × %2").arg(r.width()).arg(r.height()); QFont f(g_lightFontFamily); f.setPixelSize(12); p.setFont(f);
            QRect label(r.left(),qMax(0,r.top()-26),110,22); p.fillRect(label,QColor(20,20,20,220)); p.setPen(Qt::white); p.drawText(label.adjusted(6,0,-4,0),Qt::AlignLeft|Qt::AlignVCenter,t);
        }
    }
private:
    QRect m_globalRect; QJsonObject m_cfg; FinishedCallback m_cb; bool m_selecting=false,m_finished=false; QPoint m_origin,m_current;
    QRect selection() const { if(m_origin.isNull()&&m_current.isNull()) return {}; return QRect(m_origin,m_current).normalized().intersected(rect()); }
    void finish(const QRect& r,bool ok){ if(m_finished) return; m_finished=true; hide(); QApplication::processEvents(); auto cb=m_cb; close(); if(cb) cb(r,ok); }
};

class UiaOverlayWidget final : public QWidget {
public:
    explicit UiaOverlayWidget(bool selectedWindow):m_selectedWindow(selectedWindow){
        setWindowFlags(Qt::FramelessWindowHint|Qt::Tool|Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_TranslucentBackground,true);
        setAttribute(Qt::WA_TransparentForMouseEvents,true);
        setAttribute(Qt::WA_ShowWithoutActivating,true);
        setAttribute(Qt::WA_NoSystemBackground,true);
        setAutoFillBackground(false); setFocusPolicy(Qt::NoFocus);
        m_pulse.start();
        if(m_selectedWindow){ m_repaintTimer=new QTimer(this); m_repaintTimer->setInterval(50); QObject::connect(m_repaintTimer,&QTimer::timeout,this,[this]{update();}); m_repaintTimer->start(); }
    }
    void configure(const QJsonObject& c){m_cfg=c;update();}
    void setGlobalRect(const QRect& r){m_globalRect=r;setGeometry(r);update();}
    void setItems(const QVector<UIAItem>& items){
        if(items.size()==m_items.size()){
            bool same=true;
            for(int i=0;i<items.size();++i){
                const auto& a=items.at(i); const auto& b=m_items.at(i);
                if(a.id!=b.id||a.rect!=b.rect||a.type!=b.type||a.name!=b.name||a.clickable!=b.clickable){ same=false; break; }
            }
            if(same) return;
        }
        m_items=items; update();
    }
protected:
    bool nativeEvent(const QByteArray& eventType,void* message,qintptr* result) override {
        Q_UNUSED(eventType); MSG* msg=static_cast<MSG*>(message);
        if(msg&&msg->message==WM_NCHITTEST){ if(result)*result=HTTRANSPARENT; return true; }
        return QWidget::nativeEvent(eventType,message,result);
    }
    void showEvent(QShowEvent* e) override {
        QWidget::showEvent(e); HWND hwnd=reinterpret_cast<HWND>(winId());
        LONG_PTR ex=GetWindowLongPtrW(hwnd,GWL_EXSTYLE); ex|=WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW;
        SetWindowLongPtrW(hwnd,GWL_EXSTYLE,ex);
        SetWindowPos(hwnd,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_FRAMECHANGED);
    }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing,true);
        if(m_selectedWindow){ paintGlow(p); } else { paintItems(p); }
    }
private:
    bool m_selectedWindow=false; QRect m_globalRect; QVector<UIAItem> m_items; QJsonObject m_cfg; QElapsedTimer m_pulse; QTimer* m_repaintTimer=nullptr;
    void paintGlow(QPainter& p){
        if(!m_cfg.value("window_glow_enabled").toBool(true)) return;
        const QColor base(m_cfg.value("window_glow_color").toString("#40c977"));
        const int pulseMs=qMax(300,m_cfg.value("window_glow_pulse_ms").toInt(1200));
        const double phase=std::sin((static_cast<double>(m_pulse.elapsed()%pulseMs)/pulseMs)*6.283185307179586);
        const double factor=0.55+0.45*(phase+1.0)/2.0;
        for(int i=0;i<5;++i){
            QColor c=base; const int alpha=qBound(20,qRound((180.0-30.0*i)*factor),220); c.setAlpha(alpha);
            QPen pen(c, qMax(1, 5-i)); p.setPen(pen); p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(rect().adjusted(2+i,2+i,-3-i,-3-i),4,4);
        }
    }
    void paintItems(QPainter& p){
        bool clickableOnly=m_cfg.value("draw_clickable_only").toBool(true); bool drawId=m_cfg.value("draw_id").toBool(true), drawType=m_cfg.value("draw_type").toBool(true), drawName=m_cfg.value("draw_name").toBool(true), drawCoord=m_cfg.value("draw_coordinates").toBool(false);
        int thickness=qMax(1,m_cfg.value("border_thickness").toInt(2)), fontSize=qMax(9,m_cfg.value("label_font_size").toInt(13)), maxLen=qMax(8,m_cfg.value("max_label_length").toInt(24));
        QColor line(m_cfg.value("clickable_color").toString("#40c977")), text(m_cfg.value("text_color").toString("#ffffff")), bg(m_cfg.value("label_background").toString("#141414")); bg.setAlpha(220);
        QFont f(g_lightFontFamily); f.setPixelSize(fontSize); p.setFont(f);
        for(const UIAItem& item:m_items){
            if(clickableOnly&&!item.clickable) continue;
            QRect r=item.rect.translated(-m_globalRect.x(),-m_globalRect.y());
            if(!r.intersects(rect())) continue; r=r.intersected(rect());
            p.setPen(QPen(line,thickness)); p.setBrush(Qt::NoBrush); p.drawRect(r.adjusted(0,0,-1,-1));
            QStringList parts; if(drawId)parts<<QString("#%1").arg(item.id); if(drawType)parts<<item.type;
            if(drawName&&!item.name.isEmpty()){QString n=item.name; if(n.size()>maxLen)n=n.left(maxLen-3)+"..."; parts<<n;}
            if(drawCoord)parts<<QString("(%1,%2,%3,%4)").arg(item.rect.x()).arg(item.rect.y()).arg(item.rect.width()).arg(item.rect.height());
            QString label=parts.join(' '); if(label.isEmpty()) continue;
            QRect tr=p.boundingRect(QRect(r.left(),qMax(0,r.top()-24),qMin(width()-r.left(),520),22),Qt::AlignLeft|Qt::AlignVCenter,label).adjusted(-4,-2,4,2);
            if(tr.top()<0)tr.moveTop(r.top()); if(tr.right()>width())tr.moveRight(width()-1);
            p.fillRect(tr,bg); p.setPen(text); p.drawText(tr.adjusted(4,0,-4,0),Qt::AlignLeft|Qt::AlignVCenter,label);
        }
    }
};

// ---------- 任务协议：业务目标、工具执行和最终完成分开表示 ----------
class JsonContract {
public:
    static bool validate(const QJsonValue& value,const QJsonObject& schema,QString* error,int depth=0){
        auto fail=[&](const QString& text){if(error)*error=text;return false;};
        if(depth>12)return fail("参数嵌套超过上限");
        const auto typeMatches=[&](const QString& t){return t=="string"?value.isString():t=="object"?value.isObject():t=="array"?value.isArray():t=="boolean"?value.isBool():t=="null"?value.isNull():t=="number"?value.isDouble()&&std::isfinite(value.toDouble()):t=="integer"?value.isDouble()&&std::isfinite(value.toDouble())&&std::floor(value.toDouble())==value.toDouble():true;};
        const auto type=schema.value("type");bool match=true;
        if(type.isString())match=typeMatches(type.toString());else if(type.isArray()){match=false;for(const auto& t:type.toArray())if(typeMatches(t.toString()))match=true;}
        if(!match)return fail("参数类型不匹配："+type.toString());
        const auto choices=schema.value("enum").toArray();if(!choices.isEmpty()&&!choices.contains(value))return fail("参数不在允许枚举中");
        if(value.isDouble()){const double n=value.toDouble();if(!std::isfinite(n))return fail("参数不是有限数值");if(schema.contains("minimum")&&n<schema.value("minimum").toDouble())return fail("参数低于最小值");if(schema.contains("maximum")&&n>schema.value("maximum").toDouble())return fail("参数超过最大值");}
        if(value.isString()&&schema.contains("maxLength")&&value.toString().size()>schema.value("maxLength").toInt())return fail("文字超过长度上限");
        if(value.isObject()){
            const auto object=value.toObject();const auto props=schema.value("properties").toObject();
            for(const auto& name:schema.value("required").toArray())if(!object.contains(name.toString()))return fail("缺少必需字段："+name.toString());
            for(auto i=object.begin();i!=object.end();++i){
                if(props.contains(i.key())){QString detail;if(!validate(i.value(),props.value(i.key()).toObject(),&detail,depth+1))return fail(i.key()+": "+detail);}
                else if(schema.value("additionalProperties").isBool()&&!schema.value("additionalProperties").toBool())return fail("存在未声明参数："+i.key());
            }
        }
        if(value.isArray()){
            const auto a=value.toArray();if(a.size()>qMin(4096,schema.value("maxItems").toInt(4096)))return fail("数组超过长度上限");
            if(a.size()<schema.value("minItems").toInt(0))return fail("数组长度不足");
            if(schema.value("items").isObject())for(const auto& item:a)if(!validate(item,schema.value("items").toObject(),error,depth+1))return false;
        }
        for(const QString& mode:QStringList{"oneOf","anyOf"})if(schema.value(mode).isArray()){
            int passed=0;for(const auto& option:schema.value(mode).toArray())if(validate(value,option.toObject(),nullptr,depth+1))++passed;
            if((mode=="oneOf"&&passed!=1)||(mode=="anyOf"&&passed<1))return fail("参数不满足"+mode+"约束");
        }
        for(const auto& option:schema.value("allOf").toArray())if(!validate(value,option.toObject(),error,depth+1))return false;
        if(error)error->clear();return true;
    }
    static bool validateTool(const QJsonObject& action,const QJsonObject& definition,QString* error){
        QJsonObject properties;QJsonArray required;const auto parameters=definition.value("parameters").toObject();
        for(auto i=parameters.begin();i!=parameters.end();++i){QJsonObject schema=i.value().toObject();if(schema.value("required").toBool(false))required.append(i.key());schema.remove("required");if(schema.contains("allowed"))schema["enum"]=schema.value("allowed");properties[i.key()]=schema;}
        return validate(action,QJsonObject{{"type","object"},{"properties",properties},{"required",required}},error);
    }
};

enum class TaskKind { Chat, OpenApplication, CloseApplication, OperateApplication, CustomerService, CreativeWorkflow, DocumentWorkflow };

struct ExecutionAuthorization {
    bool authorized = false;
    AgentScope scope = AgentScope::Desktop;
    HWND targetHwnd = nullptr;
    QString reason;
};

enum class ToolOutcome { Succeeded, NeedObservation, RetryableFailure, Waiting, UserCancelled, PermissionDenied, FatalFailure };
struct ToolResult {
    ToolOutcome outcome = ToolOutcome::RetryableFailure;
    QString message;
    QJsonObject data;
    bool success() const { return outcome == ToolOutcome::Succeeded || outcome == ToolOutcome::Waiting; }
};

static QJsonObject strictJsonObject(QString text, QString* error = nullptr) {
    text = text.trimmed();
    if (text.startsWith("```")) { const int nl=text.indexOf('\n'); if(nl<0){if(error)*error="JSON围栏未闭合";return {}; } text=text.mid(nl+1).trimmed();if(text.endsWith("```"))text.chop(3); }
    QJsonParseError parse;
    const auto doc=QJsonDocument::fromJson(text.trimmed().toUtf8(),&parse);
    if(parse.error!=QJsonParseError::NoError || !doc.isObject()){if(error)*error="结构化输出不完整或不是JSON对象："+parse.errorString();return {};}
    if(error)error->clear();return doc.object();
}
static QJsonObject workflowConfig() { return loadConfig("assets/config/agent.json").value("workflow").toObject(); }

struct TaskRequest {
    TaskKind kind = TaskKind::OperateApplication;
    QString goal, application, completion;
    bool continuous = false;
    bool explicitAction = true;

    QString kindName() const {
        switch (kind) {
            case TaskKind::Chat: return "chat";
            case TaskKind::OpenApplication: return "open_application";
            case TaskKind::CloseApplication: return "close_application";
            case TaskKind::CustomerService: return "customer_service";
            case TaskKind::CreativeWorkflow: return "creative_workflow";
            case TaskKind::DocumentWorkflow: return "document_workflow";
            default: return "operate_application";
        }
    }

    QJsonObject json() const {
        return {{"kind", kindName()},{"goal", goal},{"application", application},{"completion", completion},{"continuous", continuous},{"explicit_action", explicitAction}};
    }

    static TaskRequest fromJson(const QJsonObject& o, const QString& original) {
        TaskRequest t;
        t.goal = original;
        t.application = o.value("application").toString().trimmed().left(160);
        t.completion = o.value("completion").toString().left(300);

        const QString k = o.value("kind").toString();
        if (k == "chat") t.kind = TaskKind::Chat;
        else if (k == "open_application") t.kind = TaskKind::OpenApplication;
        else if (k == "close_application") t.kind = TaskKind::CloseApplication;
        else if (k == "customer_service") t.kind = TaskKind::CustomerService;
        else if (k == "creative_workflow") t.kind = TaskKind::CreativeWorkflow;
        else if (k == "document_workflow") t.kind = TaskKind::DocumentWorkflow;
        else t.kind = TaskKind::OperateApplication;

        t.explicitAction = o.value("explicit_action").toBool(false);
        t.continuous = o.value("continuous").toBool(t.kind == TaskKind::CustomerService);
        return t;
    }
};

class TaskRouter {
public:
    static QString pureLaunch(const QString& text) {
        const QRegularExpression re("^(?:(?:请|帮我|麻烦|你帮我|请你|给我)\\s*)*(?:打开|启动|运行)\\s*(.{1,100}?)\\s*[。！!]*$");
        const auto m = re.match(text.trimmed());
        if (!m.hasMatch()) return {};
        QString app = m.captured(1).trimmed();
        app.remove(QRegularExpression("(?:软件|应用程序|应用|程序)$"));
        if (app.contains(QRegularExpression("[;&|,，；。\\r\\n？?]|怎么|如何|为什么|能不能|然后|接着|并且|并|之后|顺便|找到|查找|搜索|点击|输入|发送|发消息|回复|聊天|保存|绘制|画|后(?:给|发|找|画|帮|去|再)|去(?:给|发|找|画)"))) return {};
        if (app.contains(':') && !app.endsWith(".exe", Qt::CaseInsensitive) && !app.endsWith(".lnk", Qt::CaseInsensitive)) return {};
        return app.trimmed();
    }
    static QString pureClose(const QString& text) {
        const QRegularExpression re("^(?:(?:请|帮我|麻烦|你帮我|请你|给我)\\s*)*(?:关闭|退出|关掉|结束)\\s*(.{1,100}?)\\s*[。！!]*$");
        const auto m = re.match(text.trimmed());
        if (!m.hasMatch()) return {};
        QString app = m.captured(1).trimmed();
        app.remove(QRegularExpression("(?:软件|应用程序|应用|程序)$"));
        if (app.contains(QRegularExpression("[;&|,，；。\\r\\n？?]|怎么|如何|为什么|然后|接着|并且|之后|点击|输入|发送|回复|聊天|保存|绘制|画"))) return {};
        return app.trimmed();
    }
    static bool containsAny(const QString& text, const QStringList& words) {
        for (const QString& word : words) if (!word.isEmpty() && text.contains(word, Qt::CaseInsensitive)) return true;
        return false;
    }
    static bool looksContinuousTask(const QString& text) {
        return containsAny(text, {"一直","持续","不断","直到","为止","一直聊","聊到","一直回复","持续回复","持续聊天","连续聊天","做客服","客服值守","持续监控","有消息就回复","来了消息就回复"});
    }
    static bool looksCustomerServiceTask(const QString& text) {
        return containsAny(text, {"客服","聊天","回复","回消息","发消息","发送消息","联系人","客户","沟通","跟他聊","和他聊","微信","企业微信","钉钉","QQ","说服","谈到","聊到"});
    }
    static QString extractTargetEntity(const QString& text) {
        QString s = text.trimmed();
        const QList<QRegularExpression> patterns{
            QRegularExpression(R"((?:找到|找|联系|打开(?:与)?|进入(?:与)?|给)\s*["“]?([\p{L}\p{N}_·]{2,20})["”]?\s*(?:和他|跟他|与他|聊天|沟通|说|要求|让他|叫他|发|发送|谈|做|$))"),
            QRegularExpression(R"((?:联系人|客户|好友)\s*[:：]?\s*["“]?([\p{L}\p{N}_·]{2,20})["”]?)")
        };
        for (const auto& re : patterns) {
            const auto m = re.match(s);
            if (m.hasMatch()) {
                QString name = m.captured(1).trimmed();
                static const QSet<QString> stop{"微信","企业微信","QQ","钉钉","客户","联系人","对方","他","她","他们","网站业务","网页业务","业务","客服"};
                if (!stop.contains(name) && name.size() >= 2 && name.size() <= 20) return name;
            }
        }
        return {};
    }
    static bool looksCreativeTask(const QString& text) {
        return containsAny(text, {"画","画图","绘制","做图","修图","设计","抠图","海报","图片","图像","画布","PS","Photoshop"});
    }
    static bool looksDocumentTask(const QString& text) {
        return containsAny(text, {"文档","表格","演示","PPT","Word","Excel","WPS","报价表","合同","报告","简历","排版","写一份","做一份","插入表格","保存文档","保存表格"});
    }
    static bool isMessagingProcess(QString process) {
        process = process.toLower();
        return process.contains("weixin") || process.contains("wechat") || process.contains("wxwork") || process.contains("qq") || process.contains("dingtalk") || process.contains("feishu") || process.contains("lark");
    }
    static bool isCreativeProcess(QString process) {
        process = process.toLower();
        return process.contains("photoshop") || process.contains("mspaint") || process.contains("illustrator") || process.contains("krita") || process.contains("gimp") || process.contains("affinity");
    }
    static bool isDocumentProcess(QString process) {
        process = process.toLower();
        return process == "wps.exe" || process == "et.exe" || process == "wpp.exe" || process.contains("winword") || process.contains("excel") || process.contains("powerpnt") || process.contains("libreoffice");
    }
    static bool explicitlyRequestsNoControl(const QString& text) {
        return containsAny(text, {"不要操作","不用操作","别操作","先别动","只是问","只回答","解释一下","分析一下不要操作"});
    }
    static TaskKind normalizeSelectedWindowKind(TaskKind modelKind, const QString& process, const QString& goal) {
        if (isMessagingProcess(process) && looksCustomerServiceTask(goal)) return TaskKind::CustomerService;
        if (isCreativeProcess(process) && looksCreativeTask(goal)) return TaskKind::CreativeWorkflow;
        if (isDocumentProcess(process) && looksDocumentTask(goal)) return TaskKind::DocumentWorkflow;
        if (modelKind == TaskKind::Chat && !explicitlyRequestsNoControl(goal)) return TaskKind::OperateApplication;
        if (modelKind == TaskKind::OpenApplication) return TaskKind::OperateApplication;
        return modelKind;
    }
    static bool explicitTaskInChat(const QString& text) {
        if (!workflowConfig().value("allow_explicit_tasks_from_chat").toBool(true)) return false;
        if (!pureLaunch(text).isEmpty()) return true;
        if (!pureClose(text).isEmpty()) return true;
        return QRegularExpression("^(?:请|帮我|你帮我|请你|替我|给我).*(?:客服|操作|画图|绘制|打开|启动|发送|处理|保存|做文档|做表格)").match(text.trimmed()).hasMatch();
    }
};

class WorkflowController {
public:
    QString phase="understand";
    int failures=0, repetitions=0, workSteps=0;
    QString lastKey;
    bool canContinue(int limit)const{return failures<limit;}
    void record(const QString& action,const QString& observation,const ToolResult& result){
        if(result.outcome==ToolOutcome::Waiting){phase="waiting";failures=0;repetitions=0;return;}
        if(result.outcome==ToolOutcome::Succeeded){failures=0;phase="verify_effect";}
        else if(result.outcome==ToolOutcome::NeedObservation){phase="observe";++failures;}
        else ++failures;
        const QString key=action+"|"+observation;repetitions=key==lastKey?repetitions+1:1;lastKey=key;
    }
};

// 能力注册表只提供已安装/已配置的信息，不把enabled=true当作服务连接成功。
struct AppIdentity { QString name;QStringList aliases,processes;QJsonObject entry; };
class AppRegistry {
public:
    static QString normalized(QString text){text=QFileInfo(text.trimmed()).completeBaseName().toCaseFolded();text.remove(QRegularExpression("[\\s_\\-]"));return text;}
    static AppIdentity lookup(const QString& hint){
        const QJsonObject apps=loadConfig("assets/config/app_capabilities.json").value("applications").toObject();
        const QString key=normalized(hint);
        for(auto it=apps.begin();it!=apps.end();++it){
            const auto e=it.value().toObject();QStringList aliases{it.key(),e.value("display_name").toString()};QStringList processes{it.key()};
            for(const auto& a:e.value("aliases").toArray())aliases<<a.toString();
            for(const auto& a:e.value("process_names").toArray())processes<<a.toString();
            bool found=false;for(const QString& a:aliases+processes)if(!key.isEmpty()&&!a.isEmpty()&&normalized(a)==key){found=true;break;}
            if(found){AppIdentity out;out.name=e.value("display_name").toString(hint);out.aliases=aliases;out.aliases.removeAll("");out.aliases.removeDuplicates();out.processes=processes;out.processes.removeDuplicates();out.entry=e;return out;}
        }
        AppIdentity out;out.name=hint;out.aliases={hint};if(hint.endsWith(".exe",Qt::CaseInsensitive))out.processes={QFileInfo(hint).fileName()};return out;
    }
    static bool matches(const AppIdentity& app,const QString& process,const QString& title={}){
        for(const auto& p:app.processes)if(p.compare(process,Qt::CaseInsensitive)==0)return true;
        const QString exe=normalized(process);
        for(const auto& alias:app.aliases){const QString a=normalized(alias);if(!a.isEmpty()&&a==exe)return true;if(a.size()>=3&&!title.isEmpty()&&normalized(title)==a)return true;}
        return false;
    }
};

struct AppLaunchResult { ToolResult result;HWND hwnd=nullptr;DWORD pid=0;QString executable,title; };

class ApplicationLauncher {
public:
    static HWND existing(const AppIdentity& app){
        struct Search{const AppIdentity* app;HWND found=nullptr;} search{&app};
        EnumWindows([](HWND hwnd,LPARAM raw)->BOOL{
            auto* p=reinterpret_cast<Search*>(raw);if(!validWindowHandle(hwnd)||isShellOrDesktopWindow(hwnd))return TRUE;
            DWORD pid=0;GetWindowThreadProcessId(hwnd,&pid);if(pid==GetCurrentProcessId())return TRUE;
            if(AppRegistry::matches(*p->app,processExecutableName(hwnd),windowTitleOf(hwnd))){const HWND owner=GetAncestor(hwnd,GA_ROOTOWNER);p->found=validWindowHandle(owner)?owner:hwnd;return FALSE;}return TRUE;
        },reinterpret_cast<LPARAM>(&search));return search.found;
    }
    static QString shortcutTarget(const QString& path){
        if(!path.endsWith(".lnk",Qt::CaseInsensitive))return path;
        IShellLinkW* link=nullptr;QString target;
        if(SUCCEEDED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_IShellLinkW,reinterpret_cast<void**>(&link)))){
            IPersistFile* file=nullptr;
            if(SUCCEEDED(link->QueryInterface(IID_IPersistFile,reinterpret_cast<void**>(&file)))){
                if(SUCCEEDED(file->Load(reinterpret_cast<LPCWSTR>(path.utf16()),STGM_READ))){wchar_t buffer[32768]{};WIN32_FIND_DATAW info{};if(SUCCEEDED(link->GetPath(buffer,32768,&info,0)))target=QString::fromWCharArray(buffer);}
                file->Release();
            }link->Release();
        }
        if(!target.isEmpty()){wchar_t expanded[32768]{};const DWORD n=ExpandEnvironmentStringsW(reinterpret_cast<LPCWSTR>(target.utf16()),expanded,32768);if(n>0&&n<32768)target=QString::fromWCharArray(expanded);}
        return target;
    }
    static QString resolve(const QString& hint,QString* error){
        const AppIdentity app=AppRegistry::lookup(hint);const QFileInfo explicitFile(hint.trimmed());
        if(explicitFile.isAbsolute()&&explicitFile.exists()&&(explicitFile.suffix().compare("exe",Qt::CaseInsensitive)==0||explicitFile.suffix().compare("lnk",Qt::CaseInsensitive)==0))return explicitFile.absoluteFilePath();
        struct Candidate{QString path,exe;int score;};QVector<Candidate> candidates;
        auto add=[&](QString path,int score){
            path=path.trimmed();if(path.startsWith('"')){int q=path.indexOf('"',1);path=q>0?path.mid(1,q-1):QString();}
            else path=path.section(',',0,0).trimmed();
            const QFileInfo f(path);if(!f.exists()||!f.isFile())return;
            if(f.suffix().compare("exe",Qt::CaseInsensitive)!=0&&f.suffix().compare("lnk",Qt::CaseInsensitive)!=0)return;
            const QString exe=shortcutTarget(f.absoluteFilePath());if(exe.isEmpty()||!QFileInfo::exists(exe))return;
            const QString base=QFileInfo(exe).completeBaseName().toLower();if(base.contains("uninstall")||base=="unins000"||base=="setup"||base.contains("updater"))return;
            if(!app.processes.isEmpty()&&!AppRegistry::matches(app,QFileInfo(exe).fileName()))return;
            for(auto& c:candidates)if(c.exe.compare(exe,Qt::CaseInsensitive)==0){if(score>c.score)c={f.absoluteFilePath(),exe,score};return;}
            candidates.append({f.absoluteFilePath(),exe,score});
        };
        auto matchName=[&](const QString& name){if(name.trimmed().isEmpty())return 0;const QString n=AppRegistry::normalized(name);for(const auto& a:app.aliases){const QString x=AppRegistry::normalized(a);if(x.isEmpty())continue;if(n==x)return 100;if(x.size()>=3&&n.contains(x))return 75;}return 0;};
        for(const auto& exe:app.entry.value("executable_paths").toArray())add(exe.toString(),120);
        for(const QString& alias:app.aliases+app.processes){QString name=alias;if(!name.endsWith(".exe",Qt::CaseInsensitive))name+=".exe";const QString p=QStandardPaths::findExecutable(name);if(!p.isEmpty())add(p,95);}
        for(const QString& root:QStringList{"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\App Paths","HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\App Paths"}){
            QSettings reg(root,QSettings::NativeFormat);for(const QString& group:reg.childGroups()){if(matchName(group)==0)continue;reg.beginGroup(group);const QString path=reg.value(".").toString();reg.endGroup();add(path,105);}
        }
        QStringList roots=QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation)+QStandardPaths::standardLocations(QStandardPaths::DesktopLocation);
        const QString common=qEnvironmentVariable("PUBLIC");if(!common.isEmpty())roots<<QDir(common).filePath("Desktop");
        const quint64 resolveGeneration = RuntimeSafety::instance().generation();
        QElapsedTimer shortcutTimer; shortcutTimer.start();
        const int shortcutScanBudgetMs = qBound(300, workflowConfig().value("application_resolve_scan_budget_ms").toInt(2500), 10000);
        for(const QString& root:roots){
            QDirIterator it(root,{"*.lnk"},QDir::Files,QDirIterator::Subdirectories);
            int count=0;
            while(it.hasNext()&&count++<10000){
                if(RuntimeSafety::instance().shouldAbort(resolveGeneration))break;
                if(shortcutTimer.elapsed()>shortcutScanBudgetMs)break;
                const QString p=it.next();const int score=matchName(QFileInfo(p).completeBaseName());if(score)add(p,score);
            }
            if(shortcutTimer.elapsed()>shortcutScanBudgetMs)break;
        }
        for(const QString& root:QStringList{"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall","HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall","HKEY_LOCAL_MACHINE\\Software\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall"}){
            QSettings reg(root,QSettings::NativeFormat);for(const QString& group:reg.childGroups()){
                reg.beginGroup(group);const QString display=reg.value("DisplayName").toString(),icon=reg.value("DisplayIcon").toString(),location=reg.value("InstallLocation").toString();reg.endGroup();
                if(matchName(display)==0)continue;
                QString path=icon;if(path.startsWith('"'))path=path.section('"',1,1);else path=path.section(',',0,0);
                if(AppRegistry::matches(app,QFileInfo(path).fileName()))add(path,65);
                for(const QString& process:app.processes)if(!location.isEmpty())add(QDir(location).filePath(process),90);
            }
        }
        if(candidates.isEmpty()){if(error)*error="未在当前检测范围找到“"+hint+"”的可用安装入口，可指定完整exe路径或桌面快捷方式。";return {};}
        std::sort(candidates.begin(),candidates.end(),[](const Candidate& a,const Candidate& b){return a.score>b.score;});
        if(candidates.size()>1&&candidates.at(0).score==candidates.at(1).score){if(error)*error="存在多个同名应用入口，请指定软件完整名称或路径。";return {};}
        if(error)error->clear();return candidates.first().path;
    }
    static AppLaunchResult launch(const QString& hint,const QStringList& arguments,const std::function<bool()>& cancelled){
        AppLaunchResult out;if(cancelled()){out.result={ToolOutcome::UserCancelled,"任务已取消",{}};return out;}
        AppIdentity identity=AppRegistry::lookup(hint);HWND hwnd=existing(identity);
        if(!hwnd){
            QString error;const QString path=resolve(hint,&error);if(path.isEmpty()){out.result={ToolOutcome::FatalFailure,error,{{"code","entry_not_found"}}};return out;}
            out.executable=shortcutTarget(path);identity.processes<<QFileInfo(out.executable).fileName();
            if(cancelled()){out.result={ToolOutcome::UserCancelled,"任务已取消",{}};return out;}
            if(path.endsWith(".lnk",Qt::CaseInsensitive)){
                SHELLEXECUTEINFOW info{};info.cbSize=sizeof(info);info.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_FLAG_NO_UI;info.lpVerb=L"open";info.lpFile=reinterpret_cast<LPCWSTR>(path.utf16());info.nShow=SW_SHOWNORMAL;
                if(!ShellExecuteExW(&info)){out.result={ToolOutcome::FatalFailure,"快捷方式启动失败，Windows错误="+QString::number(GetLastError()),{}};return out;}if(info.hProcess)CloseHandle(info.hProcess);
            }else{
                QProcess process;process.setProgram(path);process.setArguments(arguments);process.setWorkingDirectory(QFileInfo(path).absolutePath());qint64 pid=0;
                if(!process.startDetached(&pid)){out.result={ToolOutcome::FatalFailure,"启动失败："+process.errorString(),{}};return out;}
            }
            QElapsedTimer timer;timer.start();const int timeout=qBound(1000,workflowConfig().value("launch_timeout_ms").toInt(15000),60000);
            while(timer.elapsed()<timeout){if(cancelled()){out.result={ToolOutcome::UserCancelled,"等待窗口时任务已取消",{}};return out;}hwnd=existing(identity);if(hwnd)break;QThread::msleep(100);}
        }
        if(cancelled()){out.result={ToolOutcome::UserCancelled,"任务已取消，未再激活窗口",{}};return out;}
        if(!hwnd){out.result={ToolOutcome::FatalFailure,"已提交启动请求，但目标窗口未在时限内出现；不能确认软件已打开。",{}};return out;}
        if(IsIconic(hwnd))ShowWindow(hwnd,SW_RESTORE);SetForegroundWindow(hwnd);
        out.hwnd=hwnd;GetWindowThreadProcessId(hwnd,&out.pid);out.title=windowTitleOf(hwnd);
        out.result={ToolOutcome::Succeeded,"已确认“"+hint+"”窗口打开。",{{"window_confirmed",true},{"pid",double(out.pid)},{"hwnd",QString::number(quintptr(hwnd))},{"title",out.title}}};return out;
    }
};

// 通用应用生命周期管理：不写死软件品牌；身份、进程、预加载全部来自配置。
class ApplicationLifecycleManager {
public:
    static bool runPreload(const AppIdentity& app,const std::function<bool()>& cancelled,QString* error){
        const QJsonObject preload=app.entry.value("preload").toObject();
        if(!preload.value("enabled").toBool(false)){if(error)error->clear();return true;}
        for(const QJsonValue& v:preload.value("commands").toArray()){
            if(cancelled()){if(error)*error="预加载期间任务已取消";return false;}
            const QJsonObject item=v.toObject();const QString program=item.value("program").toString().trimmed();if(program.isEmpty())continue;
            QStringList args;for(const QJsonValue& a:item.value("args").toArray())args<<a.toString();
            QProcess p;p.setProgram(program);p.setArguments(args);const QString wd=item.value("working_directory").toString();if(!wd.isEmpty())p.setWorkingDirectory(wd);p.start();
            const int timeout=qBound(500,item.value("timeout_ms").toInt(10000),60000);
            if(!p.waitForStarted(qMin(timeout,5000))){if(error)*error="预加载启动失败："+program+"；"+p.errorString();return false;}
            if(item.value("wait_for_finished").toBool(true)){
                if(!p.waitForFinished(timeout)){p.kill();p.waitForFinished(1000);if(error)*error="预加载超时："+program;return false;}
                if(p.exitStatus()!=QProcess::NormalExit||p.exitCode()!=0){if(error)*error=QString("预加载失败：%1 exit=%2").arg(program).arg(p.exitCode());return false;}
            }
        }
        const int settle=qBound(0,preload.value("settle_ms").toInt(0),10000);QElapsedTimer timer;timer.start();while(timer.elapsed()<settle){if(cancelled()){if(error)*error="预加载等待期间任务已取消";return false;}QThread::msleep(50);}
        if(error)error->clear();return true;
    }
    static AppLaunchResult open(const QString& hint,const QStringList& arguments,const std::function<bool()>& cancelled){
        Logger::write("APP_LIFECYCLE","stage=resolve app="+hint);
        AppLaunchResult out;const AppIdentity app=AppRegistry::lookup(hint);HWND hwnd=ApplicationLauncher::existing(app);
        if(hwnd){
            Logger::write("APP_LIFECYCLE","stage=existing_window app="+hint);
            if(IsIconic(hwnd))ShowWindow(hwnd,SW_RESTORE);SetForegroundWindow(hwnd);
            out.hwnd=hwnd;GetWindowThreadProcessId(hwnd,&out.pid);out.title=windowTitleOf(hwnd);
            out.result={ToolOutcome::Succeeded,"目标应用已经运行，已激活窗口。",{{"window_confirmed",true},{"pid",double(out.pid)},{"hwnd",QString::number(quintptr(hwnd))},{"title",out.title}}};return out;
        }
        Logger::write("APP_LIFECYCLE","stage=preload app="+hint);
        QString error;
        if(!runPreload(app,cancelled,&error)){out.result={cancelled()?ToolOutcome::UserCancelled:ToolOutcome::FatalFailure,error,{{"stage","preload"}}};return out;}
        Logger::write("APP_LIFECYCLE","stage=launch app="+hint);
        out=ApplicationLauncher::launch(hint,arguments,cancelled);
        QString readyReason;
        if(out.result.success() && !confirmReady(hint,out,&readyReason)){out.result={ToolOutcome::RetryableFailure,"应用启动尚未得到可核对的就绪证据："+readyReason,{{"stage","confirm_ready"},{"pid",double(out.pid)},{"hwnd",QString::number(quintptr(out.hwnd))}}};}
        else if(out.result.success()){Logger::write("APP_LIFECYCLE",QString("stage=ready app=%1 pid=%2 hwnd=%3 title=%4").arg(hint).arg(out.pid).arg(quintptr(out.hwnd)).arg(out.title));}
        return out;
    }
    static bool confirmReady(const QString& hint,const AppLaunchResult& launched,QString* reason=nullptr){
        const AppIdentity app=AppRegistry::lookup(hint);
        const QJsonObject lifecycle=app.entry.value("lifecycle").toObject();
        const QJsonObject success=lifecycle.value("launch_success").toObject();
        const bool allowProcessOnly=success.value("allow_process_only").toBool(false);
        const bool requireWindow=success.contains("require_window")?success.value("require_window").toBool(true):!allowProcessOnly;
        if(launched.hwnd && IsWindow(launched.hwnd)){if(reason)*reason="已确认有效顶层窗口。";return true;}
        if(!requireWindow && allowProcessOnly && launched.pid!=0){
            HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,launched.pid);
            if(process){DWORD exitCode=0;const bool alive=GetExitCodeProcess(process,&exitCode) && exitCode==STILL_ACTIVE;CloseHandle(process);if(alive){if(reason)*reason="配置允许后台/托盘应用以进程存在作为启动成功证据。";return true;}}
        }
        if(reason)*reason="没有确认有效HWND；默认GUI应用不能仅凭启动调用返回成功就标记completed。";
        return false;
    }
    static QVector<HWND> topWindows(const AppIdentity& app){
        struct Search{AppIdentity app;QVector<HWND> windows;};Search s{app,{}};
        EnumWindows([](HWND hwnd,LPARAM lp)->BOOL{auto* s=reinterpret_cast<Search*>(lp);if(!IsWindow(hwnd)||!IsWindowVisible(hwnd))return TRUE;const QString process=processExecutableName(hwnd);if(!process.isEmpty()&&AppRegistry::matches(s->app,process))s->windows.push_back(hwnd);return TRUE;},reinterpret_cast<LPARAM>(&s));return s.windows;
    }
    static bool processStillExists(const AppIdentity& app){
        if(!topWindows(app).isEmpty())return true;DWORD pids[4096]{};DWORD bytes=0;if(!EnumProcesses(pids,sizeof(pids),&bytes))return false;const int count=int(bytes/sizeof(DWORD));
        for(int i=0;i<count;++i){const DWORD pid=pids[i];if(!pid)continue;HANDLE h=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!h)continue;wchar_t path[32768]{};DWORD size=32768;QString exe;if(QueryFullProcessImageNameW(h,0,path,&size))exe=QFileInfo(QString::fromWCharArray(path)).fileName();CloseHandle(h);if(!exe.isEmpty()&&AppRegistry::matches(app,exe))return true;}return false;
    }
    static AppLaunchResult close(const QString& hint,const std::function<bool()>& cancelled){
        AppLaunchResult out;const AppIdentity app=AppRegistry::lookup(hint);QElapsedTimer timer;timer.start();const int timeout=qBound(1000,workflowConfig().value("application_close_timeout_ms").toInt(15000),60000);
        while(timer.elapsed()<timeout){if(cancelled()){out.result={ToolOutcome::UserCancelled,"关闭应用期间任务已取消。",{}};return out;}QVector<HWND> windows=topWindows(app);if(windows.isEmpty()&&!processStillExists(app)){out.result={ToolOutcome::Succeeded,"已确认“"+hint+"”完全关闭。",{{"process_closed",true}}};return out;}if(!windows.isEmpty()){HWND hwnd=windows.first();out.hwnd=hwnd;GetWindowThreadProcessId(hwnd,&out.pid);out.title=windowTitleOf(hwnd);PostMessageW(hwnd,WM_CLOSE,0,0);QThread::msleep(300);}else QThread::msleep(200);}
        const QVector<HWND> remaining=topWindows(app);if(!remaining.isEmpty()){out.hwnd=remaining.first();GetWindowThreadProcessId(out.hwnd,&out.pid);out.title=windowTitleOf(out.hwnd);out.result={ToolOutcome::NeedObservation,"应用尚未完全关闭，可能存在保存/确认/阻塞对话框，需要观察后继续关闭。",{{"remaining_window",true},{"hwnd",QString::number(quintptr(out.hwnd))},{"title",out.title}}};return out;}out.result={ToolOutcome::RetryableFailure,"关闭请求已发送，但进程仍未确认退出；需要继续检查。",{{"process_still_running",true}}};return out;
    }
};

// 支持已配置的MCP stdio与2025系列Streamable HTTP；不扫描/安装未知服务。
class McpClient {
public:
    ~McpClient(){if(process.state()!=QProcess::NotRunning){process.terminate();if(!process.waitForFinished(500)){process.kill();process.waitForFinished(500);}}}
    bool connectServer(const QJsonObject& cfg,const std::function<bool()>& stop,QString* error){
        configuration=cfg;cancelled=stop;transport=cfg.value("transport").toString("streamable_http");
        version=cfg.value("protocol_version").toString("2025-11-25");
        const QStringList supported{"2025-03-26","2025-06-18","2025-11-25"};
        if(!supported.contains(version)){if(error)*error="当前MCP客户端不支持该协议版本："+version;return false;}
        if(transport=="stdio"){
            const QString command=cfg.value("command").toString();if(command.isEmpty()){if(error)*error="MCP未配置command";return false;}
            QStringList args;for(const auto& a:cfg.value("args").toArray())args<<a.toString();process.setProgram(command);process.setArguments(args);
            process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* a){a->flags|=CREATE_NO_WINDOW;});
            QProcessEnvironment env=QProcessEnvironment::systemEnvironment();const auto extra=cfg.value("env").toObject();for(auto i=extra.begin();i!=extra.end();++i)env.insert(i.key(),i.value().toString());process.setProcessEnvironment(env);
            process.start();if(!process.waitForStarted(5000)){if(error)*error=process.errorString();return false;}processJob.attach(process.processId());
        }else if(transport!="streamable_http"){if(error)*error="不支持的MCP传输方式，未执行任何工具";return false;}
        auto result=rpc("initialize",{{"protocolVersion",version},{"capabilities",QJsonObject{}},{"clientInfo",QJsonObject{{"name","CEH-Desktop-Agent"},{"version","2.0"}}}},false,error);
        if(result.isEmpty())return false;
        const QString negotiated=result.value("protocolVersion").toString();if(!supported.contains(negotiated)){if(error)*error="MCP协议协商失败";return false;}version=negotiated;
        if(!result.value("capabilities").toObject().contains("tools")){if(error)*error="MCP服务未声明工具能力";return false;}
        rpc("notifications/initialized",{},true,error);if(error&&!error->isEmpty())return false;
        QString cursor;
        for(int page=0;page<4;++page){QJsonObject params;if(!cursor.isEmpty())params["cursor"]=cursor;result=rpc("tools/list",params,false,error);if(error&&!error->isEmpty())return false;
            for(const auto& t:result.value("tools").toArray()){const auto tool=t.toObject();const QString name=tool.value("name").toString();if(!name.isEmpty()&&tools.size()<128)tools[name]=tool;}
            cursor=result.value("nextCursor").toString();if(cursor.isEmpty())break;}
        ready=!tools.isEmpty();if(!ready&&error)*error="MCP服务没有可调用工具";return ready;
    }
    bool isReady()const{return ready;}
    QStringList toolNames()const{return tools.keys();}
    QJsonObject describe(const QString& name)const{return tools.value(name).toObject();}
    QJsonArray descriptors()const{QJsonArray out;for(auto i=tools.begin();i!=tools.end()&&out.size()<6;++i)out.append(QJsonObject{{"name",i.key()},{"description",i.value().toObject().value("description").toString().left(400)},{"inputSchema",i.value().toObject().value("inputSchema")}});return out;}
    ToolResult call(const QString& name,const QJsonObject& arguments){
        if(!ready||!tools.contains(name))return {ToolOutcome::PermissionDenied,"工具未由已连接MCP服务声明",{}};
        const QJsonObject schema=tools.value(name).toObject().value("inputSchema").toObject();
        QString validation;if(!JsonContract::validate(arguments,schema,&validation))return {ToolOutcome::RetryableFailure,"MCP参数无效："+validation,{}};
        const QByteArray identity=QCryptographicHash::hash(name.toUtf8()+QJsonDocument(arguments).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256);
        if(uncertain.contains(identity))return {ToolOutcome::PermissionDenied,"同一MCP调用的上次结果未知，禁止自动重发。",{{"uncertain",true}}};
        QString error;const QJsonObject result=rpc("tools/call",{{"name",name},{"arguments",arguments}},false,&error);
        if(!error.isEmpty()){
            const bool readOnly=tools.value(name).toObject().value("annotations").toObject().value("readOnlyHint").toBool(false);
            if(!readOnly)uncertain.insert(identity);
            return {ToolOutcome::NeedObservation,"MCP调用结果未确认："+error+"；先观察实际界面，禁止直接重发有副作用的调用。",{{"uncertain",!readOnly}}};
        }
        const bool failed=result.value("isError").toBool(false);
        return {failed?ToolOutcome::RetryableFailure:ToolOutcome::Succeeded,QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact)),result};
    }
private:
    QJsonObject configuration;QString transport,version;QByteArray sessionId,pending;QProcess process;ModelJob processJob;QMap<QString,QJsonValue> tools;QSet<QByteArray> uncertain;bool ready=false;int nextId=0;std::function<bool()> cancelled;
    QJsonObject rpc(const QString& method,const QJsonObject& params,bool notify,QString* error){
        if(error)error->clear();if(cancelled&&cancelled()){if(error)*error="任务已取消";return {};}
        const int id=++nextId;QJsonObject request{{"jsonrpc","2.0"},{"method",method},{"params",params}};if(!notify)request["id"]=id;
        const QByteArray bytes=QJsonDocument(request).toJson(QJsonDocument::Compact);if(bytes.size()>262144){if(error)*error="MCP请求过大";return {};}
        QJsonObject response;QElapsedTimer timer;timer.start();const int timeout=qBound(1000,configuration.value("timeout_ms").toInt(10000),60000);
        auto consume=[&](QByteArray line){line=line.trimmed();if(line.startsWith("data:"))line=line.mid(5).trimmed();const auto doc=QJsonDocument::fromJson(line);if(doc.isObject()&&doc.object().value("id")==QJsonValue(id))response=doc.object();};
        if(transport=="stdio"){
            if(process.state()==QProcess::NotRunning){if(error)*error="MCP进程已退出";return {};}
            process.write(bytes+'\n');if(process.bytesToWrite()>0&&!process.waitForBytesWritten(1000)){if(error)*error="MCP写入失败";return {};}
            if(notify)return {};
            while(timer.elapsed()<timeout&&response.isEmpty()){
                if(cancelled&&cancelled()){if(error)*error="任务已取消";return {};}
                process.waitForReadyRead(50);pending+=process.readAllStandardOutput();const auto diagnostic=process.readAllStandardError();if(!diagnostic.isEmpty())Logger::write("MCP_STDERR",QString::fromUtf8(diagnostic).left(4096));
                if(pending.size()>2097152){if(error)*error="MCP响应过大";return {};}
                int nl;while((nl=pending.indexOf('\n'))>=0){consume(pending.left(nl));pending.remove(0,nl+1);}
                if(process.state()==QProcess::NotRunning&&response.isEmpty())break;
            }
        }else{
            const QUrl endpoint(configuration.value("endpoint").toString());if((endpoint.scheme()!="http"&&endpoint.scheme()!="https")||endpoint.host().isEmpty()){if(error)*error="MCP endpoint无效";return {};}
            QNetworkAccessManager manager;QNetworkRequest req(endpoint);req.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");req.setRawHeader("Accept","application/json, text/event-stream");req.setRawHeader("MCP-Protocol-Version",version.toUtf8());if(!sessionId.isEmpty())req.setRawHeader("Mcp-Session-Id",sessionId);
            const auto headers=configuration.value("headers").toObject();for(auto i=headers.begin();i!=headers.end();++i)req.setRawHeader(i.key().toUtf8(),i.value().toString().toUtf8());
            QNetworkReply* reply=manager.post(req,bytes);QEventLoop loop;QTimer poll;poll.setInterval(50);QByteArray received;QString failure;
            QObject::connect(reply,&QNetworkReply::readyRead,&loop,[&]{received+=reply->readAll();const auto doc=QJsonDocument::fromJson(received);if(doc.isObject())consume(received);else{int pos=0,nl=0;while((nl=received.indexOf('\n',pos))>=0){consume(received.mid(pos,nl-pos));pos=nl+1;}}if(!response.isEmpty())loop.quit();});
            QObject::connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);
            QObject::connect(&poll,&QTimer::timeout,&loop,[&]{if(cancelled&&cancelled())failure="任务已取消";else if(timer.elapsed()>timeout)failure="MCP请求超时";else if(received.size()>2097152)failure="MCP响应过大";if(!failure.isEmpty()){reply->abort();loop.quit();}});
            poll.start();if(!reply->isFinished()&&response.isEmpty())loop.exec();poll.stop();received+=reply->readAll();if(response.isEmpty())consume(received);
            const int code=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();const QByteArray assigned=reply->rawHeader("Mcp-Session-Id");if(!assigned.isEmpty())sessionId=assigned;
            if(!reply->isFinished())reply->abort();
            if(!failure.isEmpty()||code<200||code>=300){if(error)*error=failure.isEmpty()?QString("MCP HTTP错误 %1").arg(code):failure;reply->deleteLater();return {};}
            reply->deleteLater();if(notify)return {};
        }
        if(response.isEmpty()){if(error)*error="MCP没有返回匹配的JSON-RPC结果";return {};}
        if(response.contains("error")){if(error)*error=QString::fromUtf8(QJsonDocument(response.value("error").toObject()).toJson(QJsonDocument::Compact));return {};}
        return response.value("result").toObject();
    }
};

struct VisionObservation {
    QJsonObject data;
    static bool parse(const QString& text,const QString& frame,int width,int height,QJsonObject* result,QString* error){
        QJsonObject object=strictJsonObject(text,error);if(object.isEmpty())return false;
        if(!object.value("targets").isArray()||!object.value("visible_text").isArray()){if(error)*error="视觉结果缺少targets或visible_text数组";return false;}
        QJsonArray targets;
        for(const auto& v:object.value("targets").toArray()){
            const auto t=v.toObject();const auto x=t.value("norm_x"),y=t.value("norm_y");const double nx=x.toDouble(-1),ny=y.toDouble(-1),confidence=t.value("confidence").toDouble(-1);
            if(t.value("label").toString().isEmpty()||!x.isDouble()||!y.isDouble()||!std::isfinite(nx)||!std::isfinite(ny)||nx<0||nx>1||ny<0||ny>1||confidence<0||confidence>1){if(error)*error="视觉目标坐标、名称或置信度无效";return false;}
            QJsonArray bounds=t.value("norm_rect").toArray();
            if(!bounds.isEmpty()){
                if(bounds.size()!=4){if(error)*error="视觉目标矩形不完整";return false;}
                for(const auto& n:bounds)if(!n.isDouble()||!std::isfinite(n.toDouble())||n.toDouble()<0||n.toDouble()>1){if(error)*error="视觉矩形坐标无效";return false;}
                if(bounds.at(0).toDouble()>=bounds.at(2).toDouble()||bounds.at(1).toDouble()>=bounds.at(3).toDouble()){if(error)*error="视觉矩形起终点无效";return false;}
            }
            if(targets.size()<6)targets.append(QJsonObject{{"label",t.value("label").toString().left(100)},{"norm_x",nx},{"norm_y",ny},{"confidence",confidence},{"description",t.value("description").toString().left(160)},{"norm_rect",bounds}});
        }
        QJsonArray visible;
        for(const auto& v:object.value("visible_text").toArray())
            if(visible.size()<24) visible.append(v.toString().left(120));

        auto cleanRelevantText = [](QString text) {
            text = text.trimmed();
            const QString normalized = text.toLower().remove(' ').remove('\t').remove('\r').remove('\n');
            static const QStringList emptyMarkers{"无", "没有", "暂无", "无内容", "无消息", "none", "null", "n/a", "na", "-", "--"};
            if (emptyMarkers.contains(normalized)) return QString();
            return text.left(1000);
        };

        QJsonObject taskContext;
        if(object.value("task_context").isObject()){
            const QJsonObject raw = object.value("task_context").toObject();
            taskContext = {{"summary", raw.value("summary").toString().left(500)},{"latest_relevant_text", cleanRelevantText(raw.value("latest_relevant_text").toString())},{"external_change_detected", raw.value("external_change_detected").toBool(false)}};
        }else{
            taskContext = {{"summary", object.value("screen_state").toString().left(500)},{"latest_relevant_text",QString()},{"external_change_detected",false}};
        }

        *result=QJsonObject{{"frame_id",frame},{"image_width",width},{"image_height",height},{"screen_state", object.value("screen_state").toString().left(240)},{"task_context",taskContext},{"targets",targets},{"visible_text",visible},{"valid",true}};
        if(error)error->clear();return true;
    }
};
class MainWindow : public QWidget {
public:
    explicit MainWindow(bool startRuntime = true) {
        setWindowTitle(QString::fromUtf8(CEH_APP_NAME));
        setWindowIcon(QIcon(":/icons/assets/img/icon/favicon_32x32.ico"));
        setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
        resize(500, 1000); setMinimumSize(500, 800); setMaximumWidth(500);
        buildUi();
        if (!startRuntime) return;
        createTray();
        log("[日志] 实时保存：" + Logger::filePath());
        if (!Logger::previousPath().isEmpty()) log("[日志] 上次记录：" + Logger::previousPath() + "（日志区右键打开）");
        auto* protectionTimer = new QTimer(this); protectionTimer->setInterval(250);
        connect(protectionTimer, &QTimer::timeout, this, [this] { pollRuntimeProtection(); }); protectionTimer->start();
        const QJsonObject interruptCfg = loadConfig("assets/config/agent.json").value("user_interrupt").toObject();
        m_userInputMonitor.setInterruptFlag(&m_userInterrupted);
        m_userInputMonitor.setTaskActiveFlag(&m_controlActive);
        m_userInputMonitor.configurePolicy(interruptCfg);
        m_userInputMonitor.configure(interruptCfg.value("sustain_ms").toInt(5000), interruptCfg.value("continuity_gap_ms").toInt(700));
        if (!m_userInputMonitor.start()) Logger::write("WARN", "用户输入监控安装失败");
        setAppState(AppState::Checking);
        addMessage(MessageRole::Ai, "您好！有什么可以帮助您的！");
        if (!startWorkers()) { setAppState(AppState::Failed, "Worker线程初始化失败"); addMessage(MessageRole::System, "后台工作线程初始化失败，请查看运行日志。"); return; }
        QTimer::singleShot(120, this, [this] { startupSequence(); });
    }
    ~MainWindow() override { m_acceptNewTasks = false; m_stop = true; m_userInputMonitor.stop(); if (!m_shutdownStarted) cleanupWorkersBestEffort(); }
protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == m_titleBar) {
            if (event->type() == QEvent::MouseButtonPress) {
                auto* e = static_cast<QMouseEvent*>(event);
                if (e->button() == Qt::LeftButton) { m_dragging = true; m_dragOffset = e->globalPosition().toPoint() - frameGeometry().topLeft(); return true; }
            } else if (event->type() == QEvent::MouseMove && m_dragging) { auto* e = static_cast<QMouseEvent*>(event); move(e->globalPosition().toPoint() - m_dragOffset); return true; }
            else if (event->type() == QEvent::MouseButtonRelease) { m_dragging = false; return true; }
        }
        if (watched == m_input && event->type() == QEvent::KeyPress) {
            auto* key = static_cast<QKeyEvent*>(event);
            const bool enter = key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter;
            if (enter) { if (key->modifiers().testFlag(Qt::ShiftModifier)) return false; sendTask(); return true; }
        }
        return QWidget::eventFilter(watched, event);
    }
    void closeEvent(QCloseEvent* event) override { event->ignore(); hideToTray(); }
private:
    struct RuntimeAppProfile {
        QString processProfile = "generic_application";
        QString surfaceType = "unknown";
        QString taskProfile = "operate_application";
        QString preferredObservation = "uia_then_vision";
        QStringList likelyTargets;
        QStringList preferredActionFamilies{"mcp", "uia", "vision", "mouse", "keyboard"};
        double confidence = 0.35;
        bool dynamicGenerated = false;
        QString source = "fallback";
        QString rationale;

        QJsonObject json() const {
            QJsonArray targets; for (const auto& v : likelyTargets) targets.append(v);
            QJsonArray families; for (const auto& v : preferredActionFamilies) families.append(v);
            return {{"process_profile", processProfile},{"surface_type", surfaceType},{"task_profile", taskProfile},{"preferred_observation", preferredObservation},{"likely_targets", targets},{"preferred_action_families", families},{"confidence", confidence},{"dynamic_generated", dynamicGenerated},{"source", source},{"rationale", rationale.left(600)}};
        }
    };

    struct AgentSession {
        AgentScope scope = AgentScope::Desktop;
        HWND targetHwnd = nullptr; DWORD targetPid = 0;
        QString targetTitle; QString targetProcess; QRect targetRect;
        QVector<UIAItem> uiaItems;
        QString lastScreenshot; QRect lastScreenshotRect;
        QString contextSummary; QJsonArray recentHistory;
        QString taskId; QString taskMode = "ONE_SHOT";
        int uiaPromptLimit = 120; int compactRound = 0;
        QString goal; bool restored = false; quint64 cancellationGeneration = 0;
        int stepNumber = 0; QString status = "running";
        QString lastActionKey; int repeatedActions = 0; int observationOnlySteps = 0; int failures = 0;
        QString lastVisionFingerprint; QString cachedVision;
        QJsonObject pendingAction; TaskRequest request; WorkflowController workflow;
        AgentScope originScope = AgentScope::Desktop; ToolResult lastResult;
        QJsonObject latestVision,latestMcp; QString frameId, observationId;
        std::shared_ptr<McpClient> mcp;
        bool cancelledByUser = false; bool fatalFailure = false; bool preferVision = false;
        QString observationPolicy = "uia_then_vision";
        RuntimeAppProfile runtimeProfile; bool runtimeProfileReady = false;
        bool actionReady = false; int plannerObservationViolations = 0; int genericNoProgressCount = 0;
        QString lastWorldStateHash;
        AgentPolicyPhase policyPhase = AgentPolicyPhase::ObserveOrVerify;
        bool waitingForExternalChange = false; QString waitForChangeBaselineFingerprint; int unchangedWaitCycles = 0;
        QString latestTaskContextSummary; QString latestRelevantText; bool latestExternalChangeDetected = false;
        bool hasSentInteraction = false; QString externalRelevantTextBaseline;
        QString lastResolvedTargetLabel; QString lastActionResolutionNote;
        QJsonArray recentProgress; QString lastSideEffectActionKey; int repeatedSideEffectBlocks = 0;
        quint64 interactionEpoch = 0; QString lastInteractionFingerprint;
        QJsonObject pendingSemanticAction; QString groundingTarget; int semanticGroundingAttempts = 0;
        bool pendingCommitRequired = false; bool pendingCommitDispatched = false; bool pendingCommitAwaitingVerification = false;
        QString pendingCommitBeforeFingerprint; int pendingCommitVerificationObservations = 0;
        QString pendingCommitLabel; QString lastTypedText;
        int pendingCommitClickAttempts = 0; QPoint pendingCommitLastClickPoint{-1, -1};
        double pendingCommitLastNormX = -1.0; double pendingCommitLastNormY = -1.0;
        bool mcpChecked = false; bool mcpUsable = false; QString mcpFailureReason;
        bool uiaChecked = false; bool uiaUsable = true; int uiaNodeCount = 0; int uiaActionableCount = 0; QString uiaFailureReason;
        bool visionRequired = false; QString controlChannel = "unknown";
        QSize lastCaptureSize; QJsonArray knownVisualTargets;
        QString targetEntity; QString currentContact; QString conversationState; QString nextObjective;
        int consecutiveVisionObservations = 0; int consecutiveGroundingFailures = 0;
        QString lastGroundingTarget; QString lastVisionSemanticHash; bool groundingRequested = false;
        bool uncertainSideEffect = false; QString desiredApplication; QString lastEvidenceHash;
    };

    QWidget* m_titleBar = nullptr;
    QScrollArea* m_chatScroll = nullptr; QWidget* m_chatContainer = nullptr; QVBoxLayout* m_chatLayout = nullptr;
    QPlainTextEdit* m_log = nullptr; QTextEdit* m_input = nullptr; QPushButton* m_send = nullptr;
    QLabel* m_runtimeStatus = nullptr; QLabel* m_modelStatus = nullptr; QLabel* m_llmStatus = nullptr;
    QLabel* m_vlmStatus = nullptr; QLabel* m_uiaStatus = nullptr; QPushButton* m_statusSettings = nullptr;
    QToolButton* m_uploadTool = nullptr; QToolButton* m_screenshotTool = nullptr;
    QToolButton* m_desktopTool = nullptr; QToolButton* m_windowTool = nullptr;
    QSystemTrayIcon* m_tray = nullptr;
    WorkerExecutor m_agentExecutor, m_modelExecutor, m_uiaExecutor, m_ioExecutor;
    ModelManager* m_models = nullptr;
    UIAutomationEngine* m_agentUia = nullptr; UIAutomationEngine* m_overlayUia = nullptr;
    HRESULT m_uiaWorkerComResult = E_FAIL;
    QJsonArray m_chatHistory;
    bool m_dragging = false; QPoint m_dragOffset;
    std::atomic_bool m_running{false}, m_controlActive{false}, m_stop{false}, m_userInterrupted{false};
    std::atomic_bool m_acceptNewTasks{true}, m_passiveScanInFlight{false}, m_shutdownStarted{false};
    AppState m_state = AppState::Checking;
    WorkMode m_workMode = WorkMode::Chat; WorkMode m_modeBeforeScreenshot = WorkMode::Chat;
    HWND m_selectedHwnd = nullptr; DWORD m_selectedWindowPid = 0;
    QString m_selectedWindowTitle; QRect m_selectedWindowRect;
    QTimer* m_pickTimer = nullptr; qint64 m_pickStart = 0; bool m_pickMouseDownSeen = false;
    QTimer* m_desktopRefreshTimer = nullptr; QTimer* m_windowRectTimer = nullptr; QTimer* m_windowUiaTimer = nullptr;
    UiaOverlayWidget* m_desktopOverlay = nullptr; UiaOverlayWidget* m_windowOverlay = nullptr;
    UserInputMonitor m_userInputMonitor; QThread* m_shutdownThread = nullptr;

    void postUi(std::function<void()> fn) { if (QThread::currentThread() == thread()) { fn(); return; } QMetaObject::invokeMethod(this, [fn = std::move(fn)]() mutable { fn(); }, Qt::QueuedConnection); }
    void runUiBlocking(std::function<void()> fn) { if (QThread::currentThread() == thread()) { fn(); return; } QMetaObject::invokeMethod(this, [fn = std::move(fn)]() mutable { fn(); }, Qt::BlockingQueuedConnection); }
    bool confirmOnUi(const QString& title, const QString& body) { bool accepted = false; runUiBlocking([this, &accepted, title, body] { accepted = customConfirm(this, title, body); }); return accepted; }
    bool startWorkers() {
        log("[启动] 正在启动后台工作线程...");
        if (!m_agentExecutor.start("CEH-AgentWorker")) return false;
        log("[启动] Agent Worker 启动成功");
        if (!m_modelExecutor.start("CEH-ModelWorker")) return false;
        log("[启动] Model Worker 启动成功");
        if (!m_uiaExecutor.start("CEH-UIAWorker")) return false;
        log("[启动] UIA Worker 启动成功");
        if (!m_ioExecutor.start("CEH-IOWorker")) return false;
        log("[启动] IO Worker 启动成功");
        const bool modelOk = m_modelExecutor.call([this] { m_models = new ModelManager(); return m_models != nullptr; });
        if (!modelOk) return false;
        const bool uiaOk = m_uiaExecutor.call([this] {
            m_uiaWorkerComResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            if (FAILED(m_uiaWorkerComResult) && m_uiaWorkerComResult != RPC_E_CHANGED_MODE) return false;
            m_agentUia = new UIAutomationEngine(); m_overlayUia = new UIAutomationEngine();
            return m_agentUia && m_overlayUia && m_agentUia->valid() && m_overlayUia->valid();
        });
        if (!uiaOk) return false;
        log("[启动] UIAutomationEngine 加载成功"); return true;
    }
    void cleanupWorkersBestEffort() {
        if (m_modelExecutor.isRunning()) { m_modelExecutor.call([this] { if (m_models) { m_models->stopAll(); delete m_models; m_models = nullptr; } }); }
        if (m_uiaExecutor.isRunning()) { m_uiaExecutor.call([this] { delete m_agentUia; m_agentUia = nullptr; delete m_overlayUia; m_overlayUia = nullptr; if (SUCCEEDED(m_uiaWorkerComResult)) CoUninitialize(); m_uiaWorkerComResult = E_FAIL; }); }
        m_agentExecutor.stop(3000); m_modelExecutor.stop(5000); m_uiaExecutor.stop(3000); m_ioExecutor.stop(2000);
    }
    void buildUi() {
        setStyleSheet(fallbackStyle());
        QString qssPath = writablePath("assets/style/main.qss"); if (!QFileInfo::exists(qssPath)) qssPath = resourcePath("assets/style/main.qss");
        const QString qss = readText(qssPath); if (!qss.trimmed().isEmpty()) setStyleSheet(qss);
        auto* root = new QVBoxLayout(this); root->setContentsMargins(0, 0, 0, 0); root->setSpacing(0);
        root->addWidget(createTop()); root->addWidget(createChat(), 1); root->addWidget(createStatus()); root->addWidget(createLog()); root->addWidget(createComposer());
    }
    QWidget* createTop() {
        m_titleBar = new QWidget(); m_titleBar->setObjectName("titleBar"); m_titleBar->setFixedHeight(60); m_titleBar->installEventFilter(this);
        auto* layout = new QHBoxLayout(m_titleBar); layout->setContentsMargins(10, 0, 10, 0); layout->setSpacing(10);
        auto* icon = new QLabel(); icon->setFixedSize(32, 32);
        QPixmap pix(":/icons/assets/img/icon/favicon_32x32.ico"); icon->setPixmap(pix.scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        auto* title = new QLabel("木头自动系统控制"); title->setObjectName("mainTitle");
        QFont titleFont(g_boldFontFamily); titleFont.setPixelSize(16); title->setFont(titleFont);
        auto* hide = new QPushButton(); hide->setObjectName("titleHide"); hide->setFixedSize(20, 20);
        auto* close = new QPushButton(); close->setObjectName("titleClose"); close->setFixedSize(20, 20);
        layout->addWidget(icon); layout->addWidget(title); layout->addStretch(); layout->addWidget(hide); layout->addSpacing(8); layout->addWidget(close);
        connect(hide, &QPushButton::clicked, this, &MainWindow::hideToTray);
        connect(close, &QPushButton::clicked, [this] { if (confirmExitProgram(this)) shutdown(); });
        return m_titleBar;
    }
    QWidget* createChat() {
        auto* panel = new QWidget(); panel->setObjectName("chatPanel");
        auto* panelLayout = new QVBoxLayout(panel); panelLayout->setContentsMargins(10, 8, 10, 8); panelLayout->setSpacing(0);
        m_chatScroll = new QScrollArea(); m_chatScroll->setObjectName("chatScroll"); m_chatScroll->setWidgetResizable(true);
        m_chatScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); m_chatScroll->setFrameShape(QFrame::NoFrame);
        m_chatContainer = new QWidget(); m_chatContainer->setObjectName("chatContainer");
        m_chatLayout = new QVBoxLayout(m_chatContainer); m_chatLayout->setContentsMargins(10, 2, 10, 2); m_chatLayout->setSpacing(1); m_chatLayout->addStretch(1);
        m_chatScroll->setWidget(m_chatContainer); panelLayout->addWidget(m_chatScroll);
        return panel;
    }
    QLabel* createStatusLabel(const QString& name) { auto* label = new QLabel(); label->setObjectName("statusItem"); setStatus(label, name, "#898989"); return label; }
    void setStatus(QLabel* label, const QString& text, const QString& color) { if (!label) return; label->setText(QString("<span style='color:%1;font-size:17px;'>●</span>" "<span style='color:#9b9b9b;'> %2</span>").arg(color, text)); }
    QWidget* createStatus() {
        auto* panel = new QWidget(); panel->setObjectName("statusBar"); panel->setFixedHeight(46);
        auto* layout = new QHBoxLayout(panel); layout->setContentsMargins(10, 0, 10, 0); layout->setSpacing(9);
        m_runtimeStatus = createStatusLabel("Runtime"); m_modelStatus = createStatusLabel("Model"); m_llmStatus = createStatusLabel("LLM");
        m_vlmStatus = createStatusLabel("VLM"); m_uiaStatus = createStatusLabel("UIA");
        layout->addWidget(m_runtimeStatus); layout->addWidget(m_modelStatus); layout->addWidget(m_llmStatus); layout->addWidget(m_vlmStatus); layout->addWidget(m_uiaStatus); layout->addStretch();
        m_statusSettings = new QPushButton(); m_statusSettings->setObjectName("statusSettings"); m_statusSettings->setFixedSize(30, 30);
        m_statusSettings->setIcon(QIcon(":/icons/assets/uppic/Settings.png")); m_statusSettings->setIconSize(QSize(22, 22));
        layout->addWidget(m_statusSettings);
        connect(m_statusSettings, &QPushButton::clicked, this, &MainWindow::openSettings);
        return panel;
    }
    QWidget* createLog() {
        auto* panel = new QWidget(); panel->setObjectName("logPanel"); panel->setFixedHeight(198);
        auto* layout = new QVBoxLayout(panel); layout->setContentsMargins(10, 8, 10, 8);
        m_log = new QPlainTextEdit(); m_log->setObjectName("logView"); m_log->setReadOnly(true);
        m_log->setMaximumBlockCount(2000); m_log->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_log, &QWidget::customContextMenuRequested, this, [this](const QPoint& point) {
            QMenu menu; auto* current = menu.addAction("打开本次日志"); auto* previous = menu.addAction("打开上次日志");
            previous->setEnabled(QFileInfo::exists(Logger::previousPath())); auto* history = menu.addAction("选择历史日志"); auto* folder = menu.addAction("打开日志目录");
            QAction* selected = menu.exec(m_log->mapToGlobal(point));
            if (selected == current) QDesktopServices::openUrl(QUrl::fromLocalFile(Logger::filePath()));
            else if (selected == previous) QDesktopServices::openUrl(QUrl::fromLocalFile(Logger::previousPath()));
            else if (selected == folder) QDesktopServices::openUrl(QUrl::fromLocalFile(Logger::root()));
            else if (selected == history) { const QString path = QFileDialog::getOpenFileName(this, "查看历史日志", Logger::root(), "日志 (*.txt *.log)"); if (!path.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(path)); }
        });
        m_log->setToolTip("日志实时保存。右键可打开本次、上次和历史日志。"); m_log->setLineWrapMode(QPlainTextEdit::WidgetWidth);
        layout->addWidget(m_log);
        return panel;
    }
    QWidget* createComposer() {
        auto* panel = new QWidget(); panel->setObjectName("composerPanel"); panel->setFixedHeight(168);
        auto* outer = new QVBoxLayout(panel); outer->setContentsMargins(10, 8, 10, 8); outer->setSpacing(0);
        auto* inputCard = new QWidget(); inputCard->setObjectName("inputCard");
        auto* card = new QVBoxLayout(inputCard); card->setContentsMargins(10, 8, 10, 6); card->setSpacing(4);
        m_input = new QTextEdit(); m_input->setObjectName("messageEdit"); m_input->setAcceptRichText(false);
        m_input->setPlaceholderText("请输入..."); m_input->setFrameShape(QFrame::NoFrame);
        m_input->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded); m_input->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_input->installEventFilter(this); card->addWidget(m_input, 1);
        auto* row = new QHBoxLayout(); row->setContentsMargins(0, 0, 0, 0); row->setSpacing(8);
        auto makeTool = [&](const QString& icon, const QString& tip, auto callback) {
            auto* button = new QToolButton(); button->setObjectName("composerTool"); button->setFixedSize(34, 34);
            button->setIcon(QIcon(icon)); button->setIconSize(QSize(26, 26)); button->setToolTip(tip);
            row->addWidget(button); connect(button, &QToolButton::clicked, callback); return button;
        };
        m_uploadTool = makeTool(":/icons/assets/uppic/UploadImage.png", "上传图片", [this] { uploadImage(); });
        m_screenshotTool = makeTool(":/icons/assets/uppic/Screenshot.png", "截图识别", [this] { screenshotRecognition(); });
        m_desktopTool = makeTool(":/icons/assets/uppic/DesktopRecognition.png", "桌面识别", [this] { desktopRecognition(); });
        m_windowTool = makeTool(":/icons/assets/uppic/SpecifyIdentification.png", "指定识别", [this] { beginPick(); });
        row->addStretch();
        m_send = new QPushButton("模型加载中"); m_send->setObjectName("sendButton");
        m_send->setMinimumWidth(74); m_send->setMaximumWidth(90); m_send->setFixedHeight(34);
        row->addWidget(m_send);
        card->addLayout(row); outer->addWidget(inputCard);
        connect(m_send, &QPushButton::clicked, this, &MainWindow::sendTask);
        refreshModeButtons();
        return panel;
    }
    QString fallbackStyle() const {
        return R"QSS(
            QWidget{background:#181818;color:#ffffff;font-size:12px;}
            #titleBar,#statusBar,#bottomBar{background:#141414;}
            #titleHide{background:#40c977;border-radius:10px;}
            #titleClose{background:#ff6764;border-radius:10px;}
            #chatPanel,#chatContainer,#chatScroll{background:#181818;border:none;}
            #sendButton{background:#2c67c5;color:#ffffff;border:none;border-radius:4px;}
        )QSS";
    }
    void log(const QString& text) {
        Logger::write("UI", text);
        if (QThread::currentThread() != thread()) { postUi([this, text] { appendLogUi(text); }); return; }
        appendLogUi(text);
    }
    void appendLogUi(const QString& text) {
        if (!m_log) return;
        m_log->appendPlainText("[" + nowTime() + "] " + text);
        m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
    }
    ChatMessageWidget* addMessage(MessageRole role, const QString& text) {
        if (QThread::currentThread() != thread()) { postUi([this, role, text] { addMessage(role, text); }); return nullptr; }
        const int viewportWidth = m_chatScroll && m_chatScroll->viewport() ? m_chatScroll->viewport()->width() : 470;
        const int width = role == MessageRole::System ? qMax(220, qRound(viewportWidth * 0.80)) : qMax(250, qRound(viewportWidth * 0.72));
        auto* message = new ChatMessageWidget(role, text, width, m_chatContainer);
        m_chatLayout->insertWidget(m_chatLayout->count() - 1, message);
        scrollChatToBottom();
        return message;
    }
    void addImageMessage(MessageRole role, const QString& path, const QString& caption = {}) {
        if (QThread::currentThread() != thread()) { postUi([this, role, path, caption] { addImageMessage(role, path, caption); }); return; }
        const int viewportWidth = m_chatScroll && m_chatScroll->viewport() ? m_chatScroll->viewport()->width() : 470;
        const int width = qMax(250, qRound(viewportWidth * 0.72));
        auto* message = new ChatMessageWidget(role, caption, width, m_chatContainer);
        message->setImagePath(path);
        m_chatLayout->insertWidget(m_chatLayout->count() - 1, message);
        scrollChatToBottom();
    }
    void scrollChatToBottom() {
        QTimer::singleShot(0, this, [this] { if (!m_chatScroll) return; m_chatScroll->verticalScrollBar()->setValue(m_chatScroll->verticalScrollBar()->maximum()); });
    }
    void setAppState(AppState state, const QString& reason = {}) {
        if (QThread::currentThread() != thread()) { postUi([this, state, reason] { setAppState(state, reason); }); return; }
        m_state = state;
        switch (state) {
            case AppState::Checking: m_send->setText("系统检查中"); m_send->setEnabled(false); m_input->setEnabled(false); break;
            case AppState::LoadingModel: m_send->setText("模型加载中"); m_send->setEnabled(false); m_input->setEnabled(false); break;
            case AppState::Ready: m_send->setText("发送"); m_send->setEnabled(true); m_input->setEnabled(true); m_input->setFocus(); break;
            case AppState::Generating: m_send->setText("生成中"); m_send->setEnabled(false); m_input->setEnabled(false); break;
            case AppState::Failed: m_send->setText("加载失败"); m_send->setEnabled(false); m_input->setEnabled(false); break;
        }
        if (!reason.isEmpty()) log(reason);
    }
    void refreshModeButtons() {
        auto refresh = [](QToolButton* button, bool active) {
            if (!button) return;
            button->setProperty("active", active);
            button->style()->unpolish(button); button->style()->polish(button); button->update();
        };
        refresh(m_desktopTool, m_workMode == WorkMode::DesktopControl);
        refresh(m_windowTool, m_workMode == WorkMode::WindowControl);
    }
    QJsonObject agentRoot() const { return loadConfig("assets/config/agent.json"); }
    QJsonObject agentCore() const { return agentRoot().value("agent").toObject(); }
    QJsonObject contextCfg() const { return agentRoot().value("context").toObject(); }
    QJsonObject desktopCfg() const { return agentRoot().value("control_scope").toObject().value("desktop_control").toObject(); }
    QJsonObject windowCfg() const { return agentRoot().value("control_scope").toObject().value("window_control").toObject(); }
    QJsonObject cameraRoot() const { return loadConfig("assets/config/camera.json"); }
    QJsonObject overlayCfg() const { return cameraRoot().value("uia_overlay").toObject(); }
    void startupSequence() {
        if (m_shutdownStarted) return;
        QString safetyError;
        if (!RuntimeSafety::instance().beginTask(&safetyError)) { setAppState(AppState::Failed, safetyError); return; }
        setAppState(AppState::Checking);
        log("[启动] 配置加载成功");
        if (QSqlDatabase::contains("ceh")) {
            const QSqlDatabase db = QSqlDatabase::database("ceh");
            log(db.isValid() && db.isOpen() ? "[启动] SQLite / wood.db 打开成功" : "[启动] SQLite连接存在，但当前未打开");
        } else log("[启动] 未检测到SQLite连接ceh");
        const bool uiaReady = m_uiaExecutor.call([this] { return m_agentUia && m_overlayUia && m_agentUia->valid() && m_overlayUia->valid(); });
        setStatus(m_uiaStatus, "UIA", uiaReady ? "#40c977" : "#ff6764");
        if (!uiaReady) { setAppState(AppState::Failed, "[启动] UIA初始化失败"); return; }
        log("[启动] UIA Worker / UIAutomation 加载成功");
        const QJsonObject models = loadConfig("assets/config/models.json");
        const QString active = models.value("active_profile").toString("local_gemma");
        const QJsonObject profile = models.value("profiles").toObject().value(active).toObject();
        const QJsonObject llm = profile.value("llm").toObject();
        const QString provider = llm.value("provider").toString("llama_cpp");
        QString runtimeName = provider; QString modelPath;
        if (provider == "llama_cpp") { const QJsonObject cfg = llm.value("llama_cpp").toObject(); runtimeName = cfg.value("runtime").toString("cuda13"); modelPath = cfg.value("model").toString(); }
        setStatus(m_runtimeStatus, runtimeName.toUpper(), "#40c977");
        QString modelName = QFileInfo(modelPath).baseName();
        if (modelName.isEmpty()) modelName = provider;
        if (modelName.size() > 15) modelName = modelName.left(12) + "...";
        setStatus(m_modelStatus, modelName, "#c0c0c1"); setStatus(m_llmStatus, "LLM", "#ffd240"); setStatus(m_vlmStatus, "VLM", "#ffd240");
        setAppState(AppState::LoadingModel); log("[启动] LLM服务启动中...");
        m_modelExecutor.post([this] {
            if (!m_models) { postUi([this] { setStatus(m_llmStatus, "LLM", "#ff6764"); setAppState(AppState::Failed, "[启动] ModelManager不可用"); }); return; }
            QString llmError; const bool llmReady = m_models->ensureReady("llm", &llmError);
            postUi([this, llmReady, llmError] {
                if (!llmReady) { setStatus(m_llmStatus, "LLM", "#ff6764"); setAppState(AppState::Failed, "[启动] LLM模型加载失败：" + llmError); return; }
                setStatus(m_llmStatus, "LLM", "#40c977"); log("[启动] LLM服务启动成功"); log("[启动] LLM模型加载成功"); log("[启动] 检查VLM按需加载策略...");
            });
            if (!llmReady) return;
            QString vlmError; const bool lazyVlm = RuntimeSafety::config().value("single_model_resident").toBool(true);
            const bool vlmReady = lazyVlm || m_models->ensureReady("vlm", &vlmError);
            postUi([this, vlmReady, vlmError] {
                if (!vlmReady) { setStatus(m_vlmStatus, "VLM", "#ff6764"); setAppState(AppState::Failed, "[启动] VLM模型加载失败：" + vlmError); return; }
                setStatus(m_vlmStatus, "VLM", "#40c977");
                m_workMode = WorkMode::Chat; refreshModeButtons();
                log("[启动] VLM按需加载已就绪；默认与LLM互斥驻留，实际分析图片时再加载。");
                log("[启动] Agent Worker 已就绪"); log("[启动] 系统已就绪");
                setAppState(AppState::Ready);
            });
        });
    }
    void openSettings() {
        if (m_state == AppState::LoadingModel || m_state == AppState::Generating || m_state == AppState::Checking) { addMessage(MessageRole::System, "请等待当前模型操作完成后再修改配置，避免加载过程重入。"); return; }
        if (m_running) { addMessage(MessageRole::System, "当前Agent正在执行任务，请先停止任务再修改系统配置。"); return; }
        SettingsDialog dialog(this);
        if (dialog.exec() != QDialog::Accepted) return;
        log("[设置] 配置已保存，正在重新加载模型...");
        stopDesktopControl(false); stopWindowControl(false);
        const QJsonObject cfg = loadConfig("assets/config/app.json").value("window").toObject();
        resize(qMax(420, cfg.value("width").toInt(width())), qMax(700, cfg.value("height").toInt(height())));
        setAppState(AppState::LoadingModel);
        m_modelExecutor.post([this] { if (m_models) { m_models->stopAll(); m_models->reload(); } postUi([this] { startupSequence(); }); });
    }
    void createTray() {
        if (!QSystemTrayIcon::isSystemTrayAvailable()) return;
        m_tray = new QSystemTrayIcon(this); m_tray->setIcon(QIcon(":/icons/assets/img/icon/favicon_32x32.ico")); m_tray->setToolTip("木头自动系统控制");
        auto* menu = new QMenu();
        auto* showAction = menu->addAction("显示主窗口"); auto* hideAction = menu->addAction("隐藏主窗口");
        menu->addSeparator();
        auto* desktopAction = menu->addAction("桌面识别"); auto* screenshotAction = menu->addAction("截图识别");
        auto* stopAction = menu->addAction("停止当前任务"); auto* resumeAction = menu->addAction("恢复上次未完成任务");
        menu->addSeparator();
        auto* settingsAction = menu->addAction("系统设置"); auto* configAction = menu->addAction("打开配置目录");
        auto* logAction = menu->addAction("打开日志目录"); auto* dbAction = menu->addAction("打开数据库目录");
        menu->addSeparator();
        auto* quitAction = menu->addAction("退出程序");
        connect(showAction, &QAction::triggered, [this] { showNormal(); raise(); activateWindow(); });
        connect(hideAction, &QAction::triggered, this, &MainWindow::hideToTray);
        connect(desktopAction, &QAction::triggered, this, &MainWindow::desktopRecognition);
        connect(screenshotAction, &QAction::triggered, this, &MainWindow::screenshotRecognition);
        connect(stopAction, &QAction::triggered, [this] { cancelCurrentTask("用户请求停止当前任务"); });
        connect(resumeAction, &QAction::triggered, this, &MainWindow::resumeLatestTask);
        connect(settingsAction, &QAction::triggered, this, &MainWindow::openSettings);
        connect(configAction, &QAction::triggered, [] { QDesktopServices::openUrl(QUrl::fromLocalFile(writablePath("assets/config"))); });
        connect(logAction, &QAction::triggered, [] { QDesktopServices::openUrl(QUrl::fromLocalFile(Logger::root())); });
        connect(dbAction, &QAction::triggered, [] { QDesktopServices::openUrl(QUrl::fromLocalFile(writablePath("assets/db"))); });
        connect(quitAction, &QAction::triggered, [this] { showNormal(); raise(); activateWindow(); if (confirmExitProgram(this)) shutdown(); });
        m_tray->setContextMenu(menu); m_tray->show();
        connect(m_tray, &QSystemTrayIcon::activated, [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::DoubleClick) { showNormal(); raise(); activateWindow(); }
        });
    }
    void pollRuntimeProtection() {
        if (m_controlActive.load() && m_userInterrupted.load()) {
            if (!m_stop.exchange(true)) {
                RuntimeSafety::instance().cancel("真人持续输入，接管自动控制任务");
                log("[中断] 真人持续输入，停止当前自动控制；普通聊天和图片分析不受真人输入影响。");
            }
        } else if (!m_controlActive.load() && m_userInterrupted.load()) { m_userInputMonitor.resetActivity(); m_userInterrupted = false; }
        if (RuntimeSafety::instance().blocked() && !m_stop.exchange(true)) log("[保护] 资源不足，停止当前推理和自动动作。请查看WATCHDOG资源记录。");
    }
    void invalidateActiveInference(const QString& reason, bool writeLog = true) {
        RuntimeSafety::instance().cancel(reason);
        m_stop = true; m_controlActive = false; m_userInterrupted = false;
        m_userInputMonitor.resetActivity(); m_passiveScanInFlight = false;
        if (writeLog) log("[任务切换] " + reason + "；旧generation已作废，旧推理结果和后续动作全部丢弃。");
    }
    bool beginFreshInference(const QString& reason, QString* error = nullptr) {
        invalidateActiveInference(reason, false);
        m_stop = false; m_userInterrupted = false;
        m_userInputMonitor.resetActivity();
        if (!RuntimeSafety::instance().beginTask(error)) return false;
        log("[任务切换] " + reason + "；已建立新的推理generation。");
        return true;
    }
    void cancelCurrentTask(const QString& reason) {
        invalidateActiveInference(reason, true);
        log("[任务] " + reason + "；已取消推理，后续动作不会继续执行。");
    }
    void hideToTray() {
        hide();
        if (m_tray) m_tray->showMessage("木头自动系统控制", "程序已隐藏到系统托盘，后台服务继续运行。", QSystemTrayIcon::Information, 2200);
    }
    void ensureDesktopOverlay() { if (m_desktopOverlay) return; m_desktopOverlay = new UiaOverlayWidget(false); m_desktopOverlay->configure(overlayCfg()); m_desktopOverlay->setGlobalRect(virtualDesktopRect()); }
    void ensureWindowOverlay() { if (m_windowOverlay) return; m_windowOverlay = new UiaOverlayWidget(true); m_windowOverlay->configure(overlayCfg()); }
    void requestPassiveScan(AgentScope scope) {
        if (m_running || RuntimeSafety::instance().blocked() || m_shutdownStarted || !m_overlayUia) return;
        if(!loadConfig("assets/config/permissions.json").value("uia").toObject().value("allow_scan").toBool(true))return;
        bool expected = false;
        if (!m_passiveScanInFlight.compare_exchange_strong(expected, true)) return;
        const HWND hwnd = scope == AgentScope::SelectedWindow ? m_selectedHwnd : nullptr;
        m_uiaExecutor.post([this, scope, hwnd] {
            QVector<UIAItem> items;
            if (m_overlayUia) {
                if (scope == AgentScope::SelectedWindow) { if (validWindowHandle(hwnd)) items = m_overlayUia->scanWindow(hwnd); }
                else { items = m_overlayUia->scanDesktopOverview(); }
            }
            postUi([this, scope, hwnd, items] {
                m_passiveScanInFlight = false;
                if (m_shutdownStarted) return;
                if (scope == AgentScope::Desktop) {
                    if (m_workMode != WorkMode::DesktopControl) return;
                    ensureDesktopOverlay(); m_desktopOverlay->setItems(items); m_desktopOverlay->show(); m_desktopOverlay->raise();
                } else {
                    if (m_workMode != WorkMode::WindowControl || hwnd != m_selectedHwnd) return;
                    ensureWindowOverlay(); m_windowOverlay->setGlobalRect(m_selectedWindowRect); m_windowOverlay->setItems(items); m_windowOverlay->show(); m_windowOverlay->raise();
                }
            });
        });
    }
    void desktopRecognition() {
        if (!m_acceptNewTasks) return;
        if (m_workMode == WorkMode::DesktopControl) { stopDesktopControl(true); return; }
        if (m_workMode == WorkMode::WindowControl) stopWindowControl(false);
        startDesktopControl();
    }
    void startDesktopControl() {
        invalidateActiveInference("切换到桌面识别", true);
        m_workMode = WorkMode::DesktopControl; refreshModeButtons();
        ensureDesktopOverlay(); m_desktopOverlay->show(); m_desktopOverlay->raise();
        if (!m_desktopRefreshTimer) { m_desktopRefreshTimer = new QTimer(this); connect(m_desktopRefreshTimer, &QTimer::timeout, this, [this] { requestPassiveScan(AgentScope::Desktop); }); }
        const int interval = qMax(1000, m_running ? desktopCfg().value("active_uia_refresh_interval_ms").toInt(1000) : desktopCfg().value("idle_uia_refresh_interval_ms").toInt(10000));
        m_desktopRefreshTimer->start(interval); requestPassiveScan(AgentScope::Desktop);
        log(QString("[桌面] 桌面识别已开启，当前刷新间隔 %1 ms。").arg(interval));
        addMessage(MessageRole::Ai, "桌面识别已开启。现在可以告诉我要操作什么。");
    }
    void stopDesktopControl(bool notifyUser) {
        invalidateActiveInference("退出桌面识别", true);
        if (m_desktopRefreshTimer) m_desktopRefreshTimer->stop();
        if (m_desktopOverlay) m_desktopOverlay->hide();
        if (m_workMode == WorkMode::DesktopControl) m_workMode = WorkMode::Chat;
        refreshModeButtons(); log("[桌面] 桌面识别已退出");
        if (notifyUser) addMessage(MessageRole::Ai, "桌面识别已退出，现在回到普通聊天模式。");
    }
    void beginPick() {
        if (!m_acceptNewTasks) return;
        if (m_workMode == WorkMode::WindowControl) { stopWindowControl(true); return; }
        if (m_workMode == WorkMode::DesktopControl) stopDesktopControl(false);
        invalidateActiveInference("开始选择新的指定窗口", true);
        addMessage(MessageRole::System, "请点击需要指定控制的已打开软件窗口。");
        hide(); m_pickMouseDownSeen = false;
        if (!m_pickTimer) { m_pickTimer = new QTimer(this); m_pickTimer->setInterval(40); connect(m_pickTimer, &QTimer::timeout, this, &MainWindow::pollPick); }
        m_pickStart = QDateTime::currentMSecsSinceEpoch(); m_pickTimer->start();
    }
    void pollPick() {
        if (QDateTime::currentMSecsSinceEpoch() - m_pickStart > 15000) {
            m_pickTimer->stop(); showNormal(); raise(); activateWindow();
            log("[指定] 指定窗口选择超时"); addMessage(MessageRole::System, "指定识别已取消：15秒内没有选择有效窗口。"); return;
        }
        const bool down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        if (down && !m_pickMouseDownSeen) { m_pickMouseDownSeen = true; return; }
        if (!down && m_pickMouseDownSeen) {
            POINT point{}; GetCursorPos(&point);
            HWND hwnd = WindowFromPoint(point); if (hwnd) hwnd = GetAncestor(hwnd, GA_ROOT);
            m_pickMouseDownSeen = false;
            if (!validWindowHandle(hwnd)) return;
            if (hwnd == reinterpret_cast<HWND>(winId())) return;
            if (isShellOrDesktopWindow(hwnd)) return;
            DWORD pid = 0; GetWindowThreadProcessId(hwnd, &pid);
            if (pid == GetCurrentProcessId()) return;
            const QString title = windowTitleOf(hwnd); if (title.isEmpty()) return;
            m_pickTimer->stop(); startWindowControl(hwnd, pid, title);
        }
    }
    void startWindowControl(HWND hwnd, DWORD pid, const QString& title) {
        invalidateActiveInference("切换到新的指定窗口", true);
        m_selectedHwnd = hwnd; m_selectedWindowPid = pid; m_selectedWindowTitle = title; m_selectedWindowRect = windowRectOf(hwnd);
        m_workMode = WorkMode::WindowControl; refreshModeButtons();
        showNormal(); raise(); activateWindow();
        ensureWindowOverlay(); m_windowOverlay->setGlobalRect(m_selectedWindowRect); m_windowOverlay->show(); m_windowOverlay->raise();
        if (!m_windowRectTimer) { m_windowRectTimer = new QTimer(this); connect(m_windowRectTimer, &QTimer::timeout, this, &MainWindow::refreshSelectedWindowRect); }
        if (!m_windowUiaTimer) { m_windowUiaTimer = new QTimer(this); connect(m_windowUiaTimer, &QTimer::timeout, this, [this] { requestPassiveScan(AgentScope::SelectedWindow); }); }
        const QJsonObject cfg = windowCfg();
        m_windowRectTimer->start(qMax(50, cfg.value("window_rect_refresh_interval_ms").toInt(150)));
        m_windowUiaTimer->start(qMax(500, cfg.value("uia_refresh_interval_ms").toInt(1000)));
        requestPassiveScan(AgentScope::SelectedWindow);
        log(QString("[指定] 已锁定：%1 | Process=%2 | PID=%3 | HWND=%4").arg(title).arg(processExecutableName(hwnd).isEmpty() ? "(未知)" : processExecutableName(hwnd)).arg(pid).arg(static_cast<qulonglong>(reinterpret_cast<quintptr>(hwnd))));
        addMessage(MessageRole::Ai, QString("已经锁定“%1”。绿色提示只显示在窗口边缘，后续输入默认只针对这个软件。").arg(title));
    }
    void refreshSelectedWindowRect() {
        if (m_workMode != WorkMode::WindowControl) return;
        if (!validWindowHandle(m_selectedHwnd)) { addMessage(MessageRole::System, "指定窗口已经关闭，已退出指定识别模式。"); stopWindowControl(false); return; }
        const QRect rect = windowRectOf(m_selectedHwnd);
        if (!rect.isEmpty() && rect != m_selectedWindowRect) { m_selectedWindowRect = rect; if (m_windowOverlay) m_windowOverlay->setGlobalRect(rect); }
    }
    void stopWindowControl(bool notifyUser) {
        invalidateActiveInference("退出指定识别", true);
        if (m_pickTimer) m_pickTimer->stop();
        if (m_windowRectTimer) m_windowRectTimer->stop();
        if (m_windowUiaTimer) m_windowUiaTimer->stop();
        if (m_windowOverlay) m_windowOverlay->hide();
        const QString oldTitle = m_selectedWindowTitle;
        m_selectedHwnd = nullptr; m_selectedWindowPid = 0; m_selectedWindowTitle.clear(); m_selectedWindowRect = {};
        if (m_workMode == WorkMode::WindowControl) m_workMode = WorkMode::Chat;
        refreshModeButtons(); log("[指定] 指定识别已退出");
        if (notifyUser) addMessage(MessageRole::Ai, oldTitle.isEmpty() ? "指定识别已退出，现在回到普通聊天模式。" : QString("已停止控制“%1”，现在回到普通聊天模式。").arg(oldTitle));
    }
    void restoreCurrentOverlay() {
        if (m_workMode == WorkMode::DesktopControl && m_desktopOverlay) { m_desktopOverlay->show(); m_desktopOverlay->raise(); }
        else if (m_workMode == WorkMode::WindowControl && m_windowOverlay) { m_windowOverlay->show(); m_windowOverlay->raise(); }
    }
    void screenshotRecognition() {
        if (m_state != AppState::Ready || !m_acceptNewTasks) return;
        invalidateActiveInference("切换到用户截图识别", true);
        m_stop = false;
        const QRect monitorRect = cursorMonitorRect();
        if (monitorRect.isEmpty()) { addMessage(MessageRole::System, "无法获取鼠标所在显示器。"); return; }
        const QJsonObject selectorCfg = cameraRoot().value("region_selector").toObject();
        m_modeBeforeScreenshot = m_workMode; m_workMode = WorkMode::ScreenshotSelecting;
        if (m_desktopOverlay) m_desktopOverlay->hide();
        if (m_windowOverlay) m_windowOverlay->hide();
        hide();
        auto* overlay = new ScreenshotOverlay(monitorRect, selectorCfg, [this](const QRect& globalRect, bool accepted) { finishScreenshotSelection(globalRect, accepted); });
        overlay->show(); overlay->raise(); overlay->activateWindow();
    }
    void finishScreenshotSelection(const QRect& globalRect, bool accepted) {
        m_workMode = m_modeBeforeScreenshot; showNormal(); raise(); activateWindow();
        refreshModeButtons(); restoreCurrentOverlay();
        if (!accepted || globalRect.isEmpty()) { log("[截图] 截图选区已取消"); return; }
        const CaptureResult capture = captureRect(globalRect);
        if (!capture.valid) { addMessage(MessageRole::System, "截图失败，请重新选择区域。"); return; }
        const QString path = saveScreenshot(capture.image, "region");
        const QString question = m_input->toPlainText().trimmed();
        m_input->clear();
        addImageMessage(MessageRole::User, path, question);
        analyzeImageForUser(path, "USER_SCREENSHOT_CHAT", question);
    }
    void uploadImage() {
        if (m_state != AppState::Ready || !m_acceptNewTasks) return;
        const QString path = QFileDialog::getOpenFileName(this, "选择图片", QString(), "Image (*.png *.jpg *.jpeg *.bmp *.webp)");
        if (path.isEmpty()) return;
        const QString question = m_input->toPlainText().trimmed();
        m_input->clear();
        addImageMessage(MessageRole::User, path, question);
        analyzeImageForUser(path, "USER_IMAGE_CHAT", question);
    }
        QString humanizeVisionAnswer(const QString& raw) const {
        QString parseError;
        const QJsonObject object = strictJsonObject(raw, &parseError);
        if (object.isEmpty()) return raw.trimmed();
        QStringList parts;
        const QString state = object.value("screen_state").toString().trimmed();
        if (!state.isEmpty()) parts << state;
        QStringList visible;
        for (const QJsonValue& value : object.value("visible_text").toArray()) {
            const QString t = value.toString().trimmed();
            if (!t.isEmpty() && !visible.contains(t)) visible << t;
            if (visible.size() >= 8) break;
        }
        if (!visible.isEmpty()) parts << ("我能读到的主要内容包括：" + visible.join("、"));
        if (parts.isEmpty()) return "图片已经识别，但模型没有返回可直接展示的自然语言描述。";
        return parts.join("\\n\\n");
    }

    bool validUserVisionAnswer(const QString& raw) const {
        QString text = raw.trimmed();
        if (text.isEmpty()) return false;
        const QString normalized = text.toLower().remove(' ').remove('\t').remove('\r').remove('\n');
        static const QStringList invalid{"无", "没有", "none", "null", "{}", "[]", "无法识别", "无法分析", "看不到图片"};
        if (invalid.contains(normalized)) return false;
        QString meaningful = text;
        meaningful.remove(QRegularExpression(R"([<>{}\[\]"'`_\-\s:;,，。.!！？/\\]+)"));
        return meaningful.size() >= 2;
    }

    QString buildUserVisionPrompt(const QString& visionMode, const QString& question, bool retry) const {
        QString prompt = "VISION_MODE=" + visionMode + "\n"
            "这是用户直接上传或截取的图片，不是Agent控制界面的结构化观察。\n"
            "请直接用自然中文回答，不要输出JSON，不要输出targets、norm_x、norm_y、UIA、Planner等内部字段。\n"
            "先认真查看图片，再回答用户真正关心的内容；看不清的地方明确说看不清，不要编造。\n";
        if (!question.trimmed().isEmpty()) prompt += "用户问题：\n" + question.trimmed().left(1200) + "\n";
        else prompt += "用户没有附加问题。请自然说明图片里最重要的主体、场景、明显文字和关键信息。\n";
        if (retry) prompt += "上一轮没有得到有效回答。这一轮必须给出完整、可读的自然语言结果，不要只返回“无”、几个词、空JSON或控制符。\n";
        return prompt;
    }

    void analyzeImageForUser(const QString& path, const QString& visionMode, const QString& question) {
        QString safetyError;
        if (!beginFreshInference("开始用户图片/截图分析", &safetyError)) { addMessage(MessageRole::System, safetyError); return; }
        const quint64 generation = RuntimeSafety::instance().generation();
        setAppState(AppState::Generating, "[图片] VLM正在分析图片...");
        m_modelExecutor.post([this, path, visionMode, question, generation] {
            if (RuntimeSafety::instance().shouldAbort(generation)) return;
            QString error; QString result;
            for (int attempt = 0; attempt < 2; ++attempt) {
                if (RuntimeSafety::instance().shouldAbort(generation)) return;
                const QString prompt = buildUserVisionPrompt(visionMode, question, attempt > 0);
                error.clear();
                result = m_models ? m_models->vision(path, prompt, &error) : QString();
                if (!error.isEmpty()) break;
                if (validUserVisionAnswer(result)) break;
                Logger::write("USER_VISION_RETRY", QString("generation=%1 attempt=%2 raw_chars=%3").arg(generation).arg(attempt + 1).arg(result.size()));
            }
            postUi([this, result, error, question, generation] {
                if (RuntimeSafety::instance().shouldAbort(generation)) { log("[图片] 旧generation视觉结果已丢弃。"); return; }
                if (!error.isEmpty()) { addMessage(MessageRole::Ai, "图片分析失败：" + error); log("[图片] VLM错误：" + error); setAppState(AppState::Ready); return; }
                if (!validUserVisionAnswer(result)) {
                    const QString failure = "图片分析没有得到有效内容。模型请求已经完成，但返回结果为空或过短；没有把这种情况误报为成功。";
                    addMessage(MessageRole::Ai, failure); log("[图片] VLM返回无效结果，已重试一次仍无有效内容。");
                    setAppState(AppState::Ready, "[图片] 图片分析未得到有效结果"); return;
                }
                const QString userVisible = humanizeVisionAnswer(result);
                addMessage(MessageRole::Ai, userVisible);
                m_chatHistory.append(QJsonObject{{"role", "user"},{"content", question.isEmpty() ? "[用户发送了一张图片]" : "[用户发送了一张图片]\n" + question}});
                m_chatHistory.append(QJsonObject{{"role", "assistant"},{"content", userVisible}});
                trimChatHistory();
                setAppState(AppState::Ready, "[图片] 图片分析完成");
            });
        });
    }

    void trimChatHistory() {
        while (m_chatHistory.size() > 12 || (!m_chatHistory.isEmpty() && estimateMessagesTokens(m_chatHistory) > 2200)) {
            m_chatHistory.removeAt(0);
            if (!m_chatHistory.isEmpty() && m_chatHistory.first().toObject().value("role").toString() == "assistant") m_chatHistory.removeAt(0);
        }
    }

    void runChat(const QString& text) {
        QString safetyError;
        if (!beginFreshInference("切换到普通聊天", &safetyError)) { addMessage(MessageRole::System, safetyError); return; }
        setAppState(AppState::Generating, "[聊天] 正在生成回复...");
        QString systemPath = writablePath("assets/models/prompts/system.txt");
        if (!QFileInfo::exists(systemPath)) systemPath = resourcePath("assets/models/prompts/system.txt");
        QJsonArray messages;
        messages.append(QJsonObject{{"role", "system"},{"content", "CURRENT_MODE=CHAT\n\n" "普通聊天不要主动解释内部Agent模式、UIA、Planner或Verifier；" "直接自然回答。允许Markdown标题、表格、列表、粗体和代码块。\n\n" + readText(systemPath)}});
        for (const QJsonValue& value : m_chatHistory) messages.append(value);
        messages.append(QJsonObject{{"role", "user"},{"content", text}});
        while (messages.size() > 2 && estimateMessagesTokens(messages) > plannerInputBudget()) {
            messages.removeAt(1);
            if (messages.size() > 2 && messages.at(1).toObject().value("role").toString() == "assistant") messages.removeAt(1);
        }
        if (estimateMessagesTokens(messages) > plannerInputBudget()) { addMessage(MessageRole::System, "当前问题或system提示词超过安全上下文预算，请缩短后重试。"); setAppState(AppState::Ready); return; }
        ChatMessageWidget* rawMessage = addMessage(MessageRole::Ai, "");
        QPointer<ChatMessageWidget> aiMessage(rawMessage);
        m_modelExecutor.post([this, messages, text, aiMessage]() mutable {
            QString error; QString streamed;
            const QString reply = m_models ? m_models->chatStream("llm", messages, [this, aiMessage, &streamed](const QString& delta) {
                streamed += delta; const QString snapshot = streamed;
                postUi([this, aiMessage, snapshot] { if (!aiMessage) return; aiMessage->setText(snapshot); scrollChatToBottom(); });
            }, &error) : QString();
            const QString finalText = reply.isEmpty() ? streamed : reply;
            postUi([this, aiMessage, finalText, error, text] {
                if (!error.isEmpty()) { if (aiMessage) aiMessage->setText("模型回复失败：" + error); log("[聊天] LLM错误：" + error); setAppState(AppState::Ready); return; }
                if (aiMessage) aiMessage->setText(finalText);
                m_chatHistory.append(QJsonObject{{"role", "user"},{"content", text}});
                m_chatHistory.append(QJsonObject{{"role", "assistant"},{"content", finalText}});
                trimChatHistory();
                setAppState(AppState::Ready, "[聊天] 回复完成");
            });
        });
    }

    static QJsonObject extractJson(QString text) {
        text = text.trimmed();
        text.remove(QRegularExpression("^```(?:json)?\\s*"));
        text.remove(QRegularExpression("\\s*```$"));
        const int begin = text.indexOf('{'); const int end = text.lastIndexOf('}');
        if (begin < 0 || end <= begin) return {};
        const QJsonDocument doc = QJsonDocument::fromJson(text.mid(begin, end - begin + 1).toUtf8());
        return doc.isObject() ? doc.object() : QJsonObject{};
    }

    QString taskModeForGoal(const QString& goal) const {
        if (TaskRouter::looksContinuousTask(goal)) return "CONTINUOUS";
        const QJsonObject taskCfg = agentRoot().value("task").toObject();
        for (const QJsonValue& value : taskCfg.value("continuous_keywords").toArray()) {
            const QString keyword = value.toString();
            if (!keyword.isEmpty() && goal.contains(keyword, Qt::CaseInsensitive)) return "CONTINUOUS";
        }
        return "ONE_SHOT";
    }

    ExecutionAuthorization buildExecutionAuthorization(const AgentSession& session) const {
        ExecutionAuthorization auth;
        auth.scope = session.scope; auth.targetHwnd = session.targetHwnd;
        if (session.scope == AgentScope::Desktop) {
            auth.authorized = (m_workMode == WorkMode::DesktopControl);
            auth.reason = auth.authorized ? "DESKTOP_CONTROL 已由用户主动开启，当前输入被授权为桌面任务。" : "当前不处于 DESKTOP_CONTROL。";
            return auth;
        }
        const bool validTarget = m_workMode == WorkMode::WindowControl && validWindowHandle(session.targetHwnd);
        auth.authorized = validTarget;
        auth.reason = validTarget ? QString("WINDOW_CONTROL 已由用户主动选择目标窗口：%1；当前输入被授权在该 HWND 范围内执行。").arg(session.targetTitle) : "WINDOW_CONTROL 未锁定有效目标窗口。";
        return auth;
    }

    static QStringList jsonStringList(const QJsonValue& value, int maxItems = 16) {
        QStringList out;
        for (const auto& item : value.toArray()) {
            const QString s = item.toString().trimmed();
            if (!s.isEmpty() && !out.contains(s, Qt::CaseInsensitive)) out << s.left(80);
            if (out.size() >= maxItems) break;
        }
        return out;
    }

    QString taskProfileName(const TaskRequest& request) const {
        switch (request.kind) {
            case TaskKind::CustomerService: return "customer_service";
            case TaskKind::CreativeWorkflow: return "creative_workflow";
            case TaskKind::DocumentWorkflow: return "document_workflow";
            case TaskKind::OpenApplication: return "open_application";
            case TaskKind::Chat: return "chat";
            default: return "operate_application";
        }
    }

    RuntimeAppProfile fallbackRuntimeProfile(const AgentSession& session) const {
        RuntimeAppProfile p;
        const QJsonObject known = AppRegistry::lookup(session.targetProcess).entry;
        p.processProfile = known.value("profile").toString(known.value("task_profile").toString("generic_application"));
        p.surfaceType = known.value("surface_type").toString("unknown");
        p.taskProfile = taskProfileName(session.request);
        p.preferredObservation = known.value("observation_policy").toString();
        if (p.preferredObservation.isEmpty()) {
            if (session.request.kind == TaskKind::CreativeWorkflow) p.preferredObservation = "vision_first";
            else if (session.request.kind == TaskKind::DocumentWorkflow) p.preferredObservation = "uia_first";
            else p.preferredObservation = "uia_then_vision";
        }
        p.likelyTargets = jsonStringList(known.value("vision_targets"));
        if (p.likelyTargets.isEmpty()) {
            if (session.request.kind == TaskKind::CustomerService) p.likelyTargets = {"conversation", "latest_message", "text_input", "send"};
            else if (session.request.kind == TaskKind::CreativeWorkflow) p.likelyTargets = {"primary_workspace", "tool", "canvas", "save"};
            else if (session.request.kind == TaskKind::DocumentWorkflow) p.likelyTargets = {"editor", "toolbar", "document_state", "save"};
            else p.likelyTargets = {"primary_target", "main_content", "confirm_action"};
        }
        p.preferredActionFamilies = {"mcp", "uia", "vision", "mouse", "keyboard"};
        p.source = known.isEmpty() ? "generic_fallback" : "static_hint";
        p.confidence = known.isEmpty() ? 0.35 : 0.70;
        return p;
    }

    RuntimeAppProfile validateRuntimeProfile(const QJsonObject& object, const RuntimeAppProfile& fallback) const {
        RuntimeAppProfile p = fallback;
        const QSet<QString> observations{"uia_first", "uia_then_vision", "vision_first"};
        const QSet<QString> families{"mcp", "uia", "vision", "mouse", "keyboard", "system"};
        const QString processProfile = object.value("process_profile").toString().trimmed();
        const QString surfaceType = object.value("surface_type").toString().trimmed();
        const QString preferred = object.value("preferred_observation").toString().trimmed();
        if (!processProfile.isEmpty()) p.processProfile = processProfile.left(80);
        if (!surfaceType.isEmpty()) p.surfaceType = surfaceType.left(80);
        if (observations.contains(preferred)) p.preferredObservation = preferred;
        const QStringList targets = jsonStringList(object.value("likely_targets"), 12);
        if (!targets.isEmpty()) p.likelyTargets = targets;
        QStringList validatedFamilies;
        QJsonValue familyValue = object.value("preferred_action_families");
        if (!familyValue.isArray()) familyValue = object.value("allowed_action_families");
        for (const QString& family : jsonStringList(familyValue, 10)) if (families.contains(family)) validatedFamilies << family;
        if (!validatedFamilies.isEmpty()) p.preferredActionFamilies = validatedFamilies;
        p.taskProfile = fallback.taskProfile;
        p.confidence = qBound(0.0, object.value("confidence").toDouble(fallback.confidence), 1.0);
        p.rationale = object.value("rationale").toString().left(600);
        p.dynamicGenerated = true; p.source = "dynamic_llm";
        return p;
    }

    RuntimeAppProfile buildRuntimeAppProfile(AgentSession& session, const QString& goal, QString* error = nullptr) {
        RuntimeAppProfile fallback = fallbackRuntimeProfile(session);
        const QJsonObject cfg = agentRoot().value("runtime_app_profile").toObject();
        if (!cfg.value("enabled").toBool(true)) { if (error) error->clear(); return fallback; }
        const QJsonObject known = AppRegistry::lookup(session.targetProcess).entry;
        QJsonObject staticHint{{"known", !known.isEmpty()},{"profile", known.value("profile")},{"task_profile", known.value("task_profile")},{"observation_policy", known.value("observation_policy")},{"vision_targets", known.value("vision_targets")}};
        const QString systemPrompt = "你是桌面智能体的Runtime App Profiler。根据用户任务、当前进程名、窗口标题和已有静态提示，为“本次任务”生成应用策略JSON。你不是工具执行器，绝对不能创造工具名。只输出JSON，字段：process_profile(string), surface_type(string), preferred_observation(uia_first/uia_then_vision/vision_first), likely_targets(array string), preferred_action_families(array，只能mcp/uia/vision/mouse/keyboard/system), confidence(0~1), rationale(string)。preferred_action_families只表示优先顺序/偏好，不代表权限，不能用它关闭vision/uia/mouse/keyboard等系统能力。task_profile由程序根据用户意图确定，你不得改变用户任务。不要因为exe名称陌生就拒绝；根据窗口标题和任务语义推断通用界面类型。";
        const QString userPrompt = QString("USER_GOAL=%1\nTASK_PROFILE=%2\nPROCESS=%3\nWINDOW_TITLE=%4\nWINDOW_RECT=(%5,%6,%7,%8)\nSTATIC_HINT=%9")
            .arg(goal.left(700), fallback.taskProfile, session.targetProcess, session.targetTitle)
            .arg(session.targetRect.x()).arg(session.targetRect.y()).arg(session.targetRect.width()).arg(session.targetRect.height())
            .arg(QString::fromUtf8(QJsonDocument(staticHint).toJson(QJsonDocument::Compact)));
        QString modelError;
        const QJsonArray messages{QJsonObject{{"role","system"},{"content",systemPrompt}},QJsonObject{{"role","user"},{"content",userPrompt}}};
        const QString raw = m_modelExecutor.call([this, messages, &modelError] { return m_models ? m_models->chat("llm", messages, &modelError) : QString(); });
        if (!modelError.isEmpty()) { if (error) *error = modelError; fallback.rationale = "动态画像失败，使用安全fallback：" + modelError.left(240); return fallback; }
        QString jsonError;
        const QJsonObject object = strictJsonObject(raw, &jsonError);
        if (object.isEmpty()) { if (error) *error = jsonError; fallback.rationale = "动态画像JSON无效，使用安全fallback：" + jsonError.left(240); return fallback; }
        RuntimeAppProfile profile = validateRuntimeProfile(object, fallback);
        const double minimum = qBound(0.0, cfg.value("minimum_confidence").toDouble(0.45), 1.0);
        if (profile.confidence < minimum) {
            fallback.rationale = QString("动态画像置信度%1低于阈值%2，使用fallback。模型说明：%3").arg(profile.confidence, 0, 'f', 2).arg(minimum, 0, 'f', 2).arg(profile.rationale);
            if (error) error->clear(); return fallback;
        }
        if (error) error->clear(); return profile;
    }

    void persistRuntimeAppProfile(const AgentSession& session) const {
        if (!session.runtimeProfileReady) return;
        const QString path = QDir(Logger::root()).filePath("agent_context/runtime_profile_" + session.taskId + ".json");
        QJsonObject object{
            {"task_id", session.taskId},{"goal", session.goal},{"process", session.targetProcess},{"window_title", session.targetTitle},
            {"window_rect", QJsonArray{session.targetRect.x(), session.targetRect.y(), session.targetRect.width(), session.targetRect.height()}},
            {"runtime_profile", session.runtimeProfile.json()},
            {"created_at", QDateTime::currentDateTime().toString(Qt::ISODateWithMs)}
        };
        Logger::atomicJson(path, object);
    }

    QString actionFamily(const QString& action) const {
        if (action.startsWith("mcp_")) return "mcp";
        if (action.startsWith("uia_")) return "uia";
        if (action == "screen_capture" || action == "vision_analyze") return "vision";
        if (action.startsWith("mouse_")) return "mouse";
        if (action.startsWith("keyboard_")) return "keyboard";
        if (action == "app_launch" || action == "process_start" || action == "cmd_exec" || action == "powershell_exec") return "system";
        return {};
    }

    bool observationReadyForAction(const AgentSession& session) const {
        if (session.mcp && session.mcp->isReady()) return true;
        if (session.uiaUsable && session.uiaActionableCount > 0) return true;
        if (!session.latestVision.isEmpty()) {
            const QJsonArray targets = session.latestVision.value("targets").toArray();
            for (const auto& value : targets) {
                const QJsonObject t = value.toObject();
                if (t.value("confidence").toDouble(0.0) >= 0.60 && (t.value("norm_x").toDouble(-1.0) >= 0.0 || t.value("uia_id").toInt(0) > 0)) return true;
            }
        }
        return false;
    }

    QString worldProgressHash(const AgentSession& session) const {
        QJsonObject compact{{"profile", session.runtimeProfile.json()},{"channel", session.controlChannel},{"phase", session.workflow.phase},{"uia_actionable", session.uiaActionableCount},{"vision_targets", session.latestVision.value("targets")},{"visible_text", session.latestVision.value("visible_text")},{"last_action", session.lastActionKey},{"contact", session.currentContact}};
        return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(compact).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
    }

    QString interactionTurnStateName(const AgentSession& session) const {
        if (!conversationLikeSurface(session)) return "NONE";
        if (session.pendingCommitRequired || session.pendingCommitAwaitingVerification) return "COMMITTING";
        if (!session.hasSentInteraction) return "READY_TO_INITIATE";
        const QString latest = session.latestRelevantText.trimmed();
        const QString baseline = session.externalRelevantTextBaseline.trimmed();
        if (!latest.isEmpty() && latest != baseline) return "READY_TO_RESPOND";
        return "WAITING_EXTERNAL";
    }

    QString agentPolicyPhaseName(const AgentSession& session) const {
        if(session.pendingCommitAwaitingVerification) return "VERIFY_EFFECT";
        if(!session.groundingTarget.trimmed().isEmpty()) return "NEED_GROUNDING";
        if(session.waitingForExternalChange || session.workflow.phase=="waiting") return "WAIT_FOR_CHANGE";
        if(session.actionReady) return "ACTION_READY";
        return "OBSERVE_OR_VERIFY";
    }

    void updateVisionSemanticContext(AgentSession& session) {
        const QJsonObject context = session.latestVision.value("task_context").toObject();
        session.latestTaskContextSummary = context.value("summary").toString().left(500);
        session.latestRelevantText = context.value("latest_relevant_text").toString().left(1000);
        session.latestExternalChangeDetected = context.value("external_change_detected").toBool(false);
        const QString phase = agentPolicyPhaseName(session);
        if(phase=="NEED_GROUNDING") session.policyPhase=AgentPolicyPhase::NeedGrounding;
        else if(phase=="WAIT_FOR_CHANGE") session.policyPhase=AgentPolicyPhase::WaitForChange;
        else if(phase=="VERIFY_EFFECT") session.policyPhase=AgentPolicyPhase::VerifyEffect;
        else if(phase=="ACTION_READY") session.policyPhase=AgentPolicyPhase::ActionReady;
        else session.policyPhase=AgentPolicyPhase::ObserveOrVerify;
    }

    void updateGenericProgressWatchdog(AgentSession& session) {
        const QString hash = worldProgressHash(session);
        if (!session.lastWorldStateHash.isEmpty() && hash == session.lastWorldStateHash) ++session.genericNoProgressCount;
        else session.genericNoProgressCount = 0;
        session.lastWorldStateHash = hash;
        session.actionReady = observationReadyForAction(session);
        updateVisionSemanticContext(session);
    }

    QString observationPolicyForSession(const AgentSession& session) const {
        QString policy;
        if (session.runtimeProfileReady) policy = session.runtimeProfile.preferredObservation.trimmed();
        const QJsonObject entry = AppRegistry::lookup(session.targetProcess).entry;
        if (policy.isEmpty()) policy = entry.value("observation_policy").toString().trimmed();
        if (policy.isEmpty()) {
            if (session.request.kind == TaskKind::CreativeWorkflow) policy = "vision_first";
            else if (session.request.kind == TaskKind::DocumentWorkflow) policy = "uia_first";
            else if (session.request.kind == TaskKind::CustomerService) policy = "uia_then_vision";
            else policy = "uia_then_vision";
        }
        static const QSet<QString> allowed{"uia_first", "uia_then_vision", "vision_first"};
        if (!allowed.contains(policy)) policy = "uia_then_vision";
        return policy;
    }

    static int estimateTokenCount(const QString& text) {
        double tokens = 0.0; int asciiRun = 0;
        for (QChar ch : text) {
            if (ch.unicode() < 128) { ++asciiRun; }
            else { if (asciiRun > 0) { tokens += asciiRun / 3.6; asciiRun = 0; } tokens += 1.15; }
        }
        if (asciiRun > 0) tokens += asciiRun / 3.6;
        return qCeil(tokens * 1.10);
    }
    static int estimateMessagesTokens(const QJsonArray& messages) { return estimateTokenCount(QString::fromUtf8(QJsonDocument(messages).toJson(QJsonDocument::Compact))); }

    bool isContextOverflow(const QString& error) const {
        const QString lower = error.toLower();
        for (const QJsonValue& value : contextCfg().value("overflow_error_keywords").toArray()) {
            const QString keyword = value.toString().toLower();
            if (!keyword.isEmpty() && lower.contains(keyword)) return true;
        }
        return lower.contains("context") && (lower.contains("exceed") || lower.contains("maximum"));
    }

    QString appCapabilitySummary(HWND hwnd) const {
        const auto app=AppRegistry::lookup(processExecutableName(hwnd));
        return QString("APP_PROCESS=%1\nAPP_NAME=%2\nMCP_CONFIGURED=%3\nCAPABILITY_FALLBACK=UIA>Vision>MouseKeyboard\n")
            .arg(processExecutableName(hwnd),app.name).arg(app.entry.value("mcp").toObject().value("enabled").toBool(false)?"true":"false");
    }

    void updateUiaCapabilityDecision(AgentSession& session) {
        if (session.scope != AgentScope::SelectedWindow) return;
        if (session.uiaChecked && !session.uiaUsable) return;
        session.uiaNodeCount = session.uiaItems.size();
        session.uiaActionableCount = int(std::count_if(session.uiaItems.cbegin(), session.uiaItems.cend(), [](const UIAItem& item){ return item.clickable; }));
        const QJsonObject app = AppRegistry::lookup(session.targetProcess).entry;
        const int minNodes = qMax(2, app.value("uia_min_nodes").toInt(3));
        const int minActionable = qMax(1, app.value("uia_min_actionable_items").toInt(3));
        session.uiaChecked = true;
        if (!app.value("uia").toBool(true)) { session.uiaUsable = false; session.uiaFailureReason = "应用配置禁用UIA"; }
        else if (session.uiaNodeCount < minNodes || session.uiaActionableCount < minActionable) {
            session.uiaUsable = false;
            session.uiaFailureReason = QString("UIA信息不足：nodes=%1 actionable=%2；阈值 nodes>=%3 actionable>=%4").arg(session.uiaNodeCount).arg(session.uiaActionableCount).arg(minNodes).arg(minActionable);
        } else { session.uiaUsable = true; session.uiaFailureReason.clear(); }
        if (!session.uiaUsable) { session.visionRequired = true; session.controlChannel = "VisionMouse"; log("[能力锁定] UIA不可用，本任务后续不再重复UIA扫描：" + session.uiaFailureReason); }
        else { session.controlChannel = "UIA"; log(QString("[能力锁定] UIA可用：nodes=%1 actionable=%2").arg(session.uiaNodeCount).arg(session.uiaActionableCount)); }
    }

    bool scanAgentUia(AgentSession& session, QString* error = nullptr) {
        const auto mode=agentRoot().value("modes").toObject().value(agentCore().value("mode").toString("assist")).toObject();
        const bool appAllows=session.scope!=AgentScope::SelectedWindow||AppRegistry::lookup(session.targetProcess).entry.value("uia").toBool(true);
        if (session.scope == AgentScope::SelectedWindow && session.uiaChecked && !session.uiaUsable) { if (error) *error = "本任务已锁定UIA不可用：" + session.uiaFailureReason; return false; }
        if(!appAllows||!mode.value("allow_uia").toBool(true)||!loadConfig("assets/config/permissions.json").value("uia").toObject().value("allow_scan").toBool(true)){
            session.uiaItems.clear();
            if (session.scope == AgentScope::SelectedWindow) { session.uiaChecked = true; session.uiaUsable = false; session.visionRequired = true; session.controlChannel = "VisionMouse"; session.uiaFailureReason = "权限或应用配置未开放UIA扫描"; }
            if(error)*error="当前权限未开放UIA扫描，使用其他允许的观察方式。"; return false;
        }
        if (session.scope == AgentScope::SelectedWindow) {
            if (!validWindowHandle(session.targetHwnd)) { if (error) *error = "指定窗口已经关闭或失效"; return false; }
            session.targetRect = windowRectOf(session.targetHwnd);
        }
        const HWND observedWindow=session.scope==AgentScope::SelectedWindow&&inApplicationScope(GetForegroundWindow(),session.targetHwnd)?GetForegroundWindow():session.targetHwnd;
        session.uiaItems = m_uiaExecutor.call([this, &session, observedWindow] {
            if (!m_agentUia) return QVector<UIAItem>{};
            if (session.scope == AgentScope::SelectedWindow) return m_agentUia->scanWindow(observedWindow);
            return m_agentUia->scanDesktop();
        });
        session.observationId=QUuid::createUuid().toString(QUuid::Id128);
        const QVector<UIAItem> items = session.uiaItems; const AgentScope scope = session.scope;
        postUi([this, items, scope] {
            if (scope == AgentScope::Desktop) { if (m_workMode == WorkMode::DesktopControl) { ensureDesktopOverlay(); m_desktopOverlay->setItems(items); } }
            else { if (m_workMode == WorkMode::WindowControl) { ensureWindowOverlay(); m_windowOverlay->setItems(items); } }
        });
        updateUiaCapabilityDecision(session);
        return true;
    }

    QString environmentSummary(AgentSession& session) {
        if (session.scope == AgentScope::SelectedWindow) session.targetRect = windowRectOf(session.targetHwnd);
        QString text;
        text += "OBSERVATION_ID="+session.observationId+"\nWORKFLOW="+session.request.kindName()+"\nPHASE="+session.workflow.phase+"\n";
        text += QString("CURRENT_MODE=%1\n" "TASK_MODE=%2\n").arg(session.scope == AgentScope::SelectedWindow ? "WINDOW_CONTROL" : "DESKTOP_CONTROL").arg(session.taskMode);
        if (session.scope == AgentScope::SelectedWindow) {
            text += QString("TARGET_HWND=%1\n" "TARGET_PID=%2\n" "TARGET_PROCESS_NAME=%3\n" "TARGET_WINDOW_TITLE=%4\n" "TARGET_WINDOW_RECT=(%5,%6,%7,%8)\n")
                .arg(static_cast<qulonglong>(reinterpret_cast<quintptr>(session.targetHwnd))).arg(session.targetPid).arg(session.targetProcess).arg(session.targetTitle)
                .arg(session.targetRect.x()).arg(session.targetRect.y()).arg(session.targetRect.width()).arg(session.targetRect.height());
            text += appCapabilitySummary(session.targetHwnd);
            text += QString("WINDOW_ROI_SCREEN=(%1,%2,%3,%4)\nCONTROL_CHANNEL=%5\nMCP_STATE=%6\nUIA_STATE_LOCK=%7\nUIA_NODES=%8\nUIA_ACTIONABLE=%9\n")
                .arg(session.targetRect.x()).arg(session.targetRect.y()).arg(session.targetRect.width()).arg(session.targetRect.height())
                .arg(session.controlChannel).arg(session.mcpChecked ? (session.mcpUsable ? "available" : "unavailable") : "unknown")
                .arg(session.uiaChecked ? (session.uiaUsable ? "available" : "unavailable") : "unknown").arg(session.uiaNodeCount).arg(session.uiaActionableCount);
            if (!session.uiaFailureReason.isEmpty()) text += "UIA_FAILURE_REASON=" + session.uiaFailureReason + "\n";
            if (!session.lastScreenshot.isEmpty()) text += "LAST_VISION_SCREENSHOT=" + session.lastScreenshot + "\n";
        }
        text += "UIA_STATE（仅当前扫描有效）:\n";
        int count = 0;
        for (const UIAItem& item : session.uiaItems) {
            if (!item.clickable && item.name.trimmed().isEmpty()) continue;
            text += QString("ID=%1 TYPE=%2 NAME=%3 AUTOMATION_ID=%4 RECT=(%5,%6,%7,%8)\n")
                .arg(item.id).arg(item.type).arg(item.name.isEmpty() ? "(无名称)" : item.name.left(100))
                .arg(item.automationId.isEmpty() ? "(无)" : item.automationId.left(80))
                .arg(item.rect.x()).arg(item.rect.y()).arg(item.rect.width()).arg(item.rect.height());
            if (++count >= session.uiaPromptLimit) { text += QString("UIA_STATE 已限制为前 %1 个可操作节点。\n").arg(session.uiaPromptLimit); break; }
        }
        return text;
    }

    QJsonObject normalizeAction(QJsonObject action, QString* error = nullptr) const {
        QString name = action.value("action").toString().trimmed().toLower();
        const QJsonObject toolsRoot = loadConfig("assets/config/tools.json");
        const QJsonObject aliases = toolsRoot.value("action_aliases").toObject();
        static const QMap<QString, QString> builtInAliases{
            {"uia_click", "uia_invoke"},{"invoke", "uia_invoke"},{"click_element", "uia_invoke"},
            {"click", "mouse_click"},{"move_mouse", "mouse_move"},{"drag", "mouse_drag"},{"draw_path", "mouse_path"},
            {"type_text", "keyboard_type"},{"press_key", "keyboard_hotkey"},{"hotkey", "keyboard_hotkey"},
            {"capture", "screen_capture"},{"analyze_image", "vision_analyze"},
            {"launch_app", "process_start"},{"open_app", "process_start"}
        };
        QString normalized;
        if (aliases.contains(name)) normalized = aliases.value(name).toString();
        else if (builtInAliases.contains(name)) normalized = builtInAliases.value(name);
        if (!normalized.isEmpty() && normalized != name) { Logger::write("ACTION", QString("动作归一化：%1 -> %2").arg(name, normalized)); name = normalized; }
        if(name=="process_start")name="app_launch";
        action["action"] = name;
        if(name=="keyboard_type") {
            if(!action.contains("text")&&action.contains("value")) action["text"]=action.value("value");
            action.remove("target_id");
        }
        if(name=="wait" && !action.contains("wait_ms")) {
            action["wait_ms"] = qBound(100, workflowConfig().value("default_wait_ms").toInt(1000), 30000);
        }
        if(name=="screen_capture") { if(!action.contains("scope"))action["scope"]=action.value("mode").toString("selected_window");if(action.value("scope")=="window")action["scope"]="selected_window"; }
        QJsonArray allowed = toolsRoot.value("validation").toObject().value("allowed_actions").toArray();
        if (allowed.isEmpty()) {
            const QStringList fallbackAllowed{"uia_scan", "uia_invoke", "uia_set_value", "uia_focus", "window_activate", "mouse_move", "mouse_click", "mouse_drag", "mouse_path", "keyboard_type", "keyboard_hotkey", "screen_capture", "vision_analyze", "cmd_exec", "powershell_exec", "process_start", "wait", "done"};
            for (const QString& item : fallbackAllowed) allowed.append(item);
        }
        bool known = false;
        for (const QJsonValue& value : allowed) { if (value.toString().compare(name, Qt::CaseInsensitive) == 0) { known = true; break; } }
        if (!known) { if (error) *error = "未知动作：" + name; return {}; }
        for(const auto& value:toolsRoot.value("tools").toArray()){
            const auto definition=value.toObject();if(definition.value("name").toString()==name&&!JsonContract::validateTool(action,definition,error))return {};
        }
        if ((name == "uia_invoke" || name == "uia_set_value" || name == "uia_focus") && action.value("target_id").toInt() <= 0) {
            if (error) *error = "UIA动作 target_id 必须大于0；视觉目标 uia_id=0 必须走mouse/keyboard fallback。";
            return {};
        }
        const auto number=[&](const QString& key){return action.value(key).isDouble()&&std::isfinite(action.value(key).toDouble());};
        const auto normalizedPair=[&](const QString& x,const QString& y){return number(x)&&number(y)&&action.value(x).toDouble()>=0&&action.value(x).toDouble()<=1&&action.value(y).toDouble()>=0&&action.value(y).toDouble()<=1;};
        const bool hasXY=number("x")&&number("y"),hasNorm=normalizedPair("norm_x","norm_y");
        if((name=="mouse_move"||name=="mouse_click")&&!hasXY&&!hasNorm&&action.value("target_id").toInt()<=0){if(error)*error="鼠标动作需要有效目标ID、完整屏幕坐标或完整归一化坐标";return {};}
        if((action.contains("norm_x")||action.contains("norm_y"))&&!hasNorm){if(error)*error="归一化坐标必须同时存在且在0到1之间";return {};}
        if(name=="mouse_drag"&&!((number("x1")&&number("y1")&&number("x2")&&number("y2"))||(normalizedPair("norm_x1","norm_y1")&&normalizedPair("norm_x2","norm_y2")))){if(error)*error="拖动缺少完整起终点";return {};}
        if(name=="keyboard_type" && action.value("text").isString() && action.value("text").toString().size()>8192){if(error)*error="输入文字过长";return {};}
        if(name=="keyboard_type" && (!action.value("text").isString() || action.value("text").toString().trimmed().isEmpty())){if(error)*error="keyboard_type缺少可执行text";return {};}
        if(name=="keyboard_hotkey"&&action.value("hotkey").toString().isEmpty()){if(error)*error="快捷键为空";return {};}
        if(name=="mcp_call"&&(!action.value("tool").isString()||!action.value("arguments").isObject())){if(error)*error="MCP动作缺少tool/arguments";return {};}
        if(error)error->clear();return action;
    }

    const UIAItem* findSessionItem(const AgentSession& session, int id) const {
        for (const UIAItem& item : session.uiaItems) if (item.id == id) return &item;
        return nullptr;
    }

    static bool textApproximatelyContains(const QString& haystack, const QString& needle) {
        const QString h = haystack.simplified().remove(' ');
        const QString n = needle.simplified().remove(' ');
        return !n.isEmpty() && h.contains(n, Qt::CaseInsensitive);
    }

    bool visionVisibleTextContains(const AgentSession& session, const QString& target) const {
        if (target.trimmed().isEmpty()) return false;
        for (const auto& value : session.latestVision.value("visible_text").toArray()) if (textApproximatelyContains(value.toString(), target)) return true;
        return false;
    }

    QJsonObject findVisualTargetByLabel(const AgentSession& session, const QString& target, double minConfidence = 0.70) const {
        if (target.trimmed().isEmpty()) return {};
        QJsonObject best; double bestConfidence = -1.0;
        for (const auto& value : session.latestVision.value("targets").toArray()) {
            const QJsonObject obj = value.toObject();
            const QString label = obj.value("label").toString(); const QString description = obj.value("description").toString();
            const double confidence = obj.value("confidence").toDouble(0.0);
            const bool matched = textApproximatelyContains(label, target) || textApproximatelyContains(description, target);
            if (matched && confidence >= minConfidence && confidence > bestConfidence) { best = obj; bestConfidence = confidence; }
        }
        return best;
    }

    QString visionSemanticHash(const QJsonObject& observation) const {
        QJsonObject compact; compact["screen_state"] = observation.value("screen_state").toString().simplified();
        QJsonArray labels;
        for (const auto& value : observation.value("targets").toArray()) {
            const QJsonObject t = value.toObject();
            labels.append(QJsonObject{{"label", t.value("label").toString().simplified()},{"x", qRound(t.value("norm_x").toDouble(-1.0) * 100.0) / 100.0},{"y", qRound(t.value("norm_y").toDouble(-1.0) * 100.0) / 100.0}});
        }
        compact["targets"] = labels;
        QJsonArray texts;
        for (const auto& value : observation.value("visible_text").toArray()) texts.append(value.toString().simplified());
        compact["visible_text"] = texts;
        return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(compact).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
    }

    QJsonObject directClickForVisualTarget(const AgentSession& session, const QJsonObject& target, const QString& reason) const {
        QJsonObject action{{"action", "mouse_click"},{"norm_x", target.value("norm_x").toDouble(-1.0)},{"norm_y", target.value("norm_y").toDouble(-1.0)},{"button", "left"},{"frame_id", session.frameId},{"reason", reason}};
        return action;
    }

    QString groundingPromptForEntity(const AgentSession& session, const QString& entity) const {
        return QString("这是精确GUI目标定位任务，不要描述整个界面。只定位文字/联系人【%1】所在的可点击行或可点击区域。如果【%1】在联系人列表中，target.label必须包含【%1】，norm_x/norm_y必须落在该联系人整行可点击区域中心。尽量返回norm_rect=[left,top,right,bottom]。不要返回搜索框、输入框、发送按钮等其他目标。如果画面中确实不存在【%1】，targets返回空数组，并在screen_state明确说明未找到。当前目标窗口=%2，窗口ROI=%3,%4,%5,%6。")
            .arg(entity, session.targetTitle).arg(session.targetRect.x()).arg(session.targetRect.y()).arg(session.targetRect.width()).arg(session.targetRect.height());
    }

    static QString normalizeSemanticText(QString text) {
        text = text.simplified().toLower();
        text.remove(QChar(u' ')); text.remove(QChar(u'：')); text.remove(QChar(u':'));
        text.remove(QChar(u'“')); text.remove(QChar(u'”')); text.remove(QChar(u'"'));
        return text;
    }

    static bool parseNormalizedCoordinatePair(const QString& text, double* x, double* y) {
        if (!x || !y) return false;
        const QRegularExpression re(R"((?:\(|\[)?\s*(0(?:\.\d+)?|1(?:\.0+)?)\s*[,，]\s*(0(?:\.\d+)?|1(?:\.0+)?)\s*(?:\)|\])?)");
        const auto m = re.match(text.trimmed()); if (!m.hasMatch()) return false;
        bool okX = false, okY = false;
        const double nx = m.captured(1).toDouble(&okX); const double ny = m.captured(2).toDouble(&okY);
        if (!okX || !okY || nx < 0.0 || nx > 1.0 || ny < 0.0 || ny > 1.0) return false;
        *x = nx; *y = ny; return true;
    }

    bool conversationLikeSurface(const AgentSession& session) const {
        const QString surface = normalizeSemanticText(session.runtimeProfile.surfaceType);
        const QString profile = normalizeSemanticText(session.runtimeProfile.processProfile);
        const QString task = normalizeSemanticText(session.runtimeProfile.taskProfile);
        if (surface.contains("chat") || surface.contains("message") || surface.contains("conversation") || surface.contains("聊天") || surface.contains("消息") || task.contains("customer_service")) return true;
        for (const QString& target : session.runtimeProfile.likelyTargets) {
            const QString t = normalizeSemanticText(target);
            if (t.contains("send") || t.contains("message") || t.contains("chat_input")) return true;
        }
        Q_UNUSED(profile); return false;
    }

    QJsonObject currentConversationInputTarget(const AgentSession& session, double minimumConfidence = 0.55) const {
        if (!conversationLikeSurface(session) || session.latestVision.isEmpty()) return {};
        const QStringList queries{"聊天输入框", "消息输入框", "输入消息", "文本输入区域", "输入框"};
        for (const QString& query : queries) { const QJsonObject target = bestVisualTargetForQuery(session, query, minimumConfidence); if (!target.isEmpty()) return target; }
        const QJsonArray targets = session.latestVision.value("targets").toArray();
        QJsonObject best; double bestConfidence = minimumConfidence;
        for (const QJsonValue& value : targets) {
            const QJsonObject target = value.toObject();
            const double confidence = target.value("confidence").toDouble(0.0);
            if (confidence < bestConfidence) continue;
            const QString semantic = target.value("label").toString() + " " + target.value("description").toString();
            if (semanticRole(semantic) != "input") continue;
            if (!target.value("norm_x").isDouble() || !target.value("norm_y").isDouble()) continue;
            best = target; bestConfidence = confidence;
        }
        return best;
    }

    bool plannerAttemptsUnrequestedConversationSwitch(const AgentSession& session, const QJsonObject& rawAction) const {
        if (!conversationLikeSurface(session)) return false;
        if (!session.targetEntity.trimmed().isEmpty()) return false;
        const QString action = rawAction.value("action").toString().trimmed().toLower();
        if (action != "mouse_click" && action != "uia_invoke" && action != "uia_focus") return false;
        QString target = rawAction.value("target").toString().trimmed();
        if (target.isEmpty() && rawAction.value("target_id").isString()) target = rawAction.value("target_id").toString().trimmed();
        const QString reason = rawAction.value("reason").toString().trimmed();
        const QString t = normalizeSemanticText(target); const QString r = normalizeSemanticText(reason);
        if (semanticRole(target) == "input" || looksLikeCommitTarget(target)) return false;
        const bool switchWords = r.contains("选择") || r.contains("打开") || r.contains("进入") || r.contains("点击");
        const bool conversationObject = r.contains("联系人") || r.contains("聊天对象") || r.contains("聊天列表") || r.contains("会话列表") || r.contains("列表项") || t.contains("联系人") || t.contains("聊天列表") || t.contains("会话");
        return switchWords && conversationObject;
    }

    static bool looksLikeCommitTarget(const QString& text) {
        const QString s = normalizeSemanticText(text);
        static const QStringList keys{"发送", "send", "提交", "submit", "确认", "confirm", "发布", "publish"};
        for (const QString& key : keys) if (s.contains(normalizeSemanticText(key))) return true;
        return false;
    }

    static QString semanticRole(const QString& text) {
        const QString s = normalizeSemanticText(text);
        auto hasAny = [&](std::initializer_list<const char*> keys) {
            for (const char* key : keys) if (s.contains(QString::fromUtf8(key))) return true;
            return false;
        };
        if (hasAny({"发送","提交","确认","发布","send","submit","confirm","publish"})) return "commit";
        if (hasAny({"输入框","输入区域","文本框","编辑框","输入","textinput","textbox","editor","inputfield"})) return "input";
        if (hasAny({"画布","绘图区","工作区","canvas","workspace","drawingarea"})) return "canvas";
        if (hasAny({"画笔","铅笔","钢笔","工具","brush","pencil","pen","tool"})) return "tool";
        if (hasAny({"菜单","menu"})) return "menu";
        if (hasAny({"搜索","search"})) return "search";
        if (hasAny({"列表","list"})) return "list";
        if (hasAny({"按钮","button"})) return "button";
        return {};
    }

    static bool semanticRolesCompatible(const QString& requested, const QString& observed) {
        const QString a = semanticRole(requested); const QString b = semanticRole(observed);
        if (a.isEmpty() || b.isEmpty()) return true;
        if (a == b) return true;
        if ((a=="commit" && b=="input") || (a=="input" && b=="commit") || (a=="canvas" && b=="tool") || (a=="tool" && b=="canvas")) return false;
        if (a=="button" || b=="button") return true;
        return false;
    }

    QJsonObject bestVisualTargetForQuery(const AgentSession& session, const QString& query, double minimumConfidence = 0.55) const {
        const QJsonArray targets = session.latestVision.value("targets").toArray();
        if (targets.isEmpty()) return {};
        const QString q = normalizeSemanticText(query);
        const QStringList requestedParts = query.split(QRegularExpression(R"([/／|,，、\s]+)"), Qt::SkipEmptyParts);
        QJsonObject best; double bestScore = -1.0;
        for (const QJsonValue& value : targets) {
            const QJsonObject target = value.toObject();
            const double confidence = target.value("confidence").toDouble(0.0); if (confidence < minimumConfidence) continue;
            const QString label = target.value("label").toString(); const QString desc = target.value("description").toString();
            const QString semantic = label + " " + desc;
            if (!semanticRolesCompatible(query, semantic)) continue;
            const QString l = normalizeSemanticText(label); const QString d = normalizeSemanticText(desc);
            double semanticScore = 0.0;
            if (!q.isEmpty()) {
                if (l == q) semanticScore = 4.0;
                else if (l.contains(q) || q.contains(l)) semanticScore = 3.0;
                else if (d.contains(q) || q.contains(d)) semanticScore = 2.2;
                else {
                    for (const QString& part : requestedParts) {
                        const QString p = normalizeSemanticText(part); if (p.isEmpty()) continue;
                        if (l.contains(p) || p.contains(l)) semanticScore = qMax(semanticScore, 2.6);
                        else if (d.contains(p)) semanticScore = qMax(semanticScore, 1.8);
                    }
                }
                if (semanticScore <= 0.0 && !semanticRole(query).isEmpty() && semanticRole(query) == semanticRole(semantic)) semanticScore = 1.5;
                if (semanticScore <= 0.0) continue;
            }
            const double nx = target.value("norm_x").toDouble(-1.0); const double ny = target.value("norm_y").toDouble(-1.0);
            if (nx < 0.0 || nx > 1.0 || ny < 0.0 || ny > 1.0) continue;
            const double score = semanticScore + confidence;
            if (score > bestScore) { bestScore = score; best = target; }
        }
        return best;
    }

    QJsonObject bestVisualTargetFromReason(const AgentSession& session, const QString& reason) const {
        const QString normalizedReason = normalizeSemanticText(reason);
        QJsonObject best; int longest = 0; double bestConfidence = 0.0;
        for (const QJsonValue& value : session.latestVision.value("targets").toArray()) {
            const QJsonObject target = value.toObject();
            const QString label = target.value("label").toString(); const QString normalizedLabel = normalizeSemanticText(label);
            const double confidence = target.value("confidence").toDouble(0.0);
            if (normalizedLabel.isEmpty() || !normalizedReason.contains(normalizedLabel)) continue;
            const double nx = target.value("norm_x").toDouble(-1.0); const double ny = target.value("norm_y").toDouble(-1.0);
            if (nx < 0.0 || nx > 1.0 || ny < 0.0 || ny > 1.0) continue;
            if (normalizedLabel.size() > longest || (normalizedLabel.size() == longest && confidence > bestConfidence)) { longest = normalizedLabel.size(); bestConfidence = confidence; best = target; }
        }
        return best;
    }

    QJsonObject bestCommitVisualTarget(const AgentSession& session) const {
        QJsonObject best; double bestConfidence = -1.0;
        for (const QJsonValue& value : session.latestVision.value("targets").toArray()) {
            const QJsonObject target = value.toObject();
            const QString label = target.value("label").toString(); const QString description = target.value("description").toString();
            const QString semantic = label + " " + description; const QString normalized = normalizeSemanticText(semantic);
            const bool inputLike = normalized.contains("输入框") || normalized.contains("输入区域") || normalized.contains("文本框") || normalized.contains("编辑框") || normalized.contains("textinput") || normalized.contains("textbox") || normalized.contains("editor");
            if (inputLike || !looksLikeCommitTarget(semantic)) continue;
            const double confidence = target.value("confidence").toDouble(0.0);
            const double nx = target.value("norm_x").toDouble(-1.0); const double ny = target.value("norm_y").toDouble(-1.0);
            if (confidence < 0.55 || nx < 0.0 || nx > 1.0 || ny < 0.0 || ny > 1.0) continue;
            if (confidence > bestConfidence) { bestConfidence = confidence; best = target; }
        }
        return best;
    }

    void fillMouseActionFromVisualTarget(AgentSession& session, QJsonObject& action, const QJsonObject& target, const QString& note) {
        if (target.isEmpty()) return;
        action.remove("target_id");
        double nx = target.value("norm_x").toDouble(-1.0); double ny = target.value("norm_y").toDouble(-1.0);
        const QJsonArray rect = target.value("norm_rect").toArray();
        if (rect.size() == 4) {
            const double l = rect.at(0).toDouble(); const double t = rect.at(1).toDouble();
            const double r = rect.at(2).toDouble(); const double b = rect.at(3).toDouble();
            if (l >= 0.0 && t >= 0.0 && r <= 1.0 && b <= 1.0 && r > l && b > t) { nx = (l + r) * 0.5; ny = (t + b) * 0.5; action["target_norm_rect"] = rect; }
        }
        action["norm_x"] = nx; action["norm_y"] = ny; action["frame_id"] = session.frameId;
        if (!action.contains("button")) action["button"] = "left";
        if (!action.contains("click_count")) action["click_count"] = 1;
        session.lastResolvedTargetLabel = target.value("label").toString();
        session.lastActionResolutionNote = note;
    }

    QString semanticTargetFromPlannerAction(const QJsonObject& action) const {
        for (const QString& key : {QString("target"), QString("target_name"), QString("label"), QString("element"), QString("control")}) {
            const QString value = action.value(key).toString().trimmed();
            if (!value.isEmpty()) return value.left(160);
        }
        const QString reason = action.value("reason").toString().trimmed();
        const QList<QPair<QString, QString>> patterns{{"发送", "发送/提交"},{"提交", "发送/提交"},{"确认", "确认"},{"保存", "保存"},{"输入框", "输入框"},{"输入区域", "输入框"},{"画布", "canvas"},{"canvas", "canvas"},{"菜单", "菜单"},{"按钮", "按钮"}};
        for (const auto& pair : patterns) if (reason.contains(pair.first, Qt::CaseInsensitive)) return pair.second;
        return {};
    }

    QJsonObject invalidatePlannerUiaReferenceIfLocked(AgentSession& session, QJsonObject action, QString* note = nullptr) {
        if (!(session.uiaChecked && !session.uiaUsable)) return action;
        const QString originalName = action.value("action").toString().trimmed().toLower();
        const int originalId = action.value("target_id").toInt(0);
        const bool hasUiaReference = originalId > 0 || originalName.startsWith("uia_");
        if (!hasUiaReference) return action;
        const QString semanticTarget = semanticTargetFromPlannerAction(action);
        action.remove("target_id");
        if (originalName == "uia_set_value") action["action"] = "keyboard_type";
        else if (originalName == "uia_invoke" || originalName == "uia_focus") action["action"] = "mouse_click";
        else if (originalName == "uia_scan") action["action"] = "vision_analyze";
        if (!semanticTarget.isEmpty() && !action.contains("target")) action["target"] = semanticTarget;
        if (note) *note = QString("UIA已锁定不可用，Planner的target_id=%1 / %2已作废；%3").arg(originalId).arg(originalName).arg(semanticTarget.isEmpty() ? "未发现可恢复语义目标" : "改为语义目标“" + semanticTarget + "”");
        return action;
    }

    QJsonObject resolveSemanticPlannerAction(AgentSession& session, QJsonObject action, QString* resolutionNote = nullptr) {
        QString name = action.value("action").toString().trimmed().toLower();
        QString uiaGuardNote;
        action = invalidatePlannerUiaReferenceIfLocked(session, action, &uiaGuardNote);
        name = action.value("action").toString().trimmed().toLower();
        if (name == "click") name = "mouse_click";
        else if (name == "move_mouse") name = "mouse_move";
        else if (name == "type_text") name = "keyboard_type";
        else if (name == "hotkey" || name == "press_key") name = "keyboard_hotkey";
        action["action"] = name;
        QString note; session.pendingCommitDispatched = false;

        if (name == "keyboard_type" && !action.contains("text")) {
            for (const QString& key : {QString("value"), QString("content"), QString("message"), QString("reply")}) {
                if (action.value(key).isString() && !action.value(key).toString().isEmpty()) { action["text"] = action.value(key); note = "已将Planner文本字段“" + key + "”规范为keyboard_type.text"; break; }
            }
        }

        if ((name == "mouse_click" || name == "mouse_move") && action.value("target_id").isString()) {
            const QString rawTarget = action.value("target_id").toString().trimmed();
            bool numericIdOk = false; const int numericId = rawTarget.toInt(&numericIdOk);
            if (numericIdOk && numericId > 0) action["target_id"] = numericId;
            else {
                double nx = -1.0, ny = -1.0;
                if (parseNormalizedCoordinatePair(rawTarget, &nx, &ny)) { action.remove("target_id"); action["norm_x"] = nx; action["norm_y"] = ny; action["frame_id"] = session.frameId; note = "Planner把归一化坐标写进target_id，已自动修复为Vision norm_x/norm_y，并移除UIA target_id"; }
                else { action["target"] = rawTarget; action.remove("target_id"); }
            }
        }

        if (name == "mouse_click" || name == "mouse_move") {
            const bool hasNorm = action.value("norm_x").isDouble() && action.value("norm_y").isDouble();
            const bool hasXY = action.value("x").isDouble() && action.value("y").isDouble();
            const bool hasUiaId = action.value("target_id").toInt() > 0;
            if (!hasNorm && !hasXY && !hasUiaId) {
                QString query;
                for (const QString& key : {QString("target"), QString("target_name"), QString("label"), QString("element"), QString("control")}) {
                    if (action.value(key).isString() && !action.value(key).toString().trimmed().isEmpty()) { query = action.value(key).toString().trimmed(); break; }
                }
                QJsonObject visualTarget;
                if (!query.isEmpty()) visualTarget = bestVisualTargetForQuery(session, query);
                else {
                    visualTarget = bestVisualTargetFromReason(session, action.value("reason").toString());
                    if (visualTarget.isEmpty() && session.actionReady) {
                        QJsonArray executableTargets;
                        for (const QJsonValue& value : session.latestVision.value("targets").toArray()) {
                            const QJsonObject t = value.toObject();
                            const double confidence = t.value("confidence").toDouble(0.0);
                            const double nx = t.value("norm_x").toDouble(-1.0); const double ny = t.value("norm_y").toDouble(-1.0);
                            if (confidence >= 0.75 && nx >= 0.0 && nx <= 1.0 && ny >= 0.0 && ny <= 1.0) executableTargets.append(t);
                        }
                        if (executableTargets.size() == 1) visualTarget = executableTargets.first().toObject();
                    }
                }
                if (visualTarget.isEmpty() && !query.isEmpty() && semanticRole(query) == "commit" && conversationLikeSurface(session) && availableActions(session).contains("keyboard_hotkey")) {
                    action = QJsonObject{{"action", "keyboard_hotkey"},{"hotkey", "ENTER"},{"reason", "当前视觉帧未返回与发送/提交语义兼容的控件，禁止误绑定其他目标；使用Enter提交。"}};
                    session.pendingCommitDispatched = session.pendingCommitRequired;
                    name = "keyboard_hotkey"; note = "明确提交目标未找到，已禁止错误目标替代并切换Enter fallback";
                }
                if (!visualTarget.isEmpty()) { fillMouseActionFromVisualTarget(session, action, visualTarget, "已从LATEST_VISION_OBSERVATION自动绑定语义目标“" + visualTarget.value("label").toString() + "”"); note = session.lastActionResolutionNote; }
                else if (name == "mouse_click" || name == "mouse_move") {
                    double nx = -1.0, ny = -1.0;
                    const QString reason = action.value("reason").toString();
                    const QRegularExpression pairInReason(R"((0(?:\.\d+)?|1(?:\.0+)?)\s*[,，]\s*(0(?:\.\d+)?|1(?:\.0+)?))");
                    const auto m = pairInReason.match(reason);
                    if (m.hasMatch() && parseNormalizedCoordinatePair(m.captured(0), &nx, &ny)) { action.remove("target_id"); action["norm_x"] = nx; action["norm_y"] = ny; action["frame_id"] = session.frameId; note = "已从Planner reason中的坐标自动补全Vision mouse动作，并移除UIA target_id"; }
                    else if (!query.isEmpty() && availableActions(session).contains("vision_analyze")) { action.remove("target_id"); action["target"] = query; note = "明确语义目标“" + query + "”当前帧缺少可执行坐标，交由NeedGrounding处理"; }
                }
            }
        }

        if (session.pendingCommitRequired && (name == "wait" || name == "wait_for_change" || name == "vision_analyze" || name == "screen_capture" || name == "done")) {
            QJsonObject commitTarget = bestCommitVisualTarget(session);
            if (!commitTarget.isEmpty()) {
                QJsonObject commitAction{{"action", "mouse_click"},{"reason", "上一步文字已输入但尚未提交，自动执行当前界面的提交/发送目标。"}};
                fillMouseActionFromVisualTarget(session, commitAction, commitTarget, "pending_commit：绑定“" + commitTarget.value("label").toString() + "”");
                session.pendingCommitDispatched = true; action = commitAction; name = "mouse_click"; note = session.lastActionResolutionNote;
            } else if (conversationLikeSurface(session) && availableActions(session).contains("keyboard_hotkey")) {
                action = QJsonObject{{"action", "keyboard_hotkey"},{"hotkey", "ENTER"},{"reason", "聊天/消息界面已有未提交文本，当前截图未返回发送按钮，使用Enter提交。"}};
                session.pendingCommitDispatched = true; name = "keyboard_hotkey"; note = "pending_commit：发送按钮暂未返回，使用聊天界面Enter提交fallback";
            }
        }

        for (const QString& key : {QString("target"), QString("target_name"), QString("label"), QString("element"), QString("control")}) action.remove(key);
        if (note.isEmpty() && !uiaGuardNote.isEmpty()) note = uiaGuardNote;
        if (resolutionNote) *resolutionNote = note;
        return action;
    }

    static QString normalizeCommitEvidenceText(QString text) {
        text = text.toLower().trimmed();
        text.remove(QRegularExpression(R"([\s，。！？、,.!?;；:："'“”‘’（）()【】\[\]<>《》~～\-_=+]+)"));
        return text;
    }

    static bool similarCommitText(const QString& a, const QString& b) {
        const QString x = normalizeCommitEvidenceText(a); const QString y = normalizeCommitEvidenceText(b);
        if (x.isEmpty() || y.isEmpty()) return false;
        if (x == y || x.contains(y) || y.contains(x)) return true;
        const int minLen = qMin(x.size(), y.size()); if (minLen < 4) return false;
        int common = 0; for (const QChar ch : x) if (y.contains(ch)) ++common;
        return double(common) / double(qMax(1, qMax(x.size(), y.size()))) >= 0.72;
    }

    bool latestObservationStillShowsTypedText(const AgentSession& session) const {
        if (session.lastTypedText.trimmed().isEmpty()) return false;
        if (similarCommitText(session.lastTypedText, session.latestRelevantText)) return true;
        const QJsonArray visible = session.latestVision.value("visible_text").toArray();
        for (const QJsonValue& v : visible) if (similarCommitText(session.lastTypedText, v.toString())) return true;
        return false;
    }

    void resetPendingCommitClickRetry(AgentSession& session) {
        session.pendingCommitClickAttempts = 0; session.pendingCommitLastClickPoint = QPoint(-1, -1);
        session.pendingCommitLastNormX = -1.0; session.pendingCommitLastNormY = -1.0;
    }

    void armPendingCommitAfterText(AgentSession& session, const QJsonObject& action) {
        if (!conversationLikeSurface(session)) return;
        session.pendingCommitRequired = true; session.pendingCommitDispatched = false; session.pendingCommitAwaitingVerification = false;
        session.pendingCommitVerificationObservations = 0; resetPendingCommitClickRetry(session);
        session.lastTypedText = action.value("text").toString().left(1000);
        const QJsonObject commitTarget = bestCommitVisualTarget(session);
        session.pendingCommitLabel = commitTarget.value("label").toString().trimmed();
        if (session.pendingCommitLabel.isEmpty() || !looksLikeCommitTarget(session.pendingCommitLabel)) session.pendingCommitLabel = "发送/提交";
        session.nextObjective = "提交刚输入的文本；在提交成功前不能进入等待状态";
        log("[动作事务] 文本已输入，建立pending_commit=" + session.pendingCommitLabel);
    }

    bool verifyPendingCommitFromLatestObservation(AgentSession& session) {
        if (!session.pendingCommitRequired || !session.pendingCommitAwaitingVerification) return false;
        ++session.pendingCommitVerificationObservations;
        if (conversationLikeSurface(session) && latestObservationStillShowsTypedText(session)) {
            log("[动作事务] 发送验证失败：仍检测到刚输入的文字，判定提交按钮没有真正命中。");
            session.pendingCommitDispatched = false; session.pendingCommitAwaitingVerification = false;
            session.pendingCommitBeforeFingerprint.clear(); session.pendingCommitVerificationObservations = 0;
            const int maxAttempts = qBound(1, agentRoot().value("pending_commit").toObject().value("max_click_attempts").toInt(3), 5);
            if (session.pendingCommitClickAttempts < maxAttempts) {
                session.nextObjective = "重新命中发送/提交按钮；文字仍在输入区，不能进入等待";
                session.latestVision = {}; session.visionRequired = true; session.actionReady = false;
                session.groundingTarget = "发送/提交";
                session.pendingSemanticAction = {{"action", "mouse_click"},{"target", "发送/提交"},{"reason", "上一轮点击后输入文字仍然存在，重新定位发送按钮并再次提交"}};
                session.semanticGroundingAttempts = 0;
                log(QString("[动作事务] pending_commit继续保持，准备第%1次发送按钮重试。").arg(session.pendingCommitClickAttempts + 1));
                return false;
            }
            log("[动作事务] 发送按钮多次重试仍未确认提交，保持pending_commit，不进入WAIT。");
            session.nextObjective = "发送按钮多次未命中；保持当前文字，不重复输入";
            return false;
        }
        const QString current = observationFingerprint(session);
        const bool changed = !current.isEmpty() && !session.pendingCommitBeforeFingerprint.isEmpty() && current != session.pendingCommitBeforeFingerprint;
        if (conversationLikeSurface(session)) {
            if (!changed && session.pendingCommitVerificationObservations < 2) return false;
            if (!changed) {
                log("[动作事务] 发送后未观察到可靠变化，继续保持pending_commit验证。");
                session.pendingCommitAwaitingVerification = false; session.pendingCommitBeforeFingerprint.clear();
                session.pendingCommitVerificationObservations = 0; session.latestVision = {};
                session.visionRequired = true; session.actionReady = false; session.groundingTarget = "发送/提交";
                return false;
            }
        } else {
            if (!changed && session.pendingCommitVerificationObservations < 2) return false;
        }
        log("[动作事务] pending_commit严格验证通过：未再检测到原输入文字且界面状态已变化。");
        session.pendingCommitRequired = false; session.pendingCommitDispatched = false; session.pendingCommitAwaitingVerification = false;
        session.pendingCommitBeforeFingerprint.clear(); session.pendingCommitVerificationObservations = 0;
        session.pendingCommitLabel.clear(); session.pendingSemanticAction = {}; session.groundingTarget.clear();
        session.semanticGroundingAttempts = 0; resetPendingCommitClickRetry(session);
        if (conversationLikeSurface(session)) {
            session.hasSentInteraction = true; session.externalRelevantTextBaseline = session.latestRelevantText.trimmed();
            session.waitingForExternalChange = true; session.policyPhase = AgentPolicyPhase::WaitForChange;
            session.workflow.phase = "waiting"; session.nextObjective = "等待外部回复或界面变化；没有变化时不要重新规划";
            log("[交互轮次] 当前消息已确认提交，进入WAITING_EXTERNAL。");
        } else session.nextObjective = "根据最新界面继续下一步";
        return true;
    }

    QJsonObject buildDirectPendingCommitAction(AgentSession& session, QString* note = nullptr) {
        if (!session.pendingCommitRequired) return {};
        const QJsonObject commitTarget = bestCommitVisualTarget(session);
        if (!commitTarget.isEmpty()) {
            QJsonObject action{{"action", "mouse_click"},{"target", "发送/提交"},{"reason", "pending_commit事务：提交当前已输入文本"}};
            fillMouseActionFromVisualTarget(session, action, commitTarget, "pending_commit直接绑定提交目标“" + commitTarget.value("label").toString() + "”");
            const QJsonArray rect = commitTarget.value("norm_rect").toArray();
            if (rect.size() != 4 && session.pendingCommitClickAttempts > 0) {
                const QJsonObject commitCfg = agentRoot().value("pending_commit").toObject();
                const double step = qBound(0.002, commitCfg.value("retry_downward_norm_step").toDouble(0.012), 0.04);
                double ny = action.value("norm_y").toDouble(-1.0);
                if (ny >= 0.0) { ny = qMin(0.995, ny + step * session.pendingCommitClickAttempts); action["norm_y"] = ny; session.lastActionResolutionNote += QString("；第%1次提交重试，Y向下修正到%2").arg(session.pendingCommitClickAttempts + 1).arg(ny, 0, 'f', 4); }
            }
            session.pendingCommitDispatched = true;
            if (note) *note = session.lastActionResolutionNote;
            return action;
        }
        return {};
    }

    void armPendingCommitGrounding(AgentSession& session) {
        session.pendingSemanticAction = {{"action", "mouse_click"},{"target", "发送/提交"},{"reason", "pending_commit事务：提交当前已输入文本"}};
        session.groundingTarget = "发送/提交"; session.semanticGroundingAttempts = 0;
        session.latestVision = {}; session.actionReady = false; session.visionRequired = true;
        session.nextObjective = "定位并执行发送/提交；提交完成前不允许重新输入或等待";
    }

    QJsonObject pendingCommitEnterFallback(AgentSession& session, QString* note = nullptr) {
        if (!session.pendingCommitRequired || !conversationLikeSurface(session) || !availableActions(session).contains("keyboard_hotkey")) return {};
        session.pendingCommitDispatched = true;
        if (note) *note = "pending_commit未定位到提交控件，使用Enter通用fallback";
        return {{"action", "keyboard_hotkey"},{"hotkey", "ENTER"},{"reason", "pending_commit事务：提交控件不可定位，使用Enter提交"}};
    }

    void finishPendingCommitIfNeeded(AgentSession& session, const QString& actionName, bool success) {
        if (!session.pendingCommitRequired || !session.pendingCommitDispatched) return;
        if (!success) { session.pendingCommitDispatched = false; return; }
        if (actionName == "mouse_click" || actionName == "keyboard_hotkey") {
            if (actionName == "mouse_click") ++session.pendingCommitClickAttempts;
            log(QString("[动作事务] pending_commit已执行，进入提交效果验证；click_attempt=%1").arg(session.pendingCommitClickAttempts));
            session.pendingCommitDispatched = false; session.pendingCommitAwaitingVerification = true;
            session.pendingCommitBeforeFingerprint = observationFingerprint(session);
            session.pendingCommitVerificationObservations = 0;
            session.nextObjective = "严格验证刚才文字是否真正提交；如果文字仍在输入区就重新命中发送按钮";
        }
    }

    QPoint resolveActionPoint(const AgentSession& session, const QJsonObject& action) const {
        const int targetId=action.value("target_id").toInt();
        if(targetId>0){const UIAItem* item=findSessionItem(session,targetId);return item?item->rect.center():QPoint(-1,-1);}
        const double nx = action.value("norm_x").toDouble(-1.0); const double ny = action.value("norm_y").toDouble(-1.0);
        if (nx >= 0.0 && ny >= 0.0) {
            const double safeX = qBound(0.0, nx, 1.0); const double safeY = qBound(0.0, ny, 1.0);
            QRect base = session.lastScreenshotRect;
            if (base.isEmpty()) base = session.scope == AgentScope::SelectedWindow ? session.targetRect : virtualDesktopRect();
            return QPoint(base.left() + qRound(safeX * qMax(0, base.width() - 1)), base.top() + qRound(safeY * qMax(0, base.height() - 1)));
        }
        return QPoint(action.value("x").toInt(), action.value("y").toInt());
    }

    bool pointAllowed(const AgentSession& session, const QPoint& point) const {
        if (session.scope != AgentScope::SelectedWindow) return virtualDesktopRect().contains(point);
        if(!validWindowHandle(session.targetHwnd))return false;
        const HWND foreground=GetForegroundWindow();
        if(inApplicationScope(foreground,session.targetHwnd)&&GetAncestor(foreground,GA_ROOT)!=GetAncestor(session.targetHwnd,GA_ROOT))return windowRectOf(foreground).contains(point);
        return windowRectOf(session.targetHwnd).contains(point);
    }

    void hideOverlayForCapture(AgentScope scope) {
        Q_UNUSED(scope);
        runUiBlocking([this]{if(m_desktopOverlay)m_desktopOverlay->hide();if(m_windowOverlay)m_windowOverlay->hide();});
    }
    void restoreOverlayAfterCapture(AgentScope scope) { Q_UNUSED(scope);postUi([this]{if(!m_shutdownStarted)restoreCurrentOverlay();}); }

    QString resolveInstalledApplication(const QString& hint) const {
        QString error;const QString path=ApplicationLauncher::resolve(hint,&error);
        if(path.isEmpty())Logger::write("APP_RESOLVE",error);
        return path;
    }

    bool launchResolvedApplication(const QString& target,QString* actualPath,QString* error) const {
        const auto result=ApplicationLauncher::launch(target,{},[this]{return m_stop||m_userInterrupted;});
        if(actualPath)*actualPath=result.executable;
        if(error)*error=result.result.success()?QString():result.result.message;
        return result.result.success();
    }

    QString buildWorldStateSummary(const AgentSession& session) const {
        QJsonObject capability{{"mcp", session.mcpChecked ? (session.mcpUsable ? "available" : "unavailable") : "unknown"},{"uia", session.uiaChecked ? (session.uiaUsable ? "available" : "unavailable") : "unknown"},{"vision_required", session.visionRequired},{"control_channel", session.controlChannel}};
        if (!session.mcpFailureReason.isEmpty()) capability["mcp_reason"] = session.mcpFailureReason.left(240);
        if (!session.uiaFailureReason.isEmpty()) capability["uia_reason"] = session.uiaFailureReason.left(240);
        QJsonObject target{{"process", session.targetProcess},{"title", session.targetTitle},{"hwnd", QString::number(quintptr(session.targetHwnd))},{"screen_rect", QJsonArray{session.targetRect.x(), session.targetRect.y(), session.targetRect.width(), session.targetRect.height()}}};
        QJsonArray recentFacts;
        const int begin = qMax(0, int(session.recentHistory.size()) - 2);
        for (int i = begin; i < session.recentHistory.size(); ++i) {
            const QJsonObject item = session.recentHistory.at(i).toObject(); const QJsonObject action = item.value("action").toObject();
            recentFacts.append(QJsonObject{{"action", action.value("action").toString()},{"ok", item.value("tool_success").toBool(false)},{"result", item.value("result").toString().section('\n',0,0).left(160)}});
        }
        QJsonObject world{
            {"task", session.goal},{"workflow", session.request.kindName()},{"task_mode", session.taskMode},{"phase", session.workflow.phase},
            {"runtime_app_profile", session.runtimeProfileReady ? session.runtimeProfile.json() : QJsonObject{}},
            {"generic_policy_phase", agentPolicyPhaseName(session)},{"target", target},{"capability", capability},
            {"last_screenshot", session.lastScreenshot},{"frame_id", session.frameId},{"known_visual_targets", session.knownVisualTargets},
            {"target_entity", session.targetEntity},{"current_contact", session.currentContact},{"conversation_state", session.conversationState.left(800)},{"next_objective", session.nextObjective.left(400)},
            {"action_resolution", QJsonObject{{"last_target_label", session.lastResolvedTargetLabel},{"last_note", session.lastActionResolutionNote.left(300)},{"pending_commit", session.pendingCommitRequired},{"pending_commit_label", session.pendingCommitLabel},{"last_typed_text", session.lastTypedText.left(300)}}},
            {"recent_progress", session.recentProgress},{"next_objective", session.nextObjective.left(300)},
            {"vision_watchdog", QJsonObject{{"consecutive_observations", session.consecutiveVisionObservations},{"grounding_failures", session.consecutiveGroundingFailures},{"grounding_target", session.lastGroundingTarget}}},
            {"recent_facts", recentFacts},{"rule", "旧UIA ID、旧截图坐标不可直接复用；窗口移动或重新截图后必须重新观察。"}
        };
        return QString::fromUtf8(QJsonDocument(world).toJson(QJsonDocument::Compact));
    }

    void persistContextSummary(const AgentSession& session) {
        const QString path = QDir(Logger::root()).filePath("agent_context/task_" + session.taskId + ".json");
        const QString worldState = buildWorldStateSummary(session);
        const QJsonObject object{
            {"version", 3},{"task_id", session.taskId},{"goal", session.goal},{"task_mode", session.taskMode},
            {"status", session.status},{"step", session.stepNumber},{"pending_action", session.pendingAction},
            {"task_request",session.request.json()},{"workflow_phase",session.workflow.phase},{"observation_policy", session.observationPolicy},
            {"runtime_app_profile", session.runtimeProfileReady ? session.runtimeProfile.json() : QJsonObject{}},
            {"generic_policy_phase", agentPolicyPhaseName(session)},{"target_entity", session.targetEntity},
            {"target_pid", double(session.targetPid)},{"target_hwnd", QString::number(quintptr(session.targetHwnd))},
            {"target_title", session.targetTitle},{"target_process", session.targetProcess},
            {"target_rect", QJsonArray{session.targetRect.x(), session.targetRect.y(), session.targetRect.width(), session.targetRect.height()}},
            {"scope", session.scope == AgentScope::SelectedWindow ? "window" : "desktop"},
            {"updated_at", QDateTime::currentDateTime().toString(Qt::ISODateWithMs)},
            {"world_state", worldState},{"summary", worldState},{"recent_history", session.recentHistory},{"last_screenshot", session.lastScreenshot},
            {"capability", QJsonObject{{"mcp_checked", session.mcpChecked},{"mcp_usable", session.mcpUsable},{"uia_checked", session.uiaChecked},{"uia_usable", session.uiaUsable},{"uia_nodes", session.uiaNodeCount},{"uia_actionable", session.uiaActionableCount},{"control_channel", session.controlChannel}}},
            {"resume_rule","恢复时重新核对进程、窗口位置、联系人和消息发送状态；旧UIA ID、旧frame_id和旧坐标全部失效，不得直接重放未确认动作。"}
        };
        if (!Logger::atomicJson(path, object)) Logger::write("ERROR", "任务检查点写入失败：" + path);
    }

    bool compactContext(AgentSession& session, const QString&, bool force) {
        const int keep = force ? 1 : 2;
        while (session.recentHistory.size() > keep) session.recentHistory.removeFirst();
        session.contextSummary = buildWorldStateSummary(session);
        ++session.compactRound;
        persistContextSummary(session);
        log(QString("[压缩] WorldState摘要%1字符，保留近期%2步；能力判定与视觉目标直接复用。").arg(session.contextSummary.size()).arg(session.recentHistory.size()));
        return true;
    }

    static QString evidenceRules() {
        return "\n最高优先级执行规则：最新观察优先于历史。历史摘要和Verifier声称均不是当前界面证据。"
               "文字输入成功不等于发送成功；不得假定联系人已经打开、消息已发送或对方已回复。"
               "UIA ID只对最新扫描有效。鼠标点击必须给出有效target_id或明确坐标。"
               "连续无变化应wait，不要反复截图/推理。done必须有最新观察支持。\n";
    }

    static QString compactPlannerProtocol() {
        return "你是Windows单步控制Planner。只输出一个JSON对象，不输出Markdown或动作列表。"
            "仅操作用户目标授权的范围；WINDOW_CONTROL只允许指定窗口，不能控制别的软件。"
            "只能从C++提供的AVAILABLE_ACTIONS选择动作；不在AVAILABLE_ACTIONS里的动作绝对禁止输出。"
            "如果UIA已不可用，就禁止uia_scan/uia_focus/uia_invoke/uia_set_value和target_id，改用Vision语义目标、mouse和keyboard。"
            "禁止编造程序路径、UIA ID、联系人、聊天内容和完成状态。每次只做一个动作，先观察后操作，操作后验证。"
            "聊天/交流任务若INTERACTION_TURN_STATE=READY_TO_INITIATE，必须先生成一句自然口语并keyboard_type，不能直接wait或重新扫描。"
            "只有消息确认发送后、INTERACTION_TURN_STATE=WAITING_EXTERNAL，才等待外部变化。"
            "CONTINUOUS完成一轮后继续等待；ONE_SHOT只有证据表明目标完成才done。"
            "不可继续时done并terminal=true,blocked=true，说明原因；临时未找到不等于不可继续。"
            "通用字段：action,reason,target_id,terminal,blocked,done_message。"
            "动作参数以AVAILABLE_ACTIONS对应真实工具定义为准。keyboard_type优先使用text字段；wait可省略wait_ms，由C++补默认值。";
    }

    static QString compactVerifierProtocol() {
        return "你是动作验证器。仅根据最新UIA和本次工具结果验证，不能将摘要/动作理由当证据。"
            "工具执行成功不证明任务完成；输入不证明发送，发送不证明对方回复。"
            "没有证据则goal_success=false,need_reobserve=true。WINDOW_CONTROL只验证指定窗口。只输出JSON："
            "{\"success\":false,\"step_success\":false,\"goal_success\":false,"
            "\"terminal\":false,\"blocked\":false,\"confidence\":0.0,\"reason\":\"证据说明\","
            "\"need_reobserve\":true,\"need_retry\":false,\"user_message\":\"\"}";
    }

    int plannerInputBudget() const {
        const QJsonObject models = loadConfig("assets/config/models.json");
        const QString active = models.value("active_profile").toString("local_gemma");
        const QJsonObject role = models.value("profiles").toObject().value(active).toObject().value("llm").toObject();
        const QString provider = role.value("provider").toString("llama_cpp");
        const QJsonObject cfg = role.value(provider).toObject();
        const int context = qBound(2048, cfg.value("context").toInt(8192), RuntimeSafety::contextLimit("llm"));
        return qMax(512, qMin(5600, context - RuntimeSafety::outputLimit("llm") - 768));
    }

    QString compactRecentProgress(const AgentSession& session) const {
        if (session.recentProgress.isEmpty()) return "[]";
        return QString::fromUtf8(QJsonDocument(session.recentProgress).toJson(QJsonDocument::Compact));
    }

    void recordSuccessfulProgress(AgentSession& session, const QJsonObject& action, const QString& toolResult) {
        const QString name = action.value("action").toString();
        if (name=="vision_analyze" || name=="screen_capture" || name=="uia_scan" || name=="wait" || name=="wait_for_change" || name=="done") return;
        QString target = session.lastResolvedTargetLabel.trimmed();
        if (target.isEmpty() && action.value("target_id").toInt() > 0) target = "uia_id=" + QString::number(action.value("target_id").toInt());
        if (target.isEmpty() && name=="keyboard_type") target = "当前输入焦点";
        if (target.isEmpty() && name=="keyboard_hotkey") target = action.value("hotkey").toString();
        if (target.isEmpty() && action.value("norm_x").isDouble()) target = QString("norm=(%1,%2)").arg(action.value("norm_x").toDouble(),0,'f',3).arg(action.value("norm_y").toDouble(),0,'f',3);
        QJsonObject item{{"step", session.stepNumber},{"action", name},{"target", target.left(120)},{"result", toolResult.section('\n',0,0).left(160)}};
        if (name=="keyboard_type") item["text_excerpt"] = action.value("text").toString().left(100);
        session.recentProgress.append(item);
        while (session.recentProgress.size() > 5) session.recentProgress.removeFirst();
    }

    void updateInteractionEpoch(AgentSession& session) {
        const QString fingerprint=observationFingerprint(session);
        if(fingerprint.isEmpty())return;
        if(session.lastInteractionFingerprint.isEmpty()){session.lastInteractionFingerprint=fingerprint;return;}
        if(fingerprint!=session.lastInteractionFingerprint){
            ++session.interactionEpoch;
            session.lastInteractionFingerprint=fingerprint;
            session.lastSideEffectActionKey.clear();
            session.repeatedSideEffectBlocks=0;
            log(QString("[事务推进] 世界状态已变化，interaction_epoch=%1").arg(session.interactionEpoch));
        }
    }

    QString sideEffectActionKey(const AgentSession& session, const QJsonObject& action) const {
        const QString name = action.value("action").toString();
        if (name=="vision_analyze" || name=="screen_capture" || name=="uia_scan" || name=="wait" || name=="wait_for_change" || name=="done") return {};
        QString target = session.lastResolvedTargetLabel.trimmed();
        if (target.isEmpty() && action.value("target_id").toInt()>0) target = "id:" + QString::number(action.value("target_id").toInt());
        if (target.isEmpty() && action.value("norm_x").isDouble()) target = QString("%1,%2").arg(action.value("norm_x").toDouble(),0,'f',2).arg(action.value("norm_y").toDouble(),0,'f',2);
        QString extra;
        if (name=="keyboard_type") extra = "|" + action.value("text").toString().left(160);
        else if (name=="keyboard_hotkey") extra = "|" + action.value("hotkey").toString();
        return QString::number(session.interactionEpoch) + "|" + name + "|" + target + extra;
    }

    QJsonObject compactVisionForPlanner(const QJsonObject& vision, int textLimit = 10) const {
        if (vision.isEmpty()) return {};
        QJsonObject out; out["frame_id"] = vision.value("frame_id"); out["screen_state"] = vision.value("screen_state").toString().left(500);
        out["task_context"] = vision.value("task_context").toObject(); out["valid"] = vision.value("valid");
        QJsonArray targets;
        for (const QJsonValue& value : vision.value("targets").toArray()) {
            const QJsonObject t = value.toObject();
            targets.append(QJsonObject{{"label", t.value("label").toString().left(100)},{"description", t.value("description").toString().left(180)},{"confidence", t.value("confidence")},{"norm_x", t.value("norm_x")},{"norm_y", t.value("norm_y")},{"norm_rect", t.value("norm_rect")}});
            if (targets.size() >= 12) break;
        }
        out["targets"] = targets;
        QJsonArray texts;
        for (const QJsonValue& value : vision.value("visible_text").toArray()) { texts.append(value.toString().left(120)); if (texts.size() >= textLimit) break; }
        out["visible_text"] = texts;
        return out;
    }

    QJsonArray buildLightweightPlannerMessages(AgentSession& session, const QString& goal) {
        const QString mode = session.scope == AgentScope::SelectedWindow ? "WINDOW_CONTROL" : "DESKTOP_CONTROL";
        QJsonArray messages;
        messages.append(QJsonObject{{"role","system"},{"content", QString("CURRENT_MODE=%1\\nTASK_MODE=%2\\n").arg(mode, session.taskMode) + evidenceRules() + compactPlannerProtocol() + "\\n轻量进度规则：RECENT_PROGRESS只表示最近真实成功动作；不要无意义重复最后一个成功副作用动作。Planner明确指定target时，只能操作语义兼容目标；找不到就重新观察，不能拿别的控件代替。"}});
        QString content;
        content += "TASK_ID=" + session.taskId + "\\n";
        content += "USER_GOAL=" + goal.left(800) + "\\n";
        content += "WORKFLOW=" + session.request.kindName() + "\\n";
        content += "POLICY_PHASE=" + agentPolicyPhaseName(session) + "\n";
        content += QString("CONTINUOUS_TASK=%1\n").arg(session.request.continuous ? "true" : "false");
        content += QString("WAIT_FOR_EXTERNAL_CHANGE_ALLOWED=%1\n").arg(session.request.continuous ? "true" : "false");
        content += QString("UIA_USABLE=%1\n").arg(session.uiaChecked ? (session.uiaUsable ? "true" : "false") : "unknown");
        content += QString("TARGET_ID_ALLOWED=%1\n").arg(session.uiaChecked && !session.uiaUsable ? "false" : "true");
        content += "INTERACTION_TURN_STATE=" + interactionTurnStateName(session) + "\n";
        content += "CONTROL_CHANNEL=" + session.controlChannel + "\\n";
        content += QString("UIA_USABLE=%1\\n").arg(session.uiaChecked ? (session.uiaUsable ? "true" : "false") : "unknown");
        content += QString("TARGET_ID_ALLOWED=%1\\n").arg(session.uiaChecked && !session.uiaUsable ? "false" : "true");
        content += "INTERACTION_TURN_STATE=" + interactionTurnStateName(session) + "\\n";
        content += "RECENT_PROGRESS=" + compactRecentProgress(session) + "\\n";
        if (!session.nextObjective.trimmed().isEmpty()) content += "NEXT_OBJECTIVE=" + session.nextObjective.left(300) + "\\n";
        if (session.pendingCommitRequired) { content += "PENDING_EFFECT=TEXT_TYPED_NOT_COMMITTED\\n" "PENDING_COMMIT_TARGET=" + session.pendingCommitLabel.left(100) + "\\n"; }
        content += "AVAILABLE_ACTIONS=" + availableActions(session).join(',') + "\\n";
        if (session.runtimeProfileReady) {
            QJsonObject p{{"surface_type", session.runtimeProfile.surfaceType},{"task_profile", session.runtimeProfile.taskProfile},{"preferred_observation", session.runtimeProfile.preferredObservation}};
            QJsonArray likely; for (const QString& v : session.runtimeProfile.likelyTargets) likely.append(v);
            p["likely_targets"] = likely;
            content += "APP_PROFILE=" + QString::fromUtf8(QJsonDocument(p).toJson(QJsonDocument::Compact)) + "\\n";
        }
        if (!session.latestVision.isEmpty()) content += "LATEST_VISION=" + QString::fromUtf8(QJsonDocument(compactVisionForPlanner(session.latestVision, 8)).toJson(QJsonDocument::Compact)) + "\n";
        if (!session.latestTaskContextSummary.trimmed().isEmpty()) content += "LATEST_TASK_CONTEXT_SUMMARY=" + session.latestTaskContextSummary.left(400) + "\n";
        if (!session.latestRelevantText.trimmed().isEmpty()) content += "LATEST_RELEVANT_TEXT=" + session.latestRelevantText.left(700) + "\n";
        if (session.uiaUsable && !session.uiaItems.isEmpty()) {
            QString uia = "UIA="; int count = 0;
            for (const UIAItem& item : session.uiaItems) {
                if (!item.clickable && item.name.trimmed().isEmpty()) continue;
                uia += QString("{id:%1,type:%2,name:%3};").arg(item.id).arg(item.type.left(40)).arg(item.name.left(80));
                if (++count >= 8) break;
            }
            content += uia + "\\n";
        }
        messages.append(QJsonObject{{"role","user"},{"content",content}});
        return messages;
    }

    QJsonArray buildPlannerMessages(AgentSession& session, const QString& goal, const QString& systemPrompt, const QString& plannerPrompt) {
        Q_UNUSED(systemPrompt);
        const QString mode = session.scope == AgentScope::SelectedWindow ? "WINDOW_CONTROL" : "DESKTOP_CONTROL";
        QJsonArray messages;
        messages.append(QJsonObject{{"role", "system"},{"content", QString("CURRENT_MODE=%1\nTASK_MODE=%2\n").arg(mode, session.taskMode) + evidenceRules() + (plannerPrompt.trimmed().isEmpty()?compactPlannerProtocol():plannerPrompt)}});
        QString content = "TASK_ID=" + session.taskId + "\nUSER_GOAL:\n" + goal + "\n";
        if (!session.contextSummary.isEmpty()) content += "HISTORICAL_ACTIONS_NOT_CURRENT_EVIDENCE:\n" + session.contextSummary + "\n";
        if (!session.recentHistory.isEmpty()) content += "RECENT_HISTORY:\n" + QString::fromUtf8(QJsonDocument(session.recentHistory).toJson(QJsonDocument::Compact)) + "\n";
        content += "RECENT_PROGRESS:\n" + compactRecentProgress(session) + "\n";
        content += "PROGRESS_RULE=不要无意义重复最后一个已经成功的副作用动作；明确target必须语义匹配，不能用唯一但不相关的目标替代。\n";
        content += "TASK_REQUEST:\n"+QString::fromUtf8(QJsonDocument(session.request.json()).toJson(QJsonDocument::Compact))+"\n";
        if (session.runtimeProfileReady) { content += "RUNTIME_APP_PROFILE:\n" + QString::fromUtf8(QJsonDocument(session.runtimeProfile.json()).toJson(QJsonDocument::Compact)) + "\n"; content += "PREFERRED_ACTION_FAMILIES=" + session.runtimeProfile.preferredActionFamilies.join(',') + "\n"; }
        content += "OBSERVATION_POLICY=" + session.observationPolicy + "\n";
        content += QString("GENERIC_POLICY_PHASE=%1\n").arg(agentPolicyPhaseName(session));
        content += QString("CONTINUOUS_TASK=%1\n").arg(session.request.continuous ? "true" : "false");
        content += QString("WAIT_FOR_EXTERNAL_CHANGE_ALLOWED=%1\n").arg(session.request.continuous ? "true" : "false");
        if (session.pendingCommitRequired) { content += "PENDING_EFFECT=TEXT_TYPED_NOT_COMMITTED\n"; content += "PENDING_COMMIT_TARGET=" + session.pendingCommitLabel + "\n"; content += "PENDING_EFFECT_RULE=必须先发送/提交当前文本，不能wait、done或重复观察。\n"; }
        content += "AVAILABLE_ACTIONS:\n"+availableActions(session).join(',')+"\n";
        if(session.mcp&&session.mcp->isReady())content+="MCP_TOOL_NAMES="+session.mcp->toolNames().join(',')+"\nCONNECTED_MCP_TOOLS:\n"+QString::fromUtf8(QJsonDocument(session.mcp->descriptors()).toJson(QJsonDocument::Compact))+"\n";
        if(!session.latestMcp.isEmpty())content+="LATEST_MCP_RESULT:\n"+QString::fromUtf8(QJsonDocument(session.latestMcp).toJson(QJsonDocument::Compact))+"\n";
        if(!session.latestVision.isEmpty()) content+="LATEST_VISION_OBSERVATION:\n"+QString::fromUtf8(QJsonDocument(session.latestVision).toJson(QJsonDocument::Compact))+"\n";
        if(!session.latestTaskContextSummary.trimmed().isEmpty()) content+="LATEST_TASK_CONTEXT_SUMMARY="+session.latestTaskContextSummary.left(500)+"\n";
        if(!session.latestRelevantText.trimmed().isEmpty()) content+="LATEST_RELEVANT_TEXT="+session.latestRelevantText.left(1000)+"\n";
        content += "LATEST_STATE:\n" + environmentSummary(session);
        messages.append(QJsonObject{{"role", "user"},{"content", content}});
        return messages;
    }

    void appendRecentHistory(AgentSession& session, const QJsonObject& action, bool toolSuccess, const QString& toolResult, const QJsonObject& verification) {
        Logger::write("TOOL_FULL", QString("task=%1 step=%2\n%3\n%4").arg(session.taskId).arg(session.stepNumber).arg(QString::fromUtf8(QJsonDocument(action).toJson(QJsonDocument::Compact)), toolResult));
        QJsonObject shortAction;
        for (const QString& key : {QString("action"), QString("target_id"), QString("hotkey"), QString("x"), QString("y"), QString("norm_x"), QString("norm_y")}) if (action.contains(key)) shortAction[key] = action.value(key);
        shortAction["value_excerpt"] = action.value("text").toString(action.value("value").toString()).left(160);
        QJsonObject shortVerification{{"step_success", verification.value("step_success").toBool(false)},{"goal_success", verification.value("goal_success").toBool(false)},{"reason", verification.value("reason").toString().left(140)}};
        const int resultLimit=300;
        const QString historyResult=action.value("action").toString()=="vision_analyze"?"视觉观察已校验；目标与坐标在LATEST_VISION_OBSERVATION中。":toolResult.section('\n',0,0);
        session.recentHistory.append(QJsonObject{{"action", shortAction},{"tool_success", toolSuccess},{"result", historyResult.left(resultLimit)},{"verification", shortVerification}});
        session.pendingAction = {};
        compactContext(session, session.goal, false);
        persistContextSummary(session);
    }

    void maybeCompactBeforePlanner(AgentSession& session, const QString& goal, const QString&, const QString&) { compactContext(session, goal, false); }

    QString cleanGeneratedInputText(QString text) const {
        text = text.trimmed();
        if (text.startsWith("```")) {
            const int firstNl = text.indexOf('\n');
            if (firstNl >= 0) text = text.mid(firstNl + 1);
            const int lastFence = text.lastIndexOf("```");
            if (lastFence >= 0) text = text.left(lastFence);
            text = text.trimmed();
        }
        if (text.startsWith('{') && text.endsWith('}')) {
            QJsonParseError e{};
            const QJsonDocument d = QJsonDocument::fromJson(text.toUtf8(), &e);
            if (e.error == QJsonParseError::NoError && d.isObject()) {
                const QJsonObject o = d.object();
                for (const QString& key : {QString("text"), QString("message"), QString("reply"), QString("content")}) {
                    const QString v = o.value(key).toString().trimmed();
                    if (!v.isEmpty()) { text = v; break; }
                }
            }
        }
        if (text.size() >= 2) {
            const QChar a = text.front(); const QChar b = text.back();
            if ((a == '"' && b == '"') || (a == '\'' && b == '\'') || (a == QChar(0x201C) && b == QChar(0x201D)) || (a == QChar(0x300C) && b == QChar(0x300D))) text = text.mid(1, text.size() - 2).trimmed();
        }
        if (text.size() > 500) text = text.left(500).trimmed();
        return text;
    }

    QString generateNaturalKeyboardText(AgentSession& session, const QJsonObject& plannerAction, QString* error) {
        if (!conversationLikeSurface(session)) { if (error) *error = "keyboard_type缺少text，且当前不是交流类界面，不能猜测输入内容。"; return {}; }
        const QString turnState = interactionTurnStateName(session);
        QString latest = session.latestRelevantText.trimmed();
        const QString taskSummary = session.latestTaskContextSummary.trimmed();
        QString instruction;
        if (turnState == "READY_TO_RESPOND" && !latest.isEmpty()) instruction = "根据当前最新外部内容自然回复一句。";
        else instruction = "当前是第一次主动开始或继续交流，生成一句自然、轻松、简短的口语开场。";
        const QString composerPrompt = "你负责给电脑助手生成即将输入到交流软件中的一句话。\\n"
            "只输出最终要输入的文字，不要JSON，不要解释，不要引号，不要Markdown。\\n"
            "语气自然、口语化、简短，不要机械，不要提到自己是AI。\\n"
            "不要强迫、威胁、施压对方。\\n"
            "USER_GOAL=" + session.goal.left(300) + "\\n"
            "TURN_STATE=" + turnState + "\\n"
            "TASK_CONTEXT=" + taskSummary.left(500) + "\\n"
            "LATEST_RELEVANT_TEXT=" + latest.left(800) + "\\n"
            "PLANNER_REASON=" + plannerAction.value("reason").toString().left(500) + "\\n"
            "要求：" + instruction;
        QJsonArray messages{QJsonObject{{"role","system"},{"content","你是电脑助手的自然语言输入生成器。只生成一条可直接输入的自然口语。"}},QJsonObject{{"role","user"},{"content",composerPrompt}}};
        QString modelError;
        const QString raw = m_modelExecutor.call([this, messages, &modelError, &session] {
            if (m_stop || RuntimeSafety::instance().shouldAbort(session.cancellationGeneration)) { modelError = "任务已取消"; return QString(); }
            return m_models ? m_models->chat("llm", messages, &modelError) : QString();
        });
        if (!modelError.isEmpty()) { if (error) *error = "自然口语生成失败：" + modelError; return {}; }
        const QString text = cleanGeneratedInputText(raw);
        if (text.isEmpty()) { if (error) *error = "自然口语生成结果为空"; return {}; }
        Logger::write("TEXT_COMPOSER", QString("task=%1 step=%2 turn=%3 chars=%4 text=%5").arg(session.taskId).arg(session.stepNumber).arg(turnState).arg(text.size()).arg(text.left(300)));
        if (error) error->clear();
        return text;
    }

    bool completeKeyboardTypeTextIfNeeded(AgentSession& session, QJsonObject& action, QString* error) {
        const QString name = action.value("action").toString().trimmed().toLower();
        if (name != "keyboard_type") { if (error) error->clear(); return true; }
        action.remove("target_id");
        QString text = action.value("text").toString().trimmed();
        if (text.isEmpty()) {
            for (const QString& key : {QString("value"), QString("content"), QString("message"), QString("reply")}) {
                const QString v = action.value(key).toString().trimmed();
                if (!v.isEmpty()) { text = v; action["text"] = v; Logger::write("TEXT_COMPOSER", "复用Planner字段 " + key + " 作为keyboard_type.text"); break; }
            }
        }
        if (text.isEmpty()) {
            QString composerError;
            text = generateNaturalKeyboardText(session, action, &composerError);
            if (text.isEmpty()) { if (error) *error = composerError; return false; }
            action["text"] = text;
            log("[文本生成] 已自动补全keyboard_type.text：" + text.left(120));
        }
        if (error) error->clear();
        return true;
    }

    QString callPlannerWithRecovery(AgentSession& session, const QString& goal, const QString& systemPrompt, const QString& plannerPrompt, QString* finalError) {
        for (int retry = 0; retry < 2; ++retry) {
            if (m_stop || m_userInterrupted || RuntimeSafety::instance().shouldAbort(session.cancellationGeneration)) { if (finalError) *finalError = "任务已取消或资源保护中"; return {}; }
            maybeCompactBeforePlanner(session, goal, systemPrompt, plannerPrompt);
            QJsonArray messages = buildPlannerMessages(session, goal, systemPrompt, plannerPrompt);
            const int budget = plannerInputBudget();
            while (estimateMessagesTokens(messages) > budget && session.uiaPromptLimit > 8) { session.uiaPromptLimit = qMax(8, session.uiaPromptLimit / 2); messages = buildPlannerMessages(session, goal, systemPrompt, plannerPrompt); }
            if (estimateMessagesTokens(messages) > budget) { compactContext(session, goal, true); messages = buildPlannerMessages(session, goal, systemPrompt, plannerPrompt); }
            int estimate = estimateMessagesTokens(messages);
            if (estimate > budget) { messages = buildLightweightPlannerMessages(session, goal); estimate = estimateMessagesTokens(messages); log(QString("[上下文恢复] 完整Planner输入超预算，已切换轻量上下文继续执行：%1/%2 tokens").arg(estimate).arg(budget)); }
            if (estimate > budget) {
                QJsonObject savedVision = session.latestVision;
                if (!session.latestVision.isEmpty()) { QJsonObject compact = compactVisionForPlanner(session.latestVision, 3); session.latestVision = compact; }
                messages = buildLightweightPlannerMessages(session, goal);
                estimate = estimateMessagesTokens(messages);
                session.latestVision = savedVision;
                log(QString("[上下文恢复] 二次压缩Planner输入：%1/%2 tokens").arg(estimate).arg(budget));
            }
            log(QString("[思考] task=%1 输入估计=%2 tokens 预算=%3；%4").arg(session.taskId).arg(estimate).arg(budget).arg(RuntimeSafety::instance().description()));
            if (estimate > budget) { if (finalError) *finalError = "轻量Planner上下文仍超过模型安全预算，已保留检查点；未执行任何猜测动作。"; return {}; }
            QString error; QElapsedTimer timer; timer.start();
            const QString reply = m_modelExecutor.call([this, &session, messages, &error] {
                if (m_stop || RuntimeSafety::instance().shouldAbort(session.cancellationGeneration)) { error = "任务已取消"; return QString(); }
                const QString output = m_models ? m_models->chat("llm", messages, &error) : QString();
                if (RuntimeSafety::instance().blocked() && m_models) m_models->stopAll();
                return output;
            });
            Logger::write("INFERENCE", QString("task=%1 step=%2 role=planner elapsed=%3ms output_chars=%4 error=%5").arg(session.taskId).arg(session.stepNumber).arg(timer.elapsed()).arg(reply.size()).arg(error));
            if (error.isEmpty()) { if (finalError) finalError->clear(); return reply; }
            if (retry == 0 && isContextOverflow(error)) { compactContext(session, goal, true); session.uiaPromptLimit = 8; continue; }
            if (finalError) *finalError = error;
            return {};
        }
        if (finalError) *finalError = "上下文恢复次数达到上限";
        return {};
    }

    bool activateWindowHandle(HWND hwnd) const {
        if (!validWindowHandle(hwnd)) return false;
        if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
        SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        return SetForegroundWindow(hwnd) != FALSE;
    }

    HWND windowForSessionItem(const AgentSession& session, int id) const {
        const UIAItem* item = findSessionItem(session, id); if (!item) return nullptr;
        const QPoint center = item->rect.center(); POINT point{center.x(), center.y()};
        HWND hwnd = WindowFromPoint(point); if (hwnd) hwnd = GetAncestor(hwnd, GA_ROOT);
        return hwnd;
    }

    QString captureForAgent(AgentSession& session, const QString& requestedScope, bool* success) {
        *success = false;
        CaptureResult capture;
        hideOverlayForCapture(session.scope);
        if (session.scope == AgentScope::SelectedWindow) {
            if (!validWindowHandle(session.targetHwnd)) { restoreOverlayAfterCapture(session.scope); return "截图失败：指定窗口已经关闭。"; }
            if(!inApplicationScope(GetForegroundWindow(),session.targetHwnd))activateWindowHandle(session.targetHwnd);
            HWND captureHwnd=session.targetHwnd;
            if(inApplicationScope(GetForegroundWindow(),session.targetHwnd))captureHwnd=GetForegroundWindow();
            capture = captureWindow(captureHwnd);
        } else if (requestedScope == "virtual_desktop") capture = captureVirtualDesktop();
        else capture = captureCursorMonitor();
        restoreOverlayAfterCapture(session.scope);
        if (!capture.valid || capture.image.isNull()) return "截图失败：没有获得有效图像。";
        session.lastScreenshotRect = capture.rect; session.lastCaptureSize = capture.image.size();
        session.frameId=QUuid::createUuid().toString(QUuid::Id128); session.latestVision={};
        session.lastScreenshot = saveAgentAuditScreenshot(capture.image, session.taskId, session.stepNumber, session.scope == AgentScope::SelectedWindow ? "window_observe" : "desktop_observe");
        *success = !session.lastScreenshot.isEmpty();
        if (!*success) return "截图保存失败。";
        return QString("截图成功：%1\nIMAGE_WIDTH=%2\nIMAGE_HEIGHT=%3\nSCREEN_RECT=(%4,%5,%6,%7)").arg(session.lastScreenshot).arg(capture.image.width()).arg(capture.image.height()).arg(capture.rect.x()).arg(capture.rect.y()).arg(capture.rect.width()).arg(capture.rect.height());
    }
    QString executeActionWorker(AgentSession& session,QJsonObject action,bool* success) {
        *success=false;
        if(m_stop||m_userInterrupted||RuntimeSafety::instance().shouldAbort(session.cancellationGeneration)){session.lastResult={ToolOutcome::UserCancelled,"任务已取消，未执行动作",{}};return session.lastResult.message;}
        QString error;action=normalizeAction(action,&error);
        if(action.isEmpty()){session.lastResult={ToolOutcome::RetryableFailure,error,{}};return error;}
        const QString name=action.value("action").toString();
        if(!authorizeAction(session,action,&error)){session.lastResult={ToolOutcome::PermissionDenied,error,{}};return error;}
        if(name=="app_launch"){
            const QString app=action.value("program").toString(action.value("application").toString());
            if(!loadConfig("assets/config/permissions.json").value("system").toObject().value("explicit_task_launch_is_authorized").toBool(true)&&!confirmOnUi("应用启动确认",app)){
                session.cancelledByUser=true;session.lastResult={ToolOutcome::UserCancelled,"用户取消启动，任务已停止。",{}};return session.lastResult.message;
            }
            auto launched=ApplicationLifecycleManager::open(app,{},[this,&session]{return m_stop||m_userInterrupted||RuntimeSafety::instance().shouldAbort(session.cancellationGeneration);});
            session.lastResult=launched.result;*success=launched.result.success();
            if(*success){bindApplication(session,launched);session.workflow.phase="observe";scanAgentUia(session,nullptr);}
            return launched.result.message;
        }
        if(name=="mcp_describe"){
            const auto definition=session.mcp?session.mcp->describe(action.value("tool").toString()):QJsonObject{};
            *success=!definition.isEmpty();session.latestMcp=definition;session.lastResult={*success?ToolOutcome::Succeeded:ToolOutcome::RetryableFailure,QString::fromUtf8(QJsonDocument(definition).toJson(QJsonDocument::Compact)),definition};return session.lastResult.message;
        }
        if(name=="mcp_call"){
            session.lastResult=session.mcp?session.mcp->call(action.value("tool").toString(),action.value("arguments").toObject()):ToolResult{ToolOutcome::NeedObservation,"当前没有已连接MCP服务",{}};
            *success=session.lastResult.success();session.latestMcp=boundedJson(session.lastResult.data).toObject();if(session.lastResult.data.value("uncertain").toBool(false))session.uncertainSideEffect=true;return session.lastResult.message;
        }
        if(name=="desktop_show"){
            *success=sendAiHotkey("WIN+D");session.latestVision={};session.lastResult={*success?ToolOutcome::Succeeded:ToolOutcome::RetryableFailure,*success?"已显示桌面，请观察目标快捷方式":"显示桌面失败",{}};return session.lastResult.message;
        }
        if(name=="wait_for_change"){
            const QString message=waitForChange(session);*success=!m_stop&&!m_userInterrupted&&!RuntimeSafety::instance().shouldAbort(session.cancellationGeneration);
            session.lastResult={*success?ToolOutcome::Waiting:ToolOutcome::UserCancelled,message,{}};return message;
        }
        if(name=="uia_select"||name=="uia_toggle"||name=="uia_scroll"){
            const int id=action.value("target_id").toInt();QString detail;
            *success=m_uiaExecutor.call([this,id,action,name,&detail]{return !m_stop&&m_agentUia&&m_agentUia->operate(id,name,action,&detail);});
            session.lastResult={*success?ToolOutcome::Succeeded:ToolOutcome::NeedObservation,*success?"UIA操作已执行":detail,{}};return session.lastResult.message;
        }
        if(session.scope==AgentScope::SelectedWindow&&(name.startsWith("mouse_")||name.startsWith("keyboard_"))){
            if(!inApplicationScope(GetForegroundWindow(),session.targetHwnd))activateWindowHandle(session.targetHwnd);
            if(!inApplicationScope(GetForegroundWindow(),session.targetHwnd)){session.lastResult={ToolOutcome::NeedObservation,"目标应用未获得前台，未发送任何鼠标键盘输入",{}};return session.lastResult.message;}
        }
        const QString message=executeActionImplementation(session,action,success);
        if(name!="done")session.lastResult={*success?ToolOutcome::Succeeded:ToolOutcome::RetryableFailure,message,{}};
        if(name=="done")session.lastResult.outcome=ToolOutcome::Succeeded;
        if(name=="wait")session.lastResult.outcome=*success?ToolOutcome::Waiting:ToolOutcome::UserCancelled;
        if(session.cancelledByUser)session.lastResult.outcome=ToolOutcome::UserCancelled;
        if(name=="vision_analyze"&&!*success)session.lastResult.outcome=ToolOutcome::NeedObservation;
        return message;
    }

    QString executeActionImplementation(AgentSession& session, QJsonObject action, bool* success) {
        *success = false;
        if (m_stop || m_userInterrupted || RuntimeSafety::instance().shouldAbort(session.cancellationGeneration)) return "任务已取消，拒绝执行动作。";
        QString normalizeError; action = normalizeAction(action, &normalizeError);
        if (action.isEmpty()) return "动作校验失败：" + normalizeError;
        const QString name = action.value("action").toString();
        if((name.startsWith("mouse_")&&action.contains("norm_x"))&&session.scope==AgentScope::SelectedWindow&&windowRectOf(session.targetHwnd)!=session.lastScreenshotRect&&(!inApplicationScope(GetForegroundWindow(),session.targetHwnd)||windowRectOf(GetForegroundWindow())!=session.lastScreenshotRect)){session.latestVision={};return "截图后窗口位置/大小已变化，必须重新观察，未执行坐标操作。";}

        if (name == "uia_scan") { QString error; if (!scanAgentUia(session, &error)) return "UIA扫描失败：" + error; *success = true; return environmentSummary(session); }
        if (name == "uia_invoke") {
            const int id = action.value("target_id").toInt();
            if (!findSessionItem(session, id)) return "UIA调用失败：target_id不属于当前最新扫描结果。";
            QString error;
            const bool ok = m_uiaExecutor.call([this, id, &error] { return !m_stop && !m_userInterrupted && !RuntimeSafety::instance().shouldAbort(RuntimeSafety::instance().generation()) && m_agentUia && m_agentUia->invoke(id, &error); });
            *success = ok; if (!ok) return "UIA调用失败：" + error;
            QThread::msleep(180); scanAgentUia(session, nullptr);
            return "UIA调用成功。\n" + environmentSummary(session);
        }
        if (name == "uia_set_value") {
            const int id = action.value("target_id").toInt();
            if (!findSessionItem(session, id)) return "文本输入失败：target_id不属于当前最新扫描结果。";
            const QString value = action.value("value").toString(); QString error;
            const bool ok = m_uiaExecutor.call([this, id, value, &error] { return !m_stop && !m_userInterrupted && !RuntimeSafety::instance().shouldAbort(RuntimeSafety::instance().generation()) && m_agentUia && m_agentUia->setValue(id, value, &error); });
            *success = ok; if (!ok) return "文本输入失败：" + error;
            QThread::msleep(120); scanAgentUia(session, nullptr);
            return "文本输入成功。\n" + environmentSummary(session);
        }
        if (name == "uia_focus") {
            const int id = action.value("target_id").toInt();
            if (!findSessionItem(session, id)) return "UIA聚焦失败：target_id不存在。";
            QString error;
            const bool ok = m_uiaExecutor.call([this, id, &error] { return !m_stop && !m_userInterrupted && !RuntimeSafety::instance().shouldAbort(RuntimeSafety::instance().generation()) && m_agentUia && m_agentUia->focus(id, &error); });
            *success = ok; return ok ? "UIA控件已获得焦点。" : "UIA聚焦失败：" + error;
        }
        if (name == "window_activate") {
            HWND hwnd = nullptr;
            if (session.scope == AgentScope::SelectedWindow) hwnd = session.targetHwnd;
            else {
                const int id = action.value("target_id").toInt();
                if (id > 0) hwnd = windowForSessionItem(session, id);
                if (!hwnd) { const qulonglong raw = action.value("hwnd").toVariant().toULongLong(); hwnd = reinterpret_cast<HWND>(static_cast<quintptr>(raw)); }
            }
            const bool ok = activateWindowHandle(hwnd); *success = ok;
            if (ok) { QThread::msleep(180); scanAgentUia(session, nullptr); }
            return ok ? "窗口激活成功。" : "窗口激活失败。";
        }
        if (name == "mouse_move") {
            const QPoint point = resolveActionPoint(session, action);
            if (!pointAllowed(session, point)) return "鼠标移动被拒绝：坐标超出指定窗口范围。";
            const bool ok = sendAiMouseMove(point.x(), point.y()); *success = ok;
            return ok ? QString("鼠标已移动到(%1,%2)。").arg(point.x()).arg(point.y()) : "鼠标移动失败。";
        }
        if (name == "mouse_click") {
            QPoint point;
            const int id = action.value("target_id").toInt();
            if (id > 0) { const UIAItem* item = findSessionItem(session, id); if (!item) return "点击失败：target_id已失效，需要重新扫描。"; point = item->rect.center(); }
            else {
                if (!((action.contains("x") && action.contains("y")) || (action.contains("norm_x") && action.contains("norm_y")))) return "点击失败：必须提供有效target_id或明确坐标，拒绝默认点击(0,0)。";
                point = resolveActionPoint(session, action);
            }
            if (!pointAllowed(session, point)) return "鼠标点击被拒绝：坐标超出指定窗口范围。";
            if (session.scope == AgentScope::SelectedWindow && !inApplicationScope(GetForegroundWindow(),session.targetHwnd)) activateWindowHandle(session.targetHwnd);
            const QString button = action.value("button").toString("left");
            if (session.pendingCommitRequired && session.pendingCommitDispatched) {
                session.pendingCommitLastClickPoint = point;
                session.pendingCommitLastNormX = action.value("norm_x").toDouble(-1.0);
                session.pendingCommitLastNormY = action.value("norm_y").toDouble(-1.0);
                Logger::write("COMMIT_CLICK", QString("attempt=%1 point=(%2,%3) norm=(%4,%5)").arg(session.pendingCommitClickAttempts + 1).arg(point.x()).arg(point.y()).arg(session.pendingCommitLastNormX, 0, 'f', 4).arg(session.pendingCommitLastNormY, 0, 'f', 4));
            }
            bool ok = sendAiMouseClick(point.x(), point.y(), button);
            const int clicks=action.value("click_count").toInt(1);
            if(clicks==2&&ok&&!m_stop&&!m_userInterrupted){QThread::msleep(qBound(30,int(GetDoubleClickTime()/4),100));ok=sendAiMouseClick(point.x(),point.y(),button);}
            *success = ok;
            if (ok) { QThread::msleep(180); scanAgentUia(session, nullptr); }
            return ok ? QString("鼠标点击成功：(%1,%2) %3。").arg(point.x()).arg(point.y()).arg(button) : "鼠标点击失败。";
        }
        if (name == "mouse_drag") {
            QPoint from(action.value("x1").toInt(), action.value("y1").toInt());
            QPoint to(action.value("x2").toInt(), action.value("y2").toInt());
            if (action.contains("norm_x1") && action.contains("norm_y1") && action.contains("norm_x2") && action.contains("norm_y2")) {
                QJsonObject first{{"norm_x", action.value("norm_x1")},{"norm_y", action.value("norm_y1")}};
                QJsonObject second{{"norm_x", action.value("norm_x2")},{"norm_y", action.value("norm_y2")}};
                from = resolveActionPoint(session, first); to = resolveActionPoint(session, second);
            }
            if (!pointAllowed(session, from) || !pointAllowed(session, to)) return "鼠标拖动被拒绝：起点或终点超出指定窗口范围。";
            if (session.scope == AgentScope::SelectedWindow && !inApplicationScope(GetForegroundWindow(),session.targetHwnd)) activateWindowHandle(session.targetHwnd);
            const int durationMs = qBound(50, action.value("duration_ms").toInt(300), 10000);
            const bool ok = sendAiMouseDrag(from.x(), from.y(), to.x(), to.y(), durationMs);
            *success = ok;
            if (ok) { QThread::msleep(180); scanAgentUia(session, nullptr); }
            return ok ? "鼠标拖动成功。" : "鼠标拖动失败。";
        }
        if (name == "mouse_path") {
            const QJsonArray points = action.value("points").toArray();
            if (points.size() < 2) return "mouse_path至少需要2个轨迹点。";
            QRect base = session.lastScreenshotRect;
            if (base.isEmpty()) base = session.scope == AgentScope::SelectedWindow ? session.targetRect : virtualDesktopRect();
            QVector<QPoint> path; path.reserve(points.size());
            for (const QJsonValue& value : points) {
                const QJsonArray pair = value.toArray(); if (pair.size() < 2) continue;
                const double nx = qBound(0.0, pair.at(0).toDouble(), 1.0); const double ny = qBound(0.0, pair.at(1).toDouble(), 1.0);
                const QPoint point(base.left() + qRound(nx * qMax(0, base.width() - 1)), base.top() + qRound(ny * qMax(0, base.height() - 1)));
                if (!pointAllowed(session, point)) return "mouse_path被拒绝：轨迹点超出指定窗口范围。";
                path.push_back(point);
            }
            if (path.size() < 2) return "mouse_path没有足够有效轨迹点。";
            if (session.scope == AgentScope::SelectedWindow && !inApplicationScope(GetForegroundWindow(),session.targetHwnd)) activateWindowHandle(session.targetHwnd);
            const int durationMs = qBound(100, action.value("duration_ms").toInt(900), 15000);
            if (!sendAiMouseMove(path.first().x(), path.first().y())) return "mouse_path起点移动失败。";
            INPUT down{}; down.type = INPUT_MOUSE; down.mi.dwFlags = MOUSEEVENTF_LEFTDOWN; down.mi.dwExtraInfo = CEH_AI_INPUT_MAGIC;
            if (SendInput(1, &down, sizeof(INPUT)) != 1) return "mouse_path按下鼠标失败。";
            const int segmentMs = qMax(1, durationMs / qMax(1, path.size() - 1));
            for (int i = 1; i < path.size(); ++i) {
                if (m_stop || m_userInterrupted) break;
                const QPoint a = path.at(i - 1); const QPoint b = path.at(i);
                const int subSteps = qMax(2, segmentMs / 12);
                for (int s = 1; s <= subSteps; ++s) {
                    if (m_stop || m_userInterrupted) break;
                    const double t = static_cast<double>(s) / static_cast<double>(subSteps);
                    sendAiMouseMove(qRound(a.x() + (b.x() - a.x()) * t), qRound(a.y() + (b.y() - a.y()) * t));
                    QThread::msleep(qMax(1, segmentMs / subSteps));
                }
            }
            INPUT up{}; up.type = INPUT_MOUSE; up.mi.dwFlags = MOUSEEVENTF_LEFTUP; up.mi.dwExtraInfo = CEH_AI_INPUT_MAGIC;
            const bool ok = SendInput(1, &up, sizeof(INPUT)) == 1;
            *success = ok && !m_stop && !m_userInterrupted;
            return *success ? "连续鼠标轨迹绘制完成。" : "mouse_path被中断或执行失败。";
        }
        if (name == "keyboard_type") {
            if (session.scope == AgentScope::SelectedWindow && !inApplicationScope(GetForegroundWindow(), session.targetHwnd)) activateWindowHandle(session.targetHwnd);
            if (conversationLikeSurface(session) && !session.latestVision.isEmpty()) {
                QJsonObject inputTarget = bestVisualTargetForQuery(session, "输入框", 0.55);
                if (inputTarget.isEmpty()) inputTarget = bestVisualTargetForQuery(session, "消息输入", 0.55);
                if (!inputTarget.isEmpty()) {
                    const double nx = inputTarget.value("norm_x").toDouble(-1.0); const double ny = inputTarget.value("norm_y").toDouble(-1.0);
                    if (nx >= 0.0 && nx <= 1.0 && ny >= 0.0 && ny <= 1.0) {
                        QRect base = session.lastScreenshotRect;
                        if (base.isEmpty()) base = session.scope == AgentScope::SelectedWindow ? session.targetRect : virtualDesktopRect();
                        const QPoint p(base.left() + qRound(nx * qMax(0, base.width() - 1)), base.top() + qRound(ny * qMax(0, base.height() - 1)));
                        if (pointAllowed(session, p)) { if (sendAiMouseClick(p.x(), p.y(), "left")) { QThread::msleep(120); Logger::write("INPUT_FOCUS", QString("使用Vision目标聚焦输入区域 x=%1 y=%2").arg(p.x()).arg(p.y())); } }
                    }
                }
            }
            const QString text = action.value("text").toString();
            const bool ok = sendAiUnicodeText(text);
            *success = ok; if (ok) { QThread::msleep(100); scanAgentUia(session, nullptr); }
            return ok ? "键盘文字输入成功。" : "键盘文字输入失败。";
        }
        if (name == "keyboard_hotkey") {
            if (session.scope == AgentScope::SelectedWindow && !inApplicationScope(GetForegroundWindow(),session.targetHwnd)) activateWindowHandle(session.targetHwnd);
            const QString hotkey = action.value("hotkey").toString();
            const bool ok = sendAiHotkey(hotkey); *success = ok;
            if (ok) { QThread::msleep(120); scanAgentUia(session, nullptr); }
            return ok ? "组合键执行成功：" + hotkey : "组合键执行失败：" + hotkey;
        }
        if (name == "screen_capture") { const QString requested = action.value("scope").toString(action.value("mode").toString()); return captureForAgent(session, requested, success); }
        if (name == "vision_analyze") {
            bool captureOk = false;
            const QString visionScope=session.scope==AgentScope::SelectedWindow?QString("selected_window"):action.value("scope").toString(cameraRoot().value("capture_virtual_desktop").toBool(true)?"virtual_desktop":"cursor_monitor");
            const QString captureResult = captureForAgent(session, visionScope, &captureOk);
            if (!captureOk) return captureResult;
            const QImage image(session.lastScreenshot);
            if (image.isNull()) return "VLM分析失败：最新截图无法读取。";
            const QImage thumb = image.scaled(320, 320, Qt::KeepAspectRatio, Qt::FastTransformation).convertToFormat(QImage::Format_RGB32);
            const QByteArray pixels(reinterpret_cast<const char*>(thumb.constBits()), qsizetype(thumb.sizeInBytes()));
            const QString fingerprint = QString::fromLatin1(QCryptographicHash::hash(pixels + QJsonDocument(action).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
            if(fingerprint==session.lastVisionFingerprint&&!session.cachedVision.isEmpty()){
                QJsonObject cached;if(VisionObservation::parse(session.cachedVision,session.frameId,image.width(),image.height(),&cached,nullptr)){session.latestVision=cached;*success=true;return QString::fromUtf8(QJsonDocument(cached).toJson(QJsonDocument::Compact));}
            }
            QString visionPromptPath = writablePath("assets/models/prompts/vision.txt");
            if (!QFileInfo::exists(visionPromptPath)) visionPromptPath = resourcePath("assets/models/prompts/vision.txt");
            QString prompt = QString("VISION_MODE=%1\n" "IMAGE_WIDTH=%2\n" "IMAGE_HEIGHT=%3\n" "请严格输出合法像素坐标和norm_x/norm_y。" "uia_id=0只表示没有UIA ID。\n\n")
                .arg(session.scope == AgentScope::SelectedWindow ? "AGENT_WINDOW_OBSERVATION" : "AGENT_DESKTOP_OBSERVATION").arg(image.width()).arg(image.height());
            prompt += readText(visionPromptPath);
            const QString extra = action.value("prompt").toString();
            if (!extra.isEmpty()) prompt += "\n\n本轮观察要求：\n" + extra;
            QString error;
            const QString result = m_modelExecutor.call([this, screenshot = session.lastScreenshot, prompt, &error] { return m_models ? m_models->vision(screenshot, prompt, &error) : QString(); });
            QJsonObject observation;
            *success=error.isEmpty()&&VisionObservation::parse(result,session.frameId,image.width(),image.height(),&observation,&error);
            if(*success){
                session.latestVision=observation; session.lastVisionFingerprint=fingerprint; session.cachedVision=QString::fromUtf8(QJsonDocument(observation).toJson(QJsonDocument::Compact));
                session.knownVisualTargets=observation.value("targets").toArray(); session.controlChannel="VisionMouse"; session.visionRequired=false;
                if (session.runtimeProfileReady && session.runtimeProfile.surfaceType == "unknown") {
                    const QString state = observation.value("screen_state").toString();
                    if (!state.trimmed().isEmpty()) { session.runtimeProfile.surfaceType = state.left(120); session.runtimeProfile.source += "+vision_refined"; }
                }
                const QString semanticHash = visionSemanticHash(observation);
                if (!session.lastVisionSemanticHash.isEmpty() && semanticHash == session.lastVisionSemanticHash) ++session.consecutiveVisionObservations;
                else session.consecutiveVisionObservations = qMax(1, session.consecutiveVisionObservations);
                session.lastVisionSemanticHash = semanticHash;
                return session.cachedVision;
            }
            Logger::write("VISION_INVALID","task="+session.taskId+" reason="+error+" raw="+result);
            return "视觉结果未通过校验："+error;
        }
        if (name == "app_launch") return "内部错误：启动动作必须由应用启动器执行。";
        if (name == "cmd_exec" || name == "powershell_exec") {
            const QString command = action.value("command").toString();
            if (command.trimmed().isEmpty()) return "命令为空。";
            const CommandRisk risk = commandRisk(command);
            if (risk == CommandRisk::Forbidden) return "权限系统拒绝危险命令。";
            if (risk == CommandRisk::Confirm && !confirmOnUi("AI请求执行命令", command)) {session.cancelledByUser=true;return "用户取消命令，任务停止，不允许更换工具重试。";}
            bool ok = false; QString result;
            if (name == "cmd_exec") result = execProcess("cmd.exe", {"/C", command}, agentCore().value("tool_timeout_ms").toInt(120000), &ok);
            else result = execProcess("powershell.exe", {"-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", command}, agentCore().value("tool_timeout_ms").toInt(120000), &ok);
            *success = ok; return result;
        }
        if (name == "wait") {
            const int ms = qBound(100, action.value("wait_ms").toInt(500), 30000);
            QElapsedTimer timer; timer.start();
            while (timer.elapsed() < ms) { if (m_stop || m_userInterrupted) break; QThread::msleep(25); }
            *success = !m_stop && !m_userInterrupted;
            return QString("等待%1ms%2。").arg(ms).arg(*success ? "完成" : "被中断");
        }
        if (name == "done") { *success = true; return action.value("done_message").toString("任务完成。"); }
        return "未知动作：" + name;
    }

    static QJsonValue boundedJson(const QJsonValue& value,int depth=0){
        if(depth>5)return "[深层数据省略，完整内容在日志]";
        if(value.isObject()){QJsonObject out;const auto object=value.toObject();int count=0;for(auto i=object.begin();i!=object.end()&&count<16;++i,++count)out[i.key()]=boundedJson(i.value(),depth+1);return out;}
        if(value.isArray()){QJsonArray out;for(const auto& item:value.toArray()){if(out.size()>=6)break;out.append(boundedJson(item,depth+1));}return out;}
        if(value.isString()){const QString text=value.toString();QJsonParseError error;const auto document=QJsonDocument::fromJson(text.toUtf8(),&error);if(error.error==QJsonParseError::NoError&&document.isObject())return boundedJson(document.object(),depth+1);if(text.size()>1000)return text.left(1000)+"[文本已缩短]";}
        return value;
    }

    static bool inApplicationScope(HWND candidate,HWND target){
        if(!candidate||!target)return false;candidate=GetAncestor(candidate,GA_ROOT);target=GetAncestor(target,GA_ROOT);
        for(int i=0;candidate&&i<12;++i){if(candidate==target)return true;candidate=GetWindow(candidate,GW_OWNER);}return false;
    }

    QStringList availableActions(const AgentSession& session) const {
        QStringList out;const auto permissions=loadConfig("assets/config/permissions.json");
        const auto agent=agentRoot();const auto mode=agent.value("modes").toObject().value(agentCore().value("mode").toString("assist")).toObject();
        for(const auto& v:loadConfig("assets/config/tools.json").value("tools").toArray()){
            const auto t=v.toObject();const QString name=t.value("name").toString();if(!t.value("enabled").toBool(false))continue;
            const bool observe=name=="mcp_describe"||name=="uia_scan"||name=="screen_capture"||name=="vision_analyze"||name=="wait"||name=="wait_for_change"||name=="done";
            if(!observe&&!mode.value("allow_action").toBool(true))continue;
            const auto app=AppRegistry::lookup(session.targetProcess).entry;
            if (session.actionReady && (name=="uia_scan" || name=="screen_capture" || name=="vision_analyze" || name=="mcp_describe")) continue;
            if(name=="vision_analyze"){
                const bool modeAllowsVision = mode.value("allow_vision").toBool(true);
                const bool appAllowsVision = app.isEmpty() || app.value("vision").toBool(true);
                const bool fallbackAllowsVision = agent.value("fallback").toObject().value("allow_vision_fallback").toBool(true);
                if(!modeAllowsVision || !appAllowsVision || !fallbackAllowsVision) continue;
            }
            if(name.startsWith("uia_")&&(!mode.value("allow_uia").toBool(true)||!app.value("uia").toBool(true)))continue;
            if(name=="desktop_show"&&session.scope!=AgentScope::Desktop)continue;
            if(name=="app_launch"&&(session.scope!=AgentScope::Desktop||!permissions.value("system").toObject().value("allow_start_process").toBool(true)))continue;
            if((name=="cmd_exec"||name=="powershell_exec")&&(session.scope==AgentScope::SelectedWindow||!workflowConfig().value("allow_expert_shell").toBool(false)||!mode.value("allow_shell").toBool(false)||!permissions.value("shell").toObject().value("enabled").toBool(false)))continue;
            if(name.startsWith("mcp_")&&(!session.mcp||!session.mcp->isReady()))continue;
            if(name.startsWith("uia_")){
                if(session.scope==AgentScope::SelectedWindow && session.uiaChecked && !session.uiaUsable) continue;
                const QString key=name=="uia_scan"?"allow_scan":name=="uia_invoke"?"allow_invoke":name=="uia_set_value"?"allow_set_value":name=="uia_focus"?"allow_focus":name=="uia_select"?"allow_select":name=="uia_toggle"?"allow_toggle":"allow_scroll";
                if(!permissions.value("uia").toObject().value(key).toBool(true))continue;
                if(session.preferVision&&name!="uia_scan")continue;
            }
            if(name.startsWith("mouse_")||name.startsWith("keyboard_")){
                const QString key=name=="mouse_move"?"allow_mouse_move":name=="mouse_click"?"allow_mouse_click":name=="mouse_drag"?"allow_mouse_drag":name=="mouse_path"?"allow_mouse_path":name=="keyboard_type"?"allow_keyboard_type":"allow_hotkey";
                if(!permissions.value("input").toObject().value(key).toBool(true))continue;
            }
            out<<name;
        }return out;
    }

    bool authorizeAction(const AgentSession& session,const QJsonObject& action,QString* error) const {
        const QString name=action.value("action").toString();
        const QStringList readActions{"uia_scan","screen_capture","vision_analyze","wait","wait_for_change","done","mcp_describe"};
        if(session.uncertainSideEffect&&!readActions.contains(name)){if(error)*error="上一步外部操作结果未知，当前仅允许观察和完成核对；不自动重复发送或保存。";return false;}
        if(!availableActions(session).contains(name)){if(error)*error="当前范围或权限没有开放动作："+name;return false;}
        const QString observation=action.value("observation_id").toString();
        if(!observation.isEmpty()&&observation!=session.observationId){if(error)*error="动作引用过期UIA观察，需要重新定位";return false;}
        const QString frame=action.value("frame_id").toString();
        if(!frame.isEmpty()&&frame!=session.frameId){if(error)*error="动作引用过期截图，需要重新定位";return false;}
        if(name.startsWith("uia_")&&name!="uia_scan"){
            const UIAItem* item=findSessionItem(session,action.value("target_id").toInt());
            if(!item){if(error)*error="UIA目标不属于最新扫描";return false;}
            if(!pointAllowed(session,item->rect.center())){if(error)*error="UIA目标超出指定应用窗口";return false;}
        }
        if((name=="mouse_move"||name=="mouse_click")&&action.value("target_id").toInt()>0){
            const UIAItem* item=findSessionItem(session,action.value("target_id").toInt());
            if(!item||!pointAllowed(session,item->rect.center())){if(error)*error="鼠标目标已失效或不属于当前可操作窗口，请重新观察";return false;}
        }
        if(name=="app_launch"){
            const QString app=action.value("program").toString(action.value("application").toString());
            if(app.isEmpty()||session.request.application.isEmpty()||AppRegistry::lookup(app).name.compare(AppRegistry::lookup(session.request.application).name,Qt::CaseInsensitive)!=0){if(error)*error="启动目标与用户任务不匹配；不能启动猜测出来的程序";return false;}
            if(!action.value("arguments").toArray().isEmpty()){if(error)*error="当前启动流程不接受模型追加命令参数";return false;}
        }
        const QString terminal=session.targetProcess.toLower();
        if(!workflowConfig().value("allow_expert_shell").toBool(false)&&(terminal=="cmd.exe"||terminal=="powershell.exe"||terminal=="pwsh.exe"||terminal=="windowsterminal.exe")&&(name.startsWith("keyboard_")||name.startsWith("mouse_")||name.startsWith("uia_")&&name!="uia_scan")){
            if(error)*error="未开放终端命令执行，不能通过窗口输入绕过Shell权限";return false;
        }
        if(session.scope==AgentScope::SelectedWindow&&name=="keyboard_hotkey"){
            const QString key=action.value("hotkey").toString().toUpper().remove(' ');
            if(key.contains("WIN")||key=="ALT+TAB"||key=="ALT+ESC"||key=="CTRL+ESC"){if(error)*error="指定应用模式不允许切换到桌面或其他应用的快捷键";return false;}
        }
        if((name=="mouse_click"||name=="mouse_move")&&action.value("target_id").toInt()<=0){
            if(session.latestVision.isEmpty()){if(error)*error="坐标输入需要当前截图的有效视觉观察";return false;}
            const double minimum=agentRoot().value("verification").toObject().value("minimum_confidence").toDouble(0.75);
            double nx=action.value("norm_x").toDouble(-1),ny=action.value("norm_y").toDouble(-1);
            if(nx<0&&session.lastScreenshotRect.width()>0)nx=double(action.value("x").toDouble()-session.lastScreenshotRect.left())/session.lastScreenshotRect.width();
            if(ny<0&&session.lastScreenshotRect.height()>0)ny=double(action.value("y").toDouble()-session.lastScreenshotRect.top())/session.lastScreenshotRect.height();
            bool matched=false;for(const auto& value:session.latestVision.value("targets").toArray()){const auto target=value.toObject();if(target.value("confidence").toDouble()>=minimum&&std::abs(target.value("norm_x").toDouble()-nx)<0.025&&std::abs(target.value("norm_y").toDouble()-ny)<0.025)matched=true;}
            if(!matched){if(error)*error="坐标没有匹配当前视觉观察中的高置信度目标，必须重新定位";return false;}
        }
        if(name=="mouse_drag"||name=="mouse_path"){ if(session.latestVision.isEmpty()){if(error)*error="拖动/轨迹操作需要当前视觉观察";return false;} }
        if(name.startsWith("mouse_")&&action.value("target_id").toInt()<=0&&session.scope==AgentScope::SelectedWindow){
            const QRect current=windowRectOf(session.targetHwnd);const HWND foreground=GetForegroundWindow();
            if(current!=session.lastScreenshotRect&&(!inApplicationScope(foreground,session.targetHwnd)||windowRectOf(foreground)!=session.lastScreenshotRect)){if(error)*error="窗口几何信息已变化，请重新观察后再使用坐标";return false;}
        }
        if(name=="mouse_path"){
            const auto points=action.value("points").toArray();
            if(session.request.kind==TaskKind::CreativeWorkflow){
                QJsonArray canvas;
                for(const auto& v:session.latestVision.value("targets").toArray()){const auto t=v.toObject();const QString label=t.value("label").toString();if((label.contains("画布")||label.contains("canvas",Qt::CaseInsensitive))&&t.value("confidence").toDouble()>=0.75&&t.value("norm_rect").toArray().size()==4)canvas=t.value("norm_rect").toArray();}
                if(canvas.size()!=4){if(error)*error="绘图前需要识别画布矩形范围，请重新观察";return false;}
                for(const auto& v:points){const auto xy=v.toArray();if(xy.size()!=2||xy.at(0).toDouble()<canvas.at(0).toDouble()||xy.at(0).toDouble()>canvas.at(2).toDouble()||xy.at(1).toDouble()<canvas.at(1).toDouble()||xy.at(1).toDouble()>canvas.at(3).toDouble()){if(error)*error="绘图轨迹超出画布，未执行";return false;}}
            }
            if(points.size()<2||points.size()>2048){if(error)*error="轨迹点数量不合法";return false;}
            for(const auto& v:points){const auto p=v.toArray();if(p.size()!=2||!p.at(0).isDouble()||!p.at(1).isDouble()||!std::isfinite(p.at(0).toDouble())||!std::isfinite(p.at(1).toDouble())||p.at(0).toDouble()<0||p.at(0).toDouble()>1||p.at(1).toDouble()<0||p.at(1).toDouble()>1){if(error)*error="轨迹包含非法归一化坐标";return false;}}
        }
        if(error)error->clear();return true;
    }

    TaskRequest understandTask(const QString& goal, AgentSession& session, QString* error) {
        TaskRequest request; request.goal = goal;
        const QString closeApp = TaskRouter::pureClose(goal);
        if (!closeApp.isEmpty() && session.scope == AgentScope::Desktop) {
            request.kind = TaskKind::CloseApplication; request.application = closeApp; request.completion = "目标应用窗口和进程都已经退出";
            request.explicitAction = true; request.continuous = false; if (error) error->clear(); return request;
        }
        const QString app = TaskRouter::pureLaunch(goal);
        if (!app.isEmpty() && session.scope == AgentScope::Desktop) {
            request.kind = TaskKind::OpenApplication; request.application = app; request.completion = "目标应用窗口已经出现";
            request.explicitAction = true; request.continuous = false; if (error) error->clear(); return request;
        }
        const QString prompt = "你只负责理解任务类型，不负责决定用户是否授权电脑操作。只输出JSON。kind只能为chat/open_application/close_application/operate_application/customer_service/creative_workflow/document_workflow。application为软件名或空字符串。指定窗口已经存在时不得要求重新打开其他应用。客服、持续聊天、直到/为止等任务按语义判断continuous。WPS/Word/Excel/PPT的文档、表格、演示制作归document_workflow。Photoshop/画图/修图归creative_workflow。微信/企业微信/聊天软件中的找联系人、回复、客服归customer_service。JSON字段kind,application,completion,continuous,explicit_action；explicit_action只是语义提示，不是权限授权。";
        QJsonArray messages{QJsonObject{{"role","system"},{"content",prompt}},QJsonObject{{"role","user"},{"content",QString("USER_GOAL=%1\nCONTROL_SCOPE=%2\nSELECTED_APP=%3\nSELECTED_PROCESS=%4").arg(goal, session.scope == AgentScope::SelectedWindow ? "WINDOW_CONTROL" : "DESKTOP_CONTROL", session.targetTitle, session.targetProcess)}}};
        QString modelError;
        const QString raw = m_modelExecutor.call([this, messages, &modelError] { return m_models ? m_models->chat("llm", messages, &modelError) : QString(); });
        if (!modelError.isEmpty()) { if (error) *error = modelError; return request; }
        const auto object = strictJsonObject(raw, &modelError);
        const QStringList kinds{"chat", "open_application", "close_application", "operate_application", "customer_service", "creative_workflow", "document_workflow"};
        if (object.isEmpty() || !kinds.contains(object.value("kind").toString())) { if (error) *error = "任务理解返回无效协议：" + modelError; return request; }
        request = TaskRequest::fromJson(object, goal);
        if (session.scope == AgentScope::SelectedWindow) {
            request.application = AppRegistry::lookup(session.targetProcess).name;
            if (request.kind == TaskKind::Chat && !TaskRouter::explicitlyRequestsNoControl(goal)) request.kind = TaskKind::OperateApplication;
            if (request.kind == TaskKind::OpenApplication) request.kind = TaskKind::OperateApplication;
            request.explicitAction = true;
        } else {
            if (request.kind == TaskKind::Chat) request.kind = TaskKind::OperateApplication;
            request.explicitAction = true;
        }
        request.continuous = request.continuous || TaskRouter::looksContinuousTask(goal) || (request.kind == TaskKind::CustomerService && agentRoot().value("task").toObject().value("customer_service_continuous").toBool(true) && TaskRouter::containsAny(goal, {"客服", "一直", "持续", "直到", "为止"}));
        if (error) error->clear(); return request;
    }

    void bindApplication(AgentSession& session,const AppLaunchResult& launched){
        session.scope=AgentScope::SelectedWindow;session.targetHwnd=launched.hwnd;session.targetPid=launched.pid;session.targetTitle=launched.title;
        session.targetProcess=processExecutableName(launched.hwnd);session.targetRect=windowRectOf(launched.hwnd);session.latestVision={};session.frameId.clear();
    }

    void discoverCapabilities(AgentSession& session){
        if(session.scope!=AgentScope::SelectedWindow)return;
        if(session.mcpChecked)return;
        session.mcpChecked=true;
        const auto app=AppRegistry::lookup(session.targetProcess);
        const auto cfg=app.entry.value("mcp").toObject();
        if(!cfg.value("enabled").toBool(false)){ session.mcpUsable=false; session.mcpFailureReason="未配置MCP"; log("[能力锁定] "+session.targetProcess+" 未配置MCP，本任务后续忽略MCP。"); return; }
        auto client=std::make_shared<McpClient>();
        QString error;
        if(client->connectServer(cfg,[this,&session]{ return m_stop||m_userInterrupted||RuntimeSafety::instance().shouldAbort(session.cancellationGeneration); },&error)){
            session.mcp=std::move(client); session.mcpUsable=true; session.controlChannel="MCP";
            log("[能力锁定] MCP连接和工具查询成功，本任务优先MCP。");
        }else{
            session.mcpUsable=false; session.mcpFailureReason=error;
            log("[能力锁定] MCP不可用："+error+"；本任务后续忽略MCP。");
        }
    }

    QString observationFingerprint(AgentSession& session){
        QString text;for(const auto& i:session.uiaItems)if(!i.name.isEmpty())text+=i.name.left(120)+"|";
        if(!session.latestVision.isEmpty()){QJsonObject vision=session.latestVision;vision.remove("frame_id");text+=QString::fromUtf8(QJsonDocument(vision).toJson(QJsonDocument::Compact));}
        return QString::fromLatin1(QCryptographicHash::hash(text.toUtf8(),QCryptographicHash::Sha256).toHex());
    }

    QString waitForChange(AgentSession& session){
        session.workflow.phase="waiting"; session.waitingForExternalChange=true;
        session.policyPhase=AgentPolicyPhase::WaitForChange; session.actionReady=false;
        session.plannerObservationViolations=0;
        const int poll=qBound(500, workflowConfig().value("idle_poll_ms").toInt(1500), 10000);
        const int timeout=qBound(1000, workflowConfig().value("idle_heartbeat_ms").toInt(15000), 300000);
        session.waitForChangeBaselineFingerprint=observationFingerprint(session);
        QImage previous; QString previousText;
        for(const auto& i:session.uiaItems) previousText+=i.name+"|";
        QElapsedTimer elapsed; elapsed.start();
        while(elapsed.elapsed()<timeout && !m_stop && !m_userInterrupted && !RuntimeSafety::instance().shouldAbort(session.cancellationGeneration)){
            if(session.scope==AgentScope::SelectedWindow && !validWindowHandle(session.targetHwnd)){ session.waitingForExternalChange=false; return "目标窗口已关闭"; }
            hideOverlayForCapture(session.scope);
            const CaptureResult capture= session.scope==AgentScope::SelectedWindow ?captureWindow(session.targetHwnd) :captureCursorMonitor();
            restoreOverlayAfterCapture(session.scope);
            if(capture.valid){
                const QImage current=capture.image.scaled(240,160,Qt::IgnoreAspectRatio,Qt::FastTransformation).convertToFormat(QImage::Format_Grayscale8);
                if(!previous.isNull()){
                    int changed=0;
                    for(int y=0;y<current.height();++y){ const uchar* cLine=current.constScanLine(y); const uchar* pLine=previous.constScanLine(y); for(int x=0;x<current.width();++x){ if(std::abs(int(cLine[x])-int(pLine[x]))>24) ++changed; } }
                    if(changed > current.width()*current.height()*0.005){
                        session.latestVision={}; if(session.uiaUsable) scanAgentUia(session,nullptr);
                        session.workflow.phase="observe"; session.waitingForExternalChange=false;
                        session.policyPhase=AgentPolicyPhase::ObserveOrVerify; session.unchangedWaitCycles=0;
                        ++session.interactionEpoch; return "检测到目标应用画面变化，重新观察。";
                    }
                }
                previous=current;
            }
            QElapsedTimer wait; wait.start();
            while(wait.elapsed()<poll && !m_stop && !m_userInterrupted && !RuntimeSafety::instance().shouldAbort(session.cancellationGeneration)) QThread::msleep(30);
            if(session.uiaUsable){
                scanAgentUia(session,nullptr);
                QString currentText;
                for(const auto& i:session.uiaItems) currentText+=i.name+"|";
                if(currentText!=previousText){
                    session.latestVision={}; session.workflow.phase="observe"; session.waitingForExternalChange=false;
                    session.policyPhase=AgentPolicyPhase::ObserveOrVerify; session.unchangedWaitCycles=0;
                    ++session.interactionEpoch; return "检测到新的界面文字，重新观察。";
                }
            }
        }
        if(m_stop || m_userInterrupted || RuntimeSafety::instance().shouldAbort(session.cancellationGeneration)){ session.waitingForExternalChange=false; return "等待已取消"; }
        ++session.unchangedWaitCycles;
        session.workflow.phase="waiting"; session.waitingForExternalChange=true; session.policyPhase=AgentPolicyPhase::WaitForChange;
        return "等待期间没有新变化；保持WAIT_FOR_CHANGE，后续继续低频观察，不表示任务失败。";
    }

    QJsonObject verifyAgentActionWorker(AgentSession& session,const QString& goal,const QJsonObject& action,bool toolSuccess,const QString& toolResult){
        const QString name=action.value("action").toString();
        QJsonObject result{{"success",toolSuccess},{"step_success",toolSuccess},{"goal_success",false},{"terminal",false},{"blocked",false},{"confidence",toolSuccess?1.0:0.0},{"reason",toolSuccess?"工具已执行，最终目标尚待确认":toolResult.section('\n',0,0)},{"need_reobserve",!toolSuccess},{"need_retry",false}};
        if(name!="done")return result;
        if(m_stop||m_userInterrupted||RuntimeSafety::instance().shouldAbort(session.cancellationGeneration))return result;
        QString path=writablePath("assets/models/prompts/verifier.txt");if(!QFileInfo::exists(path))path=resourcePath("assets/models/prompts/verifier.txt");
        const QString prompt=readText(path).trimmed().isEmpty()?compactVerifierProtocol():readText(path);
        const QString evidence=environmentSummary(session)+"\nLATEST_VISION="+QString::fromUtf8(QJsonDocument(session.latestVision).toJson(QJsonDocument::Compact))+"\nLATEST_MCP="+QString::fromUtf8(QJsonDocument(session.latestMcp).toJson(QJsonDocument::Compact));
        QJsonArray messages{QJsonObject{{"role","system"},{"content",evidenceRules()+prompt}},QJsonObject{{"role","user"},{"content","USER_GOAL:\n"+goal+"\nCURRENT_EVIDENCE:\n"+evidence+"\nCOMPLETION_CLAIM:\n"+action.value("done_message").toString()}}};
        if(estimateMessagesTokens(messages)>plannerInputBudget()){result["reason"]="完成验证超过预算，应缩小观察范围";result["need_reobserve"]=true;return result;}
        QString error;const QString reply=m_modelExecutor.call([this,messages,&error]{return m_models?m_models->chat("llm",messages,&error):QString();});
        Logger::write("FINAL_VERIFIER",reply);
        const auto verified=strictJsonObject(reply,&error);const double minimum=qBound(0.5,agentRoot().value("verification").toObject().value("minimum_confidence").toDouble(0.75),1.0);
        bool supported=false;for(const auto& value:verified.value("evidence").toArray()){const QString quote=value.toString().trimmed();if(quote.size()>=2&&evidence.contains(quote))supported=true;}
        const bool complete=error.isEmpty()&&verified.value("goal_success").toBool(false)&&verified.value("confidence").toDouble()<1.01&&verified.value("confidence").toDouble()>=minimum&&supported;
        result["goal_success"]=complete;result["confidence"]=complete?verified.value("confidence"):QJsonValue(0.0);
        result["reason"]=verified.value("reason").toString("缺少可核对的完成证据");result["need_reobserve"]=!complete;
        result["user_message"]=complete?action.value("done_message").toString("任务已完成。"):QString();
        return result;
    }

    void sendTask() {
        if (m_state != AppState::Ready || !m_acceptNewTasks) return;
        const QString text = m_input->toPlainText().trimmed();
        if (text.isEmpty()) return;
        m_input->clear(); addMessage(MessageRole::User, text);
        if(m_workMode==WorkMode::Chat){ if(TaskRouter::explicitTaskInChat(text)){ m_workMode=WorkMode::DesktopControl;refreshModeButtons();startAgentTask(text,AgentScope::Desktop); }else runChat(text); return; }
        if (m_workMode == WorkMode::DesktopControl) { startAgentTask(text, AgentScope::Desktop); return; }
        if (m_workMode == WorkMode::WindowControl) { startAgentTask(text, AgentScope::SelectedWindow); return; }
        addMessage(MessageRole::System, "当前正在选择截图区域，请先完成或取消截图。");
    }

    void resumeLatestTask() {
        if (m_state != AppState::Ready || m_running || !m_acceptNewTasks) { addMessage(MessageRole::System, "请等当前任务结束后再恢复。"); return; }
        const QDir dir(QDir(Logger::root()).filePath("agent_context"));
        const QFileInfoList files = dir.entryInfoList(QStringList{"task_*.json"}, QDir::Files, QDir::Time);
        QJsonObject saved;
        for (const QFileInfo& file : files) { const QJsonObject item = readJson(file.absoluteFilePath()); const QString status = item.value("status").toString(); if (item.value("version").toInt() == 2 && (status == "running" || status == "paused")) { saved = item; break; } }
        if (saved.isEmpty()) { addMessage(MessageRole::System, "没有可恢复的未完成任务。已完成或主动停止的任务不会自动恢复。"); return; }
        const QString goal = saved.value("goal").toString();
        if (goal.isEmpty()) { addMessage(MessageRole::System, "检查点缺少原任务目标，无法恢复。"); return; }
        const bool windowTask = saved.value("scope").toString() == "window";
        if (windowTask) {
            if (m_workMode != WorkMode::WindowControl || !validWindowHandle(m_selectedHwnd)) { addMessage(MessageRole::System, "请先用“指定识别”重新选中目标软件，再从托盘恢复任务。旧窗口句柄不会直接复用。"); return; }
            const QString expected = saved.value("target_process").toString();
            if (expected.isEmpty() || processExecutableName(m_selectedHwnd).compare(expected, Qt::CaseInsensitive) != 0) { addMessage(MessageRole::System, "当前指定的软件与检查点不一致，恢复已拒绝。"); return; }
        } else if (m_workMode != WorkMode::DesktopControl) { addMessage(MessageRole::System, "请先开启“桌面识别”，再恢复桌面任务。"); return; }
        log("[恢复] 读取任务检查点，将先重新扫描现场：" + saved.value("task_id").toString());
        startAgentTask(goal, windowTask ? AgentScope::SelectedWindow : AgentScope::Desktop, saved);
    }

    void startAgentTask(const QString& goal, AgentScope scope, const QJsonObject& saved = {}) {
        if (!m_acceptNewTasks) return;
        if(!agentCore().value("enabled").toBool(true)){addMessage(MessageRole::System,"Agent功能已关闭，请在设置中启用。");return;}
        const auto allowedModes=agentRoot().value("work_mode").toObject();
        if(!allowedModes.value(scope==AgentScope::SelectedWindow?"allow_window_control":"allow_desktop_control").toBool(true)){addMessage(MessageRole::System,"当前配置未开放该控制模式。");return;}
        if (m_running.exchange(true)) { addMessage(MessageRole::System, "当前已有自动任务正在执行，请先停止当前任务。"); return; }
        QString safetyError;
        if (!RuntimeSafety::instance().beginTask(&safetyError)) { m_running = false; addMessage(MessageRole::System, safetyError); return; }
        if (scope == AgentScope::SelectedWindow && !validWindowHandle(m_selectedHwnd)) { m_running = false; addMessage(MessageRole::System, "指定窗口已经关闭，请重新点击“指定识别”选择软件。"); stopWindowControl(false); return; }
        m_stop = false; m_userInterrupted = false; m_userInputMonitor.resetActivity();
        AgentSession session;
        m_controlActive=false;
        session.scope = scope; session.originScope=scope;
        session.request.goal=goal;
        session.taskMode = taskModeForGoal(goal);
        session.taskId = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz") + "_" + QUuid::createUuid().toString(QUuid::Id128).left(8);
        session.goal = goal;
        session.cancellationGeneration = RuntimeSafety::instance().generation();
        session.uiaPromptLimit = qBound(8, agentRoot().value("observation").toObject().value("maximum_uia_items_in_prompt").toInt(40), 40);
        if (!saved.isEmpty()) {
            session.restored = true;
            session.taskId = saved.value("task_id").toString(session.taskId);
            session.contextSummary = saved.value("summary").toString().left(1600);
            const QJsonArray recent = saved.value("recent_history").toArray();
            for (int i = qMax(0, int(recent.size()) - 2); i < recent.size(); ++i) session.recentHistory.append(recent.at(i));
            const QJsonObject pending = saved.value("pending_action").toObject();
            if (!pending.isEmpty()) session.contextSummary += "\n崩溃前未确认动作=" + QString::fromUtf8(QJsonDocument(pending).toJson(QJsonDocument::Compact)).left(500) + "；该动作可能已经执行，禁止直接重放，先检查实际界面和发送状态。";
        }
        persistContextSummary(session);
        log("[主题] 独立任务 " + session.taskId + (session.restored ? "，从检查点恢复并重新观察。" : "，不继承其他任务或普通聊天上下文。"));
        if (scope == AgentScope::SelectedWindow) { session.targetHwnd = m_selectedHwnd; session.targetPid = m_selectedWindowPid; session.targetTitle = m_selectedWindowTitle; session.targetRect = m_selectedWindowRect; session.targetProcess = processExecutableName(m_selectedHwnd); }
        persistContextSummary(session);
        setAppState(AppState::Generating, scope == AgentScope::SelectedWindow ? "[任务] 开始执行指定窗口任务：" + goal : "[任务] 开始执行桌面任务：" + goal);
        if (scope == AgentScope::Desktop && m_desktopRefreshTimer) { const int interval = qMax(300, desktopCfg().value("active_uia_refresh_interval_ms").toInt(1000)); m_desktopRefreshTimer->start(interval); log(QString("[观察] Agent执行期间桌面UIA覆盖层刷新切换为%1ms。").arg(interval)); }
        m_agentExecutor.post([this, goal, session = std::move(session)]() mutable { runAgentWorker(goal, std::move(session)); });
    }

    void runAgentWorker(const QString& goal,AgentSession session){
        struct Apartment{HRESULT hr=CoInitializeEx(nullptr,COINIT_MULTITHREADED);~Apartment(){if(SUCCEEDED(hr))CoUninitialize();}} apartment;
        bool finished=false;QString message;session.originScope=session.scope;
        auto cancelled=[this,&session]{return m_stop||m_userInterrupted||RuntimeSafety::instance().shouldAbort(session.cancellationGeneration);};
        auto finish=[&]{
            session.status=finished?"completed":cancelled()||session.cancelledByUser?"cancelled":"paused";
            persistContextSummary(session);
            if(!message.isEmpty())postUi([this,message]{addMessage(MessageRole::Ai,message);});
            finishAgentTaskUi(session.originScope);
        };
        QString error;bool visualLaunchFallback=false;
        const ExecutionAuthorization authorization = buildExecutionAuthorization(session);
        log(QString("[授权] %1").arg(authorization.reason));
        if (!authorization.authorized) { message = "当前控制模式没有有效操作授权：" + authorization.reason; finish(); return; }
        const QString pure=TaskRouter::pureLaunch(goal);
        if(!pure.isEmpty()&&session.scope==AgentScope::Desktop){
            session.request.goal=goal;session.request.application=pure;session.request.kind=TaskKind::OpenApplication;session.workflow.phase="ensure_application";persistContextSummary(session);
            const auto mode=agentRoot().value("modes").toObject().value(agentCore().value("mode").toString("assist")).toObject();
            if(!mode.value("allow_action").toBool(true)||!loadConfig("assets/config/permissions.json").value("system").toObject().value("allow_start_process").toBool(true)){message="当前权限不允许启动应用。";finish();return;}
            if(!loadConfig("assets/config/permissions.json").value("system").toObject().value("explicit_task_launch_is_authorized").toBool(true)&&!confirmOnUi("应用启动确认",pure)){ session.cancelledByUser=true;message="用户取消启动，任务已停止。";finish();return; }
            runUiBlocking([this]{m_controlActive=true;m_userInterrupted=false;m_userInputMonitor.resetActivity();});
            log("[APP_LIFECYCLE] 确定性打开流程开始："+pure);
            const auto launched= ApplicationLifecycleManager::open( pure,{},cancelled);
            session.lastResult=launched.result; finished=launched.result.success();
            if(finished){ bindApplication(session,launched); session.workflow.phase="completed"; persistContextSummary(session); message=launched.result.message;
                log(QString("[APP_LIFECYCLE] 打开确认完成：pid=%1 hwnd=%2 title=%3").arg(launched.pid).arg(quintptr(launched.hwnd)).arg(launched.title));
                finish(); return;
            }
            if(launched.result.data.value("code")=="entry_not_found" && workflowConfig().value("allow_visual_launch_fallback").toBool(true)){
                visualLaunchFallback=true; log("[启动回退] 未找到注册入口，显示桌面进行有限次快捷方式识别。"); sendAiHotkey("WIN+D");
            }else{ message=launched.result.message; finish(); return; }
        }
        const QString pureClose=TaskRouter::pureClose(goal);
        if(!pureClose.isEmpty()&&session.scope==AgentScope::Desktop){
            session.request.goal=goal;session.request.application=pureClose;session.request.kind=TaskKind::CloseApplication;session.workflow.phase="close_application";persistContextSummary(session);
            runUiBlocking([this]{m_controlActive=true;m_userInterrupted=false;m_userInputMonitor.resetActivity();});
            log("[APP_LIFECYCLE] close 开始："+pureClose);
            const auto closed=ApplicationLifecycleManager::close(pureClose,cancelled);session.lastResult=closed.result;log("[APP_LIFECYCLE] close 结果："+closed.result.message);
            if(closed.result.outcome==ToolOutcome::Succeeded){finished=true;message=closed.result.message;finish();return;}
            if(closed.result.outcome==ToolOutcome::NeedObservation&&closed.hwnd){session.targetHwnd=closed.hwnd;session.targetPid=closed.pid;session.targetTitle=closed.title;session.targetRect=windowRectOf(closed.hwnd);session.targetProcess=processExecutableName(closed.hwnd);session.request.kind=TaskKind::OperateApplication;session.request.completion="目标应用完全退出";session.nextObjective="处理阻止应用关闭的当前对话框，然后继续关闭应用直到进程退出";session.workflow.phase="close_application_dialog";}else{message=closed.result.message;finish();return;}
        }
        if(!m_modelExecutor.call([this,&error]{return m_models&&m_models->ensureReady("llm",&error);})){ message="模型不可用："+error;finish();return; }
        if(!visualLaunchFallback)session.request=understandTask(goal,session,&error);
        if(!error.isEmpty()){message="无法可靠理解任务："+error;finish();return;}
        if(session.request.kind==TaskKind::Chat) session.request.kind=TaskKind::OperateApplication;
        runUiBlocking([this]{m_controlActive=true;m_userInterrupted=false;m_userInputMonitor.resetActivity();});
        session.request.continuous = session.request.continuous || TaskRouter::looksContinuousTask(goal);
        session.taskMode=session.request.continuous?"CONTINUOUS":"ONE_SHOT";
        if (session.request.kind == TaskKind::CustomerService) {
            session.targetEntity = TaskRouter::extractTargetEntity(goal);
            if (!session.targetEntity.isEmpty()) { session.nextObjective = "定位并打开联系人：" + session.targetEntity; log("[语义目标] 客服联系人=" + session.targetEntity); }
        }
        log("[任务理解] "+QString::fromUtf8(QJsonDocument(session.request.json()).toJson(QJsonDocument::Compact)));
        if(!visualLaunchFallback&&session.scope==AgentScope::Desktop&&!session.request.application.isEmpty()){
            session.workflow.phase="ensure_application";persistContextSummary(session);
            const auto mode=agentRoot().value("modes").toObject().value(agentCore().value("mode").toString("assist")).toObject();
            if(!mode.value("allow_action").toBool(true)||!loadConfig("assets/config/permissions.json").value("system").toObject().value("allow_start_process").toBool(true)){message="当前权限不允许启动目标应用。";finish();return;}
            if(!loadConfig("assets/config/permissions.json").value("system").toObject().value("explicit_task_launch_is_authorized").toBool(true)&&!confirmOnUi("应用启动确认",session.request.application)){ session.cancelledByUser=true;message="用户取消启动，任务已停止。";finish();return; }
            const auto launched=ApplicationLifecycleManager::open(session.request.application,{},cancelled);
            if(!launched.result.success()){message=launched.result.message;finish();return;}
            bindApplication(session,launched);log("[应用] "+launched.result.message);
        }
        if(cancelled()){message="任务已停止。";finish();return;}
        session.workflow.phase = "profile_application";
        QString profileError;
        session.runtimeProfile = buildRuntimeAppProfile(session, goal, &profileError);
        session.runtimeProfileReady = true; persistRuntimeAppProfile(session);
        log("[运行画像] " + QString::fromUtf8(QJsonDocument(session.runtimeProfile.json()).toJson(QJsonDocument::Compact)));
        if (!profileError.isEmpty()) log("[运行画像] 动态分析失败，已安全降级到通用Profile：" + profileError.left(300));
        session.workflow.phase="discover_capabilities";
        discoverCapabilities(session);
        if (!(session.scope == AgentScope::SelectedWindow && session.uiaChecked && !session.uiaUsable)) {
            QString uiaProbeError;
            if (!scanAgentUia(session, &uiaProbeError) && session.scope == AgentScope::SelectedWindow) {
                session.uiaChecked = true; session.uiaUsable = false; session.visionRequired = true; session.controlChannel = "VisionMouse";
                if (session.uiaFailureReason.isEmpty()) session.uiaFailureReason = uiaProbeError;
                log("[能力锁定] UIA探测失败，本任务切换VisionMouse：" + session.uiaFailureReason);
            }
        }
        session.workflow.phase="observe";
        session.observationPolicy = observationPolicyForSession(session);
        session.preferVision = (session.observationPolicy == "vision_first");
        updateGenericProgressWatchdog(session);
        const int initialActionable = session.uiaUsable ? int(std::count_if(session.uiaItems.cbegin(), session.uiaItems.cend(), [](const UIAItem& item){ return item.clickable; })) : 0;
        log(QString("[观察策略] app=%1 kind=%2 policy=%3 UIA可操作=%4").arg(session.targetProcess, session.request.kindName(), session.observationPolicy).arg(initialActionable));
        QString plannerPath=writablePath("assets/models/prompts/planner.txt");if(!QFileInfo::exists(plannerPath))plannerPath=resourcePath("assets/models/prompts/planner.txt");
        const QString plannerPrompt=readText(plannerPath);
        Logger::write("PROTOCOL","planner="+plannerPath+" verifier="+resourcePath("assets/models/prompts/verifier.txt")+" version=workflow-v2");
        const int maxFailures=qBound(1,agentCore().value("max_retries_per_step").toInt(3),10);
        const int maxRepeat=qBound(1,agentCore().value("max_same_action_repeat").toInt(3),10);
        const int maxSteps=qBound(10,agentCore().value("max_steps").toInt(80),1000);
        int observationFailures=0,unverifiedDone=0;
        bool needVisual = visualLaunchFallback || session.visionRequired || session.observationPolicy == "vision_first" || (!session.mcpUsable && !session.uiaUsable);
        while(!cancelled()){
            if (session.waitingForExternalChange || session.workflow.phase == "waiting") {
                const QString waitResult = waitForChange(session); log("[wait_for_change] " + waitResult.left(500));
                if (cancelled()) break;
                if (session.waitingForExternalChange || session.workflow.phase == "waiting") continue;
                session.latestVision = {}; session.actionReady = false;
                session.policyPhase = AgentPolicyPhase::ObserveOrVerify; session.workflow.phase = "observe"; needVisual = true;
            }
            if(visualLaunchFallback){
                const HWND opened=ApplicationLauncher::existing(AppRegistry::lookup(session.request.application));
                if(opened){finished=true;message="已确认“"+session.request.application+"”窗口打开。";break;}
                if(session.stepNumber>=workflowConfig().value("visual_launch_max_steps").toInt(10)){message="未找到“"+session.request.application+"”的可用安装入口或可确认的桌面快捷方式。";break;}
            }
            if(session.scope==AgentScope::SelectedWindow&&!validWindowHandle(session.targetHwnd)){message="指定应用窗口已关闭，任务停止。";break;}
            if(!session.workflow.canContinue(maxFailures)){message="当前阶段连续失败，已暂停；具体原因："+session.lastResult.message;break;}
            if(session.workflow.workSteps>=maxSteps){
                if(session.request.continuous){session.workflow.workSteps=0;session.workflow.phase="waiting";waitForChange(session);needVisual=true;}
                else{message="当前任务达到阶段步骤上限，尚未确认完成。检查点已保存。";break;}
            }
            session.stepNumber++;persistContextSummary(session);
            const int actionable=int(std::count_if(session.uiaItems.cbegin(),session.uiaItems.cend(),[](const UIAItem& i){return i.clickable;}));
            if((needVisual||(actionable==0&&(!session.mcp||!session.mcp->isReady())))&&session.latestVision.isEmpty()&&availableActions(session).contains("vision_analyze")){
                bool observed=false;
                const QString observationPrompt=!session.groundingTarget.trimmed().isEmpty()
                    ? ("只定位当前待执行语义目标【"+session.groundingTarget.left(160)+"】。必须返回该目标的norm_x/norm_y；能判断可点击区域时同时返回norm_rect。如果目标确实不可见，targets返回空数组，不要拿联系人、聊天列表、其他控件代替。")
                    : (conversationLikeSurface(session) && session.targetEntity.trimmed().isEmpty()
                           ? ("用户已经手动选定当前应用窗口。不要替用户选择或切换联系人。优先定位当前已经打开会话的消息输入框和发送按钮；如果当前没有打开会话，只说明状态，不要返回任意聊天列表项作为下一步目标。USER_GOAL="+goal.left(500))
                           : ("仅定位完成当前任务下一步需要的目标："+goal.left(500)));
                const QJsonObject observationAction{{"action","vision_analyze"},{"prompt",observationPrompt}};
                const QString result=executeActionWorker(session,observationAction,&observed);log("[观察] "+result.left(700));
                appendRecentHistory(session,observationAction,observed,result,{});
                if(!observed){
                    ++observationFailures;
                    if(actionable>0||(session.mcp&&session.mcp->isReady())){needVisual=false;log("[观察回退] 视觉结果不可用，仍有UIA/MCP能力，继续采用其他可用方式。");}
                    else{ if(observationFailures>=maxFailures){message="当前没有有效UIA/MCP观察，视觉结果又连续未通过校验，任务暂停："+result;break;} continue; }
                }else{
                    observationFailures=0; needVisual=false;
                    updateGenericProgressWatchdog(session); updateInteractionEpoch(session);
                    verifyPendingCommitFromLatestObservation(session);
                    if (session.request.kind == TaskKind::CustomerService && session.currentContact.isEmpty() && !session.targetEntity.isEmpty()) {
                        QJsonObject target = findVisualTargetByLabel(session, session.targetEntity, 0.70);
                        if (!target.isEmpty()) {
                            const QJsonObject clickAction = directClickForVisualTarget(session, target, "视觉已精确定位联系人【" + session.targetEntity + "】，直接点击进入会话。");
                            bool clickOk = false; const QString clickResult = executeActionWorker(session, clickAction, &clickOk);
                            Logger::write("GROUNDING_CLICK", QString("task=%1 step=%2 target=%3 ok=%4\n%5").arg(session.taskId).arg(session.stepNumber).arg(session.targetEntity).arg(clickOk).arg(clickResult));
                            log(QString("[精确定位] 找到联系人“%1”，直接点击：%2").arg(session.targetEntity, clickResult.left(300)));
                            appendRecentHistory(session, clickAction, clickOk, clickResult, {});
                            if (clickOk) { session.currentContact = session.targetEntity; session.nextObjective = "验证已进入联系人会话并读取最新消息"; session.consecutiveVisionObservations = 0; session.consecutiveGroundingFailures = 0; session.groundingRequested = false; session.latestVision = {}; session.workflow.phase = "verify_effect"; needVisual = true; continue; }
                            ++session.consecutiveGroundingFailures;
                        } else if (visionVisibleTextContains(session, session.targetEntity)) {
                            const int maxGroundingAttempts = qBound(1, agentRoot().value("visual_grounding").toObject().value("max_refine_attempts").toInt(2), 3);
                            if (session.consecutiveGroundingFailures < maxGroundingAttempts) {
                                session.groundingRequested = true; session.lastGroundingTarget = session.targetEntity;
                                const QJsonObject groundingAction{{"action", "vision_analyze"},{"prompt", groundingPromptForEntity(session, session.targetEntity)}};
                                bool groundingOk = false; const QString groundingResult = executeActionWorker(session, groundingAction, &groundingOk);
                                appendRecentHistory(session, groundingAction, groundingOk, groundingResult, {});
                                QJsonObject grounded = findVisualTargetByLabel(session, session.targetEntity, agentRoot().value("visual_grounding").toObject().value("minimum_confidence").toDouble(0.70));
                                if (groundingOk && !grounded.isEmpty()) {
                                    const QJsonObject clickAction = directClickForVisualTarget(session, grounded, "精确grounding已定位联系人【" + session.targetEntity + "】，直接点击。");
                                    bool clickOk = false; const QString clickResult = executeActionWorker(session, clickAction, &clickOk);
                                    Logger::write("GROUNDING_CLICK", QString("task=%1 step=%2 target=%3 refine=1 ok=%4\n%5").arg(session.taskId).arg(session.stepNumber).arg(session.targetEntity).arg(clickOk).arg(clickResult));
                                    appendRecentHistory(session, clickAction, clickOk, clickResult, {});
                                    if (clickOk) { log("[精确定位] 专用grounding成功，已点击联系人：" + session.targetEntity); session.currentContact = session.targetEntity; session.nextObjective = "验证已进入联系人会话并读取最新消息"; session.consecutiveVisionObservations = 0; session.consecutiveGroundingFailures = 0; session.groundingRequested = false; session.latestVision = {}; session.workflow.phase = "verify_effect"; needVisual = true; continue; }
                                }
                                ++session.consecutiveGroundingFailures;
                                log(QString("[精确定位] 已看见文字“%1”，但第%2次未获得可靠可点击坐标。").arg(session.targetEntity).arg(session.consecutiveGroundingFailures));
                            }
                        }
                    }
                    const int maxVisionOnly = qBound(2, agentRoot().value("visual_grounding").toObject().value("max_vision_only_cycles").toInt(3), 5);
                    if (session.consecutiveVisionObservations >= maxVisionOnly) { message = QString("连续%1次视觉观察没有产生可靠可执行目标，已暂停，避免视觉死循环。最后截图：%2").arg(session.consecutiveVisionObservations).arg(session.lastScreenshot); log("[视觉看门狗] " + message); break; }
                }
            }
            const int genericLimit = qBound(2, agentRoot().value("generic_policy").toObject().value("max_no_progress_cycles").toInt(3), 6);
            if (session.genericNoProgressCount >= genericLimit) { message = QString("连续%1轮世界状态没有实质变化，已暂停当前策略，避免智能体死循环。当前Profile=%2/%3，最后截图=%4").arg(session.genericNoProgressCount).arg(session.runtimeProfile.processProfile, session.runtimeProfile.surfaceType, session.lastScreenshot); log("[通用看门狗] " + message); break; }
            QJsonObject resumedSemanticAction;bool hasResumedSemanticAction=false;
            if(!session.pendingSemanticAction.isEmpty()){
                QString groundingNote;resumedSemanticAction=resolveSemanticPlannerAction(session,session.pendingSemanticAction,&groundingNote);
                const QString resumedName=resumedSemanticAction.value("action").toString().trimmed().toLower();
                const bool hasPoint=resumedSemanticAction.value("target_id").toInt()>0||(resumedSemanticAction.value("norm_x").isDouble()&&resumedSemanticAction.value("norm_y").isDouble())||(resumedSemanticAction.value("x").isDouble()&&resumedSemanticAction.value("y").isDouble());
                hasResumedSemanticAction=(resumedName=="mouse_click"||resumedName=="mouse_move")?hasPoint:!resumedName.isEmpty();
                if(hasResumedSemanticAction)log("[GROUNDING] 已获得语义目标坐标，恢复待执行动作："+session.groundingTarget.left(120));
            }
            QJsonObject forcedPendingCommitAction; QString forcedPendingCommitNote; bool hasForcedPendingCommitAction = false;
            if (session.pendingCommitRequired) {
                if (session.pendingCommitAwaitingVerification) { session.latestVision = {}; session.actionReady = false; session.visionRequired = true; needVisual = true; log("[动作事务] pending_commit等待提交效果验证，继续观察，不重复发送。"); continue; }
                forcedPendingCommitAction = buildDirectPendingCommitAction(session, &forcedPendingCommitNote);
                if (!forcedPendingCommitAction.isEmpty()) { hasForcedPendingCommitAction = true; log("[动作事务] " + forcedPendingCommitNote); }
                else if (session.groundingTarget.isEmpty() && session.pendingSemanticAction.isEmpty()) { armPendingCommitGrounding(session); log("[动作事务] pending_commit缺少提交控件坐标，进入targeted grounding。"); needVisual = true; continue; }
            }
            QJsonObject forcedConversationAction; bool hasForcedConversationAction = false;
            if (!hasForcedPendingCommitAction && !hasResumedSemanticAction && conversationLikeSurface(session) && session.targetEntity.trimmed().isEmpty() && !session.pendingCommitRequired) {
                const QString turnState = interactionTurnStateName(session);
                if (turnState == "READY_TO_INITIATE" || turnState == "READY_TO_RESPOND") {
                    const QJsonObject inputTarget = currentConversationInputTarget(session, 0.55);
                    if (!inputTarget.isEmpty()) {
                        forcedConversationAction = {{"action", "keyboard_type"},{"reason", turnState == "READY_TO_INITIATE" ? "当前会话输入框已可见，直接生成并输入自然开场，不切换联系人。" : "当前会话有新的外部内容，直接生成并输入自然回复，不切换联系人。"}};
                        hasForcedConversationAction = true;
                        session.nextObjective = turnState == "READY_TO_INITIATE" ? "在当前会话输入框主动发起自然对话" : "在当前会话输入框回复最新内容";
                        log("[当前会话] 已发现输入框，直接进入keyboard_type，禁止Planner擅自切换联系人。");
                    }
                }
            }
            QString plannerError;
            log(QString("[流程] task=%1 stage=%2 step=%3 policy=%4").arg(session.taskId,session.workflow.phase).arg(session.stepNumber).arg(agentPolicyPhaseName(session)));
            QString parseError;QJsonObject action;QJsonObject plannerActionBeforeResolution;QString resolutionNote;
            if(hasForcedPendingCommitAction){action=forcedPendingCommitAction;plannerActionBeforeResolution=forcedPendingCommitAction;resolutionNote=forcedPendingCommitNote;}
            else if(hasResumedSemanticAction){action=resumedSemanticAction;plannerActionBeforeResolution=session.pendingSemanticAction;resolutionNote="NeedGrounding恢复：已根据最新视觉绑定语义目标“"+session.groundingTarget+"”";}
            else if(hasForcedConversationAction){action=forcedConversationAction;plannerActionBeforeResolution=forcedConversationAction;resolutionNote="当前会话输入框已可见，C++直接进入自然语言输入";}
            else{
                const QString reply=callPlannerWithRecovery(session,goal,{},plannerPrompt,&plannerError);
                if(cancelled())break;
                if(!plannerError.isEmpty()){message="规划请求失败："+plannerError;break;}
                Logger::write("PLANNER_RAW",reply);
                action=strictJsonObject(reply,&parseError);
                if(action.isEmpty()){ session.workflow.failures++; session.lastResult={ToolOutcome::RetryableFailure,parseError,{}}; continue; }
                if (plannerAttemptsUnrequestedConversationSwitch(session, action)) {
                    log("[当前会话保护] 已拒绝Planner擅自切换联系人：" + action.value("reason").toString().left(300));
                    session.lastResult = { ToolOutcome::NeedObservation, "用户没有指定新的联系人，不能从聊天列表随机选择。继续定位当前会话的消息输入框，不得切换会话。", {} };
                    session.groundingTarget = "当前会话消息输入框"; session.latestVision = {}; session.actionReady = false; session.visionRequired = true; needVisual = true; continue;
                }
                plannerActionBeforeResolution=action;
                action=resolveSemanticPlannerAction(session, action, &resolutionNote);
            }
            if (!resolutionNote.isEmpty()) {
                Logger::write("ACTION_RESOLVE", QString("task=%1 step=%2\nRAW=%3\nRESOLVED=%4\nNOTE=%5").arg(session.taskId).arg(session.stepNumber)
                    .arg(QString::fromUtf8(QJsonDocument(plannerActionBeforeResolution).toJson(QJsonDocument::Compact)))
                    .arg(QString::fromUtf8(QJsonDocument(action).toJson(QJsonDocument::Compact))).arg(resolutionNote));
                log("[动作解析] " + resolutionNote.left(400));
            }
            if ((action.value("norm_x").isDouble() || action.value("norm_y").isDouble()) && action.value("target_id").toInt(0) <= 0) action.remove("target_id");
            if (session.uiaChecked && !session.uiaUsable) {
                const int blockedTargetId = action.value("target_id").toInt(0);
                if (blockedTargetId > 0) {
                    action.remove("target_id");
                    log(QString("[ACTION_GUARD] UIA不可用，已移除Planner target_id=%1").arg(blockedTargetId));
                    QString recoveredTarget = semanticTargetFromPlannerAction(plannerActionBeforeResolution);
                    if (recoveredTarget.isEmpty()) recoveredTarget = semanticTargetFromPlannerAction(action);
                    if (!recoveredTarget.isEmpty() && !plannerActionBeforeResolution.contains("target")) plannerActionBeforeResolution["target"] = recoveredTarget;
                    if (!recoveredTarget.isEmpty() && !action.contains("target")) action["target"] = recoveredTarget;
                }
            }
            QString semanticTarget=plannerActionBeforeResolution.value("target").toString().trimmed();
            if (semanticTarget.isEmpty()) semanticTarget = action.value("target").toString().trimmed();
            if (semanticTarget.isEmpty()) semanticTarget = semanticTargetFromPlannerAction(plannerActionBeforeResolution);
            const QString unresolvedName=action.value("action").toString().trimmed().toLower();
            const bool coordinateAction=unresolvedName=="mouse_click"||unresolvedName=="mouse_move";
            const bool coordinateReady=action.value("target_id").toInt()>0||(action.value("norm_x").isDouble()&&action.value("norm_y").isDouble())||(action.value("x").isDouble()&&action.value("y").isDouble());
            if(coordinateAction&&!coordinateReady&&!semanticTarget.isEmpty()){
                session.pendingSemanticAction=plannerActionBeforeResolution;session.groundingTarget=semanticTarget.left(160);++session.semanticGroundingAttempts;
                const int groundingLimit=qBound(1,agentRoot().value("visual_grounding").toObject().value("max_semantic_target_attempts").toInt(3),5);
                if(session.semanticGroundingAttempts>groundingLimit){
                    if(session.pendingCommitRequired && semanticRole(semanticTarget)=="commit"){
                        QString fallbackNote;
                        QJsonObject fallback=pendingCommitEnterFallback(session,&fallbackNote);
                        if(!fallback.isEmpty()){ log("[动作事务] "+fallbackNote); action=fallback; session.pendingSemanticAction={}; session.groundingTarget.clear(); session.semanticGroundingAttempts=0; }
                        else{ session.workflow.failures++; session.lastResult={ToolOutcome::RetryableFailure,"pending_commit连续定位提交控件失败，且当前环境没有安全Enter fallback。",{}}; continue; }
                    }else{ session.workflow.failures++; session.lastResult={ToolOutcome::RetryableFailure,"语义目标“"+semanticTarget+"”连续定位失败，已超过grounding上限。",{}}; session.pendingSemanticAction={}; session.groundingTarget.clear(); session.semanticGroundingAttempts=0; continue; }
                }
                log(QString("[GROUNDING] 当前缺少目标“%1”的可执行坐标，进入NeedGrounding，第%2次。").arg(semanticTarget).arg(session.semanticGroundingAttempts));
                session.latestVision={};session.actionReady=false;needVisual=true;session.lastResult={ToolOutcome::NeedObservation,"需要定位语义目标："+semanticTarget,{}};continue;
            }
            if (action.value("action").toString().trimmed().compare("keyboard_type", Qt::CaseInsensitive) == 0) {
                QString textCompleteError;
                if (!completeKeyboardTypeTextIfNeeded(session, action, &textCompleteError)) {
                    session.workflow.failures++; session.lastResult = { ToolOutcome::RetryableFailure, "无法补全输入文字：" + textCompleteError, {} };
                    Logger::write("TEXT_COMPOSER_ERROR", QString("task=%1 step=%2 error=%3").arg(session.taskId).arg(session.stepNumber).arg(textCompleteError));
                    continue;
                }
            }
            action=normalizeAction(action,&parseError);
            if(action.isEmpty()){
                session.workflow.failures++;
                session.lastResult={ToolOutcome::RetryableFailure,"动作解析/校验失败："+parseError,{}};
                Logger::write("ACTION_RESOLVE_ERROR", QString("task=%1 step=%2 error=%3 raw=%4").arg(session.taskId).arg(session.stepNumber).arg(parseError).arg(QString::fromUtf8(QJsonDocument(plannerActionBeforeResolution).toJson(QJsonDocument::Compact))));
                continue;
            }
            QString name=action.value("action").toString();
            if(session.restored&&name!="uia_scan"&&name!="vision_analyze"&&name!="screen_capture"){
                if (session.uiaUsable) scanAgentUia(session,nullptr);
                session.latestVision={}; needVisual=true; session.restored=false; continue;
            }
            session.restored=false;
            const QString sideEffectKey = sideEffectActionKey(session, action);
            if (!session.pendingCommitRequired && !sideEffectKey.isEmpty() && sideEffectKey == session.lastSideEffectActionKey) {
                ++session.repeatedSideEffectBlocks;
                log("[进度纠偏] 拒绝立即重复上一成功动作：" + sideEffectKey.left(220) + "；强制重新观察并规划下一步。");
                session.nextObjective = "上一副作用动作已经成功执行；根据最新界面选择不同的下一步，不要重复该动作。";
                session.latestVision = {}; session.actionReady = false; needVisual = true;
                session.lastResult = { ToolOutcome::NeedObservation, "上一相同副作用动作已经成功，必须重新观察后选择下一步。", {} };
                continue;
            }
            const QString before=observationFingerprint(session);
            session.pendingAction=action;persistContextSummary(session);
            const bool targetedGroundingObservation = !session.groundingTarget.trimmed().isEmpty() && (name=="vision_analyze" || name=="screen_capture");
            if (targetedGroundingObservation) log("[GROUNDING] ACTION_READY下允许targeted grounding：" + session.groundingTarget.left(120));
            const bool genericObservationRequest = name=="vision_analyze" || name=="screen_capture" || name=="uia_scan" || name=="mcp_describe";
            if (session.actionReady && !targetedGroundingObservation && !session.pendingCommitRequired && genericObservationRequest) {
                const QString interactionState = interactionTurnStateName(session);
                if (conversationLikeSurface(session) && (interactionState == "READY_TO_INITIATE" || interactionState == "READY_TO_RESPOND")) {
                    session.lastResult = { ToolOutcome::RetryableFailure, interactionState == "READY_TO_INITIATE" ? "当前是交流任务，尚未成功发送任何消息。请直接生成一句自然口语并使用keyboard_type，不要扫描UIA、不要重新观察、不要wait。" : "检测到新的外部交流内容。请根据LATEST_RELEVANT_TEXT生成自然口语回复并使用keyboard_type，不要wait。", {} };
                    session.nextObjective = interactionState == "READY_TO_INITIATE" ? "生成一句自然口语并keyboard_type" : "根据最新外部内容生成自然回复并keyboard_type";
                    session.actionReady = true; session.waitingForExternalChange = false; session.workflow.phase = "observe";
                    log("[交互轮次] " + interactionState + "：拒绝提前等待，要求先完成一次自然语言输入。");
                    continue;
                }
                if (session.request.continuous && agentRoot().value("generic_policy").toObject().value("continuous_observation_to_wait").toBool(true) && (!conversationLikeSurface(session) || session.hasSentInteraction)) {
                    action=QJsonObject{{"action","wait_for_change"},{"reason","当前连续任务已经完成本轮有效外部操作，等待真实世界变化后再重新规划。"}};
                    name="wait_for_change"; session.waitingForExternalChange=true; session.policyPhase=AgentPolicyPhase::WaitForChange; session.workflow.phase="waiting";
                    session.plannerObservationViolations=0; session.genericNoProgressCount=0;
                    log("[策略转换] 已完成本轮交互，进入WAIT_FOR_CHANGE。");
                }else{
                    ++session.plannerObservationViolations; ++session.genericNoProgressCount;
                    log(QString("[规划纠偏] one-shot ACTION_READY下拒绝无目标重复观察：%1；违规=%2").arg(name).arg(session.plannerObservationViolations));
                    session.lastResult={ToolOutcome::RetryableFailure,"当前观察已经提供可执行目标；one-shot任务必须执行实际动作、done或明确wait。",{}};
                    const int violationLimit=qBound(1,agentRoot().value("generic_policy").toObject().value("max_planner_observation_violations").toInt(4),8);
                    if(session.plannerObservationViolations >= violationLimit){ message="Planner在one-shot任务中持续忽略已有动作证据并重复观察，任务已暂停以防真正死循环。"; log("[通用看门狗] "+message); break; }
                    continue;
                }
            }
            if (name.startsWith("uia_") && session.scope == AgentScope::SelectedWindow && session.uiaChecked && !session.uiaUsable) {
                const QString interactionState = interactionTurnStateName(session);
                log("[规划纠偏] Planner请求已锁死的UIA动作，拒绝执行：" + name);
                if (conversationLikeSurface(session) && (interactionState == "READY_TO_INITIATE" || interactionState == "READY_TO_RESPOND")) {
                    session.lastResult = { ToolOutcome::RetryableFailure, "UIA不可用，而且当前交流界面已经有视觉证据。不要再请求UIA；直接使用keyboard_type生成自然口语。", {} };
                    session.nextObjective = interactionState == "READY_TO_INITIATE" ? "主动生成一句自然口语并输入" : "根据最新内容生成一句自然回复并输入";
                    needVisual = false; session.actionReady = true; continue;
                }
                session.latestVision = {}; session.visionRequired = true; needVisual = true;
                session.lastResult = { ToolOutcome::NeedObservation, "UIA已在本任务内锁定不可用，必须使用VisionMouse。", {} };
                continue;
            }
            if (session.uiaChecked && !session.uiaUsable && action.value("target_id").toInt(0) > 0) {
                log("[ACTION_GUARD] 拒绝执行：UIA不可用但动作仍携带target_id。");
                session.lastResult = { ToolOutcome::NeedObservation, "UIA不可用，target_id已失效，必须重新Grounding。", {} };
                session.pendingSemanticAction = plannerActionBeforeResolution;
                session.groundingTarget = semanticTargetFromPlannerAction(plannerActionBeforeResolution);
                session.latestVision = {}; session.actionReady = false; needVisual = true; continue;
            }
            bool ok=false;
            const QString toolResult= executeActionWorker(session,action,&ok);
            if (ok && name=="keyboard_type") armPendingCommitAfterText(session, action);
            finishPendingCommitIfNeeded(session, name, ok);
            if (ok) {
                recordSuccessfulProgress(session, action, toolResult);
                const QString successKey = sideEffectActionKey(session, action);
                if (!successKey.isEmpty()) { session.lastSideEffectActionKey = successKey; session.repeatedSideEffectBlocks = 0; }
                if (hasResumedSemanticAction) { session.pendingSemanticAction = {}; session.groundingTarget.clear(); session.semanticGroundingAttempts = 0; log("[GROUNDING] 待执行语义动作已完成，清除grounding状态。"); }
            }
            Logger::write("ACTION_RESULT",QString("task=%1 step=%2 ok=%3\n%4\n%5").arg(session.taskId).arg(session.stepNumber).arg(ok).arg(QString::fromUtf8(QJsonDocument(action).toJson(QJsonDocument::Compact)),toolResult));
            log("["+name+"] "+toolResult.left(700));
            if(session.lastResult.outcome==ToolOutcome::UserCancelled){session.cancelledByUser=true;message="用户已取消任务，不再尝试其他工具。";break;}
            if(session.lastResult.outcome==ToolOutcome::FatalFailure){message=toolResult;break;}
            if(cancelled())break;
            QJsonObject verification;
            if(name=="done"&&session.request.kind==TaskKind::OpenApplication){
                if(ApplicationLauncher::existing(AppRegistry::lookup(session.request.application))){finished=true;message="目标应用窗口已经打开。";break;}
                message="未能确认目标应用窗口出现，不能报告打开成功。";break;
            }
            if(name=="done"){
                if (session.uiaUsable) scanAgentUia(session,nullptr);
                verification=verifyAgentActionWorker(session,goal,action,ok,toolResult);
                if(verification.value("goal_success").toBool(false)){
                    if(session.request.continuous){session.workflow.workSteps=0;session.workflow.phase="waiting";log("[持续任务] 当前一轮已确认，等待新的消息或界面变化。");waitForChange(session);needVisual=true;session.latestVision={};}
                    else{finished=true;message=verification.value("user_message").toString("任务已完成。");appendRecentHistory(session,action,ok,toolResult,verification);break;}
                }else{
                    if(++unverifiedDone>=maxFailures){message="缺少可核对的完成证据，任务已暂停："+verification.value("reason").toString();break;}
                    needVisual=true;session.latestVision={};
                }
            }else{
                verification=verifyAgentActionWorker(session,goal,action,ok,toolResult);
                if(name!="wait"&&name!="wait_for_change"&&name!="uia_scan"&&name!="screen_capture"&&name!="vision_analyze"){
                    session.actionReady = false; session.plannerObservationViolations = 0; session.genericNoProgressCount = 0;
                    session.consecutiveVisionObservations = 0; session.consecutiveGroundingFailures = 0; session.groundingRequested = false;
                    session.latestVision={};
                    if (session.uiaUsable) scanAgentUia(session,nullptr);
                    session.workflow.workSteps++;
                    const int refreshedActionable = session.uiaUsable ? int(std::count_if(session.uiaItems.cbegin(), session.uiaItems.cend(), [](const UIAItem& item){ return item.clickable; })) : 0;
                    if (session.request.kind == TaskKind::CreativeWorkflow) needVisual = session.preferVision || name == "mouse_path" || refreshedActionable == 0;
                    else if (session.request.kind == TaskKind::CustomerService) needVisual = session.preferVision || refreshedActionable == 0;
                    else if (session.request.kind == TaskKind::DocumentWorkflow) needVisual = session.preferVision || refreshedActionable == 0;
                    else needVisual = session.preferVision || refreshedActionable == 0;
                    updateGenericProgressWatchdog(session);
                    if(ok)unverifiedDone=0;
                }
                if(!ok&&name.startsWith("uia_")){session.preferVision=true;needVisual=true;session.latestVision={};session.lastResult.outcome=ToolOutcome::NeedObservation;}
                if(!ok&&session.lastResult.outcome==ToolOutcome::NeedObservation){needVisual=true;session.latestVision={};}
                if(name=="wait"||name=="wait_for_change"){session.workflow.workSteps=0;session.latestVision={};needVisual=actionable==0;}
            }
            session.workflow.record(QString::fromUtf8(QJsonDocument(action).toJson(QJsonDocument::Compact)),before,session.lastResult);
            appendRecentHistory(session,action,ok,toolResult,verification);
            if(session.workflow.repetitions>maxRepeat&&name!="wait"&&name!="wait_for_change"){
                if(session.request.continuous){waitForChange(session);session.workflow.repetitions=0;session.latestVision={};needVisual=true;}
                else{message="同一状态下重复同一动作，已暂停，避免盲目重复操作。";break;}
            }
            if(session.workflow.phase=="waiting")session.workflow.workSteps=0;
            const int delay=qBound(0,agentCore().value("step_delay_ms").toInt(150),2000);QElapsedTimer timer;timer.start();while(timer.elapsed()<delay&&!cancelled())QThread::msleep(20);
        }
        if(cancelled()&&message.isEmpty())message=m_userInterrupted?"检测到真人接管，自动操作已停止。":"任务已停止。";
        finish();
    }

    void finishAgentTaskUi(AgentScope scope) {
        postUi([this, scope] {
            m_controlActive = false; m_running = false; m_userInterrupted = false; m_userInputMonitor.resetActivity();
            if (scope == AgentScope::Desktop && m_desktopRefreshTimer && m_workMode == WorkMode::DesktopControl) {
                const int interval = qMax(3000, desktopCfg().value("idle_uia_refresh_interval_ms").toInt(10000));
                m_desktopRefreshTimer->start(interval);
                log(QString("[观察] Agent结束，桌面UIA覆盖层恢复%1ms空闲刷新。").arg(interval));
            }
            setAppState(AppState::Ready, "[任务] 任务执行结束");
        });
    }

    void shutdown() {
        if (m_shutdownStarted.exchange(true)) return;
        m_acceptNewTasks = false; m_stop = true;
        RuntimeSafety::instance().cancel("程序退出，取消模型请求");
        setEnabled(false);
        auto elapsed = std::make_shared<QElapsedTimer>(); elapsed->start();
        auto* timer = new QTimer(this); timer->setInterval(50);
        connect(timer, &QTimer::timeout, this, [this, timer, elapsed] {
            if (!m_running && !m_passiveScanInFlight && RuntimeSafety::instance().modelOperations.load() == 0 && m_state != AppState::LoadingModel && m_state != AppState::Generating) {
                timer->stop(); timer->deleteLater(); finishShutdown(); return;
            }
            if (elapsed->elapsed() >= 30000) {
                Logger::write("EXIT", "退出等待超过30秒，结束本程序进程；Windows Job负责清理所属模型服务。保留异常退出记录。");
                TerminateProcess(GetCurrentProcess(), 91);
            }
        });
        timer->start();
    }

    void finishShutdown() {
        setEnabled(false);
        log("[退出] 1/10 停止接收新任务...");
        log("[退出] 2/10 停止当前Agent与持续任务...");
        if (m_desktopRefreshTimer) m_desktopRefreshTimer->stop();
        if (m_windowRectTimer) m_windowRectTimer->stop();
        if (m_windowUiaTimer) m_windowUiaTimer->stop();
        if (m_pickTimer) m_pickTimer->stop();
        m_userInputMonitor.stop();
        if (m_desktopOverlay) m_desktopOverlay->hide();
        if (m_windowOverlay) m_windowOverlay->hide();
        QApplication::processEvents();
        const QJsonObject shutdownCfg = agentRoot().value("shutdown").toObject();
        const int gracefulTimeout = qMax(2000, shutdownCfg.value("graceful_timeout_ms").toInt(8000));
        log("[退出] 3/10 等待Agent Worker结束...");
        const bool agentStopped = m_agentExecutor.stop(gracefulTimeout);
        log(agentStopped ? "[退出] Agent Worker 已退出" : "[退出] Agent Worker 超时，将由进程级兜底结束");
        bool modelStopped = false; bool uiaStopped = false;
        if (agentStopped) {
            log("[退出] 4/10 停止LLM/VLM请求与模型服务...");
            if (m_modelExecutor.isRunning()) m_modelExecutor.call([this] { if (m_models) { m_models->stopAll(); delete m_models; m_models = nullptr; } });
            modelStopped = m_modelExecutor.stop(8000);
            log(modelStopped ? "[退出] LLM/VLM服务与Model Worker已退出" : "[退出] Model Worker退出超时");
            log("[退出] 5/10 停止UIA扫描与UIA Worker...");
            if (m_uiaExecutor.isRunning()) m_uiaExecutor.call([this] { delete m_agentUia; m_agentUia = nullptr; delete m_overlayUia; m_overlayUia = nullptr; if (SUCCEEDED(m_uiaWorkerComResult)) CoUninitialize(); m_uiaWorkerComResult = E_FAIL; });
            uiaStopped = m_uiaExecutor.stop(5000);
            log(uiaStopped ? "[退出] UIAutomationEngine与UIA Worker已退出" : "[退出] UIA Worker退出超时");
        } else log("[退出] Agent仍在运行，为避免跨线程释放正在使用的Model/UIA对象，" "跳过对象强制销毁，交由PID兜底安全结束进程。");
        log("[退出] 6/10 等待IO Worker结束...");
        const bool ioStopped = m_ioExecutor.stop(3000);
        log(ioStopped ? "[退出] IO Worker已退出" : "[退出] IO Worker退出超时");
        log("[退出] 7/10 关闭SQLite / wood.db...");
        if (QSqlDatabase::contains("ceh")) { { QSqlDatabase db = QSqlDatabase::database("ceh"); if (db.isValid() && db.isOpen()) db.close(); } log("[退出] SQLite连接已关闭"); } else log("[退出] SQLite连接不存在，无需关闭");
        log("[退出] 8/10 关闭Overlay与系统托盘...");
        if (m_desktopOverlay) m_desktopOverlay->hide();
        if (m_windowOverlay) m_windowOverlay->hide();
        if (m_tray) { m_tray->hide(); m_tray->setContextMenu(nullptr); }
        log("[退出] 9/10 释放界面与运行资源...");
        QApplication::processEvents();
        Logger::write("EXIT", QString("Worker状态：agent=%1 model=%2 uia=%3 io=%4").arg(agentStopped ? "stopped" : "timeout").arg(modelStopped ? "stopped" : "timeout/skipped").arg(uiaStopped ? "stopped" : "timeout/skipped").arg(ioStopped ? "stopped" : "timeout"));
        log("[退出] 10/10 程序关闭...");
        const DWORD pid = GetCurrentProcessId();
        const int fallbackDelay = qMax(1, shutdownCfg.value("force_kill_fallback_delay_ms").toInt(3000) / 1000);
        const QString fallback = QString("timeout /T %1 /NOBREAK >nul " "& taskkill /F /PID %2 >nul 2>nul").arg(fallbackDelay).arg(pid);
        QProcess::startDetached("cmd.exe", {"/C", fallback});
        Logger::write("EXIT", "程序退出请求已提交");
        QApplication::quit();
    }
};

static void loadFont(QApplication& app) {
    const int lightId = QFontDatabase::addApplicationFont(":/fonts/assets/font/light.ttf");
    if (lightId >= 0) {
        const QStringList families = QFontDatabase::applicationFontFamilies(lightId);
        if (!families.isEmpty()) { g_lightFontFamily = families.first(); Logger::write("SYSTEM","正文字体加载成功：" + g_lightFontFamily); }
    } else Logger::write("WARN","正文字体 light.ttf 加载失败，使用 Microsoft YaHei UI");
    const int boldId = QFontDatabase::addApplicationFont(":/fonts/assets/font/bold.ttf");
    if (boldId >= 0) {
        const QStringList families = QFontDatabase::applicationFontFamilies(boldId);
        if (!families.isEmpty()) { g_boldFontFamily = families.first(); Logger::write("SYSTEM","粗体字体加载成功：" + g_boldFontFamily); }
    } else { g_boldFontFamily = g_lightFontFamily; Logger::write("WARN","粗体 bold.ttf 加载失败，回退正文字体"); }
    QFont normalFont(g_lightFontFamily); normalFont.setPixelSize(12); normalFont.setWeight(QFont::Normal);
    app.setFont(normalFont);
}

static void initWindows() { SetCurrentProcessExplicitAppUserModelID(L"CEH.Windows_Uia.Ai.V1"); }

int main(int argc, char* argv[]) {
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);
    QCoreApplication::setApplicationName(QString::fromUtf8(CEH_APP_NAME));
    QCoreApplication::setApplicationVersion(QString::fromUtf8(CEH_APP_VERSION));
    QCoreApplication::setOrganizationName("木头智能科技");
    initWindows();
    Logger::beginRun();
    RuntimeSafety::instance().start();
    Logger::write("SYSTEM", "[启动] 程序进程已创建");
    Logger::write("SYSTEM", "[启动] 正在检查运行目录与配置文件...");
    ensureApplicationFiles();
    Logger::write("SYSTEM", "[启动] 配置与资源检查完成");
    Logger::write("SYSTEM", "[启动] 正在加载应用字体...");
    loadFont(app);
    Logger::write("SYSTEM", "[启动] 字体加载阶段完成");
    Logger::write("SYSTEM", "[启动] 正在初始化SQLite / wood.db...");
    const bool databaseReady = initDatabase();
    Logger::write(databaseReady ? "SYSTEM" : "ERROR", databaseReady ? "[启动] SQLite / wood.db 初始化成功" : "[启动] SQLite / wood.db 初始化失败");
    Logger::write("SYSTEM", "运行目录：" + appDir());
    Logger::write("SYSTEM", "源码目录：" + sourceRoot());
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [] { Logger::write("SYSTEM", "[退出] Qt事件循环准备退出"); });
    MainWindow window;
    const QJsonObject config = loadConfig("assets/config/app.json");
    const QJsonObject windowConfig = config.value("window").toObject();
    window.resize(windowConfig.value("width").toInt(500), windowConfig.value("height").toInt(1000));
    if (windowConfig.value("start_hidden").toBool(false)) window.hide();
    else window.show();
    Logger::write("SYSTEM", "[启动] 主窗口创建成功，进入Qt事件循环");
    const int result = app.exec();
    Logger::write("SYSTEM", "[退出] Qt事件循环已结束");
    if (SUCCEEDED(comResult)) CoUninitialize();
    Logger::write("SYSTEM", "[退出] 主线程COM已释放，进程返回");
    RuntimeSafety::instance().stop();
    Logger::endRun();
    return result;
}
