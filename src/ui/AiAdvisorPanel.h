#pragma once

#include <QWidget>
#include <QTextBrowser>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QLabel>
#include "../ai/AiAdvisorService.h"

class AiAdvisorPanel : public QWidget {
    Q_OBJECT
public:
    explicit AiAdvisorPanel(QWidget* parent = nullptr);

    void setCurrentContactId(int contactId) { m_contactId = contactId; }
    void evaluateContact(int contactId);

private slots:
    void onModeToggled();
    void onEvaluateGlobalClicked();
    void onEvaluateContactClicked();
    void onAskQuestionClicked();
    void onTokenReceived(const QString& token);
    void onFinished(const QString& fullText);
    void onFailed(const QString& err);

private:
    void setupUi();
    void appendAiMessage(const QString& sender, const QString& content, bool append = false);

    QRadioButton* m_radioConservative{nullptr};
    QRadioButton* m_radioAggressive{nullptr};

    QPushButton* m_btnGlobalEval{nullptr};
    QPushButton* m_btnContactEval{nullptr};

    QTextBrowser* m_chatDisplay{nullptr};
    QLabel* m_statusLabel{nullptr};

    QLineEdit* m_inputEdit{nullptr};
    QPushButton* m_btnSend{nullptr};

    int m_contactId{0};
};
