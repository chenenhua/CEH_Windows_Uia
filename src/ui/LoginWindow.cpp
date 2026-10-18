#include "LoginWindow.h"
#include "../core/SecurityManager.h"
#include "../core/DatabaseManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>
#include <QIcon>
#include <QMessageBox>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QFile>
#include <QWindow>

LoginWindow::LoginWindow(QWidget* parent) : QDialog(parent) {
    m_isFirstTime = !SecurityManager::instance().isSecurityConfigured();
    setupUi();
}

void LoginWindow::setupUi() {
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
    if (!QApplication::windowIcon().isNull()) {
        setWindowIcon(QApplication::windowIcon());
    }
    setFixedSize(480, m_isFirstTime ? 476 : 420);
    setStyleSheet(
        "QDialog { background-color: #141416; border: none; border-radius: 8px; }"
    );

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // ==================== 顶部自定义标题栏 (右上角红点退出软件) ====================
    m_topBar = new QWidget(this);
    m_topBar->setObjectName("loginTopBar");
    m_topBar->setFixedHeight(38);
    m_topBar->setStyleSheet(
        "QWidget#loginTopBar { background-color: #18181b; border: none; border-top-left-radius: 7px; border-top-right-radius: 7px; }"
    );

    auto* topLayout = new QHBoxLayout(m_topBar);
    topLayout->setContentsMargins(14, 0, 12, 0);
    topLayout->setSpacing(8);

    auto* minLogo = new QLabel(m_topBar);
    QString minLogoPath = DatabaseManager::instance().resolveAssetPath("assets/img/icon/min_logo.png");
    if (!QFile::exists(minLogoPath)) {
        minLogoPath = ":/icons/assets/img/icon/min_logo.png";
    }
    minLogo->setPixmap(QPixmap(minLogoPath).scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    minLogo->setFixedSize(16, 16);
    minLogo->setStyleSheet("border: none; background: transparent;");
    topLayout->addWidget(minLogo);

    auto* winTitle = new QLabel("云萍录", m_topBar);
    winTitle->setStyleSheet("color: #8e8e93; font-size: 12px; font-weight: bold; border: none; background: transparent;");
    topLayout->addWidget(winTitle);

    topLayout->addStretch();

    // 右上角红点退出软件
    m_closeBtn = new QPushButton("", m_topBar);
    m_closeBtn->setFixedSize(14, 14);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setToolTip("退出软件");
    m_closeBtn->setStyleSheet(
        "QPushButton { background-color: #fa5151; border: none; border-radius: 7px; padding: 0px; }"
        "QPushButton:hover { background-color: #ff6b6b; }"
    );
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    topLayout->addWidget(m_closeBtn);

    rootLayout->addWidget(m_topBar);

    // ==================== 页面主体内容 (无边框) ====================
    auto* contentWidget = new QWidget(this);
    contentWidget->setObjectName("loginContentWidget");
    contentWidget->setStyleSheet("QWidget#loginContentWidget { background: transparent; border: none; }");
    auto* layout = new QVBoxLayout(contentWidget);
    layout->setContentsMargins(50, 32, 50, 36);
    layout->setSpacing(14);

    // 1. 上下居中品牌区：上方 Logo (高度 100px、宽度自适应)，下方标题与介绍文字全部居中
    auto* brandCol = new QVBoxLayout();
    brandCol->setContentsMargins(0, 0, 0, 0);
    brandCol->setSpacing(8);

    m_logoLabel = new QLabel(contentWidget);
    m_logoLabel->setAlignment(Qt::AlignCenter);
    m_logoLabel->setStyleSheet("QLabel { background: transparent; border: none; border-radius: 5px; }");

    QString logoPath = DatabaseManager::instance().resolveAssetPath("assets/img/icon/logo.png");
    if (!QFile::exists(logoPath)) {
        logoPath = ":/icons/assets/img/icon/logo.png";
    }
    QPixmap rawLogo(logoPath);
    const int logoH = 100;
    if (!rawLogo.isNull() && rawLogo.height() > 0) {
        QPixmap scaled = rawLogo.scaledToHeight(logoH, Qt::SmoothTransformation);
        scaled.setDevicePixelRatio(1.0);
        const int logoW = qMax(1, scaled.width());
        QPixmap rounded(logoW, logoH);
        rounded.setDevicePixelRatio(1.0);
        rounded.fill(Qt::transparent);
        {
            QPainter painter(&rounded);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
            QPainterPath path;
            path.addRoundedRect(QRectF(0, 0, logoW, logoH), 5.0, 5.0);
            painter.setClipPath(path);
            painter.drawPixmap(0, 0, logoW, logoH, scaled);
        }
        m_logoLabel->setFixedSize(logoW, logoH);
        m_logoLabel->setPixmap(rounded);
    } else {
        m_logoLabel->setFixedSize(100, logoH);
    }
    brandCol->addWidget(m_logoLabel, 0, Qt::AlignHCenter);
    brandCol->addSpacing(4);

    m_titleLabel = new QLabel("云萍录", contentWidget);
    m_titleLabel->setStyleSheet("font-size: 20px; font-weight: 900; color: #e1e1e5; background: transparent; border: none;");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    brandCol->addWidget(m_titleLabel, 0, Qt::AlignHCenter);

    auto* subTitle = new QLabel("用 AI 记清人脉、人情与财务，看清自己，重新规划。", contentWidget);
    subTitle->setStyleSheet("font-size: 12px; color: #8e8e93; background: transparent; border: none;");
    subTitle->setAlignment(Qt::AlignCenter);
    subTitle->setWordWrap(true);
    brandCol->addWidget(subTitle);

    layout->addLayout(brandCol);
    layout->addSpacing(14);

    // 密码输入框样式（取消边框）
    QString pwdInputStyle =
        "QLineEdit { background-color: #222226; color: #e1e1e5; border: none; border-radius: 5px; padding: 10px 12px; font-size: 13px; }"
        "QLineEdit:focus { background-color: #26262b; border: none; }";

    // 2. 表单输入与提交（无边框，且取消“请输入主密码以联动解密打开数据库：”文字）
    if (m_isFirstTime) {
        m_passwordEdit = new QLineEdit(contentWidget);
        m_passwordEdit->setEchoMode(QLineEdit::Password);
        m_passwordEdit->setPlaceholderText("请设置主密码 (至少4位)");
        m_passwordEdit->setStyleSheet(pwdInputStyle);
        layout->addWidget(m_passwordEdit);

        m_confirmEdit = new QLineEdit(contentWidget);
        m_confirmEdit->setEchoMode(QLineEdit::Password);
        m_confirmEdit->setPlaceholderText("请再次确认主密码");
        m_confirmEdit->setStyleSheet(pwdInputStyle);
        layout->addWidget(m_confirmEdit);

        m_submitBtn = new QPushButton("初始化并安全进入", contentWidget);
    } else {
        m_passwordEdit = new QLineEdit(contentWidget);
        m_passwordEdit->setEchoMode(QLineEdit::Password);
        m_passwordEdit->setPlaceholderText("请输入主密码");
        m_passwordEdit->setStyleSheet(pwdInputStyle);
        layout->addWidget(m_passwordEdit);

        m_submitBtn = new QPushButton("安全解锁进入系统", contentWidget);
    }

    m_submitBtn->setFixedHeight(40);
    m_submitBtn->setCursor(Qt::PointingHandCursor);
    m_submitBtn->setStyleSheet(
        "QPushButton { background-color: #00a361; color: #ffffff; font-size: 14px; font-weight: bold; border-radius: 5px; border: none; }"
        "QPushButton:hover { background-color: #00b86e; border: none; }"
        "QPushButton:pressed { background-color: #008a52; border: none; }"
    );
    layout->addWidget(m_submitBtn);
    layout->addStretch();

    rootLayout->addWidget(contentWidget, 1);

    connect(m_submitBtn, &QPushButton::clicked, this, &LoginWindow::onSubmit);
    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &LoginWindow::onSubmit);
    connect(m_passwordEdit, &QLineEdit::textChanged, this, [this, pwdInputStyle]() {
        m_passwordEdit->setStyleSheet(pwdInputStyle);
    });
    if (m_confirmEdit) {
        connect(m_confirmEdit, &QLineEdit::returnPressed, this, &LoginWindow::onSubmit);
        connect(m_confirmEdit, &QLineEdit::textChanged, this, [this, pwdInputStyle]() {
            m_confirmEdit->setStyleSheet(pwdInputStyle);
        });
    }
}

void LoginWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (m_topBar && m_topBar->geometry().contains(event->pos())) {
            if (windowHandle()) {
                windowHandle()->startSystemMove();
            } else {
                m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
            }
            event->accept();
            return;
        }
    }
    QDialog::mousePressEvent(event);
}

void LoginWindow::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton && !m_dragPosition.isNull()) {
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

void LoginWindow::onSubmit() {
    QString pwd = m_passwordEdit->text();
    if (m_isFirstTime) {
        QString confirm = m_confirmEdit->text();
        if (pwd.length() < 4) {
            m_passwordEdit->setStyleSheet(
                "QLineEdit { background-color: #4a151b; color: #ffffff; border: 1px solid #fa5151; border-radius: 5px; padding: 10px 12px; font-size: 13px; }"
            );
            m_passwordEdit->setFocus();
            return;
        }
        if (pwd != confirm) {
            m_confirmEdit->setStyleSheet(
                "QLineEdit { background-color: #4a151b; color: #ffffff; border: 1px solid #fa5151; border-radius: 5px; padding: 10px 12px; font-size: 13px; }"
            );
            m_confirmEdit->selectAll();
            m_confirmEdit->setFocus();
            return;
        }

        if (SecurityManager::instance().setupMasterPassword(pwd)) {
            accept();
        } else {
            m_passwordEdit->setStyleSheet(
                "QLineEdit { background-color: #4a151b; color: #ffffff; border: 1px solid #fa5151; border-radius: 5px; padding: 10px 12px; font-size: 13px; }"
            );
            m_passwordEdit->setFocus();
        }
    } else {
        if (SecurityManager::instance().verifyMasterPassword(pwd)) {
            accept();
        } else {
            // 登入密码输入错误，不弹窗，输入框背景变红
            m_passwordEdit->setStyleSheet(
                "QLineEdit { background-color: #4a151b; color: #ffffff; border: 1px solid #fa5151; border-radius: 5px; padding: 10px 12px; font-size: 13px; }"
            );
            m_passwordEdit->selectAll();
            m_passwordEdit->setFocus();
        }
    }
}
