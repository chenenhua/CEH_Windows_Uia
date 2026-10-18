#include "SecuritySettingsDialog.h"
#include "../core/SecurityManager.h"
#include "../core/DatabaseManager.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QMessageBox>
#include <QLabel>

SecuritySettingsDialog::SecuritySettingsDialog(QWidget* parent)
    : CustomDialog("系统安全与主密码设置", parent) {
    setupUi();
}

void SecuritySettingsDialog::setupUi() {
    setMinimumWidth(400);

    auto* layout = new QVBoxLayout();
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    auto* tipLbl = new QLabel("本地私有加密数据库。主密码作为派生密钥解锁并读取本地数据，保障隐私安全。", this);
    tipLbl->setStyleSheet("color: #8e8e93; font-size: 12px; line-height: 1.4;");
    tipLbl->setWordWrap(true);
    layout->addWidget(tipLbl);

    auto* formLayout = new QFormLayout();
    formLayout->setSpacing(10);
    formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_oldPassEdit = new QLineEdit(this);
    m_oldPassEdit->setEchoMode(QLineEdit::Password);
    m_oldPassEdit->setPlaceholderText("请输入当前主密码");
    formLayout->addRow("原主密码", m_oldPassEdit);

    connect(m_oldPassEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (text.isEmpty()) {
            m_oldPassEdit->setStyleSheet("");
        } else if (SecurityManager::instance().verifyMasterPassword(text)) {
            m_oldPassEdit->setStyleSheet("QLineEdit { background-color: #162d20; color: #ffffff; border: 1px solid #00a361; border-radius: 4px; padding: 6px; }");
        } else {
            m_oldPassEdit->setStyleSheet("QLineEdit { background-color: #4a151b; color: #ff6b6b; border: 1px solid #fa5151; border-radius: 4px; padding: 6px; }");
        }
    });

    m_newPassEdit = new QLineEdit(this);
    m_newPassEdit->setEchoMode(QLineEdit::Password);
    m_newPassEdit->setPlaceholderText("请输入新主密码 (至少4位)");
    formLayout->addRow("新主密码", m_newPassEdit);

    m_confirmPassEdit = new QLineEdit(this);
    m_confirmPassEdit->setEchoMode(QLineEdit::Password);
    m_confirmPassEdit->setPlaceholderText("再次确认新主密码");
    formLayout->addRow("确认新密码", m_confirmPassEdit);

    layout->addLayout(formLayout);

    auto* btnBox = new QHBoxLayout();
    btnBox->setSpacing(10);

    // 右上角已有关闭窗口按键，原取消按键改造为“自毁”按键
    // “自毁”按钮与“保存”按钮大小一样大，左右各占一半
    m_btnDestruct = new QPushButton("自毁", this);
    m_btnDestruct->setFixedHeight(36);
    m_btnDestruct->setCursor(Qt::PointingHandCursor);
    m_btnDestruct->setStyleSheet(
        "QPushButton { background-color: #fa5151; color: #ffffff; border: none; border-radius: 4px; font-size: 13px; font-weight: bold; padding: 6px 18px; }"
        "QPushButton:hover { background-color: #ff6b6b; }"
        "QPushButton:pressed { background-color: #d93838; }"
    );
    connect(m_btnDestruct, &QPushButton::clicked, this, &SecuritySettingsDialog::onSelfDestruct);

    auto* btnSave = new QPushButton("保存", this);
    btnSave->setObjectName("primaryBtn");
    btnSave->setFixedHeight(36);
    btnSave->setCursor(Qt::PointingHandCursor);
    connect(btnSave, &QPushButton::clicked, this, &SecuritySettingsDialog::onSavePassword);

    btnBox->addWidget(m_btnDestruct, 1);
    btnBox->addWidget(btnSave, 1);
    layout->addLayout(btnBox);

    bodyLayout()->addLayout(layout);
    normalizeFormWidgets(this);
}

void SecuritySettingsDialog::onSelfDestruct() {
    QString oldPass = m_oldPassEdit->text();
    if (!SecurityManager::instance().verifyMasterPassword(oldPass)) {
        // 原密码错误：背景变红，自毁点击不了
        m_oldPassEdit->setStyleSheet("QLineEdit { background-color: #4a151b; color: #ff6b6b; border: 1px solid #fa5151; border-radius: 4px; padding: 6px; }");
        m_oldPassEdit->setFocus();
        return;
    }

    // 密码正确：点击立刻删除数据库
    if (!CustomDialog::confirm(this, "高危自毁确认",
            "主密码验证正确！此操作将彻底物理粉碎并永久删除本地加密数据库（wood.db），全部数据将不可恢复！\n确定立即自毁吗？",
            "立即自毁", "取消", true)) {
        return;
    }

    DatabaseManager::instance().selfDestructDatabase();
    QApplication::exit(0);
}

void SecuritySettingsDialog::onSavePassword() {
    QString oldPass = m_oldPassEdit->text();
    QString newPass = m_newPassEdit->text();
    QString confirmPass = m_confirmPassEdit->text();

    if (!SecurityManager::instance().verifyMasterPassword(oldPass)) {
        m_oldPassEdit->setStyleSheet("QLineEdit { background-color: #4a151b; color: #ff6b6b; border: 1px solid #fa5151; border-radius: 4px; padding: 6px; }");
        m_oldPassEdit->setFocus();
        return;
    }

    if (newPass.length() < 4) {
        markFieldInvalid(m_newPassEdit);
        return;
    }

    if (newPass != confirmPass) {
        markFieldInvalid(m_confirmPassEdit);
        return;
    }

    if (SecurityManager::instance().setupMasterPassword(newPass)) {
        CustomDialog::showInfo(this, "成功", "系统主密码已成功更新！请妥善保存。");
        accept();
    } else {
        CustomDialog::showError(this, "错误", "主密码更新保存失败！");
    }
}
