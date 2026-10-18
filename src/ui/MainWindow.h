#pragma once

#include "FramelessWindow.h"
#include <QStackedWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include "../core/DatabaseManager.h"

class GeneralDashboardWidget;
class ContactWidget;
class FinancialLedgerWidget;
class RelationGraphWidget;
class BusinessNetWidget;

class MainWindow : public FramelessWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

    void refreshAllData();

protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onNavButtonClicked(int index);
    void onTopologyToggled(bool active);
    void onBusinessNetToggled(bool active);
    void onImportBackupClicked();
    void onExportBackupClicked();
    void onModelSettingsClicked();
    void onSecuritySetupClicked();
    void onAddNewPlanClicked();
    void onPlanItemClicked(const SocialPlanItem& item);

private:
    void setupUi();
    void loadUpcomingPlansSidebar();
    void updateTopBarAiStatus(bool isRunning = false);
    void verifyModelAvailability();

    QStackedWidget* m_stack{nullptr};
    GeneralDashboardWidget* m_dashboardWidget{nullptr};
    ContactWidget* m_contactWidget{nullptr};
    FinancialLedgerWidget* m_ledgerWidget{nullptr};
    RelationGraphWidget* m_fullscreenGraphWidget{nullptr};
    BusinessNetWidget* m_businessNetWidget{nullptr};

    QList<QPushButton*> m_navButtons;
    QVBoxLayout* m_plansListLayout{nullptr};
    int m_lastPageIndex{0};
    bool m_initialReminderChecked{false};
    bool m_isModelAvailable{false};
};
