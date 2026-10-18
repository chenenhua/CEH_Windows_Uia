#include "ContactWidget.h"
#include "ContactDialog.h"
#include "RelationshipDiaryDialog.h"
#include "SocialPlanDialog.h"
#include "UnifiedTransactionDialog.h"
#include "RelationGraphWidget.h"
#include "CustomDialog.h"
#include "../core/VcfParser.h"
#include "../ai/AiAdvisorService.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QComboBox>
#include <QSpinBox>
#include <QDateEdit>
#include <QDate>
#include <QDateTime>
#include <QEvent>
#include <QTimer>
#include <QPointer>
#include <QMenu>
#include <QScrollBar>
#include <QKeyEvent>
#include "UiIcons.h"
#include <QRegularExpression>
#include <QSqlQuery>

ContactWidget::ContactWidget(QWidget* parent) : QWidget(parent) {
    setupUi();
    refreshData();
}

void ContactWidget::setupUi() {
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // ==================== Left Column: 搜索、导入、+添加与好友列表 ====================
    auto* colLeft = new QWidget(this);
    colLeft->setFixedWidth(340);
    colLeft->setObjectName("macCard");
    setupLeftColumn(colLeft);
    mainLayout->addWidget(colLeft);

    // 竖分割线 1
    auto* vline1 = new QFrame(this);
    vline1->setObjectName("vDivider");
    vline1->setFixedWidth(1);
    vline1->setStyleSheet("background-color: #28282b; border: none;");
    mainLayout->addWidget(vline1);

    // ==================== Middle Column: 个人资料、禁忌偏好与三轨档案 ====================
    auto* colMiddle = new QWidget(this);
    colMiddle->setObjectName("macCard");
    setupMiddleColumn(colMiddle);
    mainLayout->addWidget(colMiddle, 1);

    // 竖分割线 2
    auto* vline2 = new QFrame(this);
    vline2->setObjectName("vDivider");
    vline2->setFixedWidth(1);
    vline2->setStyleSheet("background-color: #28282b; border: none;");
    mainLayout->addWidget(vline2);

    // ==================== Right Column: 统计对比、关系网与AI点评 ====================
    auto* colRight = new QWidget(this);
    colRight->setFixedWidth(360);
    colRight->setObjectName("macCard");
    setupRightColumn(colRight);
    mainLayout->addWidget(colRight);
}

void ContactWidget::setupLeftColumn(QWidget* parent) {
    auto* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    // 1. 顶部操控栏 (参考图4: [搜索好友..]  导入(#be782e)  +添加(#00a361) + 右侧底部下划线)
    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(0);

    m_searchEdit = new QLineEdit(parent);
    m_searchEdit->setPlaceholderText("搜索好友..");
    m_searchEdit->setFixedHeight(32);
    m_searchEdit->setStyleSheet(
        "QLineEdit { background-color: transparent; color: #e1e1e5; border: none; border-bottom: 1px solid #28282b; border-radius: 0px; padding: 0 6px; font-size: 12px; }"
        "QLineEdit:focus { border-bottom: 1px solid #00a361; }"
    );
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ContactWidget::onSearchTextChanged);

    auto* rightBtnsContainer = new QFrame(parent);
    rightBtnsContainer->setFixedHeight(32);
    rightBtnsContainer->setStyleSheet("QFrame { background: transparent; border: none; border-bottom: 1px solid #28282b; }");
    auto* rightBtnsLayout = new QHBoxLayout(rightBtnsContainer);
    rightBtnsLayout->setContentsMargins(8, 0, 0, 0);
    rightBtnsLayout->setSpacing(6);

    m_btnImport = new QPushButton("导入", rightBtnsContainer);
    m_btnImport->setCursor(Qt::PointingHandCursor);
    m_btnImport->setStyleSheet(
        "QPushButton { background: transparent; color: #be782e; border: none; font-size: 12px; padding: 2px 4px; }"
        "QPushButton:hover { color: #d98e3e; }"
    );
    connect(m_btnImport, &QPushButton::clicked, this, &ContactWidget::onImportVcfClicked);

    m_btnAddContact = new QPushButton("+ 添加", rightBtnsContainer);
    m_btnAddContact->setCursor(Qt::PointingHandCursor);
    m_btnAddContact->setStyleSheet(
        "QPushButton { background: transparent; color: #00a361; border: none; font-size: 12px; padding: 2px 4px; }"
        "QPushButton:hover { color: #00c474; }"
    );
    connect(m_btnAddContact, &QPushButton::clicked, this, &ContactWidget::onAddContactClicked);

    rightBtnsLayout->addWidget(m_btnImport);
    rightBtnsLayout->addWidget(m_btnAddContact);

    topRow->addWidget(m_searchEdit, 1);
    topRow->addWidget(rightBtnsContainer);
    layout->addLayout(topRow);

    // 2. 好友表格: 名字(20%) | 行业(20%) | 关系(20%) | 亲疏(15%) | 投入(25%)
    // 默认文字颜色 #8e8e93，选中无背景色、文字颜色 #e1e1e5
    m_contactsTable = new QTableWidget(parent);
    m_contactsTable->setColumnCount(5);
    m_contactsTable->setHorizontalHeaderLabels({"名字", "行业", "关系", "亲疏", "投入"});
    m_contactsTable->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_contactsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_contactsTable->horizontalHeader()->setStyleSheet(
        "QHeaderView::section { background-color: #141416; color: #8e8e93; border: none; border-bottom: 1px solid #28282b; padding: 8px 4px; font-size: 12px; font-weight: normal; }"
    );
    m_contactsTable->setStyleSheet(
        "QTableWidget { background-color: #141416; color: #8e8e93; border: none; font-size: 12px; outline: none; gridline-color: transparent; }"
        "QTableWidget::item { background-color: transparent; border: none; padding: 4px; }"
        "QTableWidget::item:selected { background-color: transparent; }"
        "QTableWidget::item:focus { background-color: transparent; border: none; outline: none; }"
    );
    m_contactsTable->setFocusPolicy(Qt::NoFocus);

    m_contactsTable->verticalHeader()->setVisible(false);
    m_contactsTable->verticalHeader()->setDefaultSectionSize(38);
    m_contactsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_contactsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_contactsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_contactsTable->setShowGrid(false);
    m_contactsTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_contactsTable->viewport()->installEventFilter(this);

    connect(m_contactsTable, &QTableWidget::cellClicked, this, &ContactWidget::onContactTableRowClicked);
    layout->addWidget(m_contactsTable, 1);
}

void ContactWidget::setupMiddleColumn(QWidget* parent) {
    auto* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(12);

    // ==================== 上半区: 头像与名片信息 (严格参考图5) ====================
    auto* profileCard = new QWidget(parent);
    auto* profLayout = new QHBoxLayout(profileCard);
    profLayout->setContentsMargins(0, 0, 0, 0);
    profLayout->setSpacing(16);

    // 左侧 64x64 蓝字方块头像 (点击可直接更换头像)
    m_avatarBtn = new QPushButton("陈", profileCard);
    m_avatarBtn->setFixedSize(64, 64);
    m_avatarBtn->setCursor(Qt::PointingHandCursor);
    m_avatarBtn->setToolTip("点击更换头像");
    m_avatarBtn->setStyleSheet(
        "QPushButton { background-color: #202024; border: none; border-radius: 4px; font-size: 26px; color: #0a84ff; font-weight: 900; padding: 0px; }"
        "QPushButton:hover { background-color: #28282e; }"
    );
    connect(m_avatarBtn, &QPushButton::clicked, this, &ContactWidget::onUpdateAvatarClicked);
    profLayout->addWidget(m_avatarBtn, 0, Qt::AlignTop);

    // 右侧信息与偏好列 (行距加高 5px: spacing 8 -> 13)
    auto* infoVBox = new QVBoxLayout();
    infoVBox->setContentsMargins(0, 0, 0, 0);
    infoVBox->setSpacing(13);

    // 行 1: 姓名加大 5px (18px -> 23px) + 右侧 [图标 编辑] (#be782e) 与 [图标 删除 (红色字)] (#fa5151)
    auto* row1 = new QHBoxLayout();
    row1->setSpacing(8);
    m_lblName = new QLabel("陈恩华", profileCard);
    m_lblName->setStyleSheet("font-size: 23px; font-weight: 900; color: #e1e1e5;");

    m_btnEditContact = new QPushButton("编辑", profileCard);
    m_btnEditContact->setIcon(UiIcons::icon(UiIcons::Code::Diary, 12, QColor("#be782e")));
    m_btnEditContact->setCursor(Qt::PointingHandCursor);
    m_btnEditContact->setStyleSheet(
        "QPushButton { background: transparent; color: #be782e; border: none; border-radius: 3px; padding: 2px 6px; font-size: 12px; }"
        "QPushButton:hover { color: #d98e3e; background-color: #2b2116; }"
    );
    connect(m_btnEditContact, &QPushButton::clicked, this, &ContactWidget::onEditContactClicked);

    m_btnDeleteContact = new QPushButton("删除", profileCard);
    m_btnDeleteContact->setIcon(UiIcons::icon(UiIcons::Code::Delete, 12, QColor("#fa5151")));
    m_btnDeleteContact->setCursor(Qt::PointingHandCursor);
    m_btnDeleteContact->setStyleSheet(
        "QPushButton { background: transparent; color: #fa5151; border: none; border-radius: 3px; padding: 2px 6px; font-size: 12px; font-weight: bold; }"
        "QPushButton:hover { color: #ff6b6b; background-color: #381a1c; }"
    );
    connect(m_btnDeleteContact, &QPushButton::clicked, this, &ContactWidget::onDeleteContactClicked);

    row1->addWidget(m_lblName);
    row1->addStretch();
    row1->addWidget(m_btnEditContact);
    row1->addWidget(m_btnDeleteContact);
    infoVBox->addLayout(row1);

    // 3x3 属性网格 (下面行距加高 5px: verticalSpacing 6 -> 11)
    auto* grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(20);
    grid->setVerticalSpacing(11);

    auto addCell = [profileCard, grid](int r, int c, const QString& keyText, QLabel*& valLbl) {
        auto* cell = new QHBoxLayout();
        cell->setSpacing(6);
        auto* k = new QLabel(keyText, profileCard);
        k->setStyleSheet("color: #8e8e93; font-size: 12px;");
        valLbl = new QLabel("—", profileCard);
        valLbl->setStyleSheet("color: #e1e1e5; font-size: 12px;");
        cell->addWidget(k);
        cell->addWidget(valLbl, 1);
        grid->addLayout(cell, r, c);
    };

    addCell(0, 0, "电话:", m_lblPhone);
    addCell(0, 1, "地址:", m_lblAddress);
    addCell(0, 2, "引荐人:", m_lblIntroducer);

    addCell(1, 0, "行业:", m_lblIndustry);
    addCell(1, 1, "公司:", m_lblCompany);
    addCell(1, 2, "职务:", m_lblTitle);

    addCell(2, 0, "亲疏:", m_lblClosenessStars);
    addCell(2, 1, "能量:", m_lblInfluenceStars);
    addCell(2, 2, "信用:", m_lblCreditStars);

    grid->setColumnStretch(0, 2);
    grid->setColumnStretch(1, 3);
    grid->setColumnStretch(2, 2);

    infoVBox->addLayout(grid);

    // 偏好习惯与禁忌：左右两栏（左：喜好 | 右：禁忌，粗体）
    infoVBox->addSpacing(6);
    auto* prefTitle = new QLabel("偏好习惯与禁忌", profileCard);
    prefTitle->setStyleSheet("color: #e1e1e5; font-size: 13px; font-weight: bold;");
    infoVBox->addWidget(prefTitle);

    auto* prefTwoColRow = new QHBoxLayout();
    prefTwoColRow->setContentsMargins(0, 0, 0, 0);
    prefTwoColRow->setSpacing(20);

    m_lblPreferences = new QLabel(profileCard);
    m_lblPreferences->setTextFormat(Qt::RichText);
    m_lblPreferences->setStyleSheet("color: #8e8e93; font-size: 12px; font-weight: bold;");
    m_lblPreferences->setWordWrap(true);
    m_lblPreferences->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    m_lblTaboos = new QLabel(profileCard);
    m_lblTaboos->setTextFormat(Qt::RichText);
    m_lblTaboos->setStyleSheet("color: #8e8e93; font-size: 12px; font-weight: bold;");
    m_lblTaboos->setWordWrap(true);
    m_lblTaboos->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    prefTwoColRow->addWidget(m_lblPreferences, 1);
    prefTwoColRow->addWidget(m_lblTaboos, 1);
    infoVBox->addLayout(prefTwoColRow);

    profLayout->addLayout(infoVBox, 1);
    layout->addWidget(profileCard);

    // 分割线 1
    auto* hSep1 = new QFrame(parent);
    hSep1->setFixedHeight(1);
    hSep1->setStyleSheet("background-color: #28282b; border: none;");
    layout->addWidget(hSep1);

    // ==================== 下半区: 人脉关系 / 亲疏轨迹 / 礼尚往来 / 走动计划 / 业务往来 ====================
    auto* tabHeaderRow = new QHBoxLayout();
    tabHeaderRow->setContentsMargins(0, 0, 0, 0);
    tabHeaderRow->setSpacing(0);

    m_btnTabRelations = new QPushButton("人脉关系", parent);
    m_btnTabRelations->setCheckable(true);
    m_btnTabRelations->setChecked(true);
    m_btnTabRelations->setCursor(Qt::PointingHandCursor);

    m_btnTabTimeline = new QPushButton("亲疏轨迹", parent);
    m_btnTabTimeline->setCheckable(true);
    m_btnTabTimeline->setCursor(Qt::PointingHandCursor);

    m_btnTabEvents = new QPushButton("礼尚往来", parent);
    m_btnTabEvents->setCheckable(true);
    m_btnTabEvents->setCursor(Qt::PointingHandCursor);

    m_btnTabPlans = new QPushButton("走动计划", parent);
    m_btnTabPlans->setCheckable(true);
    m_btnTabPlans->setCursor(Qt::PointingHandCursor);

    m_btnTabBusiness = new QPushButton("业务往来", parent);
    m_btnTabBusiness->setCheckable(true);
    m_btnTabBusiness->setCursor(Qt::PointingHandCursor);

    connect(m_btnTabRelations, &QPushButton::clicked, this, [this]() { onBottomTabChanged(0); });
    connect(m_btnTabTimeline, &QPushButton::clicked, this, [this]() { onBottomTabChanged(1); });
    connect(m_btnTabEvents, &QPushButton::clicked, this, [this]() { onBottomTabChanged(2); });
    connect(m_btnTabPlans, &QPushButton::clicked, this, [this]() { onBottomTabChanged(3); });
    connect(m_btnTabBusiness, &QPushButton::clicked, this, [this]() { onBottomTabChanged(4); });

    // 右侧延伸下划线容器 + [+ 添加]
    auto* tabRightLineContainer = new QFrame(parent);
    tabRightLineContainer->setStyleSheet("QFrame { background: transparent; border: none; border-bottom: 1px solid #28282b; }");
    auto* tabRightLayout = new QHBoxLayout(tabRightLineContainer);
    tabRightLayout->setContentsMargins(0, 0, 0, 4);

    m_btnAddRecord = new QPushButton("+ 添加", tabRightLineContainer);
    m_btnAddRecord->setCursor(Qt::PointingHandCursor);
    m_btnAddRecord->setStyleSheet(
        "QPushButton { background: transparent; color: #00a361; border: none; font-size: 12px; padding: 2px 6px; }"
        "QPushButton:hover { color: #00c474; }"
    );
    connect(m_btnAddRecord, &QPushButton::clicked, this, &ContactWidget::onAddBottomRecordClicked);

    tabRightLayout->addStretch();
    tabRightLayout->addWidget(m_btnAddRecord);

    tabHeaderRow->addWidget(m_btnTabRelations);
    tabHeaderRow->addWidget(m_btnTabTimeline);
    tabHeaderRow->addWidget(m_btnTabEvents);
    tabHeaderRow->addWidget(m_btnTabPlans);
    tabHeaderRow->addWidget(m_btnTabBusiness);
    tabHeaderRow->addWidget(tabRightLineContainer, 1);
    layout->addLayout(tabHeaderRow);

    updateTabButtonsStyle();

    // 动态表格 Stack
    m_bottomTabStack = new QStackedWidget(parent);

    auto configureBottomTable = [this](QTableWidget* tbl, int cols) {
        tbl->setColumnCount(cols);
        tbl->horizontalHeader()->setVisible(false);
        tbl->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        tbl->verticalHeader()->setVisible(false);
        tbl->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        tbl->verticalHeader()->setMinimumSectionSize(44);
        tbl->setSelectionBehavior(QAbstractItemView::SelectRows);
        tbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
        tbl->setShowGrid(false);
        tbl->setWordWrap(true);
        tbl->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        tbl->setContextMenuPolicy(Qt::CustomContextMenu);
        tbl->setStyleSheet(
            "QTableWidget { background-color: #141416; color: #e1e1e5; border: none; font-size: 14px; outline: none; gridline-color: transparent; }"
            "QTableWidget::item { background-color: transparent; border: none; padding: 6px 4px; font-size: 14px; }"
            "QTableWidget::item:selected { background-color: transparent; }"
        );
        tbl->viewport()->installEventFilter(this);
    };

    m_relationsTable = new QTableWidget(parent);
    configureBottomTable(m_relationsTable, 3);
    connect(m_relationsTable, &QTableWidget::customContextMenuRequested, this, &ContactWidget::onRelationsContextMenu);
    m_bottomTabStack->addWidget(m_relationsTable);

    m_timelineTable = new QTableWidget(parent);
    configureBottomTable(m_timelineTable, 3);
    connect(m_timelineTable, &QTableWidget::customContextMenuRequested, this, &ContactWidget::onTimelineContextMenu);
    m_bottomTabStack->addWidget(m_timelineTable);

    m_eventsTable = new QTableWidget(parent);
    configureBottomTable(m_eventsTable, 4);
    connect(m_eventsTable, &QTableWidget::customContextMenuRequested, this, &ContactWidget::onEventsContextMenu);
    m_bottomTabStack->addWidget(m_eventsTable);

    m_plansTable = new QTableWidget(parent);
    configureBottomTable(m_plansTable, 4);
    connect(m_plansTable, &QTableWidget::customContextMenuRequested, this, &ContactWidget::onPlansContextMenu);
    m_bottomTabStack->addWidget(m_plansTable);

    m_businessTable = new QTableWidget(parent);
    configureBottomTable(m_businessTable, 4);
    connect(m_businessTable, &QTableWidget::customContextMenuRequested, this, &ContactWidget::onBusinessContextMenu);
    m_bottomTabStack->addWidget(m_businessTable);

    layout->addWidget(m_bottomTabStack, 1);
}

