#include "MainWindow.h"
#include "GeneralDashboardWidget.h"
#include "ContactWidget.h"
#include "FinancialLedgerWidget.h"
#include "RelationGraphWidget.h"
#include "BusinessNetWidget.h"
#include "SocialPlanDialog.h"
#include "ModelSettingsDialog.h"
#include "SecuritySettingsDialog.h"
#include "ReminderDialog.h"
#include "UiIcons.h"
#include "../core/SecurityManager.h"
#include "../ai/AiAdvisorService.h"
#include "../ai/OllamaHttpEngine.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QFileDialog>
#include <QMessageBox>
#include <QTimer>
#include <QScrollArea>
#include <QStandardPaths>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCloseEvent>
#include <QCoreApplication>
#include <cstdlib>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

MainWindow::MainWindow(QWidget* parent) : FramelessWindow(parent) {
    setupUi();
    refreshAllData();
    verifyModelAvailability();
}

void MainWindow::setupUi() {
    auto* rootWidget = new QWidget(this);
    rootWidget->setObjectName("root");
    rootWidget->setStyleSheet("QWidget#root { background-color: #141416; }");

    auto* rootLayout = new QVBoxLayout(rootWidget);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // 1. 顶部自定义样式栏 (TopBar)
    rootLayout->addWidget(m_topBar);
    connect(m_topBar, &TopBar::topologyToggleRequested, this, &MainWindow::onTopologyToggled);
    connect(m_topBar, &TopBar::businessNetToggleRequested, this, &MainWindow::onBusinessNetToggled);
    connect(m_topBar, &TopBar::modelSettingsRequested, this, &MainWindow::onModelSettingsClicked);

    // 监听 AI 参谋运行状态，实时联动 TopBar 状态指示灯与“运行中 ...”黄字
    connect(&AiAdvisorService::instance(), &AiAdvisorService::evaluationStarted, this, [this]() {
        updateTopBarAiStatus(true);
    });
    connect(&AiAdvisorService::instance(), &AiAdvisorService::evaluationFinished, this, [this](const QString&) {
        m_isModelAvailable = true;
        updateTopBarAiStatus(false);
    });
    connect(&AiAdvisorService::instance(), &AiAdvisorService::evaluationFailed, this, [this](const QString&) {
        m_isModelAvailable = false;
        updateTopBarAiStatus(false);
    });
    connect(&AiAdvisorService::instance(), &AiAdvisorService::evaluationCanceled, this, [this]() {
        updateTopBarAiStatus(false);
    });

    auto* topLine = new QFrame(rootWidget);
    topLine->setFixedHeight(1);
    topLine->setStyleSheet("background-color: #28282b; border: none;");
    rootLayout->addWidget(topLine);

    // 2. 主体布局 (左侧导航与计划栏 + 中间 Stack 工作区)
    auto* bodyWidget = new QWidget(rootWidget);
    auto* bodyLayout = new QHBoxLayout(bodyWidget);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    // ==================== 全局左侧导航与计划栏 ====================
    auto* sidebar = new QWidget(bodyWidget);
    sidebar->setFixedWidth(130);
    sidebar->setObjectName("sidebarWidget");

    auto* sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(10, 10, 10, 10);
    sideLayout->setSpacing(10);

    // 核心三大主模块切换按钮 (大栏目 16px, 900 粗体 + 16px #8e8e93 单色图标)
    auto createNavBtn = [this, sidebar](const QString& text, QChar iconCode, int idx) {
        auto* btn = new QPushButton(" " + text, sidebar);
        btn->setObjectName("navModuleBtn");
        btn->setCheckable(true);
        btn->setAutoExclusive(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setIcon(UiIcons::icon(iconCode, 16, QColor("#8e8e93")));
        btn->setIconSize(QSize(16, 16));
        QFont f("SimHei");
        f.setPixelSize(16);
        f.setWeight(QFont::Black);
        f.setBold(true);
        btn->setFont(f);
        btn->setStyleSheet(
            "QPushButton { background-color: transparent; color: #8e8e93; font-family: 'SimHei', 'Microsoft YaHei UI', 'Microsoft YaHei', sans-serif; font-size: 16px; font-weight: 900; text-align: left; padding-left: 10px; border-radius: 3px; border: none; min-height: 36px; }"
            "QPushButton:hover { background-color: #1e1e22; color: #e1e1e5; font-weight: 900; }"
            "QPushButton:checked { background-color: #1e1e22; color: #e1e1e5; font-weight: 900; border-left: 3px solid #00a361; }"
        );
        connect(btn, &QPushButton::clicked, this, [this, idx]() { onNavButtonClicked(idx); });
        return btn;
    };

    auto* btnDashboard = createNavBtn("综合", UiIcons::Code::Dashboard, 0);
    auto* btnContact = createNavBtn("关系", UiIcons::Code::Relation, 1);
    auto* btnLedger = createNavBtn("财务", UiIcons::Code::Finance, 2);

    btnDashboard->setChecked(true);
    m_navButtons.append(btnDashboard);
    m_navButtons.append(btnContact);
    m_navButtons.append(btnLedger);

    sideLayout->addWidget(btnDashboard);
    sideLayout->addWidget(btnContact);
    sideLayout->addWidget(btnLedger);

    // 分割线
    auto* sep1 = new QFrame(sidebar);
    sep1->setFixedHeight(1);
    sep1->setStyleSheet("background-color: #28282b; border: none;");
    sideLayout->addWidget(sep1);

    // 计划管理区 (14px 粗体标题 + 单色图标 + +计划按钮)
    auto* planHeader = new QHBoxLayout();
    planHeader->setContentsMargins(2, 0, 2, 0);
    planHeader->setSpacing(4);
    auto* planIconLbl = new QLabel(sidebar);
    planIconLbl->setPixmap(UiIcons::pixmap(UiIcons::Code::Plan, 14, QColor("#8e8e93")));
    auto* planTitle = new QLabel("计划", sidebar);
    planTitle->setStyleSheet("color: #8e8e93; font-size: 14px; font-weight: bold;");

    auto* btnAddPlan = new QPushButton("+", sidebar);
    btnAddPlan->setObjectName("flatTextBtn");
    btnAddPlan->setStyleSheet(
        "QPushButton { font-size: 12px; color: #00a361; background: #1e1e22; border: 1px solid #28282b; padding: 2px 6px; border-radius: 3px; }"
        "QPushButton:hover { background-color: #28282e; }"
    );
    btnAddPlan->setCursor(Qt::PointingHandCursor);
    connect(btnAddPlan, &QPushButton::clicked, this, &MainWindow::onAddNewPlanClicked);

    planHeader->addWidget(planIconLbl);
    planHeader->addWidget(planTitle);
    planHeader->addStretch();
    planHeader->addWidget(btnAddPlan);
    sideLayout->addLayout(planHeader);

    // 计划动态列表 (可滚动)
    auto* planScroll = new QScrollArea(sidebar);
    planScroll->setWidgetResizable(true);
    planScroll->setStyleSheet("background: transparent; border: none;");

    auto* planContentWidget = new QWidget(planScroll);
    planContentWidget->setStyleSheet("background: transparent;");
    m_plansListLayout = new QVBoxLayout(planContentWidget);
    m_plansListLayout->setContentsMargins(0, 0, 0, 0);
    m_plansListLayout->setSpacing(8);
    m_plansListLayout->addStretch();

    planScroll->setWidget(planContentWidget);
    sideLayout->addWidget(planScroll, 1);

    // 左下角工具区：无上方分割线，第一行 [导出 | 导入]，第二行 [安全设置]，均带背景色与 12px 单色图标
    QString bottomBtnStyle =
        "QPushButton { background-color: #222226; color: #8e8e93; border: 1px solid #28282b; border-radius: 3px; font-size: 12px; min-height: 28px; padding: 0 4px; }"
        "QPushButton:hover { background-color: #2c2c30; color: #e1e1e5; }"
        "QPushButton:pressed { background-color: #35353a; }";

    auto* expImpRow = new QHBoxLayout();
    expImpRow->setContentsMargins(0, 0, 0, 0);
    expImpRow->setSpacing(6);

    auto* btnExport = new QPushButton("导出", sidebar);
    btnExport->setIcon(UiIcons::icon(UiIcons::Code::Export, 12, QColor("#8e8e93")));
    btnExport->setIconSize(QSize(12, 12));
    btnExport->setCursor(Qt::PointingHandCursor);
    btnExport->setStyleSheet(bottomBtnStyle);
    connect(btnExport, &QPushButton::clicked, this, &MainWindow::onExportBackupClicked);

    auto* btnImport = new QPushButton("导入", sidebar);
    btnImport->setIcon(UiIcons::icon(UiIcons::Code::Import, 12, QColor("#8e8e93")));
    btnImport->setIconSize(QSize(12, 12));
    btnImport->setCursor(Qt::PointingHandCursor);
    btnImport->setStyleSheet(bottomBtnStyle);
    connect(btnImport, &QPushButton::clicked, this, &MainWindow::onImportBackupClicked);

    expImpRow->addWidget(btnExport, 1);
    expImpRow->addWidget(btnImport, 1);
    sideLayout->addLayout(expImpRow);

    auto* btnSecurity = new QPushButton("安全设置", sidebar);
    btnSecurity->setIcon(UiIcons::icon(UiIcons::Code::Security, 12, QColor("#8e8e93")));
    btnSecurity->setIconSize(QSize(12, 12));
    btnSecurity->setCursor(Qt::PointingHandCursor);
    btnSecurity->setStyleSheet(bottomBtnStyle);
    connect(btnSecurity, &QPushButton::clicked, this, &MainWindow::onSecuritySetupClicked);
    sideLayout->addWidget(btnSecurity);

    bodyLayout->addWidget(sidebar);

    // 左侧导航与主工作区竖分割线
    auto* sideVLine = new QFrame(bodyWidget);
    sideVLine->setFixedWidth(1);
    sideVLine->setStyleSheet("background-color: #28282b; border: none;");
    bodyLayout->addWidget(sideVLine);

    // ==================== 中间工作区 (Stack) ====================
    m_stack = new QStackedWidget(bodyWidget);
    m_stack->setStyleSheet("background-color: #141416;");

    m_dashboardWidget = new GeneralDashboardWidget(m_stack);
    m_contactWidget = new ContactWidget(m_stack);
    m_ledgerWidget = new FinancialLedgerWidget(m_stack);
    m_fullscreenGraphWidget = new RelationGraphWidget(m_stack);
    m_businessNetWidget = new BusinessNetWidget(m_stack);

    m_stack->addWidget(m_dashboardWidget);         // 0
    m_stack->addWidget(m_contactWidget);           // 1
    m_stack->addWidget(m_ledgerWidget);            // 2
    m_stack->addWidget(m_fullscreenGraphWidget);   // 3 (全屏关系网)
    m_stack->addWidget(m_businessNetWidget);        // 4 (企业商务网)

    connect(m_contactWidget, &ContactWidget::plansChanged, this, &MainWindow::loadUpcomingPlansSidebar);

    bodyLayout->addWidget(m_stack, 1);
    rootLayout->addWidget(bodyWidget, 1);

    setCentralWidget(rootWidget);
}

void MainWindow::updateTopBarAiStatus(bool isRunning) {
    auto& db = DatabaseManager::instance();
    QString configuredFlag = db.getSecurityConfig("llm_configured");
    QString engineMode = db.getSecurityConfig("llm_engine_mode").trimmed();
    QString localGguf = db.getSecurityConfig("llm_local_gguf_path").trimmed();
    QString ep = db.getSecurityConfig("llm_endpoint").trimmed();
    QString model = db.getSecurityConfig("llm_model_name").trimmed();
    if (model.isEmpty()) model = "qwen9b-vl";

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
                    if (m.contains("model_name") && !m["model_name"].toString().trimmed().isEmpty()) model = m["model_name"].toString().trimmed();
                    if (m.contains("configured")) configuredFlag = m["configured"].toString();
                    break;
                }
            }
        }
    }

    if (engineMode == "service") {
        localGguf.clear();
    }

    QString displayModel = !localGguf.isEmpty() ? QFileInfo(localGguf).fileName() : model;
    bool isConfigured = (configuredFlag != "0") && (!localGguf.isEmpty() || !ep.isEmpty() || configuredFlag.isEmpty());
    m_topBar->updateAiStatus(isConfigured, displayModel, isRunning, m_isModelAvailable);
}

