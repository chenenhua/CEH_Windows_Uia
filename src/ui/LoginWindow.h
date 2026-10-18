#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QMouseEvent>
#include <QPoint>

class LoginWindow : public QDialog {
    Q_OBJECT
public:
    explicit LoginWindow(QWidget* parent = nullptr);
    ~LoginWindow() override = default;

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private slots:
    void onSubmit();

private:
    void setupUi();

    QWidget* m_topBar{nullptr};
    QLabel* m_logoLabel{nullptr};
    QLabel* m_titleLabel{nullptr};
    QLabel* m_promptLabel{nullptr};

    QLineEdit* m_passwordEdit{nullptr};
    QLineEdit* m_confirmEdit{nullptr};
    QPushButton* m_submitBtn{nullptr};
    QPushButton* m_exitBtn{nullptr};
    QPushButton* m_closeBtn{nullptr};

    QPoint m_dragPosition;
    bool m_isFirstTime{false};
};
