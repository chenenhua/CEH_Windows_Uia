#include "CustomDialog.h"
#include "../core/DatabaseManager.h"
#include <QFile>
#include <QFrame>
#include <QWindow>
#include <QApplication>
#include <QPainter>
#include <QLocale>
#include <QTableView>
#include <QHeaderView>
#include <QToolButton>
#include <QMenu>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QDateEdit>
#include <QDateTimeEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QFormLayout>
#include <QStyledItemDelegate>
#include <QDate>
#include <QKeyEvent>

namespace {
class CalendarWeekdayDelegate : public QStyledItemDelegate {
public:
    explicit CalendarWeekdayDelegate(QCalendarWidget* cal)
        : QStyledItemDelegate(cal), m_cal(cal) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        if (index.row() == 0) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->fillRect(option.rect, QColor("#18181b"));

            static const QStringList kWeekNames = {"一", "二", "三", "四", "五", "六", "日"};
            int firstDay = m_cal ? static_cast<int>(m_cal->firstDayOfWeek()) : 1;
            int colOffset = (m_cal && m_cal->verticalHeaderFormat() != QCalendarWidget::NoVerticalHeader) ? 1 : 0;
            int dayIdx = (firstDay - 1 + (index.column() - colOffset) + 7) % 7;
            QString text = (dayIdx >= 0 && dayIdx < kWeekNames.size()) ? kWeekNames[dayIdx] : "";

            bool isWeekend = (dayIdx == 5 || dayIdx == 6);
            painter->setPen(isWeekend ? QColor("#fa5151") : QColor("#8e8e93"));
            QFont f("Microsoft YaHei UI", 9, QFont::Bold);
            painter->setFont(f);
            painter->drawText(option.rect, Qt::AlignCenter, text);

            painter->setPen(QColor("#28282b"));
            painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
            painter->restore();
            return;
        }
        QStyledItemDelegate::paint(painter, option, index);
    }

private:
    QCalendarWidget* m_cal{nullptr};
};

// 让带日历弹窗的 QDateEdit / QDateTimeEdit 点击任意区域直接弹出日历选择器（无需手动打字输入）
class DatePopupClickFilter : public QObject {
public:
    explicit DatePopupClickFilter(QDateTimeEdit* dt) : QObject(dt), m_dt(dt) {}

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::MouseButtonPress && m_dt && m_dt->calendarPopup()) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                if (watched == m_dt || qobject_cast<QLineEdit*>(watched)) {
                    if (auto* cal = m_dt->calendarWidget()) {
                        cal->setSelectedDate(m_dt->date());
                        if (auto* popup = cal->parentWidget()) {
                            QPoint pos = m_dt->mapToGlobal(QPoint(0, m_dt->height() + 2));
                            popup->move(pos);
                            popup->show();
                            popup->raise();
                            cal->setFocus();
                            return true;
                        }
                    }
                }
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QDateTimeEdit* m_dt{nullptr};
};
} // namespace

// ==================== ChineseCalendarWidget ====================
ChineseCalendarWidget::ChineseCalendarWidget(QWidget* parent)
    : QCalendarWidget(parent) {
    setLocale(QLocale::c());
    setFirstDayOfWeek(Qt::Monday);
    setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    setHorizontalHeaderFormat(QCalendarWidget::ShortDayNames);
    setGridVisible(false);
    setMinimumSize(280, 240);

    setStyleSheet(
        "QCalendarWidget { background-color: #141416; border: 1px solid #28282b; border-radius: 3px; }"
        "QCalendarWidget QWidget#qt_calendar_navigationbar { background-color: #1a1a1d; border-bottom: 1px solid #28282b; min-height: 32px; }"
        "QCalendarWidget QToolButton { background-color: transparent; color: #e1e1e5; border: none; border-radius: 3px; padding: 4px 8px; font-size: 12px; font-weight: bold; }"
        "QCalendarWidget QToolButton:hover { background-color: #242428; color: #ffffff; }"
        "QCalendarWidget QToolButton::menu-indicator { image: none; width: 0px; }"
        "QCalendarWidget QSpinBox { background-color: #18181b; color: #e1e1e5; border: 1px solid #28282b; border-radius: 3px; padding: 2px 6px; font-size: 12px; }"
        "QCalendarWidget QMenu { background-color: #18181b; color: #e1e1e5; border: 1px solid #28282b; }"
        "QCalendarWidget QMenu::item:selected { background-color: #00a361; color: #ffffff; }"
        "QCalendarWidget QTableView { background-color: #141416; selection-background-color: transparent; outline: none; border: none; }"
    );

    if (auto* view = findChild<QTableView*>("qt_calendar_calendarview")) {
        view->setItemDelegate(new CalendarWeekdayDelegate(this));
        if (view->horizontalHeader()) {
            view->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        }
        if (view->verticalHeader()) {
            view->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        }
    }

    if (auto* yearSpin = findChild<QSpinBox*>("qt_calendar_yearedit")) {
        yearSpin->setLocale(QLocale::c());
    }

    if (auto* monthBtn = findChild<QToolButton*>("qt_calendar_monthbutton")) {
        if (QMenu* menu = monthBtn->menu()) {
            auto actions = menu->actions();
            for (int i = 0; i < actions.size() && i < 12; ++i) {
                actions[i]->setText(QString("%1月").arg(i + 1));
            }
        }
    }

    connect(this, &QCalendarWidget::currentPageChanged, this, &ChineseCalendarWidget::updateNavigationTexts);
    updateNavigationTexts(yearShown(), monthShown());
}