void ContactWidget::updateTabButtonsStyle() {
    QString activeStyle =
        "QPushButton { background-color: #141416; color: #e1e1e5; font-size: 14px; font-weight: bold; "
        "border-top: 1px solid #28282b; border-left: 1px solid #28282b; border-right: 1px solid #28282b; border-bottom: none; "
        "border-top-left-radius: 3px; border-top-right-radius: 3px; padding: 6px 18px; }";

    QString inactiveStyle =
        "QPushButton { background-color: transparent; color: #8e8e93; font-size: 14px; font-weight: normal; "
        "border: none; border-bottom: 1px solid #28282b; border-radius: 0px; padding: 6px 18px; }"
        "QPushButton:hover { color: #e1e1e5; }";

    if (m_btnTabRelations) m_btnTabRelations->setStyleSheet(m_currentBottomTab == 0 ? activeStyle : inactiveStyle);
    if (m_btnTabTimeline) m_btnTabTimeline->setStyleSheet(m_currentBottomTab == 1 ? activeStyle : inactiveStyle);
    if (m_btnTabEvents) m_btnTabEvents->setStyleSheet(m_currentBottomTab == 2 ? activeStyle : inactiveStyle);
    if (m_btnTabPlans) m_btnTabPlans->setStyleSheet(m_currentBottomTab == 3 ? activeStyle : inactiveStyle);
    if (m_btnTabBusiness) m_btnTabBusiness->setStyleSheet(m_currentBottomTab == 4 ? activeStyle : inactiveStyle);
}

void ContactWidget::setupRightColumn(QWidget* parent) {
    auto* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    // 1. 相识时间与投入回报总结 (严格参考图5)
    m_lblMeetDate = new QLabel(parent);
    m_lblMeetDate->setStyleSheet("color: #e1e1e5; font-size: 13px; font-weight: bold;");
    layout->addWidget(m_lblMeetDate);

    m_lblSpendVsReturn = new QLabel(parent);
    m_lblSpendVsReturn->setTextFormat(Qt::RichText);
    m_lblSpendVsReturn->setStyleSheet("font-size: 12px;");
    layout->addWidget(m_lblSpendVsReturn);

    // 动态渲染该联系人（及其公司）在“人脉社交”和“商务社交”下的真实数据进度条（有就有、没有就没有）
    m_socialStatsLayout = new QVBoxLayout();
    m_socialStatsLayout->setContentsMargins(0, 0, 0, 0);
    m_socialStatsLayout->setSpacing(6);
    layout->addLayout(m_socialStatsLayout);

    // 分割线 1
    auto* hLine1 = new QFrame(parent);
    hLine1->setFixedHeight(1);
    hLine1->setStyleSheet("background-color: #28282b; border: none;");
    layout->addWidget(hLine1);

    // 2. 关系网小窗口 (参考图5: 左“关系网” ... 右“刷新”(#be782e) + “+关联”(#00a361))
    auto* netRow = new QHBoxLayout();
    netRow->setSpacing(8);
    auto* netTitle = new QLabel("关系网", parent);
    netTitle->setStyleSheet("color: #e1e1e5; font-size: 13px; font-weight: bold;");

    m_btnRefreshGraph = new QPushButton("刷新", parent);
    m_btnRefreshGraph->setCursor(Qt::PointingHandCursor);
    m_btnRefreshGraph->setStyleSheet(
        "QPushButton { background: transparent; color: #be782e; border: none; font-size: 12px; padding: 2px 4px; }"
        "QPushButton:hover { color: #d98e3e; }"
    );

    m_btnConnectRelation = new QPushButton("+ 关联", parent);
    m_btnConnectRelation->setCursor(Qt::PointingHandCursor);
    m_btnConnectRelation->setStyleSheet(
        "QPushButton { background: transparent; color: #00a361; border: none; font-size: 12px; padding: 2px 4px; }"
        "QPushButton:hover { color: #00c474; }"
    );
    connect(m_btnConnectRelation, &QPushButton::clicked, this, &ContactWidget::onConnectRelationClicked);

    netRow->addWidget(netTitle);
    netRow->addStretch();
    netRow->addWidget(m_btnRefreshGraph);
    netRow->addWidget(m_btnConnectRelation);
    layout->addLayout(netRow);

    m_miniGraphWidget = new RelationGraphWidget(parent);
    m_miniGraphWidget->setMiniMode(true);
    m_miniGraphWidget->setFixedHeight(200);
    layout->addWidget(m_miniGraphWidget);

    connect(m_btnRefreshGraph, &QPushButton::clicked, m_miniGraphWidget, &RelationGraphWidget::refreshGraph);

    // 关系网底部统计行：显示当前联系人的直接关联朋友数量
    auto* graphFooterRow = new QHBoxLayout();
    m_lblGraphFooterName = new QLabel("陈恩华的朋友关系", parent);
    m_lblGraphFooterName->setStyleSheet("color: #8e8e93; font-size: 12px;");
    m_lblGraphFooterCount = new QLabel("0位", parent);
    m_lblGraphFooterCount->setStyleSheet("color: #8e8e93; font-size: 12px;");
    graphFooterRow->addWidget(m_lblGraphFooterName);
    graphFooterRow->addStretch();
    graphFooterRow->addWidget(m_lblGraphFooterCount);
    layout->addLayout(graphFooterRow);

    // 分割线 2
    auto* hLine2 = new QFrame(parent);
    hLine2->setFixedHeight(1);
    hLine2->setStyleSheet("background-color: #28282b; border: none;");
    layout->addWidget(hLine2);

    // 3. AI点评 (参考图5: 左“AI点评” ... 右“生成”(#00a361) + #2f2f30 圆角背景框，文字加大2px，行距调高)
    auto* aiRow = new QHBoxLayout();
    auto* aiTitle = new QLabel("AI点评", parent);
    aiTitle->setStyleSheet("color: #e1e1e5; font-size: 13px; font-weight: bold;");

    m_btnAiEvaluate = new QPushButton("生成", parent);
    m_btnAiEvaluate->setCursor(Qt::PointingHandCursor);
    m_btnAiEvaluate->setStyleSheet(
        "QPushButton { background: transparent; color: #00a361; border: none; font-size: 12px; padding: 2px 4px; }"
        "QPushButton:hover { color: #00c474; }"
    );
    connect(m_btnAiEvaluate, &QPushButton::clicked, this, &ContactWidget::onEvaluateWithAiClicked);

    aiRow->addWidget(aiTitle);
    aiRow->addStretch();
    aiRow->addWidget(m_btnAiEvaluate);
    layout->addLayout(aiRow);

    m_txtAiReview = new QTextBrowser(parent);
    m_txtAiReview->setOpenExternalLinks(false);
    m_txtAiReview->setFrameShape(QFrame::NoFrame);
    m_txtAiReview->setStyleSheet(
        "QTextBrowser { background-color: #2f2f30; color: #e1e1e5; border: none; border-radius: 5px; font-size: 14px; padding: 12px 14px; line-height: 24px; }"
    );
    layout->addWidget(m_txtAiReview, 1);

    // 底部继续咨询输入卡片 (严格按用户设计UI，具备多行文本编辑与右下角发送按键)
    auto* bottomInputContainer = new QWidget(parent);
    bottomInputContainer->setObjectName("contactBottomInputCard");
    bottomInputContainer->setStyleSheet(
        "QWidget#contactBottomInputCard { background-color: #17171a; border: 1px solid #28282e; border-radius: 6px; margin-top: 6px; }"
        "QWidget#contactBottomInputCard:focus-within { border-color: #383842; }"
    );
    auto* bottomInputLayout = new QVBoxLayout(bottomInputContainer);
    bottomInputLayout->setContentsMargins(10, 8, 10, 8);
    bottomInputLayout->setSpacing(6);

    m_contactAiQuestionEdit = new QPlainTextEdit(bottomInputContainer);
    m_contactAiQuestionEdit->setPlaceholderText("请输入AI咨询....");
    m_contactAiQuestionEdit->setMinimumHeight(68);
    m_contactAiQuestionEdit->setMaximumHeight(100);
    m_contactAiQuestionEdit->installEventFilter(this);
    m_contactAiQuestionEdit->setStyleSheet(
        "QPlainTextEdit { background: transparent; color: #ffffff; border: none; font-size: 13px; line-height: 1.5; padding: 0px; selection-background-color: #00a361; }"
    );
    bottomInputLayout->addWidget(m_contactAiQuestionEdit);

    auto* btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(0, 0, 0, 0);
    btnRow->addStretch();

    m_btnSendContactQuestion = new QPushButton("发送", bottomInputContainer);
    m_btnSendContactQuestion->setCursor(Qt::PointingHandCursor);
    m_btnSendContactQuestion->setFixedSize(72, 28);
    m_btnSendContactQuestion->setStyleSheet(
        "QPushButton { background-color: #2f2f35; color: #ffffff; border: 1px solid #3c3c44; border-radius: 4px; font-size: 12px; font-weight: 500; padding: 0px; }"
        "QPushButton:hover { background-color: #00a361; border-color: #00a361; color: #ffffff; }"
        "QPushButton:disabled { background-color: #202024; border-color: #2a2a30; color: #62626a; }"
    );
    connect(m_btnSendContactQuestion, &QPushButton::clicked, this, &ContactWidget::onSendContactConsultQuestion);
    btnRow->addWidget(m_btnSendContactQuestion);

    bottomInputLayout->addLayout(btnRow);
    layout->addWidget(bottomInputContainer);
}

