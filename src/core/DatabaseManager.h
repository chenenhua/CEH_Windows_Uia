#pragma once

#include <QString>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariantList>
#include <QVariantMap>
#include <QDateTime>
#include <vector>

struct FinancialSnapshot {
    double personalBalance{0.0};      // 个人当前资金 (总进 - 总出)
    double personalLiquidFund{0.0};   // 个人流动资金 (当前资金 - 负债)
    double companyBalance{0.0};       // 公司当前资金 (总进 - 总出)
    double companyLiquidFund{0.0};    // 公司流动资金 (当前资金 - 负债)
    double personalTotalIncome{0.0};  // 个人总进资金
    double personalTotalExpense{0.0}; // 个人总出资金
    double companyTotalIncome{0.0};   // 公司总进资金
    double companyTotalExpense{0.0};  // 公司总出资金
    double monthlyCompanyExpense{0.0};
    double monthlyCompanyIncome{0.0};
    double monthlyPersonalExpense{0.0};
    double monthlyPersonalIncome{0.0};
    double personalLendOut{0.0};   // 个人借出待收回 (个人应收)
    double personalBorrowIn{0.0};  // 个人总负债 (个人欠款)
    double companyLendOut{0.0};    // 公司借出待收回 (公司应收)
    double companyBorrowIn{0.0};   // 公司总负债 (公司欠款)
    int runwayDays{0};             // 现金流安全天数
    int pendingGiftsCount{0};      // 待回礼/待还人情数
    double totalLendOut{0.0};      // 合计应收
    double totalBorrowIn{0.0};     // 合计负债
};

struct UserProfile {
    QString name{"陈恩华"};
    QString phone{"15355688333"};
    QString province{"浙江省"};
    QString city{"台州市"};
    QString district{"路桥区"};
    QString detailAddress{"螺洋街道银苑路84号"};
    QString address{"浙江省台州市路桥区螺洋街道银苑路84号"};
    QString industry{"信息技术"};
    QString company{"台州奕点网络科技有限公司"};
    QString title{"创始人"};
    QString birthDate{"1981-10-09"};
    QString aiRules;
    int companyCount{3};
    int contactCount{437};
};

struct CategoryBreakdownItem {
    QString category;
    double amount{0.0};
    double percentage{0.0};
    int count{0};
};

struct CategoryHierarchyItem {
    QString parentCategory;
    QStringList subCategories;
};

struct ContactSummary {
    int id{0};
    QString name;
    QString phone;
    QString relationType{"朋友"};
    QString company;
    int companyId{0};
    QString title;
    QString industry;
    int closenessLevel{5};
    int influenceLevel{5};
    int creditRating{5};
    QString preferences;
    QString taboos;
    double totalSpent{0.0};  // 在该联系人身上累计开销 (支出)
    double totalReturn{0.0}; // 累计回报 (收入)
    QString meetDate{"2026-10-09"};
    int helpCount{0};
    int entertainmentCount{0};
    int diningCount{0};
    int cooperationCount{0};
    QString aiReview;
    QString introducerName;
    int introducerId{0};
    QString province;
    QString city;
    QString district;
    QString address;
    QString notes;
    QString avatarPath;
};

struct RegionItem {
    QString code;
    QString name;
    int level{1};
};

struct LedgerMetrics {
    double income{0.0};
    double expense{0.0};
    double surplus{0.0};
    double debt{0.0};
    double receivable{0.0};
    int friendCount{0};
};

struct DebtItem {
    int id{0};
    QString borrowerType;
    QString direction;
    int contactId{0};
    QString contactName;
    QString institutionName;
    double principal{0.0};
    double interestRate{0.0};
    QString repayType;
    QDate startDate;
    QDate dueDate;
    int monthlyPayDay{0};
    double monthlyPayment{0.0};
    double repaidAmount{0.0};
    QString status;
    QString remark;
};

struct SocialPlanItem {
    int id{0};
    int contactId{0};
    QString contactName;
    QString scope{"PERSONAL"}; // 'PERSONAL', 'COMPANY'
    int companyId{0};
    QString companyName;
    QString planType;          // 对应 人脉社交 / 商务社交 下的具体类型
    QString title;
    QDate plannedDate;
    int remindDaysBefore{3};
    double budget{0.0};
    QString status;            // 'PENDING', 'DONE', 'CANCELLED'
    QString remark;
};