void ChineseCalendarWidget::showEvent(QShowEvent* event) {
    QCalendarWidget::showEvent(event);
    updateNavigationTexts(yearShown(), monthShown());
}

void ChineseCalendarWidget::updateNavigationTexts(int year, int month) {
    if (auto* monthBtn = findChild<QToolButton*>("qt_calendar_monthbutton")) {
        monthBtn->setText(QString("%1月").arg(month));
    }
    if (auto* yearBtn = findChild<QToolButton*>("qt_calendar_yearbutton")) {
        yearBtn->setText(QString("%1年").arg(year));
    }
    if (auto* yearSpin = findChild<QSpinBox*>("qt_calendar_yearedit")) {
        yearSpin->setLocale(QLocale::c());
    }
}

void ChineseCalendarWidget::paintCell(QPainter* painter, const QRect& rect, QDate date) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    bool isSelected = (date == selectedDate());
    bool isToday = (date == QDate::currentDate());
    bool isCurrentMonth = (date.month() == monthShown() && date.year() == yearShown());
    bool isWeekend = (date.dayOfWeek() == Qt::Saturday || date.dayOfWeek() == Qt::Sunday);

    painter->fillRect(rect, QColor("#141416"));

    QRect box = rect.adjusted(2, 2, -2, -2);
    if (isSelected) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor("#00a361"));
        painter->drawRoundedRect(box, 3, 3);
    } else if (isToday) {
        painter->setPen(QPen(QColor("#00a361"), 1));
        painter->setBrush(QColor("#1a2822"));
        painter->drawRoundedRect(box, 3, 3);
    }

    QColor textColor;
    if (isSelected) {
        textColor = QColor("#ffffff");
    } else if (!isCurrentMonth) {
        textColor = QColor("#48484d");
    } else if (isWeekend) {
        textColor = QColor("#fa5151");
    } else {
        textColor = QColor("#e1e1e5");
    }

    painter->setPen(textColor);
    QFont f("Microsoft YaHei UI", 9, isSelected || isToday ? QFont::Bold : QFont::Normal);
    painter->setFont(f);
    painter->drawText(rect, Qt::AlignCenter, QString::number(date.day()));

    painter->restore();
}

// ==================== StarRatingWidget ====================
StarRatingWidget::StarRatingWidget(int initialValue, QWidget* parent)
    : QWidget(parent), m_value(qBound(1, initialValue, 5)) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    for (int i = 1; i <= 5; ++i) {
        auto* btn = new QPushButton("★", this);
        btn->setFixedSize(28, 28);
        btn->setCursor(Qt::PointingHandCursor);
        connect(btn, &QPushButton::clicked, this, [this, i]() {
            setValue(i);
        });
        m_starBtns.append(btn);
        layout->addWidget(btn);
    }

    m_scoreLabel = new QLabel(this);
    m_scoreLabel->setStyleSheet("color: #e1e1e5; font-size: 13px; font-weight: bold; margin-left: 6px; background: transparent; border: none;");
    layout->addWidget(m_scoreLabel);
    layout->addStretch();

    updateStarsUi();
}

