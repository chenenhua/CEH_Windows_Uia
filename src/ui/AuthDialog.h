#pragma once

#include "CustomDialog.h"
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <opencv2/opencv.hpp>

class AuthDialog : public CustomDialog {
    Q_OBJECT
public:
    explicit AuthDialog(QWidget* parent = nullptr);
    ~AuthDialog() override;

private slots:
    void onCameraTimer();
    void onVerifyPassword();

private:
    void setupUi();

    QLabel* m_cameraLabel{nullptr};
    QLabel* m_statusLabel{nullptr};
    QLineEdit* m_passwordEdit{nullptr};
    QPushButton* m_unlockBtn{nullptr};

    QTimer* m_timer{nullptr};
    cv::VideoCapture m_cap;
    bool m_cameraAvailable{false};
};