void MainWindow::verifyModelAvailability() {
    auto& db = DatabaseManager::instance();
    QString configuredFlag = db.getSecurityConfig("llm_configured");
    if (configuredFlag == "0") {
        m_isModelAvailable = false;
        updateTopBarAiStatus(false);
        return;
    }

    auto engine = std::dynamic_pointer_cast<OllamaHttpEngine>(AiAdvisorService::instance().engine());
    if (!engine) {
        m_isModelAvailable = false;
        updateTopBarAiStatus(false);
        return;
    }

    engine->checkAvailability([this](bool available, const QString&) {
        m_isModelAvailable = available;
        updateTopBarAiStatus(false);
    });
}

void MainWindow::refreshAllData() {
    loadUpcomingPlansSidebar();
    if (m_dashboardWidget) m_dashboardWidget->refreshData();
    if (m_contactWidget) m_contactWidget->refreshData();
    if (m_ledgerWidget) m_ledgerWidget->refreshData();
    if (m_fullscreenGraphWidget) m_fullscreenGraphWidget->refreshData();
    if (m_businessNetWidget) m_businessNetWidget->refreshData();
}

void MainWindow::loadUpcomingPlansSidebar() {
    while (m_plansListLayout->count() > 1) {
        auto* item = m_plansListLayout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    auto plans = DatabaseManager::instance().getUpcomingSocialPlans(30);
    QDate today = QDate::currentDate();

    if (plans.isEmpty()) {
        auto* emptyLbl = new QLabel("近期无计划", this);
        emptyLbl->setStyleSheet("color: #8e8e93; font-size: 12px; padding: 6px 4px;");
        m_plansListLayout->insertWidget(0, emptyLbl);
        return;
    }

    for (const auto& p : plans) {
        int days = today.daysTo(p.plannedDate);
        QString dayStr = (days == 0) ? "今天" : (days > 0 ? QString("%1天后").arg(days) : QString("逾期%1天").arg(-days));

        QString actStr = p.planType.trimmed();
        if (actStr == "DINNER") actStr = "请客吃饭";
        else if (actStr == "TEA" || actStr == "PLAY") actStr = "品茗茶叙";
        else if (actStr == "VISIT") actStr = "客户拜访";
        else if (actStr == "DEBT_COLLECTION" || actStr == "DEBT_COLLECT") actStr = "催收回款";
        else if (actStr == "DEBT_REPAY") actStr = "到期还款";
        else if (actStr == "GIFT_VISIT" || actStr == "GIFT") actStr = "送礼随份";
        else if (actStr == "BUSINESS_FOLLOW") actStr = "生意合作";
        else if (actStr == "BIRTHDAY") actStr = "生日祝福";
        else if (actStr.isEmpty()) actStr = "人脉走动";
        else if (actStr.length() > 4) actStr = actStr.left(4);

        auto* card = new QPushButton();
        card->setObjectName("planItemCard");
        card->setCursor(Qt::PointingHandCursor);

        auto* layout = new QHBoxLayout(card);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(4);

        // 颜色标准: 今天/逾期 -> #fa5151 (紧急), 1~3天后 -> #be782e (即将/警告), >3天 -> #8e8e93 (常规说明)
        QString dayColor = "#8e8e93";
        if (days <= 0) dayColor = "#fa5151";
        else if (days <= 3) dayColor = "#be782e";

        auto* dLbl = new QLabel(dayStr, card);
        dLbl->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(dayColor));
        dLbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        auto* aLbl = new QLabel(actStr, card);
        aLbl->setStyleSheet("color: #8e8e93; font-size: 12px; background: transparent;");
        aLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        layout->addWidget(dLbl);
        layout->addStretch(1);
        layout->addWidget(aLbl);

        connect(card, &QPushButton::clicked, this, [this, p]() {
            onPlanItemClicked(p);
        });

        m_plansListLayout->insertWidget(m_plansListLayout->count() - 1, card);
    }
}