bool ContactWidget::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_contactAiQuestionEdit && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) &&
            !(keyEvent->modifiers() & Qt::ShiftModifier)) {
            onSendContactConsultQuestion();
            return true;
        }
    }
    if (event->type() == QEvent::Resize) {
        if ((m_contactsTable && watched == m_contactsTable->viewport()) ||
            (m_timelineTable && watched == m_timelineTable->viewport()) ||
            (m_eventsTable && watched == m_eventsTable->viewport()) ||
            (m_plansTable && watched == m_plansTable->viewport()) ||
            (m_businessTable && watched == m_businessTable->viewport())) {
            applyTableColumnRatios();
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ContactWidget::applyTableColumnRatios() {
    // 1. 左侧好友表格: 20% | 20% | 20% | 15% | 25%
    if (m_contactsTable && m_contactsTable->viewport()->width() > 50) {
        int w = m_contactsTable->viewport()->width();
        int c0 = static_cast<int>(w * 0.20);
        int c1 = static_cast<int>(w * 0.20);
        int c2 = static_cast<int>(w * 0.20);
        int c3 = static_cast<int>(w * 0.15);
        int c4 = w - c0 - c1 - c2 - c3;
        m_contactsTable->setColumnWidth(0, c0);
        m_contactsTable->setColumnWidth(1, c1);
        m_contactsTable->setColumnWidth(2, c2);
        m_contactsTable->setColumnWidth(3, c3);
        m_contactsTable->setColumnWidth(4, c4);
    }

    // 2. 中栏下方表格: 亲疏/人脉 (20% | 20% | 60% 或 15% | 15% | 70%)
    if (m_relationsTable && m_relationsTable->viewport()->width() > 50) {
        int w = m_relationsTable->viewport()->width();
        int c0 = static_cast<int>(w * 0.20);
        int c1 = static_cast<int>(w * 0.20);
        int c2 = w - c0 - c1;
        m_relationsTable->setColumnWidth(0, c0);
        m_relationsTable->setColumnWidth(1, c1);
        m_relationsTable->setColumnWidth(2, c2);
    }

    if (m_timelineTable && m_timelineTable->viewport()->width() > 50) {
        int w = m_timelineTable->viewport()->width();
        int c0 = static_cast<int>(w * 0.15);
        int c1 = static_cast<int>(w * 0.15);
        int c2 = w - c0 - c1;
        m_timelineTable->setColumnWidth(0, c0);
        m_timelineTable->setColumnWidth(1, c1);
        m_timelineTable->setColumnWidth(2, c2);
    }

    auto apply4ColRatio = [](QTableWidget* tbl) {
        if (!tbl || tbl->viewport()->width() <= 50) return;
        int w = tbl->viewport()->width();
        int c0 = static_cast<int>(w * 0.15);
        int c1 = static_cast<int>(w * 0.15);
        int c2 = static_cast<int>(w * 0.45);
        int c3 = w - c0 - c1 - c2;
        tbl->setColumnWidth(0, c0);
        tbl->setColumnWidth(1, c1);
        tbl->setColumnWidth(2, c2);
        tbl->setColumnWidth(3, c3);
    };
    apply4ColRatio(m_eventsTable);
    apply4ColRatio(m_plansTable);
    apply4ColRatio(m_businessTable);
}

QString ContactWidget::formatAiReportToHtml(const QString& rawMarkdown) const {
    QString text = rawMarkdown.trimmed();
    if (text.isEmpty()) return QString();

    if (text.startsWith("“*")) text = text.mid(2).trimmed();
    else if (text.startsWith("\"*")) text = text.mid(2).trimmed();
    else if (text.startsWith("* ")) { /* keep bullet list */ }
    else if (text.startsWith("*")) text = text.mid(1).trimmed();

    if (text.endsWith("*”")) text.chop(2);
    else if (text.endsWith("*\"")) text.chop(2);
    else if (text.endsWith("*") && !text.endsWith("**")) text.chop(1);

    text = text.trimmed();
    if (text.startsWith("【AI实时参谋点评】")) {
        text = text.mid(QString("【AI实时参谋点评】").length()).trimmed();
    }

    // 彻底过滤模型内部思维链 <think>...</think>，杜绝任何英文思考过程外泄
    static const QRegularExpression thinkBlockRe("<think>.*?</think>", QRegularExpression::DotMatchesEverythingOption);
    text.remove(thinkBlockRe);
    if (text.startsWith("<think>")) {
        int closeIdx = text.indexOf("</think>");
        if (closeIdx >= 0) {
            text = text.mid(closeIdx + 8).trimmed();
        } else {
            return "<div style='color:#febc2f; font-size:14px; font-weight:bold; line-height:24px; padding:4px;'>AI 参谋正在深度构思中...</div>";
        }
    }

    // 关系页面 AI 点评：内部文字加大 2px (12px -> 14px)，排版与综合页面 AI 人生规划聊天保持完全一致
    const int baseFontPx = 14;
    const int lineHeightPx = 24;
    const int titleFontPx = 16;

    auto formatInlineRich = [](QString s) -> QString {
        s = s.toHtmlEscaped();
        static const QRegularExpression boldRe("\\*\\*(.+?)\\*\\*");
        s.replace(boldRe, "<b style='color:#febc2f; font-weight:900;'>\\1</b>");
        static const QRegularExpression quoteRe("“([^”]{1,24})”");
        s.replace(quoteRe, "<span style='color:#ffd166; font-weight:bold;'>“\\1”</span>");
        static const QRegularExpression moneyRe("(¥\\s*[0-9,]+(?:\\.[0-9]+)?|[+-]?¥\\s*[0-9,]+(?:\\.[0-9]+)?|[0-9,]+(?:\\.[0-9]+)?(?:\\s*~\\s*[0-9,]+(?:\\.[0-9]+)?)?\\s*(?:万元|万|元))");
        s.replace(moneyRe, "<b style='color:#35d28d;'>\\1</b>");
        return s;
    };

    auto pickSectionColor = [](const QString& headingText) -> QString {
        if (headingText.contains("风控") || headingText.contains("红线") || headingText.contains("风险") ||
            headingText.contains("底线") || headingText.contains("警告") || headingText.contains("禁忌") ||
            headingText.contains("严禁") || headingText.contains("止损")) {
            return "#ff6b6b";
        }
        if (headingText.contains("总结") || headingText.contains("结论") || headingText.contains("核心") ||
            headingText.contains("概览") || headingText.contains("综述")) {
            return "#35d28d";
        }
        if (headingText.contains("破局") || headingText.contains("行动") || headingText.contains("方案") ||
            headingText.contains("执行") || headingText.contains("下一步")) {
            return "#38bdf8";
        }
        return "#febc2f";
    };

    QStringList lines = text.split('\n');
    QString html = QString(
        "<div style='font-family:\"Microsoft YaHei UI\",\"Microsoft YaHei\",sans-serif; "
        "font-size:%1px; color:#e1e1e5; line-height:%2px;'>"
    ).arg(baseFontPx).arg(lineHeightPx);

    bool isFirstBlock = true;
    bool inRedlineSection = false;

    static const QRegularExpression mdHeadingRe("^#+\\s*(.+)$");
    static const QRegularExpression cnSectionRe("^([一二三四五六七八九十]+[、．.]|（[一二三四五六七八九十]+）)\\s*(.+)$");
    static const QRegularExpression bracketHeaderOnlyRe("^【([^】]+)】[：:]?\\s*$");
    static const QRegularExpression bracketInlineRe("^【([^】]+)】[：:]?\\s*(.+)$");
    static const QRegularExpression numListRe("^(\\d+[.、．])\\s*(.+)$");
    static const QRegularExpression boldLeadColonRe("^\\*\\*(.+?)\\*\\*\\s*([：:])\\s*(.+)$");
    static const QRegularExpression plainLeadColonRe("^([^：:*]{2,18})([：:])\\s*(.+)$");

    auto renderMarkdownTable = [&](const QStringList& tableLines) -> QString {
        if (tableLines.size() < 2) {
            QString out;
            for (const auto& tl : tableLines) out += QString("<p style='margin:4px 0;'>%1</p>").arg(formatInlineRich(tl));
            return out;
        }
        auto splitCells = [](const QString& line) -> QStringList {
            QString clean = line.trimmed();
            if (clean.startsWith('|')) clean = clean.mid(1);
            if (clean.endsWith('|')) clean.chop(1);
            QStringList rawCols = clean.split('|');
            QStringList cols;
            for (const auto& c : rawCols) cols.append(c.trimmed());
            return cols;
        };

        QStringList headers = splitCells(tableLines[0]);
        int startRow = 1;
        if (tableLines.size() > 1 && tableLines[1].contains("---")) {
            startRow = 2;
        }

        QString tHtml = QString("<table style='width:100%; border-collapse:collapse; margin:10px 0; background-color:#202024; border-radius:4px; font-size:%1px;'>").arg(baseFontPx);
        tHtml += "<thead><tr>";
        for (const QString& h : headers) {
            tHtml += QString("<th style='background-color:#27272a; color:#febc2f; border:1px solid #3f3f46; padding:7px 10px; font-weight:bold; text-align:left;'>%1</th>").arg(formatInlineRich(h));
        }
        tHtml += "</tr></thead><tbody>";

        for (int r = startRow; r < tableLines.size(); ++r) {
            QStringList cells = splitCells(tableLines[r]);
            tHtml += "<tr>";
            for (int c = 0; c < headers.size(); ++c) {
                QString val = (c < cells.size()) ? cells[c] : "";
                tHtml += QString("<td style='border:1px solid #3f3f46; padding:6px 10px; color:#e1e1e5;'>%1</td>").arg(formatInlineRich(val));
            }
            tHtml += "</tr>";
        }
        tHtml += "</tbody></table>";
        return tHtml;
    };

    for (int i = 0; i < lines.size(); ++i) {
        QString rawLine = lines[i];
        QString trimmed = rawLine.trimmed();
        if (trimmed.isEmpty()) continue;

        if (trimmed == "---" || trimmed == "***" || trimmed == "___") {
            html += "<hr style='border:none; border-top:1px solid #454549; margin:10px 0;'/>";
            continue;
        }

        // Markdown 表格解析 (| ... |)
        if (trimmed.startsWith('|') && trimmed.endsWith('|')) {
            QStringList tableLines;
            while (i < lines.size()) {
                QString tLine = lines[i].trimmed();
                if (tLine.startsWith('|') && tLine.endsWith('|')) {
                    tableLines.append(tLine);
                    i++;
                } else {
                    break;
                }
            }
            i--;
            html += renderMarkdownTable(tableLines);
            isFirstBlock = false;
            continue;
        }

        // 引用块解析 (> ...)
        if (trimmed.startsWith('>') || trimmed.startsWith("&gt;")) {
            QString quoteContent = trimmed;
            if (quoteContent.startsWith("&gt;")) quoteContent = quoteContent.mid(4).trimmed();
            else if (quoteContent.startsWith('>')) quoteContent = quoteContent.mid(1).trimmed();
            html += QString(
                "<div style='border-left:3px solid #35d28d; background-color:rgba(53,210,141,0.1); "
                "padding:7px 12px; margin:7px 0 7px 4px; border-radius:0 4px 4px 0; color:#cbf3db; font-size:%1px; line-height:%2px;'>"
                "%3</div>"
            ).arg(baseFontPx).arg(lineHeightPx).arg(formatInlineRich(quoteContent));
            isFirstBlock = false;
            continue;
        }

        // 破折提示/引导尾行 (——...)
        if (trimmed.startsWith("——") || trimmed.startsWith("-- ")) {
            html += QString(
                "<p style='margin:10px 0 8px 4px; color:#38bdf8; font-weight:bold; font-size:%1px; line-height:%2px;'>%3</p>"
            ).arg(baseFontPx).arg(lineHeightPx).arg(formatInlineRich(trimmed));
            isFirstBlock = false;
            continue;
        }

        QRegularExpressionMatch mdMatch = mdHeadingRe.match(trimmed);
        if (mdMatch.hasMatch()) {
            QString cleanTitle = mdMatch.captured(1).trimmed();
            cleanTitle.remove("**");
            QString hColor = pickSectionColor(cleanTitle);
            inRedlineSection = (hColor == "#ff6b6b");
            int topMargin = isFirstBlock ? 2 : 14;
            isFirstBlock = false;
            html += QString(
                "<p style='margin:%1px 0 6px 0; color:%2; font-size:%3px; font-weight:900; line-height:%4px;'>%5</p>"
            ).arg(topMargin).arg(hColor).arg(titleFontPx).arg(lineHeightPx + 2).arg(cleanTitle.toHtmlEscaped());
            continue;
        }

        QRegularExpressionMatch cnMatch = cnSectionRe.match(trimmed);
        if (cnMatch.hasMatch()) {
            QString fullHeading = trimmed;
            fullHeading.remove("**");
            QString hColor = pickSectionColor(fullHeading);
            inRedlineSection = (hColor == "#ff6b6b");
            int topMargin = isFirstBlock ? 2 : 14;
            isFirstBlock = false;
            html += QString(
                "<p style='margin:%1px 0 6px 0; color:%2; font-size:%3px; font-weight:900; line-height:%4px;'>%5</p>"
            ).arg(topMargin).arg(hColor).arg(titleFontPx).arg(lineHeightPx + 2).arg(fullHeading.toHtmlEscaped());
            continue;
        }

        QRegularExpressionMatch brOnlyMatch = bracketHeaderOnlyRe.match(trimmed);
        if (brOnlyMatch.hasMatch()) {
            QString innerTitle = brOnlyMatch.captured(1).trimmed();
            QString hColor = pickSectionColor(innerTitle);
            inRedlineSection = (hColor == "#ff6b6b");
            int topMargin = isFirstBlock ? 2 : 12;
            isFirstBlock = false;
            html += QString(
                "<p style='margin:%1px 0 6px 0; color:%2; font-size:%3px; font-weight:900; line-height:%4px;'>【%5】</p>"
            ).arg(topMargin).arg(hColor).arg(titleFontPx).arg(lineHeightPx + 2).arg(innerTitle.toHtmlEscaped());
            continue;
        }

        QRegularExpressionMatch brInlineMatch = bracketInlineRe.match(trimmed);
        if (brInlineMatch.hasMatch()) {
            QString tag = brInlineMatch.captured(1).trimmed();
            QString body = brInlineMatch.captured(2).trimmed();
            QString tagColor = pickSectionColor(tag);
            int topMargin = isFirstBlock ? 2 : 10;
            isFirstBlock = false;
            html += QString(
                "<p style='margin:%1px 0 8px 0; line-height:%2px;'>"
                "<b style='color:%3; font-size:%4px; font-weight:900;'>【%5】</b>"
                "<span style='color:#ffffff; font-weight:bold;'>%6</span></p>"
            ).arg(topMargin).arg(lineHeightPx).arg(tagColor).arg(baseFontPx + 1)
             .arg(tag.toHtmlEscaped(), formatInlineRich(body));
            continue;
        }

        QRegularExpressionMatch numMatch = numListRe.match(trimmed);
        if (numMatch.hasMatch()) {
            isFirstBlock = false;
            QString idxStr = numMatch.captured(1);
            QString rest = numMatch.captured(2).trimmed();
            QString numColor = inRedlineSection ? "#ff6b6b" : "#35d28d";
            QString formattedRest;

            QRegularExpressionMatch boldLead = boldLeadColonRe.match(rest);
            if (boldLead.hasMatch()) {
                QString leadTitle = boldLead.captured(1).trimmed();
                QString colon = boldLead.captured(2);
                QString body = boldLead.captured(3).trimmed();
                bool isRedItem = inRedlineSection || leadTitle.startsWith("不") || leadTitle.contains("严禁") ||
                                 leadTitle.contains("红线") || leadTitle.contains("切勿") || leadTitle.contains("风险");
                QString leadColor = isRedItem ? "#ff7875" : "#febc2f";
                if (isRedItem) numColor = "#ff6b6b";
                formattedRest = QString("<b style='color:%1; font-weight:900;'>%2%3</b>%4")
                                    .arg(leadColor, leadTitle.toHtmlEscaped(), colon, formatInlineRich(body));
            } else {
                QRegularExpressionMatch plainLead = plainLeadColonRe.match(rest);
                if (plainLead.hasMatch()) {
                    QString leadTitle = plainLead.captured(1).trimmed();
                    QString colon = plainLead.captured(2);
                    QString body = plainLead.captured(3).trimmed();
                    bool isRedItem = inRedlineSection || leadTitle.startsWith("不") || leadTitle.contains("严禁") ||
                                     leadTitle.contains("红线") || leadTitle.contains("切勿") || leadTitle.contains("风险");
                    QString leadColor = isRedItem ? "#ff7875" : "#febc2f";
                    if (isRedItem) numColor = "#ff6b6b";
                    formattedRest = QString("<b style='color:%1; font-weight:900;'>%2%3</b>%4")
                                        .arg(leadColor, leadTitle.toHtmlEscaped(), colon, formatInlineRich(body));
                } else {
                    formattedRest = formatInlineRich(rest);
                }
            }

            html += QString(
                "<p style='margin:7px 0 7px 6px; line-height:%1px; color:#e1e1e5;'>"
                "<b style='color:%2; font-weight:900;'>%3 </b>%4</p>"
            ).arg(lineHeightPx).arg(numColor, idxStr.toHtmlEscaped(), formattedRest);
            continue;
        }

        if (trimmed.startsWith("* ") || trimmed.startsWith("- ") || trimmed.startsWith("• ")) {
            isFirstBlock = false;
            QString itemContent = trimmed.mid(2).trimmed();
            QString formattedRest;
            QRegularExpressionMatch boldLead = boldLeadColonRe.match(itemContent);
            if (boldLead.hasMatch()) {
                QString leadTitle = boldLead.captured(1).trimmed();
                QString colon = boldLead.captured(2);
                QString body = boldLead.captured(3).trimmed();
                formattedRest = QString("<b style='color:#febc2f; font-weight:900;'>%1%2</b>%3")
                                    .arg(leadTitle.toHtmlEscaped(), colon, formatInlineRich(body));
            } else {
                QRegularExpressionMatch plainLead = plainLeadColonRe.match(itemContent);
                if (plainLead.hasMatch()) {
                    QString leadTitle = plainLead.captured(1).trimmed();
                    QString colon = plainLead.captured(2);
                    QString body = plainLead.captured(3).trimmed();
                    formattedRest = QString("<b style='color:#febc2f; font-weight:900;'>%1%2</b>%3")
                                        .arg(leadTitle.toHtmlEscaped(), colon, formatInlineRich(body));
                } else {
                    formattedRest = formatInlineRich(itemContent);
                }
            }

            html += QString(
                "<p style='margin:7px 0 7px 6px; line-height:%1px; color:#e1e1e5;'>"
                "<b style='color:#35d28d; font-weight:900;'>• </b>%2</p>"
            ).arg(lineHeightPx).arg(formattedRest);
            continue;
        }

        int topMargin = isFirstBlock ? 2 : 7;
        isFirstBlock = false;
        QString paraHtml = formatInlineRich(trimmed);
        if (trimmed.endsWith("：") || trimmed.endsWith(":")) {
            html += QString(
                "<p style='margin:%1px 0 6px 0; line-height:%2px; color:#f0f0f4; font-weight:bold;'>%3</p>"
            ).arg(topMargin).arg(lineHeightPx).arg(paraHtml);
        } else {
            html += QString(
                "<p style='margin:%1px 0 6px 0; line-height:%2px; color:#e1e1e5;'>%3</p>"
            ).arg(topMargin).arg(lineHeightPx).arg(paraHtml);
        }
    }

    html += "</div>";
    return html;
}

void ContactWidget::refreshData() {
    m_currentContactList = DatabaseManager::instance().getContacts(m_searchEdit ? m_searchEdit->text().trimmed() : "");
    m_contactsTable->setRowCount(0);

    QFont nameFont = m_contactsTable->font();
    nameFont.setPixelSize(12);
    nameFont.setBold(true);

    for (int i = 0; i < m_currentContactList.size(); ++i) {
        const auto& c = m_currentContactList[i];
        m_contactsTable->insertRow(i);

        // 1. 名字：粗体，文字颜色 #e1e1e5
        auto* item0 = new QTableWidgetItem(c.name);
        item0->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        item0->setForeground(QColor("#e1e1e5"));
        item0->setFont(nameFont);
        item0->setData(Qt::UserRole, c.id);

        QString shortInd = c.industry.isEmpty() ? "IT" : c.industry.left(4);
        auto* item1 = new QTableWidgetItem(shortInd);
        item1->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        item1->setForeground(QColor("#8e8e93"));

        auto* item2 = new QTableWidgetItem(c.relationType.isEmpty() ? "朋友" : c.relationType);
        item2->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        item2->setForeground(QColor("#8e8e93"));

        auto* item3 = new QTableWidgetItem(QString("%1星").arg(c.closenessLevel));
        item3->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        item3->setForeground(QColor("#8e8e93"));

        // 2. 投入：两位小数（如 100.10），计算净投入与回报：净投入为红色 (#fa5151)，回报超过投入为绿色 (#00a361)
        double netDiff = c.totalReturn - c.totalSpent;
        double displayAmt = (qAbs(netDiff) > 0.001) ? qAbs(netDiff) : c.totalSpent;
        QColor amtColor("#8e8e93");
        if (c.totalSpent > c.totalReturn + 0.001) {
            amtColor = QColor("#fa5151"); // 投入大于回报：红色
        } else if (c.totalReturn > c.totalSpent + 0.001) {
            amtColor = QColor("#00a361"); // 回报超过我投入：绿色
        }
        auto* item4 = new QTableWidgetItem(QString::number(displayAmt, 'f', 2));
        item4->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        item4->setForeground(amtColor);
        item4->setData(Qt::UserRole, amtColor.name());

        m_contactsTable->setItem(i, 0, item0);
        m_contactsTable->setItem(i, 1, item1);
        m_contactsTable->setItem(i, 2, item2);
        m_contactsTable->setItem(i, 3, item3);
        m_contactsTable->setItem(i, 4, item4);
    }

    QTimer::singleShot(0, this, &ContactWidget::applyTableColumnRatios);

    if (!m_currentContactList.isEmpty()) {
        int targetRow = 0;
        if (m_selectedContactId > 0) {
            for (int r = 0; r < m_currentContactList.size(); ++r) {
                if (m_currentContactList[r].id == m_selectedContactId) {
                    targetRow = r;
                    break;
                }
            }
        }
        m_contactsTable->selectRow(targetRow);
        onContactTableRowClicked(targetRow, 0);
    } else {
        m_selectedContactId = 0;
        m_lblName->setText("—");
        m_avatarBtn->setIcon(QIcon());
        m_avatarBtn->setText("—");
        m_lblPhone->setText("—");
        m_lblAddress->setText("—");
        m_lblIntroducer->setText("—");
        m_lblCompany->setText("—");
        m_lblTitle->setText("—");
        m_lblIndustry->setText("—");
        m_lblClosenessStars->setText("☆☆☆☆☆");
        m_lblInfluenceStars->setText("☆☆☆☆☆");
        m_lblCreditStars->setText("☆☆☆☆☆");
        m_lblPreferences->clear();
        m_lblTaboos->clear();
        if (m_relationsTable) m_relationsTable->setRowCount(0);
        if (m_timelineTable) m_timelineTable->setRowCount(0);
        if (m_eventsTable) m_eventsTable->setRowCount(0);
        if (m_plansTable) m_plansTable->setRowCount(0);
        if (m_businessTable) m_businessTable->setRowCount(0);
        m_lblMeetDate->setText("相识: —");
        m_lblSpendVsReturn->setText("");
        m_txtAiReview->clear();
        if (m_miniGraphWidget) m_miniGraphWidget->setCenterContactId(0);
        if (m_lblGraphFooterCount) m_lblGraphFooterCount->setText("0位");
    }
}

void ContactWidget::onContactTableRowClicked(int row, int col) {
    Q_UNUSED(col);
    for (int r = 0; r < m_contactsTable->rowCount(); ++r) {
        QColor fg = (r == row) ? QColor("#e1e1e5") : QColor("#8e8e93");
        QFont f = m_contactsTable->font();
        f.setPixelSize(12);
        f.setBold(r == row);

        QFont nameFont = f;
        nameFont.setBold(true);

        for (int c = 0; c < m_contactsTable->columnCount(); ++c) {
            if (auto* it = m_contactsTable->item(r, c)) {
                if (c == 0) {
                    // 名字列始终保持粗体 + #e1e1e5
                    it->setForeground(QColor("#e1e1e5"));
                    it->setFont(nameFont);
                } else if (c == 4) {
                    // 投入列保持红/绿语义颜色
                    QString savedColor = it->data(Qt::UserRole).toString();
                    if (savedColor == "#fa5151" || savedColor == "#00a361") {
                        it->setForeground(QColor(savedColor));
                    } else {
                        it->setForeground(fg);
                    }
                    it->setFont(f);
                } else {
                    it->setForeground(fg);
                    it->setFont(f);
                }
            }
        }
    }
    if (row >= 0 && row < m_currentContactList.size()) {
        loadContactDetails(m_currentContactList[row].id);
    }
}

void ContactWidget::loadContactDetails(int contactId) {
    m_selectedContactId = contactId;
    auto cs = DatabaseManager::instance().getContactDetails(contactId);
    if (cs.id == 0) return;

    QString absAvatar = DatabaseManager::instance().resolveAssetPath(cs.avatarPath);
    QPixmap pix(absAvatar);
    if (!cs.avatarPath.isEmpty() && !pix.isNull()) {
        m_avatarBtn->setText("");
        m_avatarBtn->setIcon(QIcon(pix.scaled(64, 64, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)));
        m_avatarBtn->setIconSize(QSize(64, 64));
    } else {
        m_avatarBtn->setIcon(QIcon());
        m_avatarBtn->setText(cs.name.left(1));
    }
    m_lblName->setText(cs.name);
    m_lblPhone->setText(cs.phone.isEmpty() ? "未填写" : cs.phone);
    m_lblAddress->setText(cs.address.isEmpty() ? "暂无" : cs.address);
    m_lblIntroducer->setText(cs.introducerName.isEmpty() ? "本人直识" : cs.introducerName);
    m_lblCompany->setText(cs.company.isEmpty() ? "暂无" : cs.company);
    m_lblTitle->setText(cs.title.isEmpty() ? "暂无" : cs.title);
    m_lblIndustry->setText(cs.industry.isEmpty() ? "IT" : cs.industry);

    auto makeStars = [](int n) {
        QString s;
        for (int i = 0; i < 5; ++i) s += (i < n ? "★" : "☆");
        return s;
    };
    // 3星下(1~2星) 红色 #fa5151 | 3、4星 黄色 #be782e | 5星 绿色 #00a361
    auto starStyle = [](int n) -> QString {
        QString c = "#be782e";
        if (n < 3) c = "#fa5151";
        else if (n >= 5) c = "#00a361";
        return QString("color: %1; font-size: 12px; font-weight: bold;").arg(c);
    };

    m_lblClosenessStars->setText(makeStars(cs.closenessLevel));
    m_lblClosenessStars->setStyleSheet(starStyle(cs.closenessLevel));

    m_lblInfluenceStars->setText(makeStars(cs.influenceLevel));
    m_lblInfluenceStars->setStyleSheet(starStyle(cs.influenceLevel));

    m_lblCreditStars->setText(makeStars(cs.creditRating));
    m_lblCreditStars->setStyleSheet(starStyle(cs.creditRating));

    QString prefText = cs.preferences.trimmed().isEmpty() ? "暂无记录" : cs.preferences.toHtmlEscaped();
    QString tabooText = cs.taboos.trimmed().isEmpty() ? "暂无记录" : cs.taboos.toHtmlEscaped();
    m_lblPreferences->setText(
        QString("<p style='margin:0px; color:#8e8e93; font-size:12px; font-weight:bold; line-height:19px;'>"
                "<span style='color:#00a361;'>喜好：</span>%1</p>").arg(prefText)
    );
    if (m_lblTaboos) {
        m_lblTaboos->setText(
            QString("<p style='margin:0px; color:#8e8e93; font-size:12px; font-weight:bold; line-height:19px;'>"
                    "<span style='color:#fa5151;'>禁忌：</span>%1</p>").arg(tabooText)
        );
    }

    loadBottomTabData(contactId);
    updateRightColumnStats(cs);
}

void ContactWidget::loadBottomTabData(int contactId) {
    auto& db = DatabaseManager::instance();
    QFont tableFont = font();
    tableFont.setPixelSize(14); // 人脉关系 / 亲疏轨迹 / 礼尚往来 / 走动计划 / 业务往来 表格文字 14px

    // 0. 人脉关系 (20% | 20% | 60%)
    if (m_relationsTable) {
        m_relationsTable->setRowCount(0);
        auto relList = db.getRelationshipsForContact(contactId);
        for (int i = 0; i < relList.size(); ++i) {
            const auto& r = relList[i];
            m_relationsTable->insertRow(i);

            auto* it0 = new QTableWidgetItem(r["other_name"].toString());
            it0->setForeground(QColor("#0a84ff"));
            it0->setFont(tableFont);
            it0->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            it0->setData(Qt::UserRole, r["id"].toInt());
            it0->setData(Qt::UserRole + 1, r["other_id"].toInt());
            it0->setData(Qt::UserRole + 2, r["relation_type"].toString());
            it0->setData(Qt::UserRole + 3, r["description"].toString());

            auto* it1 = new QTableWidgetItem(r["relation_type"].toString());
            it1->setForeground(QColor("#8e8e93"));
            it1->setFont(tableFont);
            it1->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

            auto* it2 = new QTableWidgetItem(r["description"].toString());
            it2->setForeground(QColor("#e1e1e5"));
            it2->setFont(tableFont);
            it2->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

            m_relationsTable->setItem(i, 0, it0);
            m_relationsTable->setItem(i, 1, it1);
            m_relationsTable->setItem(i, 2, it2);
        }
    }

    // 1. 亲疏轨迹 (15% | 15% | 70%)
    m_timelineTable->setRowCount(0);
    auto timeline = db.getRelationshipTimeline(contactId);
    for (int i = 0; i < timeline.size(); ++i) {
        m_timelineTable->insertRow(i);

        auto* it0 = new QTableWidgetItem(timeline[i].eventDate.toString("yyyy年MM月dd日"));
        it0->setForeground(QColor("#8e8e93"));
        it0->setFont(tableFont);
        it0->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
        it0->setData(Qt::UserRole, timeline[i].id);
        it0->setData(Qt::UserRole + 1, timeline[i].stageName);
        it0->setData(Qt::UserRole + 2, timeline[i].eventDate);
        it0->setData(Qt::UserRole + 3, timeline[i].description);
        it0->setData(Qt::UserRole + 4, timeline[i].ratingAfter);

        auto* it1 = new QTableWidgetItem(timeline[i].stageName);
        it1->setForeground(QColor("#8e8e93"));
        it1->setFont(tableFont);
        it1->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);

        auto* it2 = new QTableWidgetItem(timeline[i].description);
        it2->setForeground(QColor("#e1e1e5"));
        it2->setFont(tableFont);
        it2->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);

        m_timelineTable->setItem(i, 0, it0);
        m_timelineTable->setItem(i, 1, it1);
        m_timelineTable->setItem(i, 2, it2);
    }

    // 2. 礼尚往来 (15% | 15% | 45% | 25%)
    m_eventsTable->setRowCount(0);
    auto events = db.getSocialEvents(contactId, false);
    for (int i = 0; i < events.size(); ++i) {
        m_eventsTable->insertRow(i);

        QDate dt = QDate::fromString(events[i]["event_date"].toString().left(10), "yyyy-MM-dd");
        auto* it0 = new QTableWidgetItem(dt.isValid() ? dt.toString("yyyy年MM月dd日") : events[i]["event_date"].toString());
        it0->setForeground(QColor("#8e8e93"));
        it0->setFont(tableFont);
        it0->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        it0->setData(Qt::UserRole, events[i]["id"].toInt());
        it0->setData(Qt::UserRole + 1, events[i]["event_category"].toString());
        it0->setData(Qt::UserRole + 2, events[i]["title"].toString());
        it0->setData(Qt::UserRole + 3, events[i]["my_cost"].toDouble());
        it0->setData(Qt::UserRole + 4, events[i]["their_value"].toDouble());
        it0->setData(Qt::UserRole + 5, events[i]["detail"].toString());
        it0->setData(Qt::UserRole + 6, dt);

        QString cat = events[i]["event_category"].toString().trimmed();
        QString dirStr = cat;
        if (cat == "GIFT" || cat == "GIFT_OUT") dirStr = "送礼往来";
        else if (cat == "GIFT_IN") dirStr = "受赠回礼";
        else if (cat == "DINNER" || cat == "TREAT") dirStr = "请客吃饭";
        else if (cat == "ENTERTAIN" || cat == "MEETING") dirStr = "商务宴请";
        else if (cat == "HELP" || cat == "HELP_GIVEN" || cat == "HELP_RECEIVED") dirStr = "人情帮扶";
        else if (cat == "BUSINESS") dirStr = "生意合作";
        else if (cat == "FINANCIAL") dirStr = "资金往来";
        else if (dirStr.isEmpty()) dirStr = "人脉社交";

        auto* it1 = new QTableWidgetItem(dirStr);
        it1->setForeground(QColor("#8e8e93"));
        it1->setFont(tableFont);
        it1->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        auto* it2 = new QTableWidgetItem(events[i]["title"].toString());
        it2->setForeground(QColor("#e1e1e5"));
        it2->setFont(tableFont);
        it2->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        double myCost = events[i]["my_cost"].toDouble();
        double theirVal = events[i]["their_value"].toDouble();
        auto* it3 = new QTableWidgetItem(
            myCost > 0 ? QString("-¥%1").arg(myCost, 0, 'f', 2) : QString("+¥%1").arg(theirVal, 0, 'f', 2)
        );
        it3->setForeground(myCost > 0 ? QColor("#fa5151") : QColor("#00a361"));
        it3->setFont(tableFont);
        it3->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        m_eventsTable->setItem(i, 0, it0);
        m_eventsTable->setItem(i, 1, it1);
        m_eventsTable->setItem(i, 2, it2);
        m_eventsTable->setItem(i, 3, it3);
    }

    // 3. 走动计划 (15% | 15% | 45% | 25%)
    m_plansTable->setRowCount(0);
    auto plans = db.getSocialPlans("");
    int pRow = 0;
    for (const auto& p : plans) {
        if (p.contactId == contactId) {
            m_plansTable->insertRow(pRow);

            auto* it0 = new QTableWidgetItem(p.plannedDate.toString("yyyy年MM月dd日"));
            it0->setForeground(QColor("#8e8e93"));
            it0->setFont(tableFont);
            it0->setData(Qt::UserRole, p.id);
            it0->setData(Qt::UserRole + 1, p.planType);
            it0->setData(Qt::UserRole + 2, p.title);
            it0->setData(Qt::UserRole + 3, p.budget);
            it0->setData(Qt::UserRole + 4, p.remark);
            it0->setData(Qt::UserRole + 5, p.plannedDate);
            it0->setData(Qt::UserRole + 6, p.remindDaysBefore);
            it0->setData(Qt::UserRole + 7, p.scope);

            auto* it1 = new QTableWidgetItem(p.status == "PENDING" ? "待执行" : (p.status == "CANCELLED" ? "已取消" : "已完成"));
            it1->setForeground(p.status == "PENDING" ? QColor("#be782e") : (p.status == "CANCELLED" ? QColor("#fa5151") : QColor("#00a361")));
            it1->setFont(tableFont);

            auto* it2 = new QTableWidgetItem(p.title);
            it2->setForeground(QColor("#e1e1e5"));
            it2->setFont(tableFont);

            auto* it3 = new QTableWidgetItem(p.remark);
            it3->setForeground(QColor("#8e8e93"));
            it3->setFont(tableFont);

            m_plansTable->setItem(pRow, 0, it0);
            m_plansTable->setItem(pRow, 1, it1);
            m_plansTable->setItem(pRow, 2, it2);
            m_plansTable->setItem(pRow, 3, it3);
            pRow++;
        }
    }

    // 4. 业务往来 (15% | 15% | 45% | 25%)
    // 显示与该联系人互相产生的：公司 -> 借钱/还钱/业务经营，个人 -> 借钱/还钱 的数据
    if (m_businessTable) {
        m_businessTable->setRowCount(0);
        auto bizList = db.getContactBusinessTransactions(contactId);
        for (int i = 0; i < bizList.size(); ++i) {
            const auto& r = bizList[i];
            m_businessTable->insertRow(i);

            QString rawTime = r["trans_time"].toString().trimmed();
            if (rawTime.isEmpty()) {
                rawTime = r["trans_date"].toString().trimmed();
            }
            QDate dt = QDate::fromString(rawTime.left(10), "yyyy-MM-dd");
            if (!dt.isValid()) {
                dt = QDate::fromString(rawTime.left(10), "yyyy/MM/dd");
            }
            auto* it0 = new QTableWidgetItem(dt.isValid() ? dt.toString("yyyy年MM月dd日") : rawTime.left(10));
            it0->setForeground(QColor("#8e8e93"));
            it0->setFont(tableFont);
            it0->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            it0->setData(Qt::UserRole, r["id"].toInt());

            QString bookPrefix = (r["book_type"].toString() == "COMPANY") ? "公司" : "个人";
            QString cat = r["category"].toString().trimmed();
            if (cat.isEmpty()) cat = r["category_parent"].toString().trimmed();
            QString sub = r["sub_type"].toString().trimmed();
            if (sub.isEmpty()) sub = r["category_sub"].toString().trimmed();
            QString typeStr = sub.isEmpty() ? QString("%1·%2").arg(bookPrefix, cat)
                                            : QString("%1·%2").arg(bookPrefix, sub);
            auto* it1 = new QTableWidgetItem(typeStr);
            it1->setForeground(QColor("#8e8e93"));
            it1->setFont(tableFont);
            it1->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

            QString proj = r["project_name"].toString().trimmed();
            QString rem = r["remark"].toString().trimmed();
            QString descStr;
            if (!proj.isEmpty() && !rem.isEmpty()) {
                if (proj == rem) {
                    descStr = proj;
                } else {
                    descStr = QString("%1 (%2)").arg(proj, rem);
                }
            } else if (!rem.isEmpty()) {
                descStr = rem;
            } else if (!proj.isEmpty()) {
                descStr = proj;
            } else {
                descStr = QString("%1 %2·%3").arg(bookPrefix, cat, sub);
            }
            auto* it2 = new QTableWidgetItem(descStr);
            it2->setForeground(QColor("#e1e1e5"));
            it2->setFont(tableFont);
            it2->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

            QString ioType = r["trans_type"].toString().trimmed();
            if (ioType.isEmpty()) ioType = r["io_type"].toString().trimmed();
            bool isIncome = (ioType == "INCOME");
            double amt = r["amount"].toDouble();
            auto* it3 = new QTableWidgetItem(
                QString("%1¥%2").arg(isIncome ? "+" : "-").arg(amt, 0, 'f', 2)
            );
            it3->setForeground(isIncome ? QColor("#00a361") : QColor("#fa5151"));
            it3->setFont(tableFont);
            it3->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

            m_businessTable->setItem(i, 0, it0);
            m_businessTable->setItem(i, 1, it1);
            m_businessTable->setItem(i, 2, it2);
            m_businessTable->setItem(i, 3, it3);
        }
    }

    QTimer::singleShot(0, this, &ContactWidget::applyTableColumnRatios);
}

void ContactWidget::updateRightColumnStats(const ContactSummary& cs) {
    // 1. 相识时间 (参考图5: 相识:2026年10月09日(至今2天))
    QDate meet = QDate::fromString(cs.meetDate, "yyyy-MM-dd");
    if (!meet.isValid()) meet = QDate::currentDate().addDays(-2);
    int days = meet.daysTo(QDate::currentDate());
    if (days <= 0) days = 1;
    m_lblMeetDate->setText(QString("相识:%1(至今%2天)")
                           .arg(meet.toString("yyyy年MM月dd日"))
                           .arg(days));

    // 2. 投入与回报 (参考图5: 支出 ¥54835.00 VS 回报(#8e8e93) ¥28000.00(#00a361) 总结(#8e8e93) -¥26835.00(#fa5151))
    double net = cs.totalReturn - cs.totalSpent;
    QString netColor = (net >= 0) ? "#00a361" : "#fa5151";
    m_lblSpendVsReturn->setText(
        QString("<span style='color:#8e8e93;'>支出 ¥%1 VS 回报 </span>"
                "<span style='color:#00a361;'>¥%2</span> "
                "<span style='color:#8e8e93;'>总结 </span>"
                "<span style='color:%3;'>%4¥%5</span>")
            .arg(cs.totalSpent, 0, 'f', 2)
            .arg(cs.totalReturn, 0, 'f', 2)
            .arg(netColor)
            .arg(net >= 0 ? "+" : "-")
            .arg(qAbs(net), 0, 'f', 2)
    );

    // 3. 按数据库针对该联系人和其公司在“人脉社交”和“商务社交”的真实数据显示（有就有、没有就没有）
    auto socialItems = DatabaseManager::instance().getContactSocialBreakdown(cs.id);
    if (m_socialStatsLayout) {
        while (m_socialStatsLayout->count() > 0) {
            auto* item = m_socialStatsLayout->takeAt(0);
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }

        int maxCount = 1;
        for (const auto& si : socialItems) {
            if (si.count > maxCount) maxCount = si.count;
        }

        for (const auto& si : socialItems) {
            auto* row = new QWidget(this);
            auto* h = new QHBoxLayout(row);
            h->setContentsMargins(0, 1, 0, 1);
            h->setSpacing(10);

            auto* t = new QLabel(si.category, row);
            t->setStyleSheet("color: #8e8e93; font-size: 12px;");
            t->setFixedWidth(68);

            auto* bar = new QProgressBar(row);
            bar->setFixedHeight(6);
            bar->setTextVisible(false);
            bar->setRange(0, 100);
            bar->setValue(qMin(100, (si.count * 100) / maxCount));
            bar->setStyleSheet(
                "QProgressBar { background-color: #28282b; border: none; border-radius: 2px; }"
                "QProgressBar::chunk { background-color: #0a84ff; border-radius: 2px; }"
            );

            auto* countLabel = new QLabel(QString("%1次").arg(si.count), row);
            countLabel->setStyleSheet("color: #8e8e93; font-size: 12px;");
            countLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            countLabel->setFixedWidth(36);

            h->addWidget(t);
            h->addWidget(bar, 1);
            h->addWidget(countLabel);
            m_socialStatsLayout->addWidget(row);
        }
    }

    // 4. 关系网底部计数：统计当前这个人的直接朋友关系数量（非“我”的总联系人数）
    m_lblGraphFooterName->setText(QString("%1的朋友关系").arg(cs.name));
    int friendCount = DatabaseManager::instance().getContactDirectFriendCount(cs.id);
    m_lblGraphFooterCount->setText(QString("%1位").arg(friendCount));

    // 5. AI点评：必须按当前联系人的总结 -> 对比综合页面上文财务情况和总总结 -> 想破解和交往策略
    auto snap = DatabaseManager::instance().getFinancialSnapshot();
    auto buildBriefSummary = [&cs]() -> QString {
        QString roleDesc = cs.relationType.isEmpty() ? "核心人脉" : cs.relationType;
        if (!cs.company.trimmed().isEmpty()) {
            roleDesc = QString("%1（%2%3）")
                           .arg(cs.company.trimmed())
                           .arg(cs.title.trimmed().isEmpty() ? "" : cs.title.trimmed() + " · ")
                           .arg(roleDesc);
        }
        double diff = cs.totalReturn - cs.totalSpent;
        QString roiTone;
        if (cs.totalSpent <= 0.001 && cs.totalReturn <= 0.001) {
            roiTone = "当前双方尚无资金流水往来，需以试探性成本洞悉对方筹码与人品。";
        } else if (diff >= 0.0) {
            roiTone = QString("历史往来净回报 +¥%1，属于高价值正向协作关系，重在借力与长效绑定。").arg(diff, 0, 'f', 2);
        } else {
            roiTone = QString("历史往来净投入 -¥%1，当前投入大于直接回报，需严防单向吸血，切断无收益的人情垫资。").arg(qAbs(diff), 0, 'f', 2);
        }
        return QString("【当前联系人总结】\n%1定位为%2，综合评级为亲疏%3星、能量%4星、信用%5星；%6")
            .arg(cs.name)
            .arg(roleDesc)
            .arg(cs.closenessLevel)
            .arg(cs.influenceLevel)
            .arg(cs.creditRating)
            .arg(roiTone);
    };

    QString reviewText = cs.aiReview.trimmed();
    if (reviewText.isEmpty()) {
        QStringList p2Items;
        p2Items.append(QString("1. **我方现状承压**：个人当前资金 ¥%1，流动资金 ¥%2，负债 ¥%3；公司当前资金 ¥%4，流动资金 ¥%5；现金流安全周期约 %6 天。")
                           .arg(snap.personalBalance, 0, 'f', 2)
                           .arg(snap.personalLiquidFund, 0, 'f', 2)
                           .arg(snap.personalBorrowIn, 0, 'f', 2)
                           .arg(snap.companyBalance, 0, 'f', 2)
                           .arg(snap.companyLiquidFund, 0, 'f', 2)
                           .arg(snap.runwayDays));
        p2Items.append("2. **总总结生存红线**：留在牌桌上是第一铁律，坚决拒绝打肿脸充胖子；绝不为非核心人脉盲目垫资或承担连带风险。");

        QStringList p3Items;
        p3Items.append("1. **破解之法（利益制衡）**：抛开道德枷锁与虚荣面子，坚守『合情、合法、合理、可以不道德』原则；凡涉及利益必须签订铁律合同或索取实质抵押质押筹码。");
        p3Items.append("2. **交往策略（进退有度）**：运用《影响力》之“稀缺”与“互惠”心法，表面给足体面与尊严，底线寸步不让；以局外操盘手高度驾驭博弈节奏，实现自身收益确定性。");

        reviewText = buildBriefSummary() +
                     "\n\n【对比综合页面上文财务现状与总总结】\n" + p2Items.join("\n") +
                     "\n\n【想破解和交往策略】\n" + p3Items.join("\n");
    } else {
        QString headSlice = reviewText.left(80);
        if (!headSlice.contains("总结") && !headSlice.contains("核心结论") && !headSlice.contains("综述")) {
            reviewText = buildBriefSummary() + "\n\n" + reviewText;
        }
    }

    if (m_contactAiQuestionEdit) m_contactAiQuestionEdit->clear();
    m_contactAiHistory.clear();
    if (reviewText.isEmpty()) {
        m_txtAiReview->clear();
        if (m_btnAiEvaluate) m_btnAiEvaluate->setText("AI 深度点评");
    } else {
        m_contactAiHistory.append({"assistant", reviewText});
        renderContactAiConversation();
        if (m_btnAiEvaluate) m_btnAiEvaluate->setText("重新生成");
    }

    if (m_miniGraphWidget) {
        m_miniGraphWidget->setCenterContactId(cs.id);
    }
}