struct AssetItem {
    int id{0};
    QString assetType;
    QString name;
    double estimatedValue{0.0};
    QString ownership; // 'PERSONAL', 'COMPANY'
    int companyId{0};
    QString companyName;
    QDate purchaseDate;
    int linkedContactId{0};
    QString linkedContactName;
    int linkedDebtId{0};
    QString remark;
    QString status{"HOLDING"};
};

struct TimelineNode {
    int id{0};
    int contactId{0};
    QString stageName;
    QDate eventDate;
    QString description;
    int ratingAfter{3};
};

struct ContactInteractionStats {
    QString contactName;
    QDate meetDate;
    int entertainCount{0};
    int dinnerCount{0};
    int businessCount{0};
    int helpCount{0};
    double myTotalCost{0.0};
    double theirTotalValue{0.0};
};

class DatabaseManager {
public:
    static DatabaseManager& instance();

    bool initialize(const QString& dbPath = "assets/db/wood.db");
    void close();
    QString databasePath() const { return m_dbPath; }
    QSqlDatabase database() const { return m_db; }

    // 动态浮动密钥加密容器解锁与同步接口
    bool isUnlocked() const { return m_isUnlocked && m_db.isOpen(); }
    bool isPasswordConfiguredInStorage() const;
    bool unlockWithPassword(const QString& password, bool isFirstTimeSetup = false);
    bool updateEncryptionPassword(const QString& newPassword);
    bool syncEncryptedDatabaseToDisk();

    // 项目根路径与头像/凭证上传存储辅助 ("项目地址/assets/uppic/yyyy/MM/dd/HH_mm_zzz.png")
    QString projectRootDir() const;
    QString saveUploadedFile(const QString& sourceFilePath) const;
    QString resolveAssetPath(const QString& relativeOrAbsPath) const;

    // 省市区行政区划联动查询 (基于 regions 表)
    QList<RegionItem> getRegionsByParent(const QString& parentCode = "") const;

    // 个人名片与全景档案
    UserProfile getUserProfile();
    bool saveUserProfile(const UserProfile& profile);

    // 公司主体
    int getCompaniesCount();
    QList<QVariantMap> getCompaniesList();
    bool addCompany(const QString& name, const QString& roleType = "OWNER", const QString& industry = "",
                    const QString& remark = "", const QString& taxNo = "",
                    double registeredCapital = 0.0, double shareRatio = 100.0,
                    int legalRepContactId = 0, const QString& legalRepName = "");
    bool updateCompany(int id, const QString& name, const QString& roleType = "OWNER",
                       const QString& industry = "", const QString& remark = "",
                       const QString& taxNo = "", double registeredCapital = 0.0,
                       double shareRatio = 100.0, bool clearAssociatedData = false,
                       int legalRepContactId = 0, const QString& legalRepName = "");
    bool saveOrUpdateCompany(int& companyId,
                             const QString& originalName,
                             const QString& newName,
                             const QString& roleType = "EXTERNAL",
                             const QString& industry = "",
                             const QString& remark = "",
                             const QString& taxNo = "",
                             double registeredCapital = 0.0,
                             double shareRatio = 0.0,
                             int legalRepContactId = 0,
                             const QString& legalRepName = "",
                             int isMyCompany = -1);
    int ensureCompanyExists(const QString& name, const QString& roleType = "EXTERNAL", int isMyCompany = 0);
    bool deleteCompany(int id);

    // 财务相关与分类统计
    FinancialSnapshot getFinancialSnapshot();
    LedgerMetrics getPeriodMetrics(const QString& bookType, int year, int month = 0, int day = 0);
    QList<QVariantMap> getLedgerTransactions(const QString& bookType, int year, int month = 0, int day = 0, const QString& search = "");
    QList<CategoryBreakdownItem> getMonthlyCategoryBreakdown(const QString& transType, int year = 0, int month = 0);
    QList<QVariantMap> getRecentTransactions(int limit = 50, const QString& bookType = "", int year = 0, int month = 0);
    bool addTransaction(const QString& bookType, int accountId, const QString& transType,
                        double amount, const QString& category, const QDateTime& time,
                        int contactId = 0, const QString& remark = "",
                        const QString& subType = "", int companyId = 0,
                        const QString& voucherPath = "");