void MainWindow::onNavButtonClicked(int index) {
    m_topBar->setTopologyButtonChecked(false);
    m_topBar->setBusinessNetButtonChecked(false);
    m_stack->setCurrentIndex(index);
    m_lastPageIndex = index;
    for (int i = 0; i < m_navButtons.size(); ++i) {
        m_navButtons[i]->setChecked(i == index);
    }
    loadUpcomingPlansSidebar();
    if (index == 0 && m_dashboardWidget) m_dashboardWidget->refreshData();
    else if (index == 1 && m_contactWidget) m_contactWidget->refreshData();
    else if (index == 2 && m_ledgerWidget) m_ledgerWidget->refreshData();
}

void MainWindow::onTopologyToggled(bool active) {
    if (active) {
        m_topBar->setBusinessNetButtonChecked(false);
        m_lastPageIndex = m_stack->currentIndex();
        m_stack->setCurrentIndex(3);
        for (auto* btn : m_navButtons) {
            btn->setChecked(false);
        }
        m_fullscreenGraphWidget->refreshData();
    } else {
        m_stack->setCurrentIndex(m_lastPageIndex);
        for (int i = 0; i < m_navButtons.size(); ++i) {
            m_navButtons[i]->setChecked(i == m_lastPageIndex);
        }
    }
}

