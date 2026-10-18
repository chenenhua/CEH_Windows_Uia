#include "FinancialLedgerWidget.h"
#include "UnifiedTransactionDialog.h"
#include "CustomDialog.h"
#include "UiIcons.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QButtonGroup>
#include <QDateEdit>
#include <QMessageBox>
#include <QDate>
#include <QEvent>
#include <QTimer>
#include <QMenu>
#include <QSqlQuery>

FinancialLedgerWidget::FinancialLedgerWidget(QWidget* parent) : QWidget(parent) {
    m_currentYear = QDate::currentDate().year();
    m_currentMonth = QDate::currentDate().month();
    m_currentDay = 0; // 默认展示当月全部流水与日记，点击具体几号则筛选到该日
    setupUi();
    refreshData();
}

void FinancialLedgerWidget::setupUi() {
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // ==================== Left Column: 个人/公司 + 统计 + 年/月/日选择 ====================
    auto* colLeft = new QWidget(this);
    colLeft->setFixedWidth(330);
    colLeft->setObjectName("macCard");
    setupLeftStatsColumn(colLeft);
    mainLayout->addWidget(colLeft);

    // 竖分割线 1
    auto* vline1 = new QFrame(this);
    vline1->setObjectName("vDivider");
    vline1->setFixedWidth(1);
    vline1->setStyleSheet("background-color: #28282b; border: none;");
    mainLayout->addWidget(vline1);

    // ==================== Middle Column: 搜索 + 6列无表头流水表 ====================
    auto* colMid = new QWidget(this);
    colMid->setObjectName("macCard");
    setupMiddleTableColumn(colMid);
    mainLayout->addWidget(colMid, 1);

    // 竖分割线 2
    auto* vline2 = new QFrame(this);
    vline2->setObjectName("vDivider");
    vline2->setFixedWidth(1);
    vline2->setStyleSheet("background-color: #28282b; border: none;");
    mainLayout->addWidget(vline2);

    // ==================== Right Column: 日记 + +添加 ====================
    auto* colRight = new QWidget(this);
    colRight->setFixedWidth(330);
    colRight->setObjectName("macCard");
    setupRightDiaryColumn(colRight);
    mainLayout->addWidget(colRight);
}

