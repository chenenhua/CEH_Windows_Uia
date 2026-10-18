#pragma once

#include <QWidget>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QTableWidget>
#include <QLineEdit>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QFrame>
#include "../core/DatabaseManager.h"

class FinancialLedgerWidget : public QWidget {
    Q_OBJECT
public:
    explicit FinancialLedgerWidget(QWidget* parent = nullptr);

    void refreshData();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onBookToggled(bool isPersonal);
    void onYearClicked(int year);
    void onMonthClicked(int month);
    void onDayClicked(int day);
    void onAddTransactionClicked();
    void onAddDiaryClicked();
    void onSearchChanged(const QString& text);
    void onTransTableContextMenu(const QPoint& pos);
    void onEditDiaryClicked(int diaryId);
    void onDeleteDiaryClicked(int diaryId);

private:
    void setupUi();
    void setupLeftStatsColumn(QWidget* parent);
    void setupMiddleTableColumn(QWidget* parent);
    void setupRightDiaryColumn(QWidget* parent);

    void applyTableColumnRatios();
    void rebuildYearButtons();
    void rebuildDaysGrid();
    void updateYearMetrics();
    void updateMonthMetrics();
    void updateDayMetrics();
    void loadTransactionsTable();
    void loadDiaryList();

    QString m_currentBook{"PERSONAL"};
    int m_currentYear{2026};
    int m_currentMonth{10}; // 0 = 全年, 1..12 = 月份
    int m_currentDay{0};    // 0 = 整月, 1..31 = 日期

    // 左栏控件
    QPushButton* m_btnPersonalBook{nullptr};
    QPushButton* m_btnCompanyBook{nullptr};
    QPushButton* m_btnAddTrans{nullptr};

    QGridLayout* m_yearGridLayout{nullptr};
    QList<QPushButton*> m_yearButtons;
    QLabel* m_lblYearIncome{nullptr};
    QLabel* m_lblYearSurplus{nullptr};
    QLabel* m_lblYearReceivable{nullptr};
    QLabel* m_lblYearExpense{nullptr};
    QLabel* m_lblYearDebt{nullptr};
    QLabel* m_lblYearFriends{nullptr};

    QList<QPushButton*> m_monthButtons;
    QLabel* m_lblMonthIncome{nullptr};
    QLabel* m_lblMonthSurplus{nullptr};
    QLabel* m_lblMonthReceivable{nullptr};
    QLabel* m_lblMonthExpense{nullptr};
    QLabel* m_lblMonthDebt{nullptr};
    QLabel* m_lblMonthFriends{nullptr};

    QLabel* m_lblDayIncome{nullptr};
    QLabel* m_lblDaySurplus{nullptr};
    QLabel* m_lblDayReceivable{nullptr};
    QLabel* m_lblDayExpense{nullptr};
    QLabel* m_lblDayDebt{nullptr};
    QLabel* m_lblDayFriends{nullptr};

    QWidget* m_daysGridContainer{nullptr};
    QGridLayout* m_daysGridLayout{nullptr};
    QList<QPushButton*> m_dayButtons;

    // 中栏控件
    QLineEdit* m_searchEdit{nullptr};
    QTableWidget* m_transTable{nullptr};

    // 右栏控件 (日记)
    QPushButton* m_btnAddDiary{nullptr};
    QVBoxLayout* m_diaryListLayout{nullptr};
};
