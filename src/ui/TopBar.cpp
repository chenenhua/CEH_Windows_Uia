#include "TopBar.h"
#include "../core/DatabaseManager.h"
#include <QMouseEvent>
#include <QWindow>
#include <QStyle>
#include <QApplication>
#include <QFile>

TopBar::TopBar(QWidget* parent) : QWidget(parent) {
    setObjectName("topBar");
    setFixedHeight(40);
    setStyleSheet("QWidget#topBar { background-color: #141416; border-bottom: 1px solid #28282b; }");

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 0, 10, 0);
    layout->setSpacing(10);

    // 1. Logo 图标 与 软件名称 (优先使用 <获取的安装地址>/assets/img/icon/min_logo.png)
    m_logoIcon = new QLabel(this);
    QString minLogoPath = DatabaseManager::instance().resolveAssetPath("assets/img/icon/min_logo.png");
    if (!QFile::exists(minLogoPath)) {
        minLogoPath = ":/icons/assets/img/icon/min_logo.png";
    }
    m_logoIcon->setPixmap(QPixmap(minLogoPath).scaled(18, 18, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_logoIcon->setFixedSize(18, 18);

    m_appTitle = new QLabel("云萍录", this);
    m_appTitle->setObjectName("appTitle");
    m_appTitle->setStyleSheet("color: #e1e1e5; font-size: 14px; font-weight: 900;");

    layout->addWidget(m_logoIcon);
    layout->addWidget(m_appTitle);
    layout->addSpacing(10);

    // 2. ● Ai参谋 状态与模型设置按键 (点击直接设置模型)
    m_aiStatusBtn = new QPushButton(this);
    m_aiStatusBtn->setObjectName("topAiStatusBtn");
    m_aiStatusBtn->setCursor(Qt::PointingHandCursor);
    m_aiStatusBtn->setToolTip("点击配置本地大模型接口与参数");
    connect(m_aiStatusBtn, &QPushButton::clicked, this, &TopBar::modelSettingsRequested);
    layout->addWidget(m_aiStatusBtn);

    m_aiRunningLabel = new QLabel("运行中 ...", this);
    m_aiRunningLabel->setStyleSheet("color: #be782e; font-size: 12px; font-weight: bold; background: transparent;");
    m_aiRunningLabel->setVisible(false);
    layout->addWidget(m_aiRunningLabel);

    updateAiStatus(true, "qwen9b-vl", false);

    layout->addSpacing(10);

    // 3. ● 关系网 按钮
    m_topologyBtn = new QPushButton("● 关系网", this);
    m_topologyBtn->setObjectName("topTopologyBtn");
    m_topologyBtn->setCheckable(true);
    m_topologyBtn->setCursor(Qt::PointingHandCursor);
    m_topologyBtn->setToolTip("点击切换全景关系网");
    connect(m_topologyBtn, &QPushButton::toggled, this, &TopBar::topologyToggleRequested);
    layout->addWidget(m_topologyBtn);

    // 4. ● 商务网 按钮
    m_businessNetBtn = new QPushButton("● 商务网", this);
    m_businessNetBtn->setObjectName("topBusinessNetBtn");
    m_businessNetBtn->setCheckable(true);
    m_businessNetBtn->setCursor(Qt::PointingHandCursor);
    m_businessNetBtn->setToolTip("点击切换企业商务关系网络");
    connect(m_businessNetBtn, &QPushButton::toggled, this, &TopBar::businessNetToggleRequested);
    layout->addWidget(m_businessNetBtn);

    layout->addStretch();

    // 4. 窗口控制圆点按钮 (缩小、全屏、关闭)
    auto* controlsWidget = new QWidget(this);
    auto* controlsLayout = new QHBoxLayout(controlsWidget);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setSpacing(10);

    m_minBtn = new QPushButton("", controlsWidget);
    m_minBtn->setFixedSize(14, 14);
    m_minBtn->setCursor(Qt::PointingHandCursor);
    m_minBtn->setToolTip("缩小");
    m_minBtn->setStyleSheet(
        "QPushButton { background-color: #2e2e32; border: none; border-radius: 7px; padding: 0px; }"
        "QPushButton:hover { background-color: #48484f; }"
    );

    m_maxBtn = new QPushButton("", controlsWidget);
    m_maxBtn->setFixedSize(14, 14);
    m_maxBtn->setCursor(Qt::PointingHandCursor);
    m_maxBtn->setToolTip("全屏/还原");
    m_maxBtn->setStyleSheet(
        "QPushButton { background-color: #f5c518; border: none; border-radius: 7px; padding: 0px; }"
        "QPushButton:hover { background-color: #ffd84d; }"
    );

    m_closeBtn = new QPushButton("", controlsWidget);
    m_closeBtn->setFixedSize(14, 14);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setToolTip("关闭");
    m_closeBtn->setStyleSheet(
        "QPushButton { background-color: #fa5151; border: none; border-radius: 7px; padding: 0px; }"
        "QPushButton:hover { background-color: #ff6b6b; }"
    );

    controlsLayout->addWidget(m_minBtn);
    controlsLayout->addWidget(m_maxBtn);
    controlsLayout->addWidget(m_closeBtn);

    layout->addWidget(controlsWidget);

    connect(m_minBtn, &QPushButton::clicked, this, &TopBar::minimizeRequested);
    connect(m_maxBtn, &QPushButton::clicked, this, &TopBar::maximizeRequested);
    connect(m_closeBtn, &QPushButton::clicked, this, &TopBar::closeRequested);
}

void TopBar::setStatus(const QString& text, bool isOnline) {
    updateAiStatus(isOnline, text, false, isOnline);
}

void TopBar::updateAiStatus(bool isConfigured, const QString& modelName, bool isRunning, bool isAvailable) {
    if (!isConfigured || modelName.trimmed().isEmpty() || !isAvailable) {
        m_aiStatusBtn->setText("● Ai参谋 模型不可用");
        m_aiStatusBtn->setStyleSheet(
            "QPushButton { background: transparent; color: #fa5151; border: none; border-radius: 3px; padding: 4px 8px; font-size: 12px; font-weight: bold; }"
            "QPushButton:hover { background-color: #202024; }"
        );
        m_aiRunningLabel->setVisible(false);
    } else {
        m_aiStatusBtn->setText(QString("● Ai参谋 本地就绪(%1)").arg(modelName.trimmed()));
        m_aiStatusBtn->setStyleSheet(
            "QPushButton { background: transparent; color: #00a361; border: none; border-radius: 3px; padding: 4px 8px; font-size: 12px; }"
            "QPushButton:hover { background-color: #202024; }"
        );
        m_aiRunningLabel->setVisible(isRunning);
    }
}

void TopBar::updateMaxButtonState(bool isMaximized) {
    Q_UNUSED(isMaximized);
}

void TopBar::setTopologyButtonChecked(bool checked) {
    m_topologyBtn->blockSignals(true);
    m_topologyBtn->setChecked(checked);
    m_topologyBtn->setText(checked ? "● 返回主视图" : "● 关系网");
    m_topologyBtn->blockSignals(false);
}

void TopBar::setBusinessNetButtonChecked(bool checked) {
    m_businessNetBtn->blockSignals(true);
    m_businessNetBtn->setChecked(checked);
    m_businessNetBtn->setText(checked ? "● 返回主视图" : "● 商务网");
    m_businessNetBtn->blockSignals(false);
}

void TopBar::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (window() && window()->windowHandle()) {
            window()->windowHandle()->startSystemMove();
        }
    }
    QWidget::mousePressEvent(event);
}

void TopBar::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit maximizeRequested();
    }
    QWidget::mouseDoubleClickEvent(event);
}