void FinancialLedgerWidget::setupLeftStatsColumn(QWidget* parent) {
    auto* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    // 1. 顶部: [个人] [公司] ... [+ 添加] (参考图2: 下方带分割线)
    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(8);

    QString tabBtnStyle =
        "QPushButton { background-color: #1c1c1f; color: #8e8e93; border: 1px solid #28282b; border-radius: 3px; padding: 5px 16px; font-size: 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #242428; color: #e1e1e5; }"
        "QPushButton:checked { background-color: #00a361; color: #ffffff; border-color: #00a361; font-weight: bold; }";

    m_btnPersonalBook = new QPushButton("个人", parent);
    m_btnPersonalBook->setCheckable(true);
    m_btnPersonalBook->setChecked(true);
    m_btnPersonalBook->setCursor(Qt::PointingHandCursor);
    m_btnPersonalBook->setStyleSheet(tabBtnStyle);

    m_btnCompanyBook = new QPushButton("工作", parent);
    m_btnCompanyBook->setCheckable(true);
    m_btnCompanyBook->setCursor(Qt::PointingHandCursor);
    m_btnCompanyBook->setStyleSheet(tabBtnStyle);

    connect(m_btnPersonalBook, &QPushButton::clicked, this, [this]() { onBookToggled(true); });
    connect(m_btnCompanyBook, &QPushButton::clicked, this, [this]() { onBookToggled(false); });

    m_btnAddTrans = new QPushButton("+ 添加", parent);
    m_btnAddTrans->setCursor(Qt::PointingHandCursor);
    m_btnAddTrans->setStyleSheet(
        "QPushButton { background: transparent; color: #00a361; border: none; font-size: 12px; font-weight: bold; padding: 4px 6px; }"
        "QPushButton:hover { color: #00c474; }"
    );
    connect(m_btnAddTrans, &QPushButton::clicked, this, &FinancialLedgerWidget::onAddTransactionClicked);

    topRow->addWidget(m_btnPersonalBook);
    topRow->addWidget(m_btnCompanyBook);
    topRow->addStretch();
    topRow->addWidget(m_btnAddTrans);
    layout->addLayout(topRow);

    // 个人 | 公司 | +添加 下边缘分割线 (#28282b)
    auto* topDivider = new QFrame(parent);
    topDivider->setFixedHeight(1);
    topDivider->setStyleSheet("background-color: #28282b; border: none;");
    layout->addWidget(topDivider);

    // 辅助函数: 创建 3x2 收支总结网格 (严格按参考图2顺序与配色)
    // Row 0: 收入 (#00a361) | 盈收 (#00a361 / #fa5151)
    // Row 1: 应收 (#be782e) | 支出 (#fa5151)
    // Row 2: 欠款 (#fa5151) | 朋友 (#8e8e93)
    auto createSummaryGrid = [parent](QLabel*& inc, QLabel*& sur, QLabel*& rec, QLabel*& exp, QLabel*& debt, QLabel*& frd) {
        auto* grid = new QGridLayout();
        grid->setContentsMargins(0, 2, 0, 2);
        grid->setHorizontalSpacing(10);
        grid->setVerticalSpacing(6);
        grid->setColumnStretch(0, 1);
        grid->setColumnStretch(1, 1);

        auto makeCell = [parent, grid](int r, int c, QLabel*& lbl) {
            lbl = new QLabel(parent);
            lbl->setTextFormat(Qt::RichText);
            lbl->setStyleSheet("font-size: 12px; background: transparent;");
            grid->addWidget(lbl, r, c);
        };

        makeCell(0, 0, inc);
        makeCell(0, 1, sur);
        makeCell(1, 0, rec);
        makeCell(1, 1, exp);
        makeCell(2, 0, debt);
        makeCell(2, 1, frd);
        return grid;
    };

    // 年 / 月 / 日 按钮无背景框样式: 默认 #8e8e93，选中 #e1e1e5 粗体
    QString dateSelectorBtnStyle =
        "QPushButton { background: transparent; color: #8e8e93; border: none; padding: 3px 2px; font-size: 12px; text-align: left; }"
        "QPushButton:hover { color: #e1e1e5; }"
        "QPushButton:checked { background: transparent; color: #e1e1e5; font-weight: bold; }";

    // ==================== 2. 年度汇总 + 年份选择 ====================
    layout->addLayout(createSummaryGrid(
        m_lblYearIncome, m_lblYearSurplus, m_lblYearReceivable,
        m_lblYearExpense, m_lblYearDebt, m_lblYearFriends
    ));

    m_yearGridLayout = new QGridLayout();
    m_yearGridLayout->setContentsMargins(0, 2, 0, 2);
    m_yearGridLayout->setHorizontalSpacing(6);
    m_yearGridLayout->setVerticalSpacing(6);
    layout->addLayout(m_yearGridLayout);
    rebuildYearButtons();

    auto* sep1 = new QFrame(parent);
    sep1->setFixedHeight(1);
    sep1->setStyleSheet("background-color: #28282b; border: none;");
    layout->addWidget(sep1);

    // ==================== 3. 月度汇总 + 月份选择 (1月..12月) ====================
    layout->addLayout(createSummaryGrid(
        m_lblMonthIncome, m_lblMonthSurplus, m_lblMonthReceivable,
        m_lblMonthExpense, m_lblMonthDebt, m_lblMonthFriends
    ));

    auto* monthGrid = new QGridLayout();
    monthGrid->setContentsMargins(0, 2, 0, 2);
    monthGrid->setHorizontalSpacing(6);
    monthGrid->setVerticalSpacing(6);

    for (int m = 1; m <= 12; ++m) {
        auto* btn = new QPushButton(QString("%1月").arg(m), parent);
        btn->setCheckable(true);
        btn->setChecked(m == m_currentMonth);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("monthVal", m);
        btn->setStyleSheet(dateSelectorBtnStyle);
        connect(btn, &QPushButton::clicked, this, [this, m]() {
            if (m_currentMonth == m) onMonthClicked(0);
            else onMonthClicked(m);
        });
        m_monthButtons.append(btn);
        monthGrid->addWidget(btn, (m - 1) / 6, (m - 1) % 6);
    }
    layout->addLayout(monthGrid);

    auto* sep2 = new QFrame(parent);
    sep2->setFixedHeight(1);
    sep2->setStyleSheet("background-color: #28282b; border: none;");
    layout->addWidget(sep2);

    // ==================== 4. 日度汇总 + 日期选择 (1号..31号) ====================
    layout->addLayout(createSummaryGrid(
        m_lblDayIncome, m_lblDaySurplus, m_lblDayReceivable,
        m_lblDayExpense, m_lblDayDebt, m_lblDayFriends
    ));

    m_daysGridContainer = new QWidget(parent);
    m_daysGridLayout = new QGridLayout(m_daysGridContainer);
    m_daysGridLayout->setContentsMargins(0, 2, 0, 2);
    m_daysGridLayout->setHorizontalSpacing(6);
    m_daysGridLayout->setVerticalSpacing(6);
    layout->addWidget(m_daysGridContainer);

    rebuildDaysGrid();
    layout->addStretch();
}

void FinancialLedgerWidget::setupMiddleTableColumn(QWidget* parent) {
    auto* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    // 3. 搜索框：只保留下边框 (#28282b)
    m_searchEdit = new QLineEdit(parent);
    m_searchEdit->setPlaceholderText("搜索流水..");
    m_searchEdit->setFixedHeight(32);
    m_searchEdit->setStyleSheet(
        "QLineEdit { background-color: transparent; color: #e1e1e5; border: none; border-bottom: 1px solid #28282b; border-radius: 0px; padding: 0 6px; font-size: 12px; }"
        "QLineEdit:focus { border-bottom: 1px solid #00a361; }"
    );
    connect(m_searchEdit, &QLineEdit::textChanged, this, &FinancialLedgerWidget::onSearchChanged);
    layout->addWidget(m_searchEdit);

    // 4. 流水表格：无表头 6 列，严格按 15% | 10% | 10% | 5% | 30% | 30% 比例分配
    m_transTable = new QTableWidget(parent);
    m_transTable->setColumnCount(6);
    m_transTable->horizontalHeader()->setVisible(false);
    m_transTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_transTable->verticalHeader()->setVisible(false);
    m_transTable->verticalHeader()->setDefaultSectionSize(36);
    m_transTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_transTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_transTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_transTable->setShowGrid(false);
    m_transTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_transTable->setFocusPolicy(Qt::NoFocus);
    m_transTable->setStyleSheet(
        "QTableWidget { background-color: #141416; color: #8e8e93; border: none; font-size: 12px; outline: none; }"
        "QTableWidget::item { background-color: transparent; border: none; padding: 4px; }"
        "QTableWidget::item:selected { background-color: #1c1c20; }"
    );
    m_transTable->viewport()->installEventFilter(this);
    m_transTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_transTable, &QTableWidget::customContextMenuRequested, this, &FinancialLedgerWidget::onTransTableContextMenu);

    layout->addWidget(m_transTable, 1);
}