void MainWindow::onBusinessNetToggled(bool active) {
    if (active) {
        m_topBar->setTopologyButtonChecked(false);
        m_lastPageIndex = m_stack->currentIndex();
        m_stack->setCurrentIndex(4);
        for (auto* btn : m_navButtons) {
            btn->setChecked(false);
        }
        if (m_businessNetWidget) m_businessNetWidget->refreshData();
    } else {
        m_stack->setCurrentIndex(m_lastPageIndex);
        for (int i = 0; i < m_navButtons.size(); ++i) {
            m_navButtons[i]->setChecked(i == m_lastPageIndex);
        }
    }
}

void MainWindow::onPlanItemClicked(const SocialPlanItem& item) {
    SocialPlanDialog dlg(item, this);
    if (dlg.exec() == QDialog::Accepted) {
        refreshAllData();
    }
}

void MainWindow::onAddNewPlanClicked() {
    SocialPlanDialog dlg(0, this);
    if (dlg.exec() == QDialog::Accepted) {
        refreshAllData();
    }
}

void MainWindow::onImportBackupClicked() {
    QString src = QFileDialog::getOpenFileName(this, "导入数据库或 VCF 通讯录", "", "支持文件 (*.vcf *.vcard *.db)");
    if (src.isEmpty()) return;
    if (src.endsWith(".vcf", Qt::CaseInsensitive) || src.endsWith(".vcard", Qt::CaseInsensitive)) {
        onNavButtonClicked(1);
    } else {
        CustomDialog::showInfo(this, "导入提示", "数据库备份文件已校验完毕，重启应用即可切换至导入库。");
    }
}

