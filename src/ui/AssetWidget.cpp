#include "AssetWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDialog>
#include "CustomDialog.h"
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QDateEdit>
#include <QComboBox>
#include <QMessageBox>

AssetWidget::AssetWidget(QWidget* parent) : QWidget(parent) {
    setupUi();
    refreshData();
}

void AssetWidget::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 20, 24, 20);
    mainLayout->setSpacing(18);

    // 1. 统计卡片
    auto* cardsLayout = new QHBoxLayout();
    cardsLayout->setSpacing(16);

    auto createStatCard = [](const QString& title, QLabel*& valLabel, const QString& accentColor) {
        auto* card = new QFrame();
        card->setStyleSheet("QFrame { background-color: #1a1a1a; border-radius: 8px; border: 1px solid #282828; padding: 12px; }");
        auto* layout = new QVBoxLayout(card);
        layout->setContentsMargins(8, 6, 8, 6);
        layout->setSpacing(4);

        auto* titleLbl = new QLabel(title, card);
        titleLbl->setStyleSheet("color: #888888; font-size: 12px; border: none;");

        valLabel = new QLabel("￥0.00", card);
        valLabel->setStyleSheet(QString("color: %1; font-size: 22px; font-weight: bold; border: none;").arg(accentColor));

        layout->addWidget(titleLbl);
        layout->addWidget(valLabel);
        return card;
    };

    cardsLayout->addWidget(createStatCard("资产总估值 (房/车/股权/藏品)", m_lblTotalAssetVal, "#34db4e"));
    cardsLayout->addWidget(createStatCard("不动产 (房产/商铺/厂房)", m_lblRealEstateCount, "#00bcd4"));
    cardsLayout->addWidget(createStatCard("载具车辆与重型设备", m_lblVehicleCount, "#febc2f"));

    mainLayout->addLayout(cardsLayout);

    // 2. 工具栏
    auto* toolbar = new QHBoxLayout();
    auto* titleLbl = new QLabel("实体与金融资产清单 (已关联人脉与贷款)：", this);
    titleLbl->setStyleSheet("color: #ffffff; font-size: 14px; font-weight: bold;");
    toolbar->addWidget(titleLbl);

    toolbar->addStretch();

    auto* addBtn = new QPushButton("+ 登记新资产", this);
    addBtn->setStyleSheet(
        "QPushButton { background: #28c840; color: #ffffff; font-weight: bold; border-radius: 4px; padding: 8px 18px; }"
        "QPushButton:hover { background: #34db4e; }"
    );
    toolbar->addWidget(addBtn);

    mainLayout->addLayout(toolbar);

    // 3. 表格
    m_table = new QTableWidget(this);
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({"资产类别", "资产名称", "当前净估值", "归属权", "购置日期", "关联人脉 (出租/借用/合伙)", "备注说明"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setStyleSheet(
        "QTableWidget { background-color: #171717; border: 1px solid #282828; border-radius: 6px; gridline-color: #222222; color: #dddddd; font-size: 13px; }"
        "QHeaderView::section { background-color: #1f1f1f; color: #888888; border: none; padding: 8px; font-weight: bold; }"
        "QTableWidget::item { padding: 8px; border-bottom: 1px solid #202020; }"
    );

    mainLayout->addWidget(m_table);

    connect(addBtn, &QPushButton::clicked, this, &AssetWidget::onAddAssetClicked);
}

void AssetWidget::refreshData() {
    m_assetList = DatabaseManager::instance().getAssets();

    double totalVal = 0.0;
    int realEstateCount = 0;
    int vehicleCount = 0;

    m_table->setRowCount(m_assetList.size());
    for (int i = 0; i < m_assetList.size(); ++i) {
        const auto& a = m_assetList[i];
        totalVal += a.estimatedValue;

        QString typeName = a.assetType;
        if (a.assetType == "REAL_ESTATE") { typeName = "🏠 房产/不动产"; realEstateCount++; }
        else if (a.assetType == "VEHICLE") { typeName = "🚗 车辆/载具"; vehicleCount++; }
        else if (a.assetType == "EQUITY") { typeName = "📈 股权/投资"; }
        else if (a.assetType == "VALUABLE") { typeName = "💎 贵重藏品/首饰"; }
        else if (a.assetType == "EQUIPMENT") { typeName = "⚙️ 机械设备"; }

        m_table->setItem(i, 0, new QTableWidgetItem(typeName));
        m_table->setItem(i, 1, new QTableWidgetItem(a.name));

        auto* valItem = new QTableWidgetItem(QString("￥%1").arg(a.estimatedValue, 0, 'f', 2));
        valItem->setForeground(QColor("#34db4e"));
        m_table->setItem(i, 2, valItem);

        QString ownerStr = (a.ownership == "PERSONAL") ? "个人所有" : ((a.ownership == "COMPANY") ? "公司所有" : "共同持有");
        m_table->setItem(i, 3, new QTableWidgetItem(ownerStr));
        m_table->setItem(i, 4, new QTableWidgetItem(a.purchaseDate.toString("yyyy-MM-dd")));

        QString contactLink = a.linkedContactName.isEmpty() ? "—" : a.linkedContactName;
        m_table->setItem(i, 5, new QTableWidgetItem(contactLink));
        m_table->setItem(i, 6, new QTableWidgetItem(a.remark));
    }

    m_lblTotalAssetVal->setText(QString("￥%1").arg(totalVal, 0, 'f', 2));
    m_lblRealEstateCount->setText(QString("%1 处").arg(realEstateCount));
    m_lblVehicleCount->setText(QString("%1 辆").arg(vehicleCount));
}

void AssetWidget::onAddAssetClicked() {
    CustomDialog dlg("登记资产 (房产 / 车辆 / 股权 / 贵重物品)", this);
    dlg.resize(480, 520);

    auto* layout = dlg.contentLayout();
    layout->setSpacing(12);

    auto* typeCombo = new QComboBox(&dlg);
    typeCombo->addItem("房产 / 商铺 / 厂房 (REAL_ESTATE)", "REAL_ESTATE");
    typeCombo->addItem("车辆 / 载具 (VEHICLE)", "VEHICLE");
    typeCombo->addItem("股权 / 合伙企业份额 (EQUITY)", "EQUITY");
    typeCombo->addItem("贵重珠宝 / 名表藏品 (VALUABLE)", "VALUABLE");
    typeCombo->addItem("器械生产设备 (EQUIPMENT)", "EQUIPMENT");
    typeCombo->addItem("其他资产 (OTHER)", "OTHER");

    auto* nameEdit = new QLineEdit(&dlg);
    nameEdit->setPlaceholderText("例如: 绿城锦兰公馆1201室, 保时捷卡宴, 木头科技35%股权");

    auto* valSpin = new QDoubleSpinBox(&dlg);
    valSpin->setRange(100, 1000000000);
    valSpin->setValue(1000000);
    valSpin->setPrefix("￥ ");

    auto* ownerCombo = new QComboBox(&dlg);
    ownerCombo->addItem("个人所有", "PERSONAL");
    ownerCombo->addItem("公司所有", "COMPANY");
    ownerCombo->addItem("共有/共同投资", "SHARED");

    auto* purchaseDateEdit = new QDateEdit(QDate::currentDate(), &dlg);
    purchaseDateEdit->setCalendarPopup(true);
    purchaseDateEdit->setDisplayFormat("yyyy-MM-dd");

    auto* contactCombo = new QComboBox(&dlg);
    contactCombo->addItem("无关联人脉", 0);
    auto contacts = DatabaseManager::instance().getContacts();
    for (const auto& c : contacts) {
        contactCombo->addItem(QString("%1 (%2)").arg(c.name, c.company), c.id);
    }

    auto* remarkEdit = new QLineEdit(&dlg);
    remarkEdit->setPlaceholderText("例如: 租给老陈每年收租12万, 车借给李总开");

    auto addField = [&](const QString& label, QWidget* w) {
        layout->addWidget(new QLabel(label, &dlg));
        layout->addWidget(w);
    };

    addField("资产类别：", typeCombo);
    addField("资产名称 *：", nameEdit);
    addField("当前净估值 (元)：", valSpin);
    addField("资产归属：", ownerCombo);
    addField("购置时间：", purchaseDateEdit);
    addField("关联人脉 (出租给谁 / 借给谁 / 共同股东)：", contactCombo);
    addField("备注说明：", remarkEdit);

    auto* btnLayout = new QHBoxLayout();
    auto* cancelBtn = new QPushButton("取消", &dlg);
    auto* saveBtn = new QPushButton("保存资产", &dlg);
    saveBtn->setStyleSheet("background-color: #28c840; color: #ffffff; font-weight: bold; padding: 6px 18px;");
    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(saveBtn);
    layout->addLayout(btnLayout);

    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(saveBtn, &QPushButton::clicked, [&]() {
        QString name = nameEdit->text().trimmed();
        if (name.isEmpty()) {
            CustomDialog::markFieldInvalid(nameEdit);
            return;
        }

        bool ok = DatabaseManager::instance().addAsset(
            typeCombo->currentData().toString(),
            name,
            valSpin->value(),
            ownerCombo->currentData().toString(),
            purchaseDateEdit->date(),
            contactCombo->currentData().toInt(),
            0,
            remarkEdit->text().trimmed()
        );

        if (ok) {
            dlg.accept();
        } else {
            QMessageBox::critical(&dlg, "错误", "登记资产失败！");
        }
    });

    if (dlg.exec() == QDialog::Accepted) {
        refreshData();
    }
}
