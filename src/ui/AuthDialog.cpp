#include "AuthDialog.h"
#include "../core/SecurityManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QPixmap>
#include <QMessageBox>

AuthDialog::AuthDialog(QWidget* parent) : CustomDialog("🔒 极密安全访问验证", parent) {
    setupUi();

    // 尝试开启摄像头
    if (m_cap.open(0)) {
        m_cameraAvailable = true;
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &AuthDialog::onCameraTimer);
        m_timer->start(40); // 25 fps
    } else {
        m_cameraLabel->setText("未检测到本地摄像头\n请直接使用密码解锁");
    }
}

AuthDialog::~AuthDialog() {
    if (m_timer) m_timer->stop();
    if (m_cap.isOpened()) m_cap.release();
}

void AuthDialog::setupUi() {
    resize(420, 520);

    auto* layout = contentLayout();
    layout->setSpacing(14);

    auto* title = new QLabel("身份核验 · 数据保险箱", this);
    title->setStyleSheet("color: #ffffff; font-size: 15px; font-weight: bold;");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    // 摄像头预览区
    m_cameraLabel = new QLabel(this);
    m_cameraLabel->setFixedSize(384, 220);
    m_cameraLabel->setStyleSheet("background-color: #0d0d0d; border: 1px solid #282828; border-radius: 6px;");
    m_cameraLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_cameraLabel);

    m_statusLabel = new QLabel("正在进行人脸识别对比...", this);
    m_statusLabel->setStyleSheet("color: #febc2f; font-size: 12px;");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_statusLabel);

    // 密码输入备用区
    auto* pwdRow = new QHBoxLayout();
    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText("或输入主密码快速解锁");
    m_passwordEdit->setStyleSheet(
        "QLineEdit { background-color: #202020; color: #ffffff; border: 1px solid #333333; border-radius: 4px; padding: 6px 12px; font-size: 12px; }"
        "QLineEdit:focus { border: 1px solid #28c840; }"
    );

    m_unlockBtn = new QPushButton("解锁", this);
    m_unlockBtn->setStyleSheet(
        "QPushButton { background-color: #28c840; color: #ffffff; font-weight: bold; border-radius: 4px; padding: 6px 18px; }"
        "QPushButton:hover { background-color: #34db4e; }"
    );

    pwdRow->addWidget(m_passwordEdit);
    pwdRow->addWidget(m_unlockBtn);
    layout->addLayout(pwdRow);

    connect(m_unlockBtn, &QPushButton::clicked, this, &AuthDialog::onVerifyPassword);
    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &AuthDialog::onVerifyPassword);
}

void AuthDialog::onCameraTimer() {
    if (!m_cap.isOpened()) return;

    cv::Mat frame;
    m_cap >> frame;
    if (frame.empty()) return;

    // 显示画面
    cv::Mat rgb;
    cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
    QImage img(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
    m_cameraLabel->setPixmap(QPixmap::fromImage(img).scaled(m_cameraLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

    // 比对人脸
    if (SecurityManager::instance().isFaceEnrolled()) {
        if (SecurityManager::instance().verifyFaceFrame(frame)) {
            m_statusLabel->setText("人脸识别成功！正在安全放行...");
            m_statusLabel->setStyleSheet("color: #28c840; font-weight: bold;");
            if (m_timer) m_timer->stop();
            QTimer::singleShot(400, this, &QDialog::accept);
        }
    } else {
        m_statusLabel->setText("未录入人脸模板，请使用密码直接解锁");
    }
}

void AuthDialog::onVerifyPassword() {
    QString pwd = m_passwordEdit->text();
    if (pwd.isEmpty()) return;

    if (SecurityManager::instance().verifyMasterPassword(pwd)) {
        if (m_timer) m_timer->stop();
        accept();
    } else {
        m_statusLabel->setText("主密码验证失败，请重试！");
        m_statusLabel->setStyleSheet("color: #ff5f57; font-weight: bold;");
        m_passwordEdit->clear();
    }
}
