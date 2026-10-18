#include <QApplication>
#include <QFile>
#include <QDebug>
#include <QDir>
#include <QLocale>
#include <QEvent>
#include <QAbstractSpinBox>
#include <QDateTimeEdit>
#include <memory>
#include <QFontDatabase>
#include "src/core/DatabaseManager.h"
#include "src/core/SecurityManager.h"
#include "src/ai/OllamaHttpEngine.h"
#include "src/ai/AiAdvisorService.h"
#include "src/ui/MainWindow.h"
#include "src/ui/LoginWindow.h"
#include "src/ui/CustomDialog.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdlib>
namespace WoodPlatform {
    void initializeWindowsPlatform();
}
#endif

namespace {
class GlobalWidgetNormalizer : public QObject {
public:
    using QObject::QObject;
protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::Show) {
            if (auto* spin = qobject_cast<QAbstractSpinBox*>(watched)) {
                if (spin->locale().language() != QLocale::C) {
                    spin->setLocale(QLocale::c());
                }
            }
        }
        return QObject::eventFilter(watched, event);
    }
};
} // namespace

int main(int argc, char *argv[]) {
#ifdef _WIN32
    WoodPlatform::initializeWindowsPlatform();
    // 启动前强杀所有残留的 llama-server.exe 孤儿进程，确保 18089 端口与文件句柄绝对干净释放（无窗口静默执行）
    OllamaHttpEngine::killLlamaServerSilently();
#endif

    // 全局强制阿拉伯数字，彻底解决 Windows 苏州码子数字乱码
    QLocale::setDefault(QLocale::c());

    QApplication app(argc, argv);
    // 强制将工作目录锁定为当前 exe 所在安装目录（如 cmake-build-release 或客户安装目录）
    QDir::setCurrent(QCoreApplication::applicationDirPath());

    app.installEventFilter(new GlobalWidgetNormalizer(&app));
    app.setApplicationName("云萍录");
    app.setApplicationDisplayName("云萍录");
    app.setOrganizationName("木头智能科技");

    // 设置全局窗口与任务栏多尺寸图标 (优先读取 <获取的安装地址>/assets/img/icon)
    QIcon appIcon;
    const QStringList relIconPaths = {
        "assets/img/icon/min_logo.png",
        "assets/img/icon/favicon_128x128.ico",
        "assets/img/icon/favicon_64x64.ico",
        "assets/img/icon/favicon_32x32.ico",
        "assets/img/icon/logo.png"
    };
    for (const QString& relPath : relIconPaths) {
        QString resolved = DatabaseManager::instance().resolveAssetPath(relPath);
        if (QFile::exists(resolved)) {
            appIcon.addFile(resolved);
        }
    }
    if (appIcon.isNull()) {
        const QStringList qrcIconPaths = {
            ":/icons/assets/img/icon/min_logo.png",
            ":/icons/assets/img/icon/favicon_128x128.ico",
            ":/icons/assets/img/icon/favicon_64x64.ico",
            ":/icons/assets/img/icon/favicon_32x32.ico",
            ":/icons/assets/img/icon/logo.png"
        };
        for (const QString& p : qrcIconPaths) {
            if (QFile::exists(p)) {
                appIcon.addFile(p);
            }
        }
    }
    if (!appIcon.isNull()) {
        app.setWindowIcon(appIcon);
    }

    QFont font("Microsoft YaHei UI", 10);
    font.setStyleStrategy(QFont::PreferAntialias);
    app.setFont(font);

    // 1. 加载全局深色样式表 (优先使用 <获取的安装地址>/assets/style/main.qss)
    QString qssPath = DatabaseManager::instance().resolveAssetPath("assets/style/main.qss");
    QFile qssFile(qssPath);
    if (!qssFile.exists()) {
        qssPath = ":/styles/assets/style/main.qss";
        qssFile.setFileName(qssPath);
    }
    if (qssFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(qssFile.readAll());
        qssFile.close();
    } else {
        qWarning() << "[UI] Failed to load stylesheet:" << qssPath;
    }

    // 2. 初始化本地加密数据库容器 (严格使用 <获取的安装地址>/assets/db/wood.db，未输入密码前保持锁定密文状态)
    if (!DatabaseManager::instance().initialize("wood.db")) {
        qCritical() << "[Main] Failed to initialize database!";
    }

    auto cleanupAll = []() {
        AiAdvisorService::instance().shutdown();
        OllamaHttpEngine::shutdownLocalServer();
        DatabaseManager::instance().close();
#ifdef _WIN32
        OllamaHttpEngine::killLlamaServerSilently();
#endif
    };
    QObject::connect(&app, &QCoreApplication::aboutToQuit, cleanupAll);

    // 3. 极密安全品牌身份门禁
    LoginWindow loginDlg;
    if (!appIcon.isNull()) loginDlg.setWindowIcon(appIcon);
    if (loginDlg.exec() != QDialog::Accepted) {
        cleanupAll();
#ifdef _WIN32
        ::TerminateProcess(::GetCurrentProcess(), 0);
#else
        return 0;
#endif
    }

    // 4. 装配模型推理引擎 (支持本地 GGUF llama.cpp 驱动 或 远程/本地模型服务接口)
    AiAdvisorService::instance().setEngine(OllamaHttpEngine::createFromConfig());

    // 5. 显示主窗口
    app.setQuitOnLastWindowClosed(true);
    int exitCode = 0;
    {
        MainWindow window;
        if (!appIcon.isNull()) window.setWindowIcon(appIcon);
        window.show();
        exitCode = app.exec();
    }

    cleanupAll();
#ifdef _WIN32
    ::TerminateProcess(::GetCurrentProcess(), 0);
#else
    return exitCode;
#endif
}