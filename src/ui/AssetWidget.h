#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include "../core/DatabaseManager.h"

class AssetWidget : public QWidget {
    Q_OBJECT
public:
    explicit AssetWidget(QWidget* parent = nullptr);

    void refreshData();

private slots:
    void onAddAssetClicked();

private:
    void setupUi();

    QLabel* m_lblTotalAssetVal{nullptr};
    QLabel* m_lblRealEstateCount{nullptr};
    QLabel* m_lblVehicleCount{nullptr};

    QTableWidget* m_table{nullptr};
    QList<AssetItem> m_assetList;
};