void StarRatingWidget::setValue(int val) {
    int clamped = qBound(1, val, 5);
    if (m_value != clamped) {
        m_value = clamped;
        updateStarsUi();
        emit valueChanged(m_value);
    } else {
        updateStarsUi();
    }
}

void StarRatingWidget::updateStarsUi() {
    for (int i = 0; i < m_starBtns.size(); ++i) {
        bool active = (i < m_value);
        m_starBtns[i]->setText(active ? "★" : "☆");
        m_starBtns[i]->setStyleSheet(
            QString("QPushButton { background: transparent; border: none; font-size: 18px; padding: 0px; color: %1; }"
                    "QPushButton:hover { color: #00c474; }")
                .arg(active ? "#00a361" : "#55555a")
        );
    }
    if (m_scoreLabel) {
        m_scoreLabel->setText(QString("%1分").arg(m_value));
    }
}

// ==================== StepperSpinBox ====================
StepperSpinBox::StepperSpinBox(const QString& prefixText, int minVal, int maxVal, int initVal, QWidget* parent)
    : QWidget(parent) {
    setStyleSheet("background: transparent; border: none;");
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    auto* container = new QFrame(this);
    container->setFixedHeight(32);
    container->setStyleSheet(
        "QFrame { background-color: #141416; border: 1px solid #28282b; border-radius: 3px; }"
    );
    auto* cLayout = new QHBoxLayout(container);
    cLayout->setContentsMargins(6, 0, 2, 0);
    cLayout->setSpacing(2);

    m_spin = new QSpinBox(container);
    m_spin->setLocale(QLocale::c());
    m_spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_spin->setRange(minVal, maxVal);
    m_spin->setValue(qBound(minVal, initVal, maxVal));
    if (!prefixText.isEmpty()) {
        m_spin->setPrefix(prefixText + " ");
    }
    m_spin->setStyleSheet("QSpinBox { background: transparent; border: none; color: #e1e1e5; font-size: 12px; padding: 0px; }");
    cLayout->addWidget(m_spin, 1);

    auto* arrowsCol = new QVBoxLayout();
    arrowsCol->setContentsMargins(0, 1, 0, 1);
    arrowsCol->setSpacing(1);

    QString arrowBtnStyle =
        "QPushButton { background-color: #222226; color: #8e8e93; border: none; border-radius: 2px; font-size: 8px; padding: 0px; min-height: 13px; max-height: 13px; min-width: 18px; max-width: 18px; }"
        "QPushButton:hover { background-color: #00a361; color: #ffffff; }"
        "QPushButton:pressed { background-color: #008f55; color: #ffffff; }";

    auto* btnUp = new QPushButton("▲", container);
    btnUp->setFixedSize(18, 13);
    btnUp->setCursor(Qt::PointingHandCursor);
    btnUp->setStyleSheet(arrowBtnStyle);
    connect(btnUp, &QPushButton::clicked, m_spin, &QSpinBox::stepUp);

    auto* btnDown = new QPushButton("▼", container);
    btnDown->setFixedSize(18, 13);
    btnDown->setCursor(Qt::PointingHandCursor);
    btnDown->setStyleSheet(arrowBtnStyle);
    connect(btnDown, &QPushButton::clicked, m_spin, &QSpinBox::stepDown);

    arrowsCol->addWidget(btnUp);
    arrowsCol->addWidget(btnDown);
    cLayout->addLayout(arrowsCol);

    layout->addWidget(container);
}

// ==================== CustomDialog ====================
CustomDialog::CustomDialog(const QString& title, QWidget* parent) : QDialog(parent) {
    setLocale(QLocale::c());
    setupBaseUi(title);
}

QPlainTextEdit* CustomDialog::createMultiLineEdit(const QString& placeholder, QWidget* parent, int visibleLines) {
    auto* edit = new QPlainTextEdit(parent);
    edit->setPlaceholderText(placeholder);
    int h = qMax(76, visibleLines * 24 + 8);
    edit->setFixedHeight(h);
    edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    edit->setStyleSheet(
        "QPlainTextEdit { background-color: transparent; color: #e1e1e5; border: none; font-size: 13px; padding: 2px 2px; }"
    );
    return edit;
}

