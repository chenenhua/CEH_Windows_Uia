#include "AiAdvisorPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QButtonGroup>
#include <QScrollBar>

AiAdvisorPanel::AiAdvisorPanel(QWidget* parent) : QWidget(parent) {
    setupUi();

    connect(&AiAdvisorService::instance(), &AiAdvisorService::tokenReceived, this, &AiAdvisorPanel::onTokenReceived);
    connect(&AiAdvisorService::instance(), &AiAdvisorService::evaluationFinished, this, &AiAdvisorPanel::onFinished);
    connect(&AiAdvisorService::instance(), &AiAdvisorService::evaluationFailed, this, &AiAdvisorPanel::onFailed);
}

void AiAdvisorPanel::setupUi() {
    setFixedWidth(420);
    setStyleSheet("background-color: #141414; border-left: 1px solid #282828;");

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(14);

    // 1. 标题与状态
    auto* titleRow = new QHBoxLayout();
    auto* panelTitle = new QLabel("🤖 本地 AI 实时谋士", this);
    panelTitle->setStyleSheet("color: #ffffff; font-size: 16px; font-weight: bold;");
    titleRow->addWidget(panelTitle);

    m_statusLabel = new QLabel("空闲", this);
    m_statusLabel->setStyleSheet("color: #777777; font-size: 11px;");
    titleRow->addStretch();
    titleRow->addWidget(m_statusLabel);
    mainLayout->addLayout(titleRow);

    // 2. 模式切换器 (保守 vs 激进)
    auto* modeFrame = new QFrame(this);
    modeFrame->setStyleSheet("QFrame { background-color: #1b1b1b; border-radius: 6px; padding: 6px; border: 1px solid #292929; }");
    auto* modeLayout = new QHBoxLayout(modeFrame);
    modeLayout->setContentsMargins(4, 4, 4, 4);

    m_radioConservative = new QRadioButton("🛡️ 保守稳健 (风控)", modeFrame);
    m_radioAggressive = new QRadioButton("⚔️ 激进扩张 (进攻)", modeFrame);
    m_radioConservative->setChecked(true);

    m_radioConservative->setStyleSheet("QRadioButton { color: #cccccc; font-size: 12px; font-weight: bold; } QRadioButton::indicator:checked { color: #28c840; }");
    m_radioAggressive->setStyleSheet("QRadioButton { color: #cccccc; font-size: 12px; font-weight: bold; } QRadioButton::indicator:checked { color: #ff5f57; }");

    auto* btnGroup = new QButtonGroup(this);
    btnGroup->addButton(m_radioConservative);
    btnGroup->addButton(m_radioAggressive);

    modeLayout->addWidget(m_radioConservative);
    modeLayout->addWidget(m_radioAggressive);
    mainLayout->addWidget(modeFrame);

    // 3. 快捷动作按钮栏
    auto* actionRow = new QHBoxLayout();
    actionRow->setSpacing(8);

    m_btnGlobalEval = new QPushButton("📊 全局财务体检", this);
    m_btnGlobalEval->setStyleSheet(
        "QPushButton { background-color: #202020; color: #ffffff; border: 1px solid #333333; border-radius: 4px; padding: 6px; font-size: 12px; font-weight: 500; }"
        "QPushButton:hover { background-color: #2b2b2b; border-color: #444444; }"
    );

    m_btnContactEval = new QPushButton("👤 专项评估联系人", this);
    m_btnContactEval->setStyleSheet(
        "QPushButton { background-color: #202020; color: #ffffff; border: 1px solid #333333; border-radius: 4px; padding: 6px; font-size: 12px; font-weight: 500; }"
        "QPushButton:hover { background-color: #2b2b2b; border-color: #444444; }"
    );

    actionRow->addWidget(m_btnGlobalEval);
    actionRow->addWidget(m_btnContactEval);
    mainLayout->addLayout(actionRow);

    // 4. 对话与推理展示区
    m_chatDisplay = new QTextBrowser(this);
    m_chatDisplay->setOpenExternalLinks(true);
    m_chatDisplay->setStyleSheet(
        "QTextBrowser { background-color: #181818; color: #d0d0d0; border: 1px solid #282828; border-radius: 6px; padding: 12px; font-size: 13px; line-height: 1.5; }"
    );
    m_chatDisplay->setHtml(
        "<p style='color:#777777;'>欢迎使用本地参谋！当前后端：<b>qwen9b-vl (9B)</b>。<br><br>"
        "• <b>保守稳健模式</b>：优先捍卫安全现金流，严审人情与送礼ROI，拒绝无底线垫资。<br>"
        "• <b>激进扩张模式</b>：善用杠杆与关键贵人，抓住窗口期舍小博大。<br><br>"
        "点击上方【全局财务体检】或直接在下方输入问题即可获得即时分析。</p>"
    );
    mainLayout->addWidget(m_chatDisplay);

    // 5. 底部请教输入行
    auto* inputRow = new QHBoxLayout();
    inputRow->setSpacing(8);

    m_inputEdit = new QLineEdit(this);
    m_inputEdit->setPlaceholderText("请教参谋 (例如: 老张开口借2万我该借吗？)...");
    m_inputEdit->setStyleSheet(
        "QLineEdit { background: #1c1c1c; color: #ffffff; border: 1px solid #333333; border-radius: 4px; padding: 8px 10px; font-size: 12px; }"
    );

    m_btnSend = new QPushButton("咨询", this);
    m_btnSend->setFixedWidth(64);
    m_btnSend->setStyleSheet(
        "QPushButton { background: #28c840; color: #ffffff; font-weight: bold; border-radius: 4px; padding: 8px; font-size: 12px; }"
        "QPushButton:hover { background: #34db4e; }"
    );

    inputRow->addWidget(m_inputEdit);
    inputRow->addWidget(m_btnSend);
    mainLayout->addLayout(inputRow);

    connect(m_radioConservative, &QRadioButton::toggled, this, &AiAdvisorPanel::onModeToggled);
    connect(m_radioAggressive, &QRadioButton::toggled, this, &AiAdvisorPanel::onModeToggled);
    connect(m_btnGlobalEval, &QPushButton::clicked, this, &AiAdvisorPanel::onEvaluateGlobalClicked);
    connect(m_btnContactEval, &QPushButton::clicked, this, &AiAdvisorPanel::onEvaluateContactClicked);
    connect(m_btnSend, &QPushButton::clicked, this, &AiAdvisorPanel::onAskQuestionClicked);
    connect(m_inputEdit, &QLineEdit::returnPressed, this, &AiAdvisorPanel::onAskQuestionClicked);
}

void AiAdvisorPanel::onModeToggled() {
    AdvisorMode mode = m_radioConservative->isChecked() ? AdvisorMode::Conservative : AdvisorMode::Aggressive;
    AiAdvisorService::instance().setMode(mode);
    m_statusLabel->setText(QString("已切换至：%1").arg(AiAdvisorService::instance().modeName()));
}

void AiAdvisorPanel::onEvaluateGlobalClicked() {
    m_statusLabel->setText("参谋推演中...");
    m_chatDisplay->append("<br><hr><br><b style='color:#febc2f;'>【全局财务与人情资产体检】</b><br>");
    AiAdvisorService::instance().evaluateGlobalSituation();
}

void AiAdvisorPanel::evaluateContact(int contactId) {
    m_contactId = contactId;
    onEvaluateContactClicked();
}

void AiAdvisorPanel::onEvaluateContactClicked() {
    if (m_contactId <= 0) {
        m_chatDisplay->append("<br><span style='color:#ff5f57;'>请先在人脉列表中选择一位联系人！</span><br>");
        return;
    }

    m_statusLabel->setText("参谋正在分析人脉...");
    m_chatDisplay->append("<br><hr><br><b style='color:#00bcd4;'>【联系人专项策略推演】</b><br>");
    AiAdvisorService::instance().evaluateContact(m_contactId);
}

void AiAdvisorPanel::onAskQuestionClicked() {
    QString q = m_inputEdit->text().trimmed();
    if (q.isEmpty()) return;

    m_inputEdit->clear();
    m_statusLabel->setText("参谋正在解答...");
    m_chatDisplay->append(QString("<br><hr><br><b style='color:#28c840;'>【请教事项】: %1</b><br>").arg(q));
    AiAdvisorService::instance().askCustomQuestion(q, m_contactId);
}

void AiAdvisorPanel::onTokenReceived(const QString& token) {
    // 实时打字机效果
    QTextCursor cursor = m_chatDisplay->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(token);
    m_chatDisplay->setTextCursor(cursor);
    m_chatDisplay->verticalScrollBar()->setValue(m_chatDisplay->verticalScrollBar()->maximum());
}

void AiAdvisorPanel::onFinished(const QString& /*fullText*/) {
    m_statusLabel->setText("推演完毕");
    m_chatDisplay->append("<br><span style='color:#555555;'>———— 参谋推演结束 ————</span><br>");
    m_chatDisplay->verticalScrollBar()->setValue(m_chatDisplay->verticalScrollBar()->maximum());
}

void AiAdvisorPanel::onFailed(const QString& err) {
    m_statusLabel->setText("推演异常");
    m_chatDisplay->append(QString("<br><span style='color:#ff5f57;'>[异常提示]: %1</span><br>").arg(err));
}
