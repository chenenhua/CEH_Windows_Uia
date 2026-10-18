#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPoint>
#include <QCalendarWidget>
#include <QPlainTextEdit>
#include <QSpinBox>

// 解决 Windows 区域设置苏州码子乱码 + 日历全中文星期/年月 + 阿拉伯数字日期
class ChineseCalendarWidget : public QCalendarWidget {
    Q_OBJECT
public:
    explicit ChineseCalendarWidget(QWidget* parent = nullptr);

protected:
    void paintCell(QPainter* painter, const QRect& rect, QDate date) const override;
    void showEvent(QShowEvent* event) override;

private slots:
    void updateNavigationTexts(int year, int month);
};

// 交互式 5 颗星评分组件（点击第几颗星即得几分，用于亲疏轨迹等）
class StarRatingWidget : public QWidget {
    Q_OBJECT
public:
    explicit StarRatingWidget(int initialValue = 3, QWidget* parent = nullptr);

    int value() const { return m_value; }
    void setValue(int val);

signals:
    void valueChanged(int newValue);

private:
    void updateStarsUi();
    int m_value{3};
    QList<QPushButton*> m_starBtns;
    QLabel* m_scoreLabel{nullptr};
};

// 带上下点击加减箭头按钮的数值微调框（最小1 最大5，用于三维评价等）
class StepperSpinBox : public QWidget {
    Q_OBJECT
public:
    explicit StepperSpinBox(const QString& prefixText, int minVal = 1, int maxVal = 5, int initVal = 3, QWidget* parent = nullptr);

    int value() const { return m_spin->value(); }
    void setValue(int val) { m_spin->setValue(val); }
    QSpinBox* spinBox() const { return m_spin; }

private:
    QSpinBox* m_spin{nullptr};
};

class CustomDialog : public QDialog {
    Q_OBJECT
public:
    explicit CustomDialog(const QString& title, QWidget* parent = nullptr);
    ~CustomDialog() override = default;

    void setDialogTitle(const QString& title);
    void setTitle(const QString& title) { setDialogTitle(title); }

    QVBoxLayout* contentLayout() const { return m_contentLayout; }
    QVBoxLayout* bodyLayout() const { return m_contentLayout; }

    // 创建默认3行可见的多行文本输入框 (类似 textarea)
    static QPlainTextEdit* createMultiLineEdit(const QString& placeholder, QWidget* parent = nullptr, int visibleLines = 3);
    static QPlainTextEdit* createMultiLineEdit(QWidget* parent, const QString& placeholder, int visibleLines = 3) {
        return createMultiLineEdit(placeholder, parent, visibleLines);
    }

    // 统一为任意容器递归修正数字控件 Locale、点击即弹中文日历弹窗与表单样式
    static void normalizeFormWidgets(QWidget* root);

    // 统一自定义暗黑无边框提示与确认弹窗（替代系统原生 QMessageBox）
    static bool confirm(QWidget* parent,
                        const QString& title,
                        const QString& message,
                        const QString& confirmText = "确定",
                        const QString& cancelText = "取消",
                        bool isDanger = false);
    static void showInfo(QWidget* parent, const QString& title, const QString& message);
    static void showWarning(QWidget* parent, const QString& title, const QString& message);
    static void showError(QWidget* parent, const QString& title, const QString& message);

    // 表单必填项未输入时将输入框高亮为红底，重新输入时自动复原，彻底替代阻断式弹窗
    static void markFieldInvalid(QWidget* widget);
    static void resetFieldInvalid(QWidget* widget);

protected:
    void showEvent(QShowEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    void setupBaseUi(const QString& title);

    QWidget* m_topBarWidget{nullptr};
    QLabel* m_titleLabel{nullptr};
    QPushButton* m_closeBtn{nullptr};
    QVBoxLayout* m_contentLayout{nullptr};
    QPoint m_dragPosition;
};