void ContactWidget::onSearchTextChanged(const QString& text) {
    Q_UNUSED(text);
    refreshData();
}

void ContactWidget::onAddContactClicked() {
    ContactDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        refreshData();
    }
}

void ContactWidget::onEditContactClicked() {
    if (m_selectedContactId <= 0) return;
    auto cs = DatabaseManager::instance().getContactDetails(m_selectedContactId);
    ContactDialog dlg(cs, this);
    if (dlg.exec() == QDialog::Accepted) {
        refreshData();
    }
}

void ContactWidget::onImportVcfClicked() {
    QString filePath = QFileDialog::getOpenFileName(this, "导入手机通讯录 VCF 文件", "", "vCard 文件 (*.vcf *.vcard)");
    if (filePath.isEmpty()) return;

    auto contacts = VcfParser::parseFile(filePath);
    if (contacts.isEmpty()) {
        QMessageBox::warning(this, "导入失败", "未在选定文件中解析出合法的通讯录联系人。");
        return;
    }

    int successCount = 0;
    auto& db = DatabaseManager::instance();
    for (const auto& c : contacts) {
        if (db.addContactFull(c.name, c.phone, "朋友", c.company, c.title, "IT",
                              3, 3, 3, 0, "", c.note, "", "", "", "")) {
            successCount++;
        }
    }

    QMessageBox::information(this, "导入完成", QString("成功从通讯录导入 %1 位联系人！").arg(successCount));
    refreshData();
}

void ContactWidget::onUpdateAvatarClicked() {
    QString file = QFileDialog::getOpenFileName(this, "选择头像图片", "", "图片文件 (*.png *.jpg *.jpeg *.bmp *.webp *.ico)");
    if (!file.isEmpty()) {
        QString savedRelPath = DatabaseManager::instance().saveUploadedFile(file);
        if (!savedRelPath.isEmpty() && m_selectedContactId > 0) {
            DatabaseManager::instance().updateContactAvatar(m_selectedContactId, savedRelPath);
        }
        QString absPath = savedRelPath.isEmpty() ? file : DatabaseManager::instance().resolveAssetPath(savedRelPath);
        QPixmap pix(absPath);
        if (!pix.isNull()) {
            m_avatarBtn->setText("");
            m_avatarBtn->setIcon(QIcon(pix.scaled(64, 64, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)));
            m_avatarBtn->setIconSize(QSize(64, 64));
        }
    }
}

void ContactWidget::onBottomTabChanged(int index) {
    m_currentBottomTab = index;
    if (m_btnTabRelations) m_btnTabRelations->setChecked(index == 0);
    if (m_btnTabTimeline) m_btnTabTimeline->setChecked(index == 1);
    if (m_btnTabEvents) m_btnTabEvents->setChecked(index == 2);
    if (m_btnTabPlans) m_btnTabPlans->setChecked(index == 3);
    if (m_btnTabBusiness) m_btnTabBusiness->setChecked(index == 4);
    updateTabButtonsStyle();
    m_bottomTabStack->setCurrentIndex(index);
    QTimer::singleShot(0, this, &ContactWidget::applyTableColumnRatios);
}

void ContactWidget::onAddBottomRecordClicked() {
    if (m_selectedContactId <= 0) {
        CustomDialog::showWarning(this, "提示", "请先在左侧选择一位联系人。");
        return;
    }

    auto& db = DatabaseManager::instance();
    auto cs = db.getContactDetails(m_selectedContactId);

    if (m_currentBottomTab == 0) {
        onConnectRelationClicked();
    } else if (m_currentBottomTab == 1) {
        CustomDialog dlg(QString("添加亲疏轨迹 - %1").arg(cs.name), this);
        dlg.setFixedWidth(460);

        auto* layout = new QVBoxLayout();
        layout->setContentsMargins(6, 4, 6, 4);
        layout->setSpacing(10);

        auto* form = new QFormLayout();
        form->setSpacing(10);

        auto* stageCombo = new QComboBox(&dlg);
        stageCombo->addItems({"初认识", "升温热络", "亲密合作", "冷淡疏远", "破冰回暖"});
        form->addRow("发展阶段", stageCombo);

        auto* dateEdit = new QDateEdit(QDate::currentDate(), &dlg);
        dateEdit->setCalendarPopup(true);
        dateEdit->setDisplayFormat("yyyy-MM-dd");
        form->addRow("节点日期", dateEdit);

        // 5 颗星星点击评分组件（点第几颗星就是几分，1~5分）
        auto* starWidget = new StarRatingWidget(cs.closenessLevel, &dlg);
        form->addRow("亲疏星级", starWidget);

        // 事件说明：默认 3 行可见多行输入框
        auto* descEdit = CustomDialog::createMultiLineEdit("输入关键转折事件或交往说明...", &dlg, 3);
        form->addRow("事件说明", descEdit);

        layout->addLayout(form);

        auto* btnRow = new QHBoxLayout();
        btnRow->setSpacing(10);

        auto* btnCancel = new QPushButton("取消", &dlg);
        btnCancel->setCursor(Qt::PointingHandCursor);
        auto* btnSave = new QPushButton("保存", &dlg);
        btnSave->setObjectName("primaryBtn");
        btnSave->setCursor(Qt::PointingHandCursor);

        btnRow->addWidget(btnCancel, 1);
        btnRow->addWidget(btnSave, 1);
        layout->addLayout(btnRow);

        dlg.bodyLayout()->addLayout(layout);
        CustomDialog::normalizeFormWidgets(&dlg);

        connect(btnCancel, &QPushButton::clicked, &dlg, &QDialog::reject);
        connect(btnSave, &QPushButton::clicked, &dlg, [&]() {
            db.addTimelineNode(m_selectedContactId, stageCombo->currentText(), dateEdit->date(), descEdit->toPlainText().trimmed(), starWidget->value());
            dlg.accept();
        });

        if (dlg.exec() == QDialog::Accepted) {
            loadContactDetails(m_selectedContactId);
        }
    } else if (m_currentBottomTab == 2) {
        RelationshipDiaryDialog dlg(m_selectedContactId, cs.name, this);
        if (dlg.exec() == QDialog::Accepted) {
            refreshData();
        }
    } else if (m_currentBottomTab == 3) {
        SocialPlanDialog dlg(m_selectedContactId, this);
        if (dlg.exec() == QDialog::Accepted) {
            loadBottomTabData(m_selectedContactId);
            emit plansChanged();
        }
    } else if (m_currentBottomTab == 4) {
        UnifiedTransactionDialog dlg("EXPENSE", this);
        dlg.setPreselectedContact(m_selectedContactId);
        if (dlg.exec() == QDialog::Accepted) {
            refreshData();
        }
    }
}

void ContactWidget::onConnectRelationClicked() {
    if (m_selectedContactId <= 0) return;
    auto cs = DatabaseManager::instance().getContactDetails(m_selectedContactId);
    auto contacts = DatabaseManager::instance().getContacts();

    CustomDialog dlg("添加关系网关联", this);
    dlg.setFixedWidth(460);

    auto* layout = new QVBoxLayout();
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(10);

    auto* form = new QFormLayout();
    form->setSpacing(10);

    auto* curNameEdit = new QLineEdit(QString("%1 (%2)").arg(cs.name, cs.industry.isEmpty() ? "IT" : cs.industry), &dlg);
    curNameEdit->setReadOnly(true);
    form->addRow("当前中心", curNameEdit);

    auto* targetCombo = new QComboBox(&dlg);
    for (const auto& c : contacts) {
        if (c.id != m_selectedContactId) {
            targetCombo->addItem(QString("%1 (%2)").arg(c.name, c.industry.isEmpty() ? c.relationType : c.industry), c.id);
        }
    }
    form->addRow("关联对象", targetCombo);

    // 关系性质：左下拉选择 + 右边自定义输入
    auto* relRow = new QHBoxLayout();
    relRow->setSpacing(8);
    auto* relCombo = new QComboBox(&dlg);
    QStringList relTypes = DatabaseManager::instance().getDictionaryValues("RELATION_TYPE");
    if (relTypes.isEmpty()) {
        relTypes = {"朋友", "合伙人", "引荐", "同学", "客户", "同事", "亲人", "投资人"};
    }
    relCombo->addItems(relTypes);

    auto* customRelEdit = new QLineEdit(&dlg);
    customRelEdit->setPlaceholderText("自定义关系性质...");
    relRow->addWidget(relCombo, 1);
    relRow->addWidget(customRelEdit, 1);
    form->addRow("关系性质", relRow);

    // 关系描述 (relationships.description)
    auto* descEdit = CustomDialog::createMultiLineEdit("输入关系背景说明或具体往来描述...", &dlg, 2);
    form->addRow("关系描述", descEdit);

    layout->addLayout(form);

    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);

    auto* btnCancel = new QPushButton("取消", &dlg);
    btnCancel->setCursor(Qt::PointingHandCursor);
    auto* btnSave = new QPushButton("保存", &dlg);
    btnSave->setObjectName("primaryBtn");
    btnSave->setCursor(Qt::PointingHandCursor);

    btnRow->addWidget(btnCancel, 1);
    btnRow->addWidget(btnSave, 1);
    layout->addLayout(btnRow);

    dlg.bodyLayout()->addLayout(layout);
    CustomDialog::normalizeFormWidgets(&dlg);

    connect(btnCancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(btnSave, &QPushButton::clicked, &dlg, [&]() {
        if (targetCombo->count() == 0) {
            QMessageBox::warning(&dlg, "提示", "请先添加其他联系人后再建立关联。");
            return;
        }
        int targetId = targetCombo->currentData().toInt();
        QString relType = customRelEdit->text().trimmed();
        if (!relType.isEmpty()) {
            DatabaseManager::instance().addDictionaryValue("RELATION_TYPE", relType);
        } else {
            relType = relCombo->currentText().trimmed();
        }
        if (relType.isEmpty()) relType = "朋友";

        QString desc = descEdit->toPlainText().trimmed();

        QSqlQuery q(DatabaseManager::instance().database());
        q.prepare("INSERT OR REPLACE INTO relationships (from_contact_id, to_contact_id, relation_type, description) VALUES (:f, :t, :r, :desc);");
        q.bindValue(":f", m_selectedContactId);
        q.bindValue(":t", targetId);
        q.bindValue(":r", relType);
        q.bindValue(":desc", desc);
        q.exec();

        dlg.accept();
    });

    if (dlg.exec() == QDialog::Accepted) {
        if (m_miniGraphWidget) {
            m_miniGraphWidget->refreshGraph();
        }
        int friendCount = DatabaseManager::instance().getContactDirectFriendCount(m_selectedContactId);
        m_lblGraphFooterCount->setText(QString("%1位").arg(friendCount));
        loadBottomTabData(m_selectedContactId);
    }
}

void ContactWidget::onDeleteContactClicked() {
    if (m_selectedContactId <= 0) return;
    auto cs = DatabaseManager::instance().getContactDetails(m_selectedContactId);
    if (CustomDialog::confirm(this, "删除联系人", QString("确定要彻底删除联系人【%1】及其所有关联记录吗？\n注意：此操作将清理所有往来与亲疏记录，不可恢复！").arg(cs.name), "删除", "取消", true)) {
        if (DatabaseManager::instance().deleteContact(m_selectedContactId)) {
            m_selectedContactId = 0;
            refreshData();
            emit plansChanged();
        } else {
            CustomDialog::showError(this, "错误", "删除联系人失败，请重试。");
        }
    }
}

void ContactWidget::onRelationsContextMenu(const QPoint& pos) {
    int row = m_relationsTable->rowAt(pos.y());
    if (row < 0) return;
    auto* item0 = m_relationsTable->item(row, 0);
    if (!item0) return;
    int relId = item0->data(Qt::UserRole).toInt();
    QString otherName = item0->text();
    QString relType = item0->data(Qt::UserRole + 2).toString();
    QString desc = item0->data(Qt::UserRole + 3).toString();

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #202024; border: 1px solid #3f3f46; padding: 4px; border-radius: 4px; }"
        "QMenu::item { color: #e1e1e5; padding: 6px 18px; border-radius: 3px; font-size: 13px; }"
        "QMenu::item:selected { background-color: #2b2b30; color: #ffffff; }"
    );
    QAction* actEdit = menu.addAction(UiIcons::icon(UiIcons::Code::Diary, 12, QColor("#be782e")), "编辑");
    QAction* actDelete = menu.addAction(UiIcons::icon(UiIcons::Code::Delete, 12, QColor("#fa5151")), "删除");
    QAction* selected = menu.exec(m_relationsTable->viewport()->mapToGlobal(pos));

    if (selected == actEdit) {
        CustomDialog dlg(QString("编辑人脉关系 - %1").arg(otherName), this);
        dlg.setFixedWidth(460);
        auto* form = new QFormLayout();
        form->setSpacing(10);

        auto* nameEdit = new QLineEdit(otherName, &dlg);
        nameEdit->setReadOnly(true);
        form->addRow("关联对象", nameEdit);

        auto* relRow = new QHBoxLayout();
        relRow->setSpacing(8);
        auto* relCombo = new QComboBox(&dlg);
        QStringList relTypes = DatabaseManager::instance().getDictionaryValues("RELATION_TYPE");
        if (relTypes.isEmpty()) relTypes = {"朋友", "合伙人", "引荐", "同学", "客户", "同事", "亲人", "投资人"};
        relCombo->addItems(relTypes);
        int idx = relCombo->findText(relType);
        if (idx >= 0) relCombo->setCurrentIndex(idx);

        auto* customRelEdit = new QLineEdit(&dlg);
        customRelEdit->setPlaceholderText("自定义关系性质...");
        relRow->addWidget(relCombo, 1);
        relRow->addWidget(customRelEdit, 1);
        form->addRow("关系性质", relRow);

        auto* descEdit = CustomDialog::createMultiLineEdit("输入关系描述...", &dlg, 3);
        descEdit->setPlainText(desc);
        form->addRow("关系描述", descEdit);

        auto* btnRow = new QHBoxLayout();
        btnRow->setSpacing(10);
        auto* btnCancel = new QPushButton("取消", &dlg);
        btnCancel->setCursor(Qt::PointingHandCursor);
        auto* btnSave = new QPushButton("保存", &dlg);
        btnSave->setObjectName("primaryBtn");
        btnSave->setCursor(Qt::PointingHandCursor);
        btnRow->addWidget(btnCancel);
        btnRow->addWidget(btnSave);

        dlg.bodyLayout()->addLayout(form);
        dlg.bodyLayout()->addLayout(btnRow);
        CustomDialog::normalizeFormWidgets(&dlg);

        connect(btnCancel, &QPushButton::clicked, &dlg, &QDialog::reject);
        connect(btnSave, &QPushButton::clicked, &dlg, [&]() {
            QString chosenRel = customRelEdit->text().trimmed();
            if (!chosenRel.isEmpty()) {
                DatabaseManager::instance().addDictionaryValue("RELATION_TYPE", chosenRel);
            } else {
                chosenRel = relCombo->currentText().trimmed();
            }
            if (chosenRel.isEmpty()) chosenRel = "朋友";
            DatabaseManager::instance().updateRelationship(relId, chosenRel, descEdit->toPlainText().trimmed());
            dlg.accept();
        });

        if (dlg.exec() == QDialog::Accepted) {
            loadBottomTabData(m_selectedContactId);
            if (m_miniGraphWidget) m_miniGraphWidget->refreshGraph();
        }
    } else if (selected == actDelete) {
        if (CustomDialog::confirm(this, "删除关联", QString("确定要删除与【%1】的关系关联吗？").arg(otherName), "删除", "取消", true)) {
            DatabaseManager::instance().deleteRelationship(relId);
            loadBottomTabData(m_selectedContactId);
            if (m_miniGraphWidget) m_miniGraphWidget->refreshGraph();
            int friendCount = DatabaseManager::instance().getContactDirectFriendCount(m_selectedContactId);
            m_lblGraphFooterCount->setText(QString("%1位").arg(friendCount));
        }
    }
}

void ContactWidget::onTimelineContextMenu(const QPoint& pos) {
    int row = m_timelineTable->rowAt(pos.y());
    if (row < 0) return;
    auto* item0 = m_timelineTable->item(row, 0);
    if (!item0) return;
    int nodeId = item0->data(Qt::UserRole).toInt();
    QString stage = item0->data(Qt::UserRole + 1).toString();
    QDate dt = item0->data(Qt::UserRole + 2).toDate();
    QString desc = item0->data(Qt::UserRole + 3).toString();
    int rating = item0->data(Qt::UserRole + 4).toInt();

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #202024; border: 1px solid #3f3f46; padding: 4px; border-radius: 4px; }"
        "QMenu::item { color: #e1e1e5; padding: 6px 18px; border-radius: 3px; font-size: 13px; }"
        "QMenu::item:selected { background-color: #2b2b30; color: #ffffff; }"
    );
    QAction* actEdit = menu.addAction(UiIcons::icon(UiIcons::Code::Diary, 12, QColor("#be782e")), "编辑");
    QAction* actDelete = menu.addAction(UiIcons::icon(UiIcons::Code::Delete, 12, QColor("#fa5151")), "删除");
    QAction* selected = menu.exec(m_timelineTable->viewport()->mapToGlobal(pos));

    if (selected == actEdit) {
        CustomDialog dlg("编辑亲疏轨迹", this);
        dlg.setFixedWidth(460);
        auto* form = new QFormLayout();
        form->setSpacing(10);

        auto* stageCombo = new QComboBox(&dlg);
        stageCombo->addItems({"初认识", "升温热络", "亲密合作", "冷淡疏远", "破冰回暖"});
        int sIdx = stageCombo->findText(stage);
        if (sIdx >= 0) stageCombo->setCurrentIndex(sIdx);
        form->addRow("发展阶段", stageCombo);

        auto* dateEdit = new QDateEdit(dt.isValid() ? dt : QDate::currentDate(), &dlg);
        dateEdit->setCalendarPopup(true);
        dateEdit->setDisplayFormat("yyyy-MM-dd");
        form->addRow("节点日期", dateEdit);

        auto* starWidget = new StarRatingWidget(rating > 0 ? rating : 3, &dlg);
        form->addRow("亲疏星级", starWidget);

        auto* descEdit = CustomDialog::createMultiLineEdit("输入关键转折说明...", &dlg, 3);
        descEdit->setPlainText(desc);
        form->addRow("事件说明", descEdit);

        auto* btnRow = new QHBoxLayout();
        btnRow->setSpacing(10);
        auto* btnCancel = new QPushButton("取消", &dlg);
        btnCancel->setCursor(Qt::PointingHandCursor);
        auto* btnSave = new QPushButton("保存", &dlg);
        btnSave->setObjectName("primaryBtn");
        btnSave->setCursor(Qt::PointingHandCursor);
        btnRow->addWidget(btnCancel);
        btnRow->addWidget(btnSave);

        dlg.bodyLayout()->addLayout(form);
        dlg.bodyLayout()->addLayout(btnRow);
        CustomDialog::normalizeFormWidgets(&dlg);

        connect(btnCancel, &QPushButton::clicked, &dlg, &QDialog::reject);
        connect(btnSave, &QPushButton::clicked, &dlg, [&]() {
            DatabaseManager::instance().updateTimelineNode(nodeId, stageCombo->currentText(), dateEdit->date(), descEdit->toPlainText().trimmed(), starWidget->value());
            dlg.accept();
        });

        if (dlg.exec() == QDialog::Accepted) {
            loadContactDetails(m_selectedContactId);
        }
    } else if (selected == actDelete) {
        if (CustomDialog::confirm(this, "删除轨迹", "确定要删除该亲疏轨迹记录吗？", "删除", "取消", true)) {
            DatabaseManager::instance().deleteTimelineNode(nodeId);
            loadContactDetails(m_selectedContactId);
        }
    }
}

void ContactWidget::onEventsContextMenu(const QPoint& pos) {
    int row = m_eventsTable->rowAt(pos.y());
    if (row < 0) return;
    auto* item0 = m_eventsTable->item(row, 0);
    if (!item0) return;
    int eventId = item0->data(Qt::UserRole).toInt();
    QString cat = item0->data(Qt::UserRole + 1).toString();
    QString title = item0->data(Qt::UserRole + 2).toString();
    double myCost = item0->data(Qt::UserRole + 3).toDouble();
    double theirVal = item0->data(Qt::UserRole + 4).toDouble();
    QString detail = item0->data(Qt::UserRole + 5).toString();
    QDate dt = item0->data(Qt::UserRole + 6).toDate();

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #202024; border: 1px solid #3f3f46; padding: 4px; border-radius: 4px; }"
        "QMenu::item { color: #e1e1e5; padding: 6px 18px; border-radius: 3px; font-size: 13px; }"
        "QMenu::item:selected { background-color: #2b2b30; color: #ffffff; }"
    );
    QAction* actEdit = menu.addAction(UiIcons::icon(UiIcons::Code::Diary, 12, QColor("#be782e")), "编辑");
    QAction* actDelete = menu.addAction(UiIcons::icon(UiIcons::Code::Delete, 12, QColor("#fa5151")), "删除");
    QAction* selected = menu.exec(m_eventsTable->viewport()->mapToGlobal(pos));

    if (selected == actEdit) {
        auto cs = DatabaseManager::instance().getContactDetails(m_selectedContactId);
        RelationshipDiaryDialog dlg(eventId, m_selectedContactId, cs.name, this);
        if (dlg.exec() == QDialog::Accepted) {
            loadContactDetails(m_selectedContactId);
            refreshData();
        }
    } else if (selected == actDelete) {
        if (CustomDialog::confirm(this, "删除往来", "确定要删除该礼尚往来记录吗？", "删除", "取消", true)) {
            DatabaseManager::instance().deleteSocialEvent(eventId);
            loadContactDetails(m_selectedContactId);
            refreshData();
        }
    }
}

void ContactWidget::onPlansContextMenu(const QPoint& pos) {
    int row = m_plansTable->rowAt(pos.y());
    if (row < 0) return;
    auto* item0 = m_plansTable->item(row, 0);
    if (!item0) return;
    int planId = item0->data(Qt::UserRole).toInt();
    QString planType = item0->data(Qt::UserRole + 1).toString();
    QString title = item0->data(Qt::UserRole + 2).toString();
    double budget = item0->data(Qt::UserRole + 3).toDouble();
    QString remark = item0->data(Qt::UserRole + 4).toString();
    QDate dt = item0->data(Qt::UserRole + 5).toDate();
    int remindDays = item0->data(Qt::UserRole + 6).toInt();
    if (remindDays <= 0) remindDays = 3;
    QString scope = item0->data(Qt::UserRole + 7).toString();
    if (scope.isEmpty()) scope = "PERSONAL";

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #202024; border: 1px solid #3f3f46; padding: 4px; border-radius: 4px; }"
        "QMenu::item { color: #e1e1e5; padding: 6px 18px; border-radius: 3px; font-size: 13px; }"
        "QMenu::item:selected { background-color: #2b2b30; color: #ffffff; }"
    );
    QAction* actEdit = menu.addAction(UiIcons::icon(UiIcons::Code::Diary, 12, QColor("#be782e")), "编辑");
    QAction* actDelete = menu.addAction(UiIcons::icon(UiIcons::Code::Delete, 12, QColor("#fa5151")), "删除");
    QAction* selected = menu.exec(m_plansTable->viewport()->mapToGlobal(pos));

    if (selected == actEdit) {
        CustomDialog dlg("编辑走动计划", this);
        dlg.setFixedWidth(460);
        auto* form = new QFormLayout();
        form->setSpacing(10);

        // 事项类型：左下拉联动 + 右边自定义输入 (工作->商务社交，个人->人脉社交)
        auto* typeBox = new QWidget(&dlg);
        auto* typeLayout = new QHBoxLayout(typeBox);
        typeLayout->setContentsMargins(0, 0, 0, 0);
        typeLayout->setSpacing(8);

        auto* typeCombo = new QComboBox(typeBox);
        QString parentCat = (scope == "COMPANY") ? "商务社交" : "人脉社交";
        QStringList subs = DatabaseManager::instance().getSubCategoriesByParent(scope, "EXPENSE", parentCat);
        if (subs.isEmpty()) {
            subs = (scope == "COMPANY") ? QStringList{"餐饮", "娱乐", "礼品", "差旅", "形象", "商务洽谈"}
                                        : QStringList{"聚餐", "拜访", "娱乐", "礼物", "帮扶", "应酬", "商务洽谈"};
        }
        for (const auto& s : subs) {
            if (typeCombo->findText(s) < 0) typeCombo->addItem(s);
        }
        if (!planType.isEmpty() && typeCombo->findText(planType) < 0) {
            typeCombo->addItem(planType);
        }
        int typeIdx = typeCombo->findText(planType);
        if (typeIdx >= 0) typeCombo->setCurrentIndex(typeIdx);

        auto* customTypeEdit = new QLineEdit(typeBox);
        customTypeEdit->setPlaceholderText("或自定义类型");
        typeLayout->addWidget(typeCombo, 1);
        typeLayout->addWidget(customTypeEdit, 1);
        form->addRow("事项类型", typeBox);

        // 计划主题 (多行可见输入框)
        auto* titleEdit = CustomDialog::createMultiLineEdit("请输入计划主题（例如：请陈总吃饭商谈二期合作...）", &dlg, 3);
        titleEdit->setPlainText(title);
        form->addRow("计划主题", titleEdit);

        // 预定日期
        auto* dateEdit = new QDateEdit(dt.isValid() ? dt : QDate::currentDate(), &dlg);
        dateEdit->setCalendarPopup(true);
        dateEdit->setDisplayFormat("yyyy-MM-dd");
        form->addRow("预定日期", dateEdit);

        // 提前预警
        auto* remindSpin = new QSpinBox(&dlg);
        remindSpin->setRange(1, 30);
        remindSpin->setValue(remindDays > 0 ? remindDays : 3);
        remindSpin->setSuffix(" 天前提醒");
        form->addRow("提前预警", remindSpin);

        // 预算金额
        auto* budgetSpin = new QDoubleSpinBox(&dlg);
        budgetSpin->setRange(0, 10000000);
        budgetSpin->setDecimals(2);
        budgetSpin->setValue(budget);
        form->addRow("预算金额(¥)", budgetSpin);

        // 补充备忘
        auto* remarkEdit = CustomDialog::createMultiLineEdit("地点安排、备选餐厅、注意事项...", &dlg, 2);
        remarkEdit->setPlainText(remark);
        form->addRow("补充备忘", remarkEdit);

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

        dlg.bodyLayout()->addLayout(form);
        dlg.bodyLayout()->addLayout(btnRow);
        CustomDialog::normalizeFormWidgets(&dlg);

        connect(btnCancel, &QPushButton::clicked, &dlg, &QDialog::reject);
        connect(btnSave, &QPushButton::clicked, &dlg, [&]() {
            QString finalTitle = titleEdit->toPlainText().trimmed();
            if (finalTitle.isEmpty()) {
                CustomDialog::markFieldInvalid(titleEdit);
                return;
            }

            QString customType = customTypeEdit->text().trimmed();
            QString finalType = customType.isEmpty() ? typeCombo->currentText().trimmed() : customType;
            if (finalType.isEmpty()) finalType = (scope == "COMPANY") ? "商务洽谈" : "人脉社交";

            if (!customType.isEmpty()) {
                DatabaseManager::instance().addCustomCategory("EXPENSE", parentCat, customType, scope);
            }

            DatabaseManager::instance().updateSocialPlan(
                planId,
                finalType,
                finalTitle,
                budgetSpin->value(),
                remarkEdit->toPlainText().trimmed(),
                dateEdit->date(),
                remindSpin->value()
            );
            dlg.accept();
        });

        if (dlg.exec() == QDialog::Accepted) {
            loadBottomTabData(m_selectedContactId);
            emit plansChanged();
        }
    } else if (selected == actDelete) {
        if (CustomDialog::confirm(this, "删除计划", "确定要删除该走动计划吗？", "删除", "取消", true)) {
            DatabaseManager::instance().deleteSocialPlan(planId);
            loadBottomTabData(m_selectedContactId);
            emit plansChanged();
        }
    }
}