void CustomDialog::setupBaseUi(const QString& title) {
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    if (!QApplication::windowIcon().isNull()) {
        setWindowIcon(QApplication::windowIcon());
    }
    setAttribute(Qt::WA_TranslucentBackground, false);
    setStyleSheet(
        "QDialog { background-color: #141416; font-family: 'Microsoft YaHei UI', 'Microsoft YaHei', sans-serif; color: #e1e1e5; }"
        "QLabel { color: #8e8e93; font-size: 13px; background: transparent; border: none; }"
        "QLineEdit, QSpinBox, QDoubleSpinBox, QDateEdit, QDateTimeEdit, QComboBox, QTextEdit, QPlainTextEdit { "
        "    background-color: #18181b; color: #e1e1e5; border: 1px solid #28282b; border-radius: 3px; padding: 6px 10px; font-size: 13px; "
        "}"
        "QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QDateEdit:focus, QDateTimeEdit:focus, QComboBox:focus, QTextEdit:focus, QPlainTextEdit:focus { "
        "    border-color: #00a361; "
        "}"
        "QAbstractSpinBox::up-button, QAbstractSpinBox::down-button { width: 0px; border: none; }"
        "QDateEdit::drop-down, QDateTimeEdit::drop-down { subcontrol-origin: padding; subcontrol-position: top right; width: 24px; border: none; }"
        "QDateEdit::down-arrow, QDateTimeEdit::down-arrow { image: none; border-left: 5px solid transparent; border-right: 5px solid transparent; border-top: 6px solid #e1e1e5; margin-right: 8px; }"
        "QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: top right; width: 24px; border: none; }"
        "QComboBox::down-arrow { image: none; border-left: 5px solid transparent; border-right: 5px solid transparent; border-top: 6px solid #e1e1e5; margin-right: 8px; }"
        "QComboBox QAbstractItemView { background-color: #18181b; color: #e1e1e5; border: 1px solid #28282b; selection-background-color: #00a361; selection-color: #ffffff; }"
        "QTabWidget::pane { border: 1px solid #28282b; border-radius: 3px; background-color: #141416; top: -1px; }"
        "QTabBar::tab { background-color: #1c1c1f; color: #8e8e93; border: 1px solid #28282b; border-radius: 3px; padding: 6px 18px; margin-right: 6px; font-size: 13px; font-weight: bold; }"
        "QTabBar::tab:selected { background-color: #00a361; color: #ffffff; border-color: #00a361; }"
        "QPushButton { "
        "    background-color: #1c1c1f; color: #e1e1e5; border: 1px solid #28282b; border-radius: 3px; padding: 7px 16px; font-size: 13px; "
        "}"
        "QPushButton:hover { background-color: #28282b; color: #ffffff; }"
        "QPushButton:pressed { background-color: #323236; color: #ffffff; }"
        "QPushButton#primaryBtn, QPushButton#successBtn { "
        "    background-color: #00a361; color: #ffffff; font-weight: bold; border: none; border-radius: 3px; "
        "}"
        "QPushButton#primaryBtn:hover, QPushButton#successBtn:hover { background-color: #00b86d; color: #ffffff; }"
        "QPushButton#primaryBtn:pressed, QPushButton#successBtn:pressed { background-color: #008f55; color: #ffffff; }"
    );

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    auto* rootFrame = new QFrame(this);
    rootFrame->setObjectName("dialogFrame");
    rootFrame->setStyleSheet(
        "QFrame#dialogFrame { background-color: #141416; border: 1px solid #28282b; border-radius: 3px; }"
    );

    auto* frameLayout = new QVBoxLayout(rootFrame);
    frameLayout->setContentsMargins(0, 0, 0, 0);
    frameLayout->setSpacing(0);

    // ==================== 顶部自定义标题栏 ====================
    m_topBarWidget = new QWidget(rootFrame);
    m_topBarWidget->setFixedHeight(40);
    m_topBarWidget->setStyleSheet(
        "QWidget { background-color: #141416; border-bottom: 1px solid #28282b; }"
    );

    auto* topLayout = new QHBoxLayout(m_topBarWidget);
    topLayout->setContentsMargins(14, 0, 10, 0);
    topLayout->setSpacing(8);

    auto* logoLabel = new QLabel(m_topBarWidget);
    QString minLogoPath = DatabaseManager::instance().resolveAssetPath("assets/img/icon/min_logo.png");
    if (!QFile::exists(minLogoPath)) {
        minLogoPath = ":/icons/assets/img/icon/min_logo.png";
    }
    logoLabel->setPixmap(QPixmap(minLogoPath).scaled(18, 18, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logoLabel->setFixedSize(18, 18);
    logoLabel->setStyleSheet("border: none; background: transparent;");
    topLayout->addWidget(logoLabel);

    m_titleLabel = new QLabel(title, m_topBarWidget);
    m_titleLabel->setStyleSheet("color: #e1e1e5; font-size: 13px; font-weight: bold; border: none; background: transparent;");
    topLayout->addWidget(m_titleLabel);

    topLayout->addStretch();

    m_closeBtn = new QPushButton("✕", m_topBarWidget);
    m_closeBtn->setFixedSize(28, 28);
    m_closeBtn->setStyleSheet(
        "QPushButton { background: transparent; color: #8e8e93; border: none; font-size: 13px; border-radius: 3px; padding: 0px; }"
        "QPushButton:hover { background-color: #fa5151; color: #ffffff; }"
    );
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    topLayout->addWidget(m_closeBtn);

    frameLayout->addWidget(m_topBarWidget);

    // ==================== 弹窗主体内容区 ====================
    auto* contentWidget = new QWidget(rootFrame);
    contentWidget->setStyleSheet("background-color: transparent; border: none;");
    m_contentLayout = new QVBoxLayout(contentWidget);
    m_contentLayout->setContentsMargins(14, 12, 14, 14);
    m_contentLayout->setSpacing(10);

    frameLayout->addWidget(contentWidget, 1);
    outerLayout->addWidget(rootFrame);
}

void CustomDialog::normalizeFormWidgets(QWidget* root) {
    if (!root) return;
    root->setLocale(QLocale::c());

    // 1. 强制所有数字输入与日期选择器使用标准阿拉伯数字 + 点击即弹中文日历
    const auto spins = root->findChildren<QAbstractSpinBox*>();
    for (auto* s : spins) {
        s->setLocale(QLocale::c());
        auto* dtSpin = qobject_cast<QDateTimeEdit*>(s);
        if (!dtSpin || !dtSpin->calendarPopup()) {
            s->setButtonSymbols(QAbstractSpinBox::NoButtons);
        }
    }

    const auto dateEdits = root->findChildren<QDateTimeEdit*>();
    for (auto* dt : dateEdits) {
        dt->setLocale(QLocale::c());
        if (dt->calendarPopup()) {
            if (!qobject_cast<ChineseCalendarWidget*>(dt->calendarWidget())) {
                auto* cal = new ChineseCalendarWidget(dt);
                dt->setCalendarWidget(cal);
                connect(cal, &QCalendarWidget::clicked, dt, [dt](const QDate& date) {
                    dt->setDate(date);
                    if (auto* calW = dt->calendarWidget()) {
                        if (auto* popup = calW->parentWidget()) {
                            popup->close();
                        }
                    }
                });
            }
            dt->setCursor(Qt::PointingHandCursor);
            if (!dt->property("popupFilterInstalled").toBool()) {
                dt->setProperty("popupFilterInstalled", true);
                auto* filter = new DatePopupClickFilter(dt);
                dt->installEventFilter(filter);
                if (auto* le = dt->findChild<QLineEdit*>()) {
                    le->setReadOnly(true);
                    le->setCursor(Qt::PointingHandCursor);
                    le->installEventFilter(filter);
                }
            }
        }
    }

    // 2. 将所有 QFormLayout 行统一包装为内嵌标签输入框 (#18181b 背景 + #28282b 边框 + 3px 圆角)
    const auto forms = root->findChildren<QFormLayout*>();
    for (auto* form : forms) {
        if (form->property("inlineBoxStyled").toBool()) continue;
        form->setProperty("inlineBoxStyled", true);
        form->setVerticalSpacing(10);

        int rows = form->rowCount();
        for (int r = 0; r < rows; ++r) {
            QLayoutItem* lblItem = form->itemAt(r, QFormLayout::LabelRole);
            QLayoutItem* fldItem = form->itemAt(r, QFormLayout::FieldRole);
            if (!lblItem || !fldItem) continue;

            QWidget* lblW = lblItem->widget();
            QWidget* fldW = fldItem->widget();
            if (!lblW || !fldW) continue;

            form->removeItem(lblItem);
            form->removeItem(fldItem);
            delete lblItem;
            delete fldItem;

            bool isMultiLine = qobject_cast<QPlainTextEdit*>(fldW) != nullptr ||
                               qobject_cast<QTextEdit*>(fldW) != nullptr ||
                               (fldW->minimumHeight() >= 60);

            auto* rowBox = new QFrame(root);
            rowBox->setObjectName("inlineFormRowBox");
            int boxH = isMultiLine ? qMax(78, fldW->minimumHeight() + 12) : 38;
            rowBox->setFixedHeight(boxH);
            rowBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            rowBox->setStyleSheet(
                "QFrame#inlineFormRowBox { background-color: #18181b; border: 1px solid #28282b; border-radius: 3px; }"
                "QFrame#inlineFormRowBox QLineEdit, QFrame#inlineFormRowBox QSpinBox, QFrame#inlineFormRowBox QDoubleSpinBox, "
                "QFrame#inlineFormRowBox QDateEdit, QFrame#inlineFormRowBox QDateTimeEdit, QFrame#inlineFormRowBox QComboBox, "
                "QFrame#inlineFormRowBox QPlainTextEdit, QFrame#inlineFormRowBox QTextEdit { "
                "    background: transparent; border: none; color: #e1e1e5; font-size: 13px; padding: 2px 4px; "
                "}"
            );

            auto* h = new QHBoxLayout(rowBox);
            h->setContentsMargins(12, isMultiLine ? 6 : 2, 8, isMultiLine ? 6 : 2);
            h->setSpacing(10);

            if (auto* qLbl = qobject_cast<QLabel*>(lblW)) {
                QString cleanText = qLbl->text().trimmed();
                if (cleanText.endsWith(":") || cleanText.endsWith("：")) {
                    cleanText.chop(1);
                }
                if (cleanText.endsWith(" *")) {
                    cleanText.chop(2);
                }
                qLbl->setText(cleanText);
                if (isMultiLine) {
                    qLbl->setAlignment(Qt::AlignLeft | Qt::AlignTop);
                    qLbl->setContentsMargins(0, 3, 0, 0);
                }
            }
            lblW->setStyleSheet("color: #8e8e93; font-size: 13px; background: transparent; border: none;");
            lblW->setMinimumWidth(62);

            if (isMultiLine) {
                h->addWidget(lblW, 0, Qt::AlignTop);
                h->addWidget(fldW, 1, Qt::AlignTop);
            } else {
                h->addWidget(lblW);
                h->addWidget(fldW, 1);
            }

            if (lblW->isHidden() || fldW->isHidden()) {
                rowBox->setVisible(false);
            }

            form->setWidget(r, QFormLayout::SpanningRole, rowBox);
        }
    }

    // 3. 统一底部“取消”、“取消计划”、“保存”按钮样式
    const auto buttons = root->findChildren<QPushButton*>();
    for (auto* btn : buttons) {
        if (btn->text() == "✕" || btn->text() == "★" || btn->text() == "☆" ||
            btn->text() == "▲" || btn->text() == "▼" || btn->objectName() == "inlineBrowseBtn") continue;

        QString txt = btn->text().trimmed();
        if (txt == "取消计划" || btn->objectName() == "dangerBtn") {
            btn->setMinimumHeight(38);
            btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            btn->setStyleSheet(
                "QPushButton { background-color: #1c1c1f; color: #fa5151; border: 1px solid #28282b; border-radius: 3px; font-size: 15px; font-weight: bold; }"
                "QPushButton:hover { background-color: #2b181a; color: #ff6b6b; border-color: #fa5151; }"
            );
        } else if (txt == "取消" || txt == "返回") {
            btn->setMinimumHeight(38);
            btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            btn->setStyleSheet(
                "QPushButton { background-color: #1c1c1f; color: #e1e1e5; border: 1px solid #28282b; border-radius: 3px; font-size: 15px; }"
                "QPushButton:hover { background-color: #28282b; color: #ffffff; }"
            );
            if (QWidget* pw = btn->parentWidget()) {
                for (auto* hb : pw->findChildren<QHBoxLayout*>()) {
                    if (hb->indexOf(btn) >= 0) {
                        for (int i = hb->count() - 1; i >= 0; --i) {
                            if (hb->itemAt(i)->spacerItem()) {
                                QLayoutItem* sp = hb->takeAt(i);
                                delete sp;
                            }
                        }
                    }
                }
            }
        } else if (btn->objectName() == "primaryBtn" || btn->objectName() == "successBtn" ||
                   txt.contains("保存") || txt.contains("确认") || txt.contains("确定") || txt.contains("制定") || txt.contains("完成")) {
            btn->setMinimumHeight(38);
            btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            btn->setStyleSheet(
                "QPushButton { background-color: #00a361; color: #ffffff; border: none; border-radius: 3px; font-size: 15px; font-weight: bold; }"
                "QPushButton:hover { background-color: #00b86d; color: #ffffff; }"
                "QPushButton:pressed { background-color: #008f55; color: #ffffff; }"
            );
        }
    }
}

bool CustomDialog::confirm(QWidget* parent,
                           const QString& title,
                           const QString& message,
                           const QString& confirmText,
                           const QString& cancelText,
                           bool isDanger) {
    CustomDialog dlg(title, parent);
    dlg.setFixedWidth(400);

    auto* layout = new QVBoxLayout();
    layout->setContentsMargins(10, 12, 10, 8);
    layout->setSpacing(18);

    auto* msgLbl = new QLabel(message, &dlg);
    msgLbl->setWordWrap(true);
    msgLbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    msgLbl->setStyleSheet("color: #e1e1e5; font-size: 14px; line-height: 1.5; background: transparent; border: none;");
    layout->addWidget(msgLbl);

    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);

    auto* btnCancel = new QPushButton(cancelText, &dlg);
    btnCancel->setCursor(Qt::PointingHandCursor);
    connect(btnCancel, &QPushButton::clicked, &dlg, &QDialog::reject);

    auto* btnConfirm = new QPushButton(confirmText, &dlg);
    btnConfirm->setObjectName(isDanger ? "dangerBtn" : "primaryBtn");
    btnConfirm->setCursor(Qt::PointingHandCursor);
    connect(btnConfirm, &QPushButton::clicked, &dlg, &QDialog::accept);

    btnRow->addWidget(btnCancel, 1);
    btnRow->addWidget(btnConfirm, 1);
    layout->addLayout(btnRow);

    dlg.bodyLayout()->addLayout(layout);
    normalizeFormWidgets(&dlg);
    return dlg.exec() == QDialog::Accepted;
}

static void showCustomAlertDialog(QWidget* parent, const QString& title, const QString& message, const QString& accentColor) {
    CustomDialog dlg(title, parent);
    dlg.setFixedWidth(390);

    auto* layout = new QVBoxLayout();
    layout->setContentsMargins(10, 12, 10, 8);
    layout->setSpacing(18);

    auto* msgLbl = new QLabel(message, &dlg);
    msgLbl->setWordWrap(true);
    msgLbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    msgLbl->setStyleSheet(QString("color: %1; font-size: 14px; line-height: 1.5; background: transparent; border: none;").arg(accentColor));
    layout->addWidget(msgLbl);

    auto* btnRow = new QHBoxLayout();
    auto* btnOk = new QPushButton("知道了", &dlg);
    btnOk->setObjectName("primaryBtn");
    btnOk->setCursor(Qt::PointingHandCursor);
    QObject::connect(btnOk, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnRow->addWidget(btnOk, 1);
    layout->addLayout(btnRow);

    dlg.bodyLayout()->addLayout(layout);
    CustomDialog::normalizeFormWidgets(&dlg);
    dlg.exec();
}

void CustomDialog::showInfo(QWidget* parent, const QString& title, const QString& message) {
    showCustomAlertDialog(parent, title, message, "#e1e1e5");
}

void CustomDialog::showWarning(QWidget* parent, const QString& title, const QString& message) {
    showCustomAlertDialog(parent, title, message, "#be782e");
}

void CustomDialog::showError(QWidget* parent, const QString& title, const QString& message) {
    showCustomAlertDialog(parent, title, message, "#fa5151");
}

void CustomDialog::markFieldInvalid(QWidget* widget) {
    if (!widget) return;
    widget->setFocus();

    QWidget* parentFrame = widget->parentWidget();
    bool isInRowBox = parentFrame && (parentFrame->objectName() == "inlineFormRowBox");
    QWidget* targetBox = isInRowBox ? parentFrame : widget;

    QString errorStyle;
    if (isInRowBox) {
        errorStyle = "QFrame#inlineFormRowBox { background-color: #3d1418; border: 1px solid #fa5151; border-radius: 3px; }"
                     "QFrame#inlineFormRowBox QLineEdit, QFrame#inlineFormRowBox QSpinBox, QFrame#inlineFormRowBox QDoubleSpinBox, "
                     "QFrame#inlineFormRowBox QDateEdit, QFrame#inlineFormRowBox QDateTimeEdit, QFrame#inlineFormRowBox QComboBox, "
                     "QFrame#inlineFormRowBox QPlainTextEdit, QFrame#inlineFormRowBox QTextEdit { "
                     "    background: transparent; border: none; color: #ff8888; font-size: 13px; padding: 2px 4px; "
                     "}";
    } else {
        errorStyle = "background-color: #3d1418; color: #ff8888; border: 1px solid #fa5151; border-radius: 3px;";
    }

    targetBox->setStyleSheet(errorStyle);

    // 用户重新输入或修改值时，自动恢复正常样式
    if (!widget->property("invalidListenerInstalled").toBool()) {
        widget->setProperty("invalidListenerInstalled", true);
        if (auto* le = qobject_cast<QLineEdit*>(widget)) {
            QObject::connect(le, &QLineEdit::textChanged, le, [widget]() {
                CustomDialog::resetFieldInvalid(widget);
            });
        } else if (auto* pte = qobject_cast<QPlainTextEdit*>(widget)) {
            QObject::connect(pte, &QPlainTextEdit::textChanged, pte, [widget]() {
                CustomDialog::resetFieldInvalid(widget);
            });
        } else if (auto* te = qobject_cast<QTextEdit*>(widget)) {
            QObject::connect(te, &QTextEdit::textChanged, te, [widget]() {
                CustomDialog::resetFieldInvalid(widget);
            });
        } else if (auto* sb = qobject_cast<QDoubleSpinBox*>(widget)) {
            QObject::connect(sb, QOverload<double>::of(&QDoubleSpinBox::valueChanged), sb, [widget]() {
                CustomDialog::resetFieldInvalid(widget);
            });
        } else if (auto* isb = qobject_cast<QSpinBox*>(widget)) {
            QObject::connect(isb, QOverload<int>::of(&QSpinBox::valueChanged), isb, [widget]() {
                CustomDialog::resetFieldInvalid(widget);
            });
        }
    }
}

void CustomDialog::resetFieldInvalid(QWidget* widget) {
    if (!widget) return;
    QWidget* parentFrame = widget->parentWidget();
    bool isInRowBox = parentFrame && (parentFrame->objectName() == "inlineFormRowBox");
    QWidget* targetBox = isInRowBox ? parentFrame : widget;

    if (isInRowBox) {
        targetBox->setStyleSheet(
            "QFrame#inlineFormRowBox { background-color: #18181b; border: 1px solid #28282b; border-radius: 3px; }"
            "QFrame#inlineFormRowBox QLineEdit, QFrame#inlineFormRowBox QSpinBox, QFrame#inlineFormRowBox QDoubleSpinBox, "
            "QFrame#inlineFormRowBox QDateEdit, QFrame#inlineFormRowBox QDateTimeEdit, QFrame#inlineFormRowBox QComboBox, "
            "QFrame#inlineFormRowBox QPlainTextEdit, QFrame#inlineFormRowBox QTextEdit { "
            "    background: transparent; border: none; color: #e1e1e5; font-size: 13px; padding: 2px 4px; "
            "}"
        );
    } else {
        targetBox->setStyleSheet("");
    }
}

void CustomDialog::showEvent(QShowEvent* event) {
    normalizeFormWidgets(this);
    QDialog::showEvent(event);
}

void CustomDialog::setDialogTitle(const QString& title) {
    if (m_titleLabel) {
        m_titleLabel->setText(title);
    }
}

void CustomDialog::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (m_topBarWidget && m_topBarWidget->geometry().contains(event->pos())) {
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

void CustomDialog::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton && !m_dragPosition.isNull()) {
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