    // 动态层级大类/小类管理
    QList<CategoryHierarchyItem> getCategoryHierarchy(const QString& transType, const QString& scope = "");
    QStringList getSubCategoriesByParent(const QString& bookType, const QString& ioType, const QString& parentCategory);
    bool addCustomCategory(const QString& transType, const QString& parentCategory, const QString& subCategory, const QString& scope = "PERSONAL");
    QStringList getCategories(const QString& type) {
        QStringList cats;
        auto items = getCategoryHierarchy(type);
        for (const auto& item : items) {
            cats.append(item.subCategories);
        }
        return cats;
    }
    bool addCategory(const QString& type, const QString& name) {
        return addCustomCategory(type, "日常分类", name);
    }

    // 动态字典 (单位/职务/行业/关系)
    QStringList getDictionaryValues(const QString& category);
    bool addDictionaryValue(const QString& category, const QString& value);

    // 账户相关 (已废弃，保留空接口兼容旧文件编译)
    QList<QVariantMap> getAccounts(const QString& bookType = "");

    // 人脉相关
    int getContactsCount();
    QList<ContactSummary> getContacts(const QString& keyword = "");
    ContactSummary getContactDetails(int contactId);
    int getContactDirectFriendCount(int contactId);
    QList<CategoryBreakdownItem> getContactSocialBreakdown(int contactId);
    bool addContact(const QString& name, const QString& phone, const QString& relationType,
                    const QString& company, const QString& title, const QString& industry,
                    int closeness, int influence, int credit, int introducerId,
                    const QString& preferences, const QString& notes);
    bool addContactFull(const QString& name, const QString& phone, const QString& relationType,
                        const QString& company, const QString& title, const QString& industry,
                        int closeness, int influence, int credit, int introducerId,
                        const QString& preferences, const QString& notes,
                        const QString& province, const QString& city, const QString& district,
                        const QString& address, const QString& taboos = "", const QString& knownSince = "");
    bool updateContact(int contactId, const QString& name, const QString& phone, const QString& relationType,
                       const QString& company, const QString& title, const QString& industry,
                       int closeness, int influence, int credit, const QString& preferences);
    bool updateContactFull(int contactId, const QString& name, const QString& phone, const QString& relationType,
                           const QString& company, const QString& title, const QString& industry,
                           int closeness, int influence, int credit, const QString& preferences,
                           const QString& province, const QString& city, const QString& district,
                           const QString& address, const QString& notes, const QString& taboos = "",
                           const QString& knownSince = "");
    bool updateContactAvatar(int contactId, const QString& avatarPath);
    bool saveContactAiReview(int contactId, const QString& reviewText);
    ContactInteractionStats getContactInteractionStats(int contactId);
    QList<QVariantMap> getContactBusinessTransactions(int contactId);
    QList<int> getAvailableTransactionYears();

    // 亲疏演变轨迹
    QList<TimelineNode> getRelationshipTimeline(int contactId);
    bool addTimelineNode(int contactId, const QString& stageName, const QDate& eventDate,
                         const QString& description, int ratingAfter);

    // 人情事件与闭环
    QList<QVariantMap> getSocialEvents(int contactId = 0, bool onlyPending = false);
    bool addSocialEvent(int contactId, const QString& eventType, const QString& title,
                        const QString& detail, double estimatedValue, const QDate& eventDate,
                        const QString& status, int transactionId = 0);

    // 财务与心境日记
    QList<QVariantMap> getDiaries(const QString& bookType = "", int limit = 100);
    QList<QVariantMap> getDiariesFiltered(const QString& bookType, int year = 0, int month = 0, int day = 0, const QString& search = "");
    bool addDiary(const QString& dateStr, const QString& content, const QString& mood = "平静",
                  const QString& bookType = "PERSONAL", int contactId = 0);

    // 关系图谱边
    QList<QVariantMap> getRelationshipEdges();

    // 借贷中心兼容接口
    QList<DebtItem> getDebts(const QString& directionFilter = "");
    bool addDebt(const QString& borrowerType, const QString& direction, int contactId,
                 const QString& institutionName, double principal, double interestRate,
                 const QString& repayType, const QDate& startDate, const QDate& dueDate,
                 int monthlyPayDay, double monthlyPayment, const QString& remark);
    QList<DebtItem> getUpcomingDebts(int daysAhead = 30);