void ContactWidget::onBusinessContextMenu(const QPoint& pos) {
    int row = m_businessTable->rowAt(pos.y());
    if (row < 0) return;
    auto* item0 = m_businessTable->item(row, 0);
    if (!item0) return;
    int transId = item0->data(Qt::UserRole).toInt();

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #202024; border: 1px solid #3f3f46; padding: 4px; border-radius: 4px; }"
        "QMenu::item { color: #e1e1e5; padding: 6px 18px; border-radius: 3px; font-size: 13px; }"
        "QMenu::item:selected { background-color: #2b2b30; color: #ffffff; }"
    );
    QAction* actEdit = menu.addAction(UiIcons::icon(UiIcons::Code::Diary, 12, QColor("#be782e")), "编辑");
    QAction* actDelete = menu.addAction(UiIcons::icon(UiIcons::Code::Delete, 12, QColor("#fa5151")), "删除");
    QAction* selected = menu.exec(m_businessTable->viewport()->mapToGlobal(pos));

    if (selected == actEdit) {
        UnifiedTransactionDialog dlg("EXPENSE", this);
        dlg.setEditTransactionId(transId);
        if (dlg.exec() == QDialog::Accepted) {
            refreshData();
        }
    } else if (selected == actDelete) {
        if (CustomDialog::confirm(this, "删除往来", "确定要删除该笔业务往来流水吗？此操作不可恢复。", "删除", "取消", true)) {
            DatabaseManager::instance().deleteTransaction(transId);
            refreshData();
        }
    }
}

void ContactWidget::onEvaluateWithAiClicked() {
    if (m_selectedContactId <= 0) return;
    auto cs = DatabaseManager::instance().getContactDetails(m_selectedContactId);

    m_btnAiEvaluate->setEnabled(false);
    m_btnAiEvaluate->setText("生成中...");
    m_txtAiReview->setHtml("<div style='color:#febc2f; font-size:14px; font-weight:bold; line-height:24px;'>AI 参谋正在结合人脉能量、交往频次与财务往来深度推演点评中...</div>");

    m_contactAiHistory.clear();
    m_accumulatedContactAiReview.clear();
    m_contactAiDirty = false;
    if (!m_contactAiThrottleTimer) {
        m_contactAiThrottleTimer = new QTimer(this);
        m_contactAiThrottleTimer->setInterval(60);
        connect(m_contactAiThrottleTimer, &QTimer::timeout, this, [this]() {
            if (m_contactAiDirty && m_txtAiReview) {
                m_contactAiDirty = false;
                m_txtAiReview->setHtml(formatAiReportToHtml(m_accumulatedContactAiReview));
                m_txtAiReview->verticalScrollBar()->setValue(m_txtAiReview->verticalScrollBar()->maximum());
            }
        });
    }

    auto connToken = std::make_shared<QMetaObject::Connection>();
    auto connFinished = std::make_shared<QMetaObject::Connection>();
    auto connFailed = std::make_shared<QMetaObject::Connection>();
    QPointer<ContactWidget> guard(this);

    *connToken = connect(&AiAdvisorService::instance(), &AiAdvisorService::tokenReceived, this,
        [guard](const QString& token) {
            if (!guard) return;
            guard->m_accumulatedContactAiReview += token;
            guard->m_contactAiDirty = true;
            if (guard->m_contactAiThrottleTimer && !guard->m_contactAiThrottleTimer->isActive()) {
                guard->m_contactAiThrottleTimer->start();
            }
        });

    *connFinished = connect(&AiAdvisorService::instance(), &AiAdvisorService::evaluationFinished, this,
        [guard, cs, connToken, connFinished, connFailed](const QString& evaluation) {
            disconnect(*connToken);
            disconnect(*connFinished);
            disconnect(*connFailed);
            if (!guard) return;
            if (guard->m_contactAiThrottleTimer) guard->m_contactAiThrottleTimer->stop();
            guard->m_contactAiDirty = false;
            guard->m_btnAiEvaluate->setEnabled(true);
            guard->m_btnAiEvaluate->setText("重新生成");
            guard->m_contactAiHistory.clear();
            guard->m_contactAiHistory.append({"assistant", evaluation});
            DatabaseManager::instance().saveContactAiReview(cs.id, evaluation);
            auto updatedCs = DatabaseManager::instance().getContactDetails(cs.id);
            guard->updateRightColumnStats(updatedCs);
            guard->renderContactAiConversation();
        });

    *connFailed = connect(&AiAdvisorService::instance(), &AiAdvisorService::evaluationFailed, this,
        [guard, cs, connToken, connFinished, connFailed](const QString& err) {
            disconnect(*connToken);
            disconnect(*connFinished);
            disconnect(*connFailed);
            if (!guard) return;
            if (guard->m_contactAiThrottleTimer) guard->m_contactAiThrottleTimer->stop();
            guard->m_contactAiDirty = false;
            guard->m_btnAiEvaluate->setEnabled(true);
            guard->m_btnAiEvaluate->setText("生成");
            guard->m_txtAiReview->setHtml(QString("<div style='color:#fa5151; font-size:14px; padding:4px;'>AI 评估失败：%1</div>").arg(err.toHtmlEscaped()));
            guard->updateRightColumnStats(cs);
        });

    AiAdvisorService::instance().evaluateContact(m_selectedContactId);
}

void ContactWidget::renderContactAiConversation() {
    if (!m_txtAiReview) return;
    QString fullHtml;

    for (int i = 0; i < m_contactAiHistory.size(); ++i) {
        const auto& item = m_contactAiHistory[i];
        if (item.role == "user") {
            fullHtml += QString(
                "<div style='margin-top:12px; margin-bottom:8px; padding:8px 12px; background-color:#202024; border-left:3px solid #00a361; border-radius:4px;'>"
                "<b style='color:#00d68f; font-size:13px;'>💬 我的咨询：</b><br/>"
                "<span style='color:#ffffff; font-size:14px; line-height:22px;'>%1</span>"
                "</div>"
            ).arg(item.content.toHtmlEscaped());
        } else if (item.role == "assistant") {
            fullHtml += formatAiReportToHtml(item.content);
        }
    }

    if (m_isContactAiGenerating) {
        if (!m_accumulatedContactAiReview.isEmpty()) {
            fullHtml += formatAiReportToHtml(m_accumulatedContactAiReview);
        } else {
            fullHtml += "<div style='color:#febc2f; font-size:14px; font-weight:bold; line-height:24px; padding:4px;'>AI 军师正在深入分析解答中...</div>";
        }
    }

    m_txtAiReview->setHtml(fullHtml);
    m_txtAiReview->verticalScrollBar()->setValue(m_txtAiReview->verticalScrollBar()->maximum());
}

void ContactWidget::onSendContactConsultQuestion() {
    if (m_selectedContactId <= 0 || m_isContactAiGenerating) return;
    if (!m_contactAiQuestionEdit) return;

    QString q = m_contactAiQuestionEdit->toPlainText().trimmed();
    if (q.isEmpty()) return;

    auto engine = AiAdvisorService::instance().engine();
    if (!engine) {
        CustomDialog::showWarning(this, "提示", "AI 推理引擎尚未就绪！");
        return;
    }

    m_contactAiQuestionEdit->clear();
    m_isContactAiGenerating = true;
    m_accumulatedContactAiReview.clear();

    if (m_btnSendContactQuestion) {
        m_btnSendContactQuestion->setText("生成中...");
        m_btnSendContactQuestion->setEnabled(false);
    }
    if (m_contactAiQuestionEdit) m_contactAiQuestionEdit->setEnabled(false);

    QString trimmedLower = q.trimmed().toLower();
    QString promptForLlm = q;
    if (trimmedLower == "继续" || trimmedLower == "请继续" || trimmedLower == "接着写" || trimmedLower == "接着说" || trimmedLower == "继续说") {
        promptForLlm = "请承接你上面未阐述完毕的内容继续往下深入剖析，直接输出后续内容，条理清晰，严禁重复前文已说过的废话。";
    }

    m_contactAiHistory.append({"user", q});
    renderContactAiConversation();

    // 组装多轮历史对话消息（首条为合并的 system，紧接着 initial user，然后 assistant 与本次 user 问答）
    auto cs = DatabaseManager::instance().getContactDetails(m_selectedContactId);
    QString contextMsg = QString("【当前咨询联系人背景】\n- 姓名：%1 | 单位：%2 | 职位：%3 | 关系性质：%4\n【语言最高铁律】：从第一个字开始，必须全程严格使用纯中文回复，严禁擅自使用英文！")
        .arg(cs.name).arg(cs.company).arg(cs.title).arg(cs.relationType);

    QString fullSysPrompt = AiAdvisorService::instance().buildSystemPrompt() + "\n\n" + contextMsg;
    QList<ILlmEngine::EngineChatMessage> msgs;
    msgs.append({"system", fullSysPrompt});

    // 确保在 assistant 报告前有一条对应的 user 请求，彻底杜绝 Ollama 500 角色颠倒错误
    QString initialUserPrompt = QString("请结合当前联系人背景出具深度人脉战略与破解交往策略点评报告：\n姓名：%1，单位：%2，职位：%3，关系定位：%4")
        .arg(cs.name, cs.company, cs.title, cs.relationType);
    msgs.append({"user", initialUserPrompt});

    for (int i = 0; i < m_contactAiHistory.size(); ++i) {
        const auto& item = m_contactAiHistory[i];
        if (i == m_contactAiHistory.size() - 1 && item.role == "user") {
            msgs.append({"user", promptForLlm});
        } else {
            msgs.append({item.role, item.content});
        }
    }

    if (!m_contactAiThrottleTimer) {
        m_contactAiThrottleTimer = new QTimer(this);
        m_contactAiThrottleTimer->setInterval(60);
        connect(m_contactAiThrottleTimer, &QTimer::timeout, this, [this]() {
            if (m_contactAiDirty && m_txtAiReview) {
                m_contactAiDirty = false;
                renderContactAiConversation();
            }
        });
    }

    QPointer<ContactWidget> guard(this);
    engine->chatStreamMessages(
        msgs,
        [guard](const QString& token, bool isFinished) {
            if (!guard) return;
            if (!token.isEmpty()) {
                guard->m_accumulatedContactAiReview += token;
                guard->m_contactAiDirty = true;
                if (guard->m_contactAiThrottleTimer && !guard->m_contactAiThrottleTimer->isActive()) {
                    guard->m_contactAiThrottleTimer->start();
                }
            }
            if (isFinished) {
                if (guard->m_contactAiThrottleTimer) guard->m_contactAiThrottleTimer->stop();
                guard->m_contactAiDirty = false;
                guard->m_isContactAiGenerating = false;
                if (!guard->m_accumulatedContactAiReview.isEmpty()) {
                    guard->m_contactAiHistory.append({"assistant", guard->m_accumulatedContactAiReview});
                }
                guard->renderContactAiConversation();
                if (guard->m_btnSendContactQuestion) {
                    guard->m_btnSendContactQuestion->setText("发送");
                    guard->m_btnSendContactQuestion->setEnabled(true);
                }
                if (guard->m_contactAiQuestionEdit) guard->m_contactAiQuestionEdit->setEnabled(true);
            }
        },
        [guard](const QString& err) {
            if (!guard) return;
            if (guard->m_contactAiThrottleTimer) guard->m_contactAiThrottleTimer->stop();
            guard->m_contactAiDirty = false;
            guard->m_isContactAiGenerating = false;
            guard->m_contactAiHistory.append({"assistant", "【AI 生成失败】：" + err});
            guard->renderContactAiConversation();
            if (guard->m_btnSendContactQuestion) {
                guard->m_btnSendContactQuestion->setText("发送");
                guard->m_btnSendContactQuestion->setEnabled(true);
            }
            if (guard->m_contactAiQuestionEdit) guard->m_contactAiQuestionEdit->setEnabled(true);
        }
    );
}