void FinancialLedgerWidget::setupRightDiaryColumn(QWidget* parent) {
    auto* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    // 5. 顶部标题: 日记 (14px 粗体 + 单色图标) ... + 添加 (参考图3: 下方带下划线)
    auto* topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(6);

    auto* iconLbl = new QLabel(parent);
    iconLbl->setPixmap(UiIcons::pixmap(UiIcons::Code::Diary, 14, QColor("#8e8e93")));

    auto* titleLbl = new QLabel("日记", parent);
    titleLbl->setStyleSheet("color: #e1e1e5; font-size: 14px; font-weight: bold;");

    m_btnAddDiary = new QPushButton("+ 添加", parent);
    m_btnAddDiary->setCursor(Qt::PointingHandCursor);
    m_btnAddDiary->setStyleSheet(
        "QPushButton { background: transparent; color: #00a361; border: none; font-size: 12px; font-weight: bold; padding: 4px 6px; }"
        "QPushButton:hover { color: #00c474; }"
    );
    connect(m_btnAddDiary, &QPushButton::clicked, this, &FinancialLedgerWidget::onAddDiaryClicked);

    topRow->addWidget(iconLbl);
    topRow->addWidget(titleLbl);
    topRow->addStretch();
    topRow->addWidget(m_btnAddDiary);
    layout->addLayout(topRow);

    // 日记 与 +添加 下边缘分割线 (#28282b)
    auto* diaryDivider = new QFrame(parent);
    diaryDivider->setFixedHeight(1);
    diaryDivider->setStyleSheet("background-color: #28282b; border: none;");
    layout->addWidget(diaryDivider);

    // 日记滚动列表 (参考图3: 上行日期 #8e8e93，下行内容 #e1e1e5)
    auto* scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("background: transparent; border: none;");

    auto* contentWidget = new QWidget(scroll);
    contentWidget->setStyleSheet("background: transparent;");
    m_diaryListLayout = new QVBoxLayout(contentWidget);
    m_diaryListLayout->setContentsMargins(0, 4, 0, 4);
    m_diaryListLayout->setSpacing(14);
    m_diaryListLayout->addStretch();

    scroll->setWidget(contentWidget);
    layout->addWidget(scroll, 1);
}

bool FinancialLedgerWidget::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::Resize && m_transTable && watched == m_transTable->viewport()) {
        applyTableColumnRatios();
    }
    return QWidget::eventFilter(watched, event);
}

void FinancialLedgerWidget::applyTableColumnRatios() {
    // 严格落实财务表格 6 列比例: 15% | 10% | 10% | 5% | 30% | 30%
    if (!m_transTable || m_transTable->viewport()->width() <= 60) return;
    int w = m_transTable->viewport()->width();
    int c0 = static_cast<int>(w * 0.15);
    int c1 = static_cast<int>(w * 0.10);
    int c2 = static_cast<int>(w * 0.10);
    int c3 = static_cast<int>(w * 0.05);
    int c4 = static_cast<int>(w * 0.30);
    int c5 = w - c0 - c1 - c2 - c3 - c4;

    m_transTable->setColumnWidth(0, c0);
    m_transTable->setColumnWidth(1, c1);
    m_transTable->setColumnWidth(2, c2);
    m_transTable->setColumnWidth(3, c3);
    m_transTable->setColumnWidth(4, c4);
    m_transTable->setColumnWidth(5, c5);
}