    // 社交走动计划与到期提醒
    QList<SocialPlanItem> getSocialPlans(const QString& status = "");
    bool addSocialPlan(int contactId, const QString& planType, const QString& title,
                       const QDate& plannedDate, int remindDaysBefore, const QString& remark,
                       const QString& scope = "PERSONAL", int companyId = 0, double budget = 0.0);
    bool updateSocialPlanStatus(int planId, const QString& status);
    bool completeSocialPlanWithExpense(int planId, double expenseAmount, const QString& category,
                                      int accountId, const QString& remark);
    QList<SocialPlanItem> getUpcomingSocialPlans(int daysAhead = 30);

    // 资产管理 (添加、修改、出售变现联动记账)
    QList<AssetItem> getAssets();
    bool addAsset(const QString& assetType, const QString& name, double estimatedValue,
                  const QString& ownership, const QDate& purchaseDate, int linkedContactId,
                  int linkedDebtId, const QString& remark, int companyId = 0);
    bool updateAsset(int assetId, const QString& assetType, const QString& name,
                     double estimatedValue, const QString& ownership, int companyId, const QString& remark);
    bool sellAsset(int assetId, const QString& assetType, const QString& name,
                   double salePrice, const QString& ownership, int companyId, const QString& remark);

    // 安全设置
    QString getSecurityConfig(const QString& key);
    bool setSecurityConfig(const QString& key, const QString& value);

    // 数据备份与自毁
    bool exportDatabaseBackup(const QString& destinationPath);
    bool selfDestructDatabase();

    // 关系、流水、日记、人情删除与修改接口
    bool deleteContact(int contactId);
    bool deleteTransaction(int transactionId);
    bool updateTransaction(int id, const QString& bookType, int accountId, const QString& transType,
                           double amount, const QString& category, const QDateTime& transTime,
                           int contactId, const QString& remark, const QString& parentCategory,
                           int companyId = 0, const QString& voucherPath = "");
    QVariantMap getTransactionById(int id);

    // 人脉关系网详细操作
    QList<QVariantMap> getRelationshipsForContact(int contactId);
    bool updateRelationship(int id, const QString& relationType, const QString& description);
    bool deleteRelationship(int id);

    // 亲疏轨迹修改与删除
    bool updateTimelineNode(int id, const QString& stageName, const QDate& eventDate,
                            const QString& description, int ratingAfter);
    bool deleteTimelineNode(int id);

    // 礼尚往来查询、修改与删除
    QVariantMap getSocialEventById(int id);
    bool updateSocialEvent(int id, const QString& category, const QString& title,
                           double myCost, double theirValue, const QString& detail, const QDate& eventDate);
    bool updateSocialEventFull(int id, int contactId, const QString& scope, int companyId,
                               const QString& ioType, const QString& category, const QString& title,
                               double myCost, double theirValue, const QString& detail,
                               const QDate& eventDate);
    bool deleteSocialEvent(int id);

    // 走动计划修改与删除
    bool updateSocialPlan(int id, const QString& planType, const QString& title,
                          double budget, const QString& detail, const QDate& plannedDate,
                          int remindDaysBefore = 3);
    bool deleteSocialPlan(int id);

    // 日记修改与删除
    bool updateDiary(int id, const QString& bookType, const QDate& diaryDate,
                     const QString& mood, int contactId, const QString& content);
    bool deleteDiary(int id);

private:
    DatabaseManager() = default;
    ~DatabaseManager();
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    bool executeSqlScript(const QString& scriptPath);
    void migrateSchemaIfNeeded();
    void seedInitialSampleDataIfNeeded();
    void cleanupStaleRuntimeFiles() const;
    void shredAndRemoveFile(const QString& filePath) const;

    QSqlDatabase m_db;
    QString m_connectionName{"SocialRelationsConnection"};
    QString m_dbPath{"wood.db"};
    QString m_runtimeDbPath;
    QString m_activePasswordHash;
    bool m_isUnlocked{false};
    int m_lastSyncedTotalChanges{-1};
    class QTimer* m_autoSyncTimer{nullptr};
    mutable QHash<QString, QList<RegionItem>> m_regionChildrenCache;
    mutable bool m_regionsCached{false};
};