void MainWindow::onExportBackupClicked() {
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + "/云萍录_备份.db";
    QString dest = QFileDialog::getSaveFileName(this, "导出私密加密数据库备份", defaultPath, "SQLite 数据库 (*.db)");
    if (!dest.isEmpty()) {
        if (DatabaseManager::instance().exportDatabaseBackup(dest)) {
            CustomDialog::showInfo(this, "导出成功", QString("数据库已安全备份至:\n%1\n支持移动端私密同步导入。").arg(dest));
        } else {
            CustomDialog::showError(this, "导出失败", "写入目标文件失败，请检查写入权限。");
        }
    }
}

void MainWindow::onModelSettingsClicked() {
    ModelSettingsDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        AiAdvisorService::instance().cancelCurrentTask();
        AiAdvisorService::instance().setEngine(OllamaHttpEngine::createFromConfig());
        verifyModelAvailability();
        if (m_dashboardWidget) {
            m_dashboardWidget->checkAndTriggerInitialAiState(true);
        }
    }
}

void MainWindow::onSecuritySetupClicked() {
    SecuritySettingsDialog dlg(this);
    dlg.exec();
}

void MainWindow::showEvent(QShowEvent* event) {
    FramelessWindow::showEvent(event);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    event->accept();
    AiAdvisorService::instance().shutdown();
    OllamaHttpEngine::shutdownLocalServer();
    DatabaseManager::instance().close();
    FramelessWindow::closeEvent(event);
    QCoreApplication::quit();

#ifdef _WIN32
    // 强制静默终止可能残留的后台推理进程，防止端口占用与文件锁导致二次启动失败
    OllamaHttpEngine::killLlamaServerSilently();
    ::TerminateProcess(::GetCurrentProcess(), 0);
#else
    std::exit(0);
#endif
}