void FinancialLedgerWidget::rebuildDaysGrid() {
    while (m_daysGridLayout->count() > 0) {
        auto* item = m_daysGridLayout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    m_dayButtons.clear();

    int daysInMonth = 31;
    if (m_currentMonth >= 1 && m_currentMonth <= 12) {
        QDate dt(m_currentYear, m_currentMonth, 1);
        if (dt.isValid()) daysInMonth = dt.daysInMonth();
    }

    QString dayBtnStyle =
        "QPushButton { background: transparent; color: #8e8e93; border: none; padding: 3px 2px; font-size: 12px; text-align: left; }"
        "QPushButton:hover { color: #e1e1e5; }"
        "QPushButton:checked { background: transparent; color: #e1e1e5; font-weight: bold; }";

    for (int d = 1; d <= daysInMonth; ++d) {
        auto* btn = new QPushButton(QString("%1号").arg(d), m_daysGridContainer);
        btn->setCheckable(true);
        btn->setChecked(d == m_currentDay);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("dayVal", d);
        btn->setStyleSheet(dayBtnStyle);
        connect(btn, &QPushButton::clicked, this, [this, d]() {
            if (m_currentDay == d) onDayClicked(0);
            else onDayClicked(d);
        });
        m_dayButtons.append(btn);
        m_daysGridLayout->addWidget(btn, (d - 1) / 6, (d - 1) % 6);
    }
}

static void fillMetricsLabels(const LedgerMetrics& m,
                              QLabel* inc, QLabel* sur, QLabel* rec,
                              QLabel* exp, QLabel* debt, QLabel* frd) {
    // 严格遵守总结配色：营收(#00a361)，亏本/欠款/支出(#fa5151)，可收回来的/应收(#be782e)
    inc->setText(QString("<span style='color:#8e8e93;'>收入: </span><span style='color:#00a361;'>¥%1</span>")
                     .arg(m.income, 0, 'f', 2));

    QString surColor = (m.surplus >= 0.0) ? "#00a361" : "#fa5151";
    QString surPrefix = (m.surplus >= 0.0) ? "" : "-";
    sur->setText(QString("<span style='color:#8e8e93;'>盈收: </span><span style='color:%1;'>%2¥%3</span>")
                     .arg(surColor, surPrefix)
                     .arg(qAbs(m.surplus), 0, 'f', 2));

    rec->setText(QString("<span style='color:#8e8e93;'>应收: </span><span style='color:#be782e;'>¥%1</span>")
                     .arg(m.receivable, 0, 'f', 2));

    exp->setText(QString("<span style='color:#8e8e93;'>支出: </span><span style='color:#fa5151;'>¥%1</span>")
                     .arg(m.expense, 0, 'f', 2));

    debt->setText(QString("<span style='color:#8e8e93;'>欠款: </span><span style='color:#fa5151;'>¥%1</span>")
                      .arg(m.debt, 0, 'f', 2));

    frd->setText(QString("<span style='color:#8e8e93;'>朋友: %1位</span>")
                     .arg(m.friendCount));
}

void FinancialLedgerWidget::rebuildYearButtons() {
    if (!m_yearGridLayout) return;

    while (m_yearGridLayout->count() > 0) {
        auto* item = m_yearGridLayout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    m_yearButtons.clear();

    QString dateSelectorBtnStyle =
        "QPushButton { background: transparent; color: #8e8e93; border: none; padding: 3px 2px; font-size: 12px; text-align: left; }"
        "QPushButton:hover { color: #e1e1e5; }"
        "QPushButton:checked { background: transparent; color: #e1e1e5; font-weight: bold; }";

    // 只有真正有数据的年份才显示出来，如 2026, 2025, 2022，不全部盲目排序
    QList<int> years = DatabaseManager::instance().getAvailableTransactionYears();
    if (years.isEmpty()) {
        years.append(QDate::currentDate().year());
    }

    if (!years.contains(m_currentYear)) {
        m_currentYear = years.first();
    }

    // 5 列一行，整齐排布
    for (int i = 0; i < years.size(); ++i) {
        int y = years[i];
        auto* btn = new QPushButton(QString("%1年").arg(y), this);
        btn->setCheckable(true);
        btn->setChecked(y == m_currentYear);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("yearVal", y);
        btn->setStyleSheet(dateSelectorBtnStyle);
        connect(btn, &QPushButton::clicked, this, [this, y]() { onYearClicked(y); });
        m_yearButtons.append(btn);
        m_yearGridLayout->addWidget(btn, i / 5, i % 5);
    }
}

void FinancialLedgerWidget::refreshData() {
    rebuildYearButtons();
    updateYearMetrics();
    updateMonthMetrics();
    updateDayMetrics();
    loadTransactionsTable();
    loadDiaryList();
}

void FinancialLedgerWidget::updateYearMetrics() {
    // 1. 点年选择上面 是 总账 (所有年份不限，year=0, month=0, day=0)
    auto m = DatabaseManager::instance().getPeriodMetrics(m_currentBook, 0, 0, 0);
    fillMetricsLabels(m, m_lblYearIncome, m_lblYearSurplus, m_lblYearReceivable,
                      m_lblYearExpense, m_lblYearDebt, m_lblYearFriends);
}

void FinancialLedgerWidget::updateMonthMetrics() {
    // 2. 月选择 上面 是 今年总结 (当前选中年份全年汇总，year=m_currentYear, month=0, day=0)
    auto m = DatabaseManager::instance().getPeriodMetrics(m_currentBook, m_currentYear, 0, 0);
    fillMetricsLabels(m, m_lblMonthIncome, m_lblMonthSurplus, m_lblMonthReceivable,
                      m_lblMonthExpense, m_lblMonthDebt, m_lblMonthFriends);
}

void FinancialLedgerWidget::updateDayMetrics() {
    // 3. 日选择 上面 是 今月总结 (当前选中月份全月汇总，year=m_currentYear, month=m_currentMonth, day=0)
    int targetMonth = (m_currentMonth > 0) ? m_currentMonth : QDate::currentDate().month();
    auto m = DatabaseManager::instance().getPeriodMetrics(m_currentBook, m_currentYear, targetMonth, 0);
    fillMetricsLabels(m, m_lblDayIncome, m_lblDaySurplus, m_lblDayReceivable,
                      m_lblDayExpense, m_lblDayDebt, m_lblDayFriends);
}

void FinancialLedgerWidget::loadTransactionsTable() {
    QString search = m_searchEdit ? m_searchEdit->text().trimmed() : "";
    auto list = DatabaseManager::instance().getLedgerTransactions(m_currentBook, m_currentYear, m_currentMonth, m_currentDay, search);

    m_transTable->setRowCount(0);
    for (int i = 0; i < list.size(); ++i) {
        const auto& r = list[i];
        m_transTable->insertRow(i);

        // Col 0 (15%): 时间 (2026年10月09日)
        QString rawTime = r["trans_time"].toString();
        QDate dt = QDate::fromString(rawTime.left(10), "yyyy-MM-dd");
        QString dateDisplay = dt.isValid() ? dt.toString("yyyy年MM月dd日") : rawTime.left(10);
        auto* item0 = new QTableWidgetItem(dateDisplay);
        item0->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        item0->setForeground(QColor("#8e8e93"));
        item0->setData(Qt::UserRole, r["id"].toInt());

        // Col 1 (10%): 分类
        QString subType = r["sub_type"].toString();
        QString catStr = subType.isEmpty() ? r["category"].toString() : subType;
        auto* item1 = new QTableWidgetItem(catStr);
        item1->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        item1->setForeground(QColor("#8e8e93"));

        // Col 2 (10%): 金额 (收入 #00a361, 支出 #fa5151)
        bool isIncome = (r["trans_type"].toString() == "INCOME");
        double amt = r["amount"].toDouble();
        auto* item2 = new QTableWidgetItem(QString("%1¥%2").arg(isIncome ? "+" : "-").arg(amt, 0, 'f', 2));
        item2->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        item2->setForeground(isIncome ? QColor("#00a361") : QColor("#fa5151"));

        // Col 3 (5%): 关联人
        QString relStr = r["contact_name"].toString();
        if (relStr.isEmpty()) relStr = "—";
        auto* item3 = new QTableWidgetItem(relStr);
        item3->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        item3->setForeground(QColor("#8e8e93"));

        // Col 4 (30%): 事项摘要
        QString projStr = r["project_name"].toString();
        if (projStr.isEmpty()) {
            projStr = QString("%1·%2").arg(r["category"].toString(), catStr);
        }
        auto* item4 = new QTableWidgetItem(projStr);
        item4->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        item4->setForeground(QColor("#e1e1e5"));

        // Col 5 (30%): 备注说明
        auto* item5 = new QTableWidgetItem(r["remark"].toString());
        item5->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        item5->setForeground(QColor("#8e8e93"));

        m_transTable->setItem(i, 0, item0);
        m_transTable->setItem(i, 1, item1);
        m_transTable->setItem(i, 2, item2);
        m_transTable->setItem(i, 3, item3);
        m_transTable->setItem(i, 4, item4);
        m_transTable->setItem(i, 5, item5);
    }

    QTimer::singleShot(0, this, &FinancialLedgerWidget::applyTableColumnRatios);
}

void FinancialLedgerWidget::loadDiaryList() {
    while (m_diaryListLayout->count() > 1) {
        auto* item = m_diaryListLayout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    QString search = m_searchEdit ? m_searchEdit->text().trimmed() : "";
    auto diaries = DatabaseManager::instance().getDiariesFiltered(
        m_currentBook, m_currentYear, m_currentMonth, m_currentDay, search
    );

    if (diaries.isEmpty()) {
        auto* emptyLbl = new QLabel("当前所选日期范围暂无日记记录", this);
        emptyLbl->setStyleSheet("color: #8e8e93; font-size: 12px; padding: 6px 0;");
        m_diaryListLayout->insertWidget(0, emptyLbl);
        return;
    }

    for (const auto& d : diaries) {
        int diaryId = d["id"].toInt();
        QString dateStr = d["diary_date"].toString();
        QString contentStr = d["content"].toString();
        QString contactName = d["contact_name"].toString().trimmed();
        QString moodStr = d["mood"].toString().trimmed();
        if (moodStr.isEmpty()) moodStr = "记录";

        // 上行日期 (#8e8e93)，中行内容 (#e1e1e5)，下行带图标展示【关系到什么人 + 状态怎么样】
        auto* itemWidget = new QWidget();
        itemWidget->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(itemWidget, &QWidget::customContextMenuRequested, this, [this, diaryId, itemWidget](const QPoint& pos) {
            QMenu menu(this);
            menu.setStyleSheet(
                "QMenu { background-color: #202024; border: 1px solid #3f3f46; padding: 4px; border-radius: 4px; }"
                "QMenu::item { color: #e1e1e5; padding: 6px 18px; border-radius: 3px; font-size: 13px; }"
                "QMenu::item:selected { background-color: #2b2b30; color: #ffffff; }"
            );
            QAction* actEdit = menu.addAction(UiIcons::icon(UiIcons::Code::Diary, 12, QColor("#be782e")), "编辑");
            QAction* actDelete = menu.addAction(UiIcons::icon(UiIcons::Code::Delete, 12, QColor("#fa5151")), "删除");
            QAction* selected = menu.exec(itemWidget->mapToGlobal(pos));

            if (selected == actEdit) {
                onEditDiaryClicked(diaryId);
            } else if (selected == actDelete) {
                onDeleteDiaryClicked(diaryId);
            }
        });

        auto* itemLayout = new QVBoxLayout(itemWidget);
        itemLayout->setContentsMargins(0, 4, 0, 8);
        itemLayout->setSpacing(5);

        auto* dateLbl = new QLabel(dateStr, itemWidget);
        dateLbl->setStyleSheet("color: #8e8e93; font-size: 12px;");
        dateLbl->setAttribute(Qt::WA_TransparentForMouseEvents);

        auto* contentLbl = new QLabel(contentStr, itemWidget);
        contentLbl->setStyleSheet("color: #e1e1e5; font-size: 12px; line-height: 1.5;");
        contentLbl->setWordWrap(true);
        contentLbl->setAttribute(Qt::WA_TransparentForMouseEvents);

        // 下行：关联人图标 + 关联对象 | 状态图标 + 状态
        auto* metaRow = new QHBoxLayout();
        metaRow->setContentsMargins(0, 2, 0, 0);
        metaRow->setSpacing(6);

        QColor contactColor = contactName.isEmpty() ? QColor("#8e8e93") : QColor("#0a84ff");
        auto* contactIconLbl = new QLabel(itemWidget);
        contactIconLbl->setPixmap(UiIcons::pixmap(UiIcons::Code::Relation, 12, contactColor));
        contactIconLbl->setFixedSize(14, 14);
        contactIconLbl->setAttribute(Qt::WA_TransparentForMouseEvents);

        auto* contactTextLbl = new QLabel(
            QString("关联: %1").arg(contactName.isEmpty() ? "未关联人脉" : contactName),
            itemWidget
        );
        contactTextLbl->setStyleSheet(QString("color: %1; font-size: 11px;").arg(contactColor.name()));
        contactTextLbl->setAttribute(Qt::WA_TransparentForMouseEvents);

        QColor moodColor("#be782e");
        if (moodStr == "欣喜" || moodStr == "振奋" || moodStr == "从容" ||
            moodStr == "笃定" || moodStr == "期待" || moodStr == "推进" || moodStr == "签约") {
            moodColor = QColor("#00a361");
        } else if (moodStr == "焦虑" || moodStr == "羞愧" || moodStr == "承压" || moodStr == "维权") {
            moodColor = QColor("#fa5151");
        }

        auto* moodIconLbl = new QLabel(itemWidget);
        moodIconLbl->setPixmap(UiIcons::pixmap(UiIcons::Code::Diary, 12, moodColor));
        moodIconLbl->setFixedSize(14, 14);
        moodIconLbl->setAttribute(Qt::WA_TransparentForMouseEvents);

        auto* moodTextLbl = new QLabel(QString("状态: %1").arg(moodStr), itemWidget);
        moodTextLbl->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold;").arg(moodColor.name()));
        moodTextLbl->setAttribute(Qt::WA_TransparentForMouseEvents);

        metaRow->addWidget(contactIconLbl);
        metaRow->addWidget(contactTextLbl);
        metaRow->addSpacing(12);
        metaRow->addWidget(moodIconLbl);
        metaRow->addWidget(moodTextLbl);
        metaRow->addStretch();

        auto* sepLine = new QFrame(itemWidget);
        sepLine->setFixedHeight(1);
        sepLine->setStyleSheet("background-color: #202024; border: none;");

        itemLayout->addWidget(dateLbl);
        itemLayout->addWidget(contentLbl);
        itemLayout->addLayout(metaRow);
        itemLayout->addWidget(sepLine);

        m_diaryListLayout->insertWidget(m_diaryListLayout->count() - 1, itemWidget);
    }
}

void FinancialLedgerWidget::onBookToggled(bool isPersonal) {
    m_currentBook = isPersonal ? "PERSONAL" : "COMPANY";
    m_btnPersonalBook->setChecked(isPersonal);
    m_btnCompanyBook->setChecked(!isPersonal);
    refreshData();
}

void FinancialLedgerWidget::onYearClicked(int year) {
    m_currentYear = year;
    m_currentDay = 0;
    for (auto* btn : m_yearButtons) {
        btn->setChecked(btn->property("yearVal").toInt() == year);
    }
    rebuildDaysGrid();
    updateYearMetrics();
    updateMonthMetrics();
    updateDayMetrics();
    loadTransactionsTable();
    loadDiaryList();
}

void FinancialLedgerWidget::onMonthClicked(int month) {
    m_currentMonth = month;
    m_currentDay = 0;
    for (auto* btn : m_monthButtons) {
        btn->setChecked(btn->property("monthVal").toInt() == month);
    }
    rebuildDaysGrid();
    updateMonthMetrics();
    updateDayMetrics();
    loadTransactionsTable();
    loadDiaryList();
}

void FinancialLedgerWidget::onDayClicked(int day) {
    m_currentDay = day;
    for (auto* btn : m_dayButtons) {
        btn->setChecked(btn->property("dayVal").toInt() == day);
    }
    updateDayMetrics();
    loadTransactionsTable();
    loadDiaryList();
}

void FinancialLedgerWidget::onAddTransactionClicked() {
    QString defaultBook = (m_currentBook == "PERSONAL") ? "PERSONAL" : "COMPANY";
    UnifiedTransactionDialog dlg("EXPENSE", this, false, defaultBook, true);
    if (dlg.exec() == QDialog::Accepted) {
        refreshData();
    }
}

void FinancialLedgerWidget::onAddDiaryClicked() {
    CustomDialog dlg("添加日记", this);
    dlg.setFixedWidth(460);

    auto* layout = new QVBoxLayout();
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    auto* form = new QFormLayout();
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // 1. 账本类型选择 (book_type: 个人 / 工作)
    auto* bookRow = new QHBoxLayout();
    bookRow->setSpacing(10);
    auto* btnPersonal = new QPushButton("个人", &dlg);
    auto* btnWork = new QPushButton("工作", &dlg);
    btnPersonal->setCheckable(true);
    btnWork->setCheckable(true);
    btnPersonal->setCursor(Qt::PointingHandCursor);
    btnWork->setCursor(Qt::PointingHandCursor);
    btnPersonal->setFixedHeight(32);
    btnWork->setFixedHeight(32);

    bool initIsPersonal = (m_currentBook != "COMPANY");
    btnPersonal->setChecked(initIsPersonal);
    btnWork->setChecked(!initIsPersonal);

    QString toggleBtnStyle =
        "QPushButton { background-color: #202024; color: #8e8e93; border: 1px solid #2c2c2e; border-radius: 4px; font-size: 12px; }"
        "QPushButton:checked { background-color: #00a361; color: #ffffff; border: 1px solid #00a361; font-weight: bold; }";
    btnPersonal->setStyleSheet(toggleBtnStyle);
    btnWork->setStyleSheet(toggleBtnStyle);

    bookRow->addWidget(btnPersonal, 1);
    bookRow->addWidget(btnWork, 1);
    form->addRow("日记账本", bookRow);

    // 2. 状态选择 (mood 表各状态：随“个人”/“工作”动态切换)
    auto* moodCombo = new QComboBox(&dlg);
    auto populateMoods = [moodCombo](bool isPersonal) {
        QString prev = moodCombo->currentText();
        moodCombo->clear();
        QStringList moods;
        if (isPersonal) {
            moods = {"平静", "欣喜", "振奋", "从容", "笃定", "期待", "焦虑", "羞愧", "记录"};
        } else {
            moods = {"记录", "振奋", "推进", "承压", "签约", "维权", "复盘", "平静"};
        }
        moodCombo->addItems(moods);
        int idx = moodCombo->findText(prev);
        if (idx >= 0) moodCombo->setCurrentIndex(idx);
    };
    populateMoods(initIsPersonal);
    form->addRow("当前状态", moodCombo);

    connect(btnPersonal, &QPushButton::clicked, &dlg, [btnPersonal, btnWork, populateMoods]() {
        btnPersonal->setChecked(true);
        btnWork->setChecked(false);
        populateMoods(true);
    });
    connect(btnWork, &QPushButton::clicked, &dlg, [btnPersonal, btnWork, populateMoods]() {
        btnWork->setChecked(true);
        btnPersonal->setChecked(false);
        populateMoods(false);
    });

    QDate initDate = QDate::currentDate();
    if (m_currentYear > 0) {
        int m = (m_currentMonth > 0) ? m_currentMonth : initDate.month();
        int d = (m_currentDay > 0) ? m_currentDay : initDate.day();
        QDate candidate(m_currentYear, m, qMin(d, QDate(m_currentYear, m, 1).daysInMonth()));
        if (candidate.isValid()) initDate = candidate;
    }

    auto* dateEdit = new QDateEdit(initDate, &dlg);
    dateEdit->setCalendarPopup(true);
    dateEdit->setDisplayFormat("yyyy-MM-dd");
    form->addRow("记录日期", dateEdit);

    auto* contactCombo = new QComboBox(&dlg);
    contactCombo->addItem("不关联特定人脉", 0);
    auto contacts = DatabaseManager::instance().getContacts();
    for (const auto& c : contacts) {
        contactCombo->addItem(QString("%1 (%2)").arg(c.name, c.relationType), c.id);
    }
    form->addRow("关联对象", contactCombo);

    auto* contentEdit = CustomDialog::createMultiLineEdit("输入今日财务复盘或走动心境记录...", &dlg, 3);
    form->addRow("日记内容", contentEdit);

    layout->addLayout(form);

    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);
    btnRow->addStretch();

    auto* btnCancel = new QPushButton("取消", &dlg);
    btnCancel->setCursor(Qt::PointingHandCursor);
    auto* btnSave = new QPushButton("保存", &dlg);
    btnSave->setObjectName("primaryBtn");
    btnSave->setCursor(Qt::PointingHandCursor);

    btnRow->addWidget(btnCancel);
    btnRow->addWidget(btnSave);
    layout->addLayout(btnRow);

    dlg.bodyLayout()->addLayout(layout);
    CustomDialog::normalizeFormWidgets(&dlg);
    btnPersonal->setStyleSheet(toggleBtnStyle);
    btnWork->setStyleSheet(toggleBtnStyle);

    QString savedBook = m_currentBook;
    connect(btnCancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(btnSave, &QPushButton::clicked, &dlg, [&]() {
        QString content = contentEdit->toPlainText().trimmed();
        if (content.isEmpty()) {
            CustomDialog::markFieldInvalid(contentEdit);
            return;
        }
        savedBook = btnPersonal->isChecked() ? "PERSONAL" : "COMPANY";
        QString selectedMood = moodCombo->currentText().trimmed();
        if (selectedMood.isEmpty()) selectedMood = "记录";

        DatabaseManager::instance().addDiary(
            dateEdit->date().toString("yyyy-MM-dd"),
            content,
            selectedMood,
            savedBook,
            contactCombo->currentData().toInt()
        );
        dlg.accept();
    });

    if (dlg.exec() == QDialog::Accepted) {
        if (savedBook != m_currentBook) {
            onBookToggled(savedBook == "PERSONAL");
        } else {
            loadDiaryList();
        }
    }
}

void FinancialLedgerWidget::onSearchChanged(const QString& text) {
    Q_UNUSED(text);
    loadTransactionsTable();
    loadDiaryList();
}

void FinancialLedgerWidget::onTransTableContextMenu(const QPoint& pos) {
    int row = m_transTable->rowAt(pos.y());
    if (row < 0) return;
    auto* item0 = m_transTable->item(row, 0);
    if (!item0) return;
    int transId = item0->data(Qt::UserRole).toInt();
    if (transId <= 0) return;

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #202024; border: 1px solid #3f3f46; padding: 4px; border-radius: 4px; }"
        "QMenu::item { color: #e1e1e5; padding: 6px 18px; border-radius: 3px; font-size: 13px; }"
        "QMenu::item:selected { background-color: #2b2b30; color: #ffffff; }"
    );
    QAction* actEdit = menu.addAction(UiIcons::icon(UiIcons::Code::Diary, 12, QColor("#be782e")), "编辑");
    QAction* actDelete = menu.addAction(UiIcons::icon(UiIcons::Code::Delete, 12, QColor("#fa5151")), "删除");
    QAction* selected = menu.exec(m_transTable->viewport()->mapToGlobal(pos));

    if (selected == actEdit) {
        UnifiedTransactionDialog dlg("EXPENSE", this);
        dlg.setEditTransactionId(transId);
        if (dlg.exec() == QDialog::Accepted) {
            refreshData();
        }
    } else if (selected == actDelete) {
        if (CustomDialog::confirm(this, "删除流水", "确定要彻底删除该笔收支流水记录吗？此操作不可恢复。", "删除", "取消", true)) {
            DatabaseManager::instance().deleteTransaction(transId);
            refreshData();
        }
    }
}

void FinancialLedgerWidget::onEditDiaryClicked(int diaryId) {
    if (diaryId <= 0) return;
    QSqlQuery q(DatabaseManager::instance().database());
    q.prepare("SELECT id, book_type, diary_date, mood, related_contact_id AS contact_id, content FROM diaries WHERE id = :id;");
    q.bindValue(":id", diaryId);
    if (!q.exec() || !q.next()) return;

    QString currentBookType = q.value("book_type").toString();
    QDate currentDate = QDate::fromString(q.value("diary_date").toString().left(10), "yyyy-MM-dd");
    QString currentMood = q.value("mood").toString();
    int currentContactId = q.value("contact_id").toInt();
    QString currentContent = q.value("content").toString();

    CustomDialog dlg("编辑日记", this);
    dlg.setFixedWidth(460);

    auto* layout = new QVBoxLayout();
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    auto* form = new QFormLayout();
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // 1. 账本类型选择
    auto* bookRow = new QHBoxLayout();
    bookRow->setSpacing(10);
    auto* btnPersonal = new QPushButton("个人", &dlg);
    auto* btnWork = new QPushButton("工作", &dlg);
    btnPersonal->setCheckable(true);
    btnWork->setCheckable(true);
    btnPersonal->setCursor(Qt::PointingHandCursor);
    btnWork->setCursor(Qt::PointingHandCursor);
    btnPersonal->setFixedHeight(32);
    btnWork->setFixedHeight(32);

    bool initIsPersonal = (currentBookType != "COMPANY");
    btnPersonal->setChecked(initIsPersonal);
    btnWork->setChecked(!initIsPersonal);

    QString toggleBtnStyle =
        "QPushButton { background-color: #202024; color: #8e8e93; border: 1px solid #2c2c2e; border-radius: 4px; font-size: 12px; }"
        "QPushButton:checked { background-color: #00a361; color: #ffffff; border: 1px solid #00a361; font-weight: bold; }";
    btnPersonal->setStyleSheet(toggleBtnStyle);
    btnWork->setStyleSheet(toggleBtnStyle);

    bookRow->addWidget(btnPersonal, 1);
    bookRow->addWidget(btnWork, 1);
    form->addRow("日记账本", bookRow);

    // 2. 状态选择
    auto* moodCombo = new QComboBox(&dlg);
    auto populateMoods = [moodCombo](bool isPersonal) {
        QString prev = moodCombo->currentText();
        moodCombo->clear();
        QStringList moods;
        if (isPersonal) {
            moods = {"平静", "欣喜", "振奋", "从容", "笃定", "期待", "焦虑", "羞愧", "记录"};
        } else {
            moods = {"记录", "振奋", "推进", "承压", "签约", "维权", "复盘", "平静"};
        }
        moodCombo->addItems(moods);
        int idx = moodCombo->findText(prev);
        if (idx >= 0) moodCombo->setCurrentIndex(idx);
    };
    populateMoods(initIsPersonal);
    int mIdx = moodCombo->findText(currentMood);
    if (mIdx >= 0) moodCombo->setCurrentIndex(mIdx);
    form->addRow("当前状态", moodCombo);

    connect(btnPersonal, &QPushButton::clicked, &dlg, [btnPersonal, btnWork, populateMoods]() {
        btnPersonal->setChecked(true);
        btnWork->setChecked(false);
        populateMoods(true);
    });
    connect(btnWork, &QPushButton::clicked, &dlg, [btnPersonal, btnWork, populateMoods]() {
        btnWork->setChecked(true);
        btnPersonal->setChecked(false);
        populateMoods(false);
    });

    auto* dateEdit = new QDateEdit(currentDate.isValid() ? currentDate : QDate::currentDate(), &dlg);
    dateEdit->setCalendarPopup(true);
    dateEdit->setDisplayFormat("yyyy-MM-dd");
    form->addRow("记录日期", dateEdit);

    auto* contactCombo = new QComboBox(&dlg);
    contactCombo->addItem("不关联特定人脉", 0);
    auto contacts = DatabaseManager::instance().getContacts();
    int cSelectedIdx = 0;
    for (int i = 0; i < contacts.size(); ++i) {
        contactCombo->addItem(QString("%1 (%2)").arg(contacts[i].name, contacts[i].relationType), contacts[i].id);
        if (contacts[i].id == currentContactId) {
            cSelectedIdx = i + 1;
        }
    }
    contactCombo->setCurrentIndex(cSelectedIdx);
    form->addRow("关联对象", contactCombo);

    auto* contentEdit = CustomDialog::createMultiLineEdit("输入今日财务复盘或走动心境记录...", &dlg, 3);
    contentEdit->setPlainText(currentContent);
    form->addRow("日记内容", contentEdit);

    layout->addLayout(form);

    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);

    auto* btnCancel = new QPushButton("取消", &dlg);
    btnCancel->setCursor(Qt::PointingHandCursor);
    btnCancel->setFixedHeight(36);
    auto* btnSave = new QPushButton("保存", &dlg);
    btnSave->setObjectName("primaryBtn");
    btnSave->setCursor(Qt::PointingHandCursor);
    btnSave->setFixedHeight(36);

    btnRow->addWidget(btnCancel, 1);
    btnRow->addWidget(btnSave, 1);
    layout->addLayout(btnRow);

    dlg.bodyLayout()->addLayout(layout);
    CustomDialog::normalizeFormWidgets(&dlg);
    btnPersonal->setStyleSheet(toggleBtnStyle);
    btnWork->setStyleSheet(toggleBtnStyle);

    connect(btnCancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(btnSave, &QPushButton::clicked, &dlg, [&]() {
        QString content = contentEdit->toPlainText().trimmed();
        if (content.isEmpty()) {
            CustomDialog::markFieldInvalid(contentEdit);
            return;
        }
        QString savedBook = btnPersonal->isChecked() ? "PERSONAL" : "COMPANY";
        QString selectedMood = moodCombo->currentText().trimmed();
        if (selectedMood.isEmpty()) selectedMood = "记录";

        DatabaseManager::instance().updateDiary(
            diaryId,
            savedBook,
            dateEdit->date(),
            selectedMood,
            contactCombo->currentData().toInt(),
            content
        );
        dlg.accept();
    });

    if (dlg.exec() == QDialog::Accepted) {
        loadDiaryList();
    }
}

void FinancialLedgerWidget::onDeleteDiaryClicked(int diaryId) {
    if (diaryId <= 0) return;
    if (CustomDialog::confirm(this, "删除日记", "确定要彻底删除该篇日记记录吗？此操作不可恢复。", "删除", "取消", true)) {
        DatabaseManager::instance().deleteDiary(diaryId);
        loadDiaryList();
    }
}
