#pragma once

#include "CustomDialog.h"
#include <QLineEdit>

class SecuritySettingsDialog : public CustomDialog {
    Q_OBJECT
public:
    explicit SecuritySettingsDialog(QWidget* parent = nullptr);

private slots:
    void onSavePassword();
    void onSelfDestruct();

private:
    void setupUi();

    QLineEdit* m_oldPassEdit{nullptr};
    QLineEdit* m_newPassEdit{nullptr};
    QLineEdit* m_confirmPassEdit{nullptr};
    QPushButton* m_btnDestruct{nullptr};
};
