#include "DatabaseManager.h"
#include "SecurityManager.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDebug>
#include <QDir>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QUuid>
#include <QTimer>
#include <QDate>
#include <QImage>
#include <QSet>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

DatabaseManager& DatabaseManager::instance() {
    static DatabaseManager instance;
    return instance;
}

DatabaseManager::~DatabaseManager() {
    close();
}

QString DatabaseManager::projectRootDir() const {
    QString appDir = QCoreApplication::applicationDirPath();
    return appDir.isEmpty() ? QDir::currentPath() : appDir;
}

QString DatabaseManager::saveUploadedFile(const QString& sourceFilePath) const {
    if (sourceFilePath.trimmed().isEmpty() || !QFile::exists(sourceFilePath)) {
        return "";
    }
    QDateTime now = QDateTime::currentDateTime();
    QString relDir = QString("assets/uppic/%1").arg(now.toString("yyyy/MM/dd"));
    QString fileName = QString("%1.png").arg(now.toString("HH_mm_zzz"));
    QString relPath = relDir + "/" + fileName;

    QString rootDir = projectRootDir();
    QDir dir(rootDir);
    dir.mkpath(relDir);
    QString targetAbsPath = dir.filePath(relPath);

    QImage img(sourceFilePath);
    bool saved = false;
    if (!img.isNull()) {
        saved = img.save(targetAbsPath, "PNG");
    }
    if (!saved) {
        if (QFile::exists(targetAbsPath)) QFile::remove(targetAbsPath);
        saved = QFile::copy(sourceFilePath, targetAbsPath);
    }

    return saved ? relPath : "";
}

QString DatabaseManager::resolveAssetPath(const QString& relativeOrAbsPath) const {
    QString normalized = relativeOrAbsPath.trimmed();
    if (normalized.isEmpty()) return "";
    normalized.replace('\\', '/');

    while (normalized.startsWith("./")) {
        normalized = normalized.mid(2);
    }

    // 1. 优先检查当前软件运行/安装目录 (<获取的安装地址>/assets/...)
    QDir installDir(projectRootDir());
    if (QDir::isRelativePath(normalized)) {
        QString fromInstall = installDir.filePath(normalized);
        if (QFile::exists(fromInstall)) {
            return QFileInfo(fromInstall).absoluteFilePath();
        }
    }

    // 2. 如果是已存在的绝对路径，直接返回
    QFileInfo fi(normalized);
    if (fi.isAbsolute() && fi.exists()) {
        return fi.absoluteFilePath();
    }

    // 3. 若传入的是旧电脑/其他路径下的绝对路径但包含 "assets/"，自动重映射到当前安装目录的 assets/
    int assetsIdx = normalized.indexOf("assets/", 0, Qt::CaseInsensitive);
    if (assetsIdx >= 0) {
        QString subAssetPath = normalized.mid(assetsIdx);
        QString remapped = installDir.filePath(subAssetPath);
        if (QFile::exists(remapped)) {
            return QFileInfo(remapped).absoluteFilePath();
        }
    }

    // 4. 开发环境回退：如果 cmake-build-debug / cmake-build-release 下尚未同步最新文件，回退检查上一级项目源码目录
    QDir parentDir(installDir.filePath(".."));
    QString fromParent = parentDir.filePath(normalized);
    if (QFile::exists(fromParent)) {
        return QFileInfo(fromParent).absoluteFilePath();
    }

    // 5. 智能别名映射：自动兼容 "uppic/..." 与 "assets/uppic/..." 互相查找
    if (normalized.startsWith("uppic/", Qt::CaseInsensitive)) {
        QString alt = "assets/" + normalized;
        if (QFile::exists(installDir.filePath(alt))) return QFileInfo(installDir.filePath(alt)).absoluteFilePath();
        if (QFile::exists(parentDir.filePath(alt))) return QFileInfo(parentDir.filePath(alt)).absoluteFilePath();
    } else if (normalized.startsWith("assets/uppic/", Qt::CaseInsensitive)) {
        QString alt = normalized.mid(7); // 去掉 "assets/"
        if (QFile::exists(installDir.filePath(alt))) return QFileInfo(installDir.filePath(alt)).absoluteFilePath();
        if (QFile::exists(parentDir.filePath(alt))) return QFileInfo(parentDir.filePath(alt)).absoluteFilePath();
    }

    return installDir.filePath(normalized);
}

void DatabaseManager::shredAndRemoveFile(const QString& filePath) const {
    if (filePath.isEmpty() || !QFile::exists(filePath)) return;
#ifdef _WIN32
    SetFileAttributesW(reinterpret_cast<LPCWSTR>(filePath.utf16()), FILE_ATTRIBUTE_NORMAL);
#endif
    {
        QFile f(filePath);
        qint64 sz = f.size();
        if (sz > 0 && f.open(QIODevice::ReadWrite)) {
            QByteArray zeroChunk(qMin<qint64>(sz, 65536), '\0');
            f.seek(0);
            f.write(zeroChunk);
            f.flush();
            f.close();
        }
    }
    QFile::remove(filePath);
    QFile::remove(filePath + "-wal");
    QFile::remove(filePath + "-shm");
    QFile::remove(filePath + "-journal");
}

void DatabaseManager::cleanupStaleRuntimeFiles() const {
    QString tempDirStr = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (tempDirStr.isEmpty()) return;
    QDir tempDir(tempDirStr);
    const QFileInfoList staleList = tempDir.entryInfoList(
        QStringList() << ".wd_rt_*.sys*" << ".wd_mig_*.tmp*",
        QDir::Files | QDir::Hidden | QDir::System
    );
    for (const QFileInfo& fi : staleList) {
        if (!m_runtimeDbPath.isEmpty() && fi.absoluteFilePath() == QFileInfo(m_runtimeDbPath).absoluteFilePath()) {
            continue;
        }
        shredAndRemoveFile(fi.absoluteFilePath());
    }
}

bool DatabaseManager::initialize(const QString& dbPath) {
    Q_UNUSED(dbPath);
    close();
    m_regionsCached = false;
    m_regionChildrenCache.clear();
    cleanupStaleRuntimeFiles();

    QString root = projectRootDir();
    QDir rootDir(root);
    rootDir.mkpath("assets/db");

    m_dbPath = rootDir.filePath("assets/db/wood.db");
    QString sqlFilePath = rootDir.filePath("assets/db/sqlite3.sql");

    // 清理遗留的明文 WAL / SHM 临时文件
    QFile::remove(m_dbPath + "-wal");
    QFile::remove(m_dbPath + "-shm");

    // 1. 若磁盘上的 wood.db 已经是 WOODENC2 加密容器，保持锁定状态，等待用户输入主密码后才可解密打开
    if (SecurityManager::isEncryptedContainer(m_dbPath)) {
        m_isUnlocked = false;
        return true;
    }

    // 2. 若磁盘上的 wood.db 是旧版明文 SQLite3 或尚未创建，立即在启动阶段将其转换为当前电脑浮动密钥加密容器 (WOODENC2)
    QString tempDirStr = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString migPath = QDir(tempDirStr).filePath(QString(".wd_mig_%1.tmp").arg(QUuid::createUuid().toString(QUuid::Id128)));

    bool needInitFromSql = !QFile::exists(m_dbPath) || (QFileInfo(m_dbPath).size() == 0);
    if (!needInitFromSql) {
        QFile::copy(m_dbPath, migPath);
    }

    if (QSqlDatabase::contains(m_connectionName)) {
        m_db = QSqlDatabase::database(m_connectionName);
        if (m_db.isOpen()) m_db.close();
    } else {
        m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    }

    m_db.setDatabaseName(migPath);
    if (!m_db.open()) {
        qWarning() << "[DB] Failed to open migration database:" << m_db.lastError().text();
        shredAndRemoveFile(migPath);
        return false;
    }

    {
        QSqlQuery pragmaQuery(m_db);
        pragmaQuery.exec("PRAGMA journal_mode = DELETE;");
        pragmaQuery.exec("PRAGMA synchronous = NORMAL;");

        if (!needInitFromSql) {
            QSqlQuery checkQ("SELECT name FROM sqlite_master WHERE type='table' AND name='contacts';", m_db);
            if (!checkQ.next()) {
                needInitFromSql = true;
            }
        }

        if (needInitFromSql && QFile::exists(sqlFilePath)) {
            qInfo() << "[DB] Initializing wood.db schema from:" << sqlFilePath;
            executeSqlScript(sqlFilePath);
        }

        pragmaQuery.exec("PRAGMA foreign_keys = ON;");
        migrateSchemaIfNeeded();
    }

    QString existingPwdHash = getSecurityConfig("master_password_hash");
    m_db.close();

    QFile migFile(migPath);
    QByteArray plainBytes;
    if (migFile.open(QIODevice::ReadOnly)) {
        plainBytes = migFile.readAll();
        migFile.close();
    }
    shredAndRemoveFile(migPath);

    if (plainBytes.isEmpty()) {
        qWarning() << "[DB] Failed to read migration database bytes.";
        return false;
    }

    bool encryptedOk = SecurityManager::instance().encryptDatabaseFile(m_dbPath, plainBytes, existingPwdHash);
    plainBytes.fill('\0');
    plainBytes.clear();

    m_isUnlocked = false;
    return encryptedOk;
}

bool DatabaseManager::isPasswordConfiguredInStorage() const {
    if (m_isUnlocked && m_db.isOpen()) {
        QSqlQuery q(m_db);
        q.prepare("SELECT value FROM security_config WHERE key = 'master_password_hash';");
        if (q.exec() && q.next()) {
            return !q.value(0).toString().trimmed().isEmpty();
        }
        return false;
    }
    bool hasPwd = false;
    if (SecurityManager::isEncryptedContainer(m_dbPath, &hasPwd)) {
        return hasPwd;
    }
    return false;
}

bool DatabaseManager::unlockWithPassword(const QString& password, bool isFirstTimeSetup) {
    if (m_isUnlocked && m_db.isOpen()) {
        return true;
    }

    if (!SecurityManager::isEncryptedContainer(m_dbPath)) {
        if (!initialize(m_dbPath)) {
            return false;
        }
    }

    QString candidateHash = SecurityManager::instance().hashPassword(password);
    QByteArray plainBytes;
    bool usedPortableSlot = false;

    if (isFirstTimeSetup) {
        if (!SecurityManager::instance().decryptDatabaseFile(m_dbPath, QString(), plainBytes, &usedPortableSlot)) {
            if (!SecurityManager::instance().decryptDatabaseFile(m_dbPath, candidateHash, plainBytes, &usedPortableSlot)) {
                return false;
            }
        }
    } else {
        if (!SecurityManager::instance().decryptDatabaseFile(m_dbPath, candidateHash, plainBytes, &usedPortableSlot)) {
            return false;
        }
    }

    if (plainBytes.size() < 100 || !plainBytes.startsWith("SQLite format 3")) {
        plainBytes.fill('\0');
        return false;
    }

    QString tempDirStr = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    m_runtimeDbPath = QDir(tempDirStr).filePath(QString(".wd_rt_%1.sys").arg(QUuid::createUuid().toString(QUuid::Id128)));

    {
        QFile rtFile(m_runtimeDbPath);
        if (!rtFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            plainBytes.fill('\0');
            return false;
        }
        if (rtFile.write(plainBytes) != plainBytes.size()) {
            rtFile.close();
            shredAndRemoveFile(m_runtimeDbPath);
            plainBytes.fill('\0');
            return false;
        }
        rtFile.flush();
        rtFile.close();
    }
    plainBytes.fill('\0');
    plainBytes.clear();

#ifdef _WIN32
    SetFileAttributesW(reinterpret_cast<LPCWSTR>(m_runtimeDbPath.utf16()), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
#endif

    if (QSqlDatabase::contains(m_connectionName)) {
        m_db = QSqlDatabase::database(m_connectionName);
        if (m_db.isOpen()) m_db.close();
    } else {
        m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    }

    m_db.setDatabaseName(m_runtimeDbPath);
    if (!m_db.open()) {
        qWarning() << "[DB] Failed to open unlocked runtime database:" << m_db.lastError().text();
        shredAndRemoveFile(m_runtimeDbPath);
        m_runtimeDbPath.clear();
        return false;
    }

    {
        QSqlQuery pragmaQuery(m_db);
        pragmaQuery.exec("PRAGMA journal_mode = MEMORY;");
        pragmaQuery.exec("PRAGMA synchronous = NORMAL;");
        pragmaQuery.exec("PRAGMA foreign_keys = ON;");
    }

    migrateSchemaIfNeeded();

    m_activePasswordHash = candidateHash;
    m_isUnlocked = true;

    if (isFirstTimeSetup) {
        setSecurityConfig("master_password_hash", candidateHash);
    }

    // 每次成功输入密码打开软件时，立即按当前电脑硬件指纹 + 新随机浮动密钥重写加密 wood.db
    syncEncryptedDatabaseToDisk();

    if (!m_autoSyncTimer) {
        m_autoSyncTimer = new QTimer(qApp);
        m_autoSyncTimer->setInterval(1500);
        QObject::connect(m_autoSyncTimer, &QTimer::timeout, []() {
            auto& dbMgr = DatabaseManager::instance();
            if (!dbMgr.isUnlocked()) return;
            QSqlQuery cq("SELECT total_changes();", dbMgr.database());
            if (cq.next()) {
                int currentChanges = cq.value(0).toInt();
                if (currentChanges != dbMgr.m_lastSyncedTotalChanges) {
                    dbMgr.syncEncryptedDatabaseToDisk();
                }
            }
        });
    }
    m_autoSyncTimer->start();

    return true;
}

bool DatabaseManager::updateEncryptionPassword(const QString& newPassword) {
    if (!isUnlocked()) return false;
    QString newHash = SecurityManager::instance().hashPassword(newPassword);
    m_activePasswordHash = newHash;
    if (!setSecurityConfig("master_password_hash", newHash)) {
        return false;
    }
    return syncEncryptedDatabaseToDisk();
}

bool DatabaseManager::syncEncryptedDatabaseToDisk() {
    if (!m_isUnlocked || !m_db.isOpen() || m_runtimeDbPath.isEmpty()) {
        return false;
    }

    {
        QSqlQuery cq("SELECT total_changes();", m_db);
        if (cq.next()) {
            m_lastSyncedTotalChanges = cq.value(0).toInt();
        }
    }

    QFile rtFile(m_runtimeDbPath);
    if (!rtFile.open(QIODevice::ReadOnly)) {
        return false;
    }
    QByteArray plainBytes = rtFile.readAll();
    rtFile.close();

    if (plainBytes.isEmpty()) {
        return false;
    }

    bool ok = SecurityManager::instance().encryptDatabaseFile(m_dbPath, plainBytes, m_activePasswordHash);
    plainBytes.fill('\0');
    plainBytes.clear();
    return ok;
}

void DatabaseManager::migrateSchemaIfNeeded() {
    QSqlQuery q(m_db);

    auto ensureColumn = [this, &q](const QString& table, const QString& col, const QString& typeDef) {
        QSqlQuery info(QString("PRAGMA table_info(%1);").arg(table), m_db);
        bool found = false;
        while (info.next()) {
            if (info.value("name").toString().compare(col, Qt::CaseInsensitive) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            q.exec(QString("ALTER TABLE %1 ADD COLUMN %2 %3;").arg(table, col, typeDef));
        }
    };

    ensureColumn("user_profile", "avatar_path", "TEXT DEFAULT ''");
    ensureColumn("user_profile", "phone", "TEXT DEFAULT '13800138000'");
    ensureColumn("user_profile", "detail_address", "TEXT DEFAULT ''");
    ensureColumn("user_profile", "ai_rules", "TEXT DEFAULT ''");
    ensureColumn("contacts", "avatar_path", "TEXT DEFAULT ''");
    ensureColumn("contacts", "detail_address", "TEXT DEFAULT ''");
    ensureColumn("contacts", "introducer_id", "INTEGER DEFAULT 0");
    ensureColumn("contacts", "ai_review", "TEXT DEFAULT ''");
    ensureColumn("contacts", "influence_score", "INTEGER DEFAULT 80");
    ensureColumn("contacts", "credit_score", "INTEGER DEFAULT 90");
    ensureColumn("contacts", "taboos", "TEXT DEFAULT ''");
    ensureColumn("social_plans", "scope", "TEXT DEFAULT 'PERSONAL'");
    ensureColumn("social_plans", "company_id", "INTEGER DEFAULT NULL");
    ensureColumn("social_plans", "budget", "REAL DEFAULT 0.0");
    ensureColumn("social_events", "io_type", "TEXT DEFAULT 'EXPENSE'");
    ensureColumn("transactions", "project_name", "TEXT DEFAULT ''");
    ensureColumn("transactions", "voucher_path", "TEXT DEFAULT ''");
    ensureColumn("companies", "legal_rep_contact_id", "INTEGER DEFAULT 0");
    ensureColumn("companies", "legal_rep_name", "TEXT DEFAULT ''");
    ensureColumn("companies", "is_my_company", "INTEGER DEFAULT 0");

    // 若旧表带有严格枚举 CHECK 约束阻碍自定义分类写入，自动无损升级表结构
    auto removeRestrictiveCheckIfNeeded = [this, &q](const QString& tableName, const QString& checkPattern, const QString& createNewSql, const QString& copyCols) {
        QSqlQuery sq(QString("SELECT sql FROM sqlite_master WHERE type='table' AND name='%1';").arg(tableName), m_db);
        if (sq.next()) {
            QString ddl = sq.value(0).toString();
            if (ddl.contains(checkPattern, Qt::CaseInsensitive)) {
                q.exec("PRAGMA foreign_keys = OFF;");
                m_db.transaction();
                q.exec(QString("ALTER TABLE %1 RENAME TO %1_old_tmp;").arg(tableName));
                q.exec(createNewSql);
                q.exec(QString("INSERT INTO %1 (%2) SELECT %2 FROM %1_old_tmp;").arg(tableName, copyCols));
                q.exec(QString("DROP TABLE %1_old_tmp;").arg(tableName));
                m_db.commit();
                q.exec("PRAGMA foreign_keys = ON;");
            }
        }
    };

    removeRestrictiveCheckIfNeeded(
        "social_plans",
        "plan_type IN (",
        "CREATE TABLE \"social_plans\" ("
        "  \"id\" INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  \"contact_id\" INTEGER,"
        "  \"scope\" TEXT NOT NULL DEFAULT 'PERSONAL',"
        "  \"company_id\" INTEGER,"
        "  \"plan_type\" TEXT NOT NULL,"
        "  \"title\" TEXT NOT NULL,"
        "  \"detail\" TEXT,"
        "  \"planned_date\" DATETIME NOT NULL,"
        "  \"remind_days_before\" INTEGER DEFAULT 3,"
        "  \"budget\" REAL DEFAULT 0.0,"
        "  \"status\" TEXT DEFAULT 'PENDING',"
        "  \"created_at\" DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  FOREIGN KEY (\"contact_id\") REFERENCES \"contacts\" (\"id\") ON DELETE CASCADE ON UPDATE NO ACTION"
        ");",
        "id, contact_id, scope, company_id, plan_type, title, detail, planned_date, remind_days_before, budget, status, created_at"
    );

    removeRestrictiveCheckIfNeeded(
        "social_events",
        "event_category IN (",
        "CREATE TABLE \"social_events\" ("
        "  \"id\" INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  \"contact_id\" INTEGER NOT NULL,"
        "  \"scope\" TEXT NOT NULL DEFAULT 'PRIVATE',"
        "  \"company_id\" INTEGER,"
        "  \"io_type\" TEXT NOT NULL DEFAULT 'EXPENSE',"
        "  \"event_category\" TEXT NOT NULL,"
        "  \"title\" TEXT NOT NULL,"
        "  \"detail\" TEXT,"
        "  \"event_date\" DATE NOT NULL,"
        "  \"my_cost\" REAL DEFAULT 0.0,"
        "  \"their_value\" REAL DEFAULT 0.0,"
        "  \"status\" TEXT DEFAULT 'CLOSED',"
        "  \"created_at\" DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  FOREIGN KEY (\"contact_id\") REFERENCES \"contacts\" (\"id\") ON DELETE CASCADE ON UPDATE NO ACTION,"
        "  FOREIGN KEY (\"company_id\") REFERENCES \"companies\" (\"id\") ON DELETE SET NULL ON UPDATE NO ACTION"
        ");",
        "id, contact_id, scope, company_id, io_type, event_category, title, detail, event_date, my_cost, their_value, status, created_at"
    );

    removeRestrictiveCheckIfNeeded(
        "assets",
        "asset_type IN (",
        "CREATE TABLE \"assets\" ("
        "  \"id\" INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  \"owner_type\" TEXT NOT NULL DEFAULT 'PERSONAL',"
        "  \"company_id\" INTEGER,"
        "  \"asset_type\" TEXT NOT NULL,"
        "  \"name\" TEXT NOT NULL,"
        "  \"purchase_date\" DATE,"
        "  \"purchase_price\" REAL DEFAULT 0.0,"
        "  \"current_valuation\" REAL DEFAULT 0.0,"
        "  \"related_contact_id\" INTEGER,"
        "  \"location_or_code\" TEXT,"
        "  \"description\" TEXT,"
        "  \"status\" TEXT DEFAULT 'HOLDING',"
        "  \"created_at\" DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  FOREIGN KEY (\"company_id\") REFERENCES \"companies\" (\"id\") ON DELETE SET NULL ON UPDATE NO ACTION,"
        "  FOREIGN KEY (\"related_contact_id\") REFERENCES \"contacts\" (\"id\") ON DELETE SET NULL ON UPDATE NO ACTION"
        ");",
        "id, owner_type, company_id, asset_type, name, purchase_date, purchase_price, current_valuation, related_contact_id, location_or_code, description, status, created_at"
    );

    removeRestrictiveCheckIfNeeded(
        "companies",
        "role_type IN (",
        "CREATE TABLE \"companies\" ("
        "  \"id\" INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  \"name\" TEXT NOT NULL,"
        "  \"role_type\" TEXT NOT NULL DEFAULT 'EXTERNAL',"
        "  \"industry\" TEXT,"
        "  \"tax_no\" TEXT,"
        "  \"registered_capital\" REAL DEFAULT 0.0,"
        "  \"share_ratio\" REAL DEFAULT 0.0,"
        "  \"status\" TEXT DEFAULT 'ACTIVE',"
        "  \"notes\" TEXT,"
        "  \"legal_rep_contact_id\" INTEGER DEFAULT 0,"
        "  \"legal_rep_name\" TEXT DEFAULT '',"
        "  \"is_my_company\" INTEGER DEFAULT 0,"
        "  \"created_at\" DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  UNIQUE (\"name\" ASC)"
        ");",
        "id, name, role_type, industry, tax_no, registered_capital, share_ratio, status, notes, legal_rep_contact_id, legal_rep_name, is_my_company, created_at"
    );

    // 初始化“我的公司”主体归属：本人名片设置的企业、以及系统初始预置的 1~6 号企业
    UserProfile myProf = getUserProfile();
    if (!myProf.company.trimmed().isEmpty()) {
        QSqlQuery qMine(m_db);
        qMine.prepare("UPDATE companies SET is_my_company = 1 WHERE name = :cname;");
        qMine.bindValue(":cname", myProf.company.trimmed());
        qMine.exec();
    }
    q.exec("UPDATE companies SET is_my_company = 1 WHERE id <= 6 AND role_type IN ('OWNER', 'PARTNER', 'EMPLOYEE');");
    q.exec("UPDATE companies SET is_my_company = 1 WHERE legal_rep_contact_id = -1 OR legal_rep_name = '本人';");

    // 确保所有联动分类在 transaction_categories 表中完整存在
    struct CatSeed { const char* book; const char* io; const char* parent; const char* sub; };
    static const CatSeed kRequiredCats[] = {
        // 支出 -> 个人 -> 人脉社交
        {"PERSONAL", "EXPENSE", "人脉社交", "聚餐"},
        {"PERSONAL", "EXPENSE", "人脉社交", "娱乐"},
        {"PERSONAL", "EXPENSE", "人脉社交", "礼物"},
        {"PERSONAL", "EXPENSE", "人脉社交", "通勤"},
        {"PERSONAL", "EXPENSE", "人脉社交", "应酬"},
        {"PERSONAL", "EXPENSE", "人脉社交", "帮扶"},
        // 支出 -> 工作 -> 商务社交
        {"COMPANY", "EXPENSE", "商务社交", "餐饮"},
        {"COMPANY", "EXPENSE", "商务社交", "娱乐"},
        {"COMPANY", "EXPENSE", "商务社交", "礼品"},
        {"COMPANY", "EXPENSE", "商务社交", "差旅"},
        {"COMPANY", "EXPENSE", "商务社交", "形象"},
        {"COMPANY", "EXPENSE", "商务社交", "杂费"},
        // 支出 -> 个人 -> 资产购置
        {"PERSONAL", "EXPENSE", "资产购置", "房产"},
        {"PERSONAL", "EXPENSE", "资产购置", "车辆"},
        {"PERSONAL", "EXPENSE", "资产购置", "收藏"},
        {"PERSONAL", "EXPENSE", "资产购置", "设备"},
        // 支出 -> 工作 -> 投资理财
        {"COMPANY", "EXPENSE", "投资理财", "收藏"},
        {"COMPANY", "EXPENSE", "投资理财", "产权"},
        {"COMPANY", "EXPENSE", "投资理财", "股票"},
        {"COMPANY", "EXPENSE", "投资理财", "基金"},
        // 个人 资产变现 (收入)
        {"PERSONAL", "INCOME", "资产变现", "设备"},
        {"PERSONAL", "INCOME", "资产变现", "收藏"},
        {"PERSONAL", "INCOME", "资产变现", "房产"},
        {"PERSONAL", "INCOME", "资产变现", "车辆"},
        // 工作 投资理财 (收入)
        {"COMPANY", "INCOME", "投资理财", "分红"},
        {"COMPANY", "INCOME", "投资理财", "债券"},
        {"COMPANY", "INCOME", "投资理财", "售股"},
        {"COMPANY", "INCOME", "投资理财", "利息"},
        {"COMPANY", "INCOME", "投资理财", "贵金属"},
        {"COMPANY", "INCOME", "投资理财", "收回"},
        // 个人与工作 押抵借贷 & 业务经营 核心借贷/应收/还款分类 (严格执行9条归类规则)
        // 规则1: 支出 -> 个人 -> 押抵借贷 (还网贷、还银行、还个人) 全部是还款
        // 规则2: 支出 -> 个人 -> 押抵借贷 (借个人) 是应收
        {"PERSONAL", "EXPENSE", "押抵借贷", "借个人"},
        {"PERSONAL", "EXPENSE", "押抵借贷", "还网贷"},
        {"PERSONAL", "EXPENSE", "押抵借贷", "还银行"},
        {"PERSONAL", "EXPENSE", "押抵借贷", "还个人"},
        // 规则3: 支出 -> 工作 -> 押抵借贷 (还贷、还息) 全部是还款
        {"COMPANY", "EXPENSE", "押抵借贷", "还贷"},
        {"COMPANY", "EXPENSE", "押抵借贷", "还息"},
        // 规则4: 支出 -> 工作 -> 押抵借贷 (借出) 是应收
        {"COMPANY", "EXPENSE", "押抵借贷", "借出"},
        // 规则5: 支出 -> 工作 -> 业务经营 (进度合同、账期合同) 全部是应收
        {"COMPANY", "EXPENSE", "业务经营", "进度合同"},
        {"COMPANY", "EXPENSE", "业务经营", "账期合同"},
        // 规则6: 收入 -> 个人 -> 押抵借贷 (借入) 是负债
        {"PERSONAL", "INCOME", "押抵借贷", "借入"},
        // 规则7: 收入 -> 个人 -> 押抵借贷 (认领) 是负债
        {"PERSONAL", "INCOME", "押抵借贷", "认领"},
        // 规则8: 收入 -> 个人 -> 押抵借贷 (收回) 是还款
        {"PERSONAL", "INCOME", "押抵借贷", "收回"},
        // 规则9: 收入 -> 工作 -> 投资理财 (收回) 是还款 (已在上方投资理财中包含)
        // 收入 -> 个人 -> 职业收入 (仅保留 顾问、返点)
        {"PERSONAL", "INCOME", "职业收入", "顾问"},
        {"PERSONAL", "INCOME", "职业收入", "返点"}
    };

    // 1. 删除 记账 -> 支出 -> 工作 -> (人脉社交/商务社交 -> 业务)
    q.exec("DELETE FROM transaction_categories "
           "WHERE (book_type = 'COMPANY' OR book_type = 'BOTH') AND io_type = 'EXPENSE' "
           "  AND (parent_name = '人脉社交' OR parent_name = '商务社交') AND sub_name = '业务';");

    // 2. 删除 收入 -> 个人 -> 职业收入 里的（主营业务、兼职副业、项目分红）
    q.exec("DELETE FROM transaction_categories "
           "WHERE (book_type = 'PERSONAL' OR book_type = 'BOTH') AND io_type = 'INCOME' "
           "  AND parent_name = '职业收入' AND sub_name IN ('主营业务', '兼职副业', '项目分红');");

    // 3. 收入 -> 个人 -> 职业收入 里的（咨询顾问 改 顾问 ，佣金返点 改 返点）
    q.exec("UPDATE transaction_categories SET sub_name = '顾问' "
           "WHERE (book_type = 'PERSONAL' OR book_type = 'BOTH') AND io_type = 'INCOME' AND parent_name = '职业收入' AND sub_name = '咨询顾问';");
    q.exec("UPDATE transaction_categories SET sub_name = '返点' "
           "WHERE (book_type = 'PERSONAL' OR book_type = 'BOTH') AND io_type = 'INCOME' AND parent_name = '职业收入' AND sub_name = '佣金返点';");
    q.exec("UPDATE transactions SET category_sub = '顾问' "
           "WHERE book_type = 'PERSONAL' AND io_type = 'INCOME' AND category_parent = '职业收入' AND category_sub = '咨询顾问';");
    q.exec("UPDATE transactions SET category_sub = '返点' "
           "WHERE book_type = 'PERSONAL' AND io_type = 'INCOME' AND category_parent = '职业收入' AND category_sub = '佣金返点';");

    // 4. 收入 -> 个人 -> 抵押借贷 认领 = 删除；并将历史抵押借贷统一规范为押抵借贷
    q.exec("DELETE FROM transaction_categories "
           "WHERE (book_type = 'PERSONAL' OR book_type = 'BOTH') AND io_type = 'INCOME' AND parent_name = '抵押借贷' AND sub_name = '认领';");
    q.exec("UPDATE transaction_categories SET parent_name = '押抵借贷' WHERE parent_name = '抵押借贷';");
    q.exec("UPDATE transactions SET category_parent = '押抵借贷' WHERE category_parent = '抵押借贷';");

    // 5. 收入 -> 个人 -> 押抵借贷 认领 = 添加；并将历史错误单据修正（工作->人脉社交/商务社交->业务 修改为 个人->收入->押抵借贷->认领，计算到应还）
    q.exec("UPDATE transactions "
           "SET book_type = 'PERSONAL', io_type = 'INCOME', category_parent = '押抵借贷', category_sub = '认领' "
           "WHERE book_type = 'COMPANY' AND (category_parent = '人脉社交' OR category_parent = '商务社交') AND category_sub = '业务';");

    // 6. 收入 -> 工作 -> 职业收入（主营业务收入、项目结算、技术服务费、战略咨询费、渠道佣金）全部删除
    q.exec("DELETE FROM transaction_categories "
           "WHERE (book_type = 'COMPANY' OR book_type = 'BOTH') AND io_type = 'INCOME' AND parent_name = '职业收入';");

    // 规范工作收入的业务经营父类名
    q.exec("UPDATE transaction_categories SET parent_name = '业务经营' "
           "WHERE (book_type = 'COMPANY' OR book_type = 'BOTH') AND io_type = 'INCOME' AND parent_name = '业务';");

    // 7. 规范化个人支出 押抵借贷 历史数据：将 还网贷、还银行、借个人、还个人 统一归入 押抵借贷 大分类
    q.exec("UPDATE transactions SET category_parent = '押抵借贷', category_sub = '还网贷' "
           "WHERE book_type = 'PERSONAL' AND io_type = 'EXPENSE' "
           "  AND (category_parent = '还网贷' OR (category_parent IN ('押抵借贷', '抵押借贷') AND category_sub IN ('花呗', '美团', '微信', '抖音')));");
    q.exec("UPDATE transactions SET category_parent = '押抵借贷', category_sub = '借个人' "
           "WHERE book_type = 'PERSONAL' AND io_type = 'EXPENSE' AND category_parent = '借个人';");
    q.exec("UPDATE transactions SET category_parent = '押抵借贷', category_sub = '还银行' "
           "WHERE book_type = 'PERSONAL' AND io_type = 'EXPENSE' AND category_parent = '还银行';");
    q.exec("UPDATE transactions SET category_parent = '押抵借贷', category_sub = '还个人' "
           "WHERE book_type = 'PERSONAL' AND io_type = 'EXPENSE' AND category_parent = '还个人';");
    q.exec("DELETE FROM transaction_categories WHERE parent_name IN ('还网贷', '还银行', '借个人', '还个人');");

    // 8. 规范化工作支出 押抵借贷 历史数据：将 还贷、还息 统一归入 押抵借贷
    q.exec("UPDATE transactions SET category_parent = '押抵借贷', category_sub = '还贷' "
           "WHERE book_type = 'COMPANY' AND io_type = 'EXPENSE' AND category_parent = '还贷';");
    q.exec("UPDATE transactions SET category_parent = '押抵借贷', category_sub = '还息' "
           "WHERE book_type = 'COMPANY' AND io_type = 'EXPENSE' AND category_parent = '还息';");

    // 9. 清理 transaction_categories 中历史误加的英文代号或被删除分类 (如 GIFT, HELP_RECEIVED, 业务等)
    q.exec("DELETE FROM transaction_categories "
           "WHERE sub_name IN ('GIFT', 'GIFT_OUT', 'GIFT_IN', 'HELP_RECEIVED', 'HELP_GIVEN', 'HELP', 'DINNER', 'TREAT', 'ENTERTAIN', 'MEETING', 'BUSINESS', '业务', 'FINANCIAL') "
           "   OR sub_name LIKE '%HELP%' OR sub_name LIKE '%GIFT%';");

    // 10. 规范化 social_events 礼尚往来表中的历史旧分类与英文枚举代码为标准中文分类
    q.exec("UPDATE social_events SET event_category = '礼物' WHERE (scope != 'COMPANY') AND (event_category IN ('GIFT', 'GIFT_OUT', 'GIFT_IN', '送礼随份子', '送礼往来', '受赠回礼', '礼品馈赠') OR event_category LIKE '%GIFT%');");
    q.exec("UPDATE social_events SET event_category = '礼品' WHERE (scope = 'COMPANY') AND (event_category IN ('GIFT', 'GIFT_OUT', 'GIFT_IN', '送礼随份子', '送礼往来', '受赠回礼', '礼品馈赠') OR event_category LIKE '%GIFT%');");
    q.exec("UPDATE social_events SET event_category = '聚餐' WHERE (scope != 'COMPANY') AND event_category IN ('DINNER', 'TREAT', '请客吃饭', '餐饮宴请', '品茗茶叙');");
    q.exec("UPDATE social_events SET event_category = '餐饮' WHERE (scope = 'COMPANY') AND event_category IN ('DINNER', 'TREAT', '请客吃饭', '餐饮宴请', '品茗茶叙');");
    q.exec("UPDATE social_events SET event_category = '娱乐' WHERE event_category IN ('ENTERTAIN', '休闲娱乐应酬', '娱乐休闲', '高端圈层维护');");
    q.exec("UPDATE social_events SET event_category = '帮扶' WHERE event_category IN ('HELP', 'HELP_GIVEN', 'HELP_RECEIVED', '互相帮扶', '人情帮扶', '人情探望') OR event_category LIKE '%HELP%';");
    q.exec("UPDATE social_events SET event_category = '差旅' WHERE (scope = 'COMPANY') AND event_category IN ('MEETING', '差旅接待');");
    q.exec("UPDATE social_events SET event_category = '通勤' WHERE (scope != 'COMPANY') AND event_category IN ('MEETING', '差旅接待');");
    q.exec("UPDATE social_events SET event_category = '应酬' WHERE (scope != 'COMPANY') AND event_category IN ('BUSINESS', '业务', '生意合作', '客户招待应酬', '形象包装');");
    q.exec("UPDATE social_events SET event_category = '形象' WHERE (scope = 'COMPANY') AND event_category IN ('客户招待应酬', '形象包装');");
    q.exec("UPDATE social_events SET event_category = '餐饮' WHERE (scope = 'COMPANY') AND event_category IN ('BUSINESS', '业务', '生意合作');");

    for (const auto& c : kRequiredCats) {
        addCustomCategory(c.io, c.parent, c.sub, c.book);
    }

    q.exec("CREATE INDEX IF NOT EXISTS idx_regions_level_parent ON regions (level, parent_code);");
    q.exec("CREATE INDEX IF NOT EXISTS idx_regions_code ON regions (code);");

    // 确保个人名片中登记的公司名称已同步注册在 companies 公司主体表中
    QSqlQuery upQ("SELECT company, industry FROM user_profile WHERE id = 1;", m_db);
    if (upQ.next()) {
        QString upComp = upQ.value("company").toString().trimmed();
        QString upInd = upQ.value("industry").toString().trimmed();
        if (!upComp.isEmpty()) {
            QSqlQuery checkComp(m_db);
            checkComp.prepare("SELECT id FROM companies WHERE name = :n;");
            checkComp.bindValue(":n", upComp);
            if (checkComp.exec() && !checkComp.next()) {
                QSqlQuery insComp(m_db);
                insComp.prepare("INSERT INTO companies (name, role_type, industry, share_ratio, registered_capital, notes) "
                                "VALUES (:n, 'OWNER', :ind, 100.0, 1000000.0, '个人名片所属主体');");
                insComp.bindValue(":n", upComp);
                insComp.bindValue(":ind", upInd);
                insComp.exec();
            }
        }
    }
}

void DatabaseManager::seedInitialSampleDataIfNeeded() {
}

void DatabaseManager::close() {
    if (m_autoSyncTimer) {
        m_autoSyncTimer->stop();
    }
    if (m_db.isOpen()) {
        if (m_isUnlocked && !m_runtimeDbPath.isEmpty()) {
            syncEncryptedDatabaseToDisk();
        }
        m_db.close();
    }
    if (!m_runtimeDbPath.isEmpty()) {
        shredAndRemoveFile(m_runtimeDbPath);
        m_runtimeDbPath.clear();
    }
    m_isUnlocked = false;
    m_activePasswordHash.clear();
}

bool DatabaseManager::executeSqlScript(const QString& scriptPath) {
    QFile file(scriptPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream in(&file);
    QString cleanSql;
    bool inBlockComment = false;
    bool inCreateTable = false;
    while (!in.atEnd()) {
        QString line = in.readLine();
        QString trimmed = line.trimmed();
        if (inBlockComment) {
            if (trimmed.contains("*/")) {
                inBlockComment = false;
            }
            continue;
        }
        if (trimmed.startsWith("/*")) {
            if (!trimmed.contains("*/")) {
                inBlockComment = true;
            }
            continue;
        }
        if (trimmed.startsWith("--") || trimmed.isEmpty()) {
            continue;
        }
        if (trimmed.startsWith("CREATE TABLE", Qt::CaseInsensitive)) {
            inCreateTable = true;
        }
        // 兼容 Navicat 导出 SQLite 时无名 CHECK 约束省略 CHECK 关键字的语法 (仅在内存中修复，绝不改动 sqlite3.sql 原文件)
        if (inCreateTable && trimmed.startsWith("(") &&
            (trimmed.contains(" IN (", Qt::CaseInsensitive) || trimmed.contains(" BETWEEN ", Qt::CaseInsensitive))) {
            line = "  CHECK " + trimmed;
        }
        if (inCreateTable && trimmed.endsWith(");")) {
            inCreateTable = false;
        }
        cleanSql += line + "\n";
    }
    file.close();

    m_db.transaction();
    const QStringList statements = cleanSql.split(";\n", Qt::SkipEmptyParts);
    for (const QString& rawStatement : statements) {
        QString stmt = rawStatement.trimmed();
        if (stmt.isEmpty()) continue;
        if (stmt.compare("PRAGMA foreign_keys = false", Qt::CaseInsensitive) == 0 ||
            stmt.compare("PRAGMA foreign_keys = true", Qt::CaseInsensitive) == 0) {
            continue;
        }
        QSqlQuery query(m_db);
        if (!query.exec(stmt)) {
            if (!query.lastError().text().contains("already exists", Qt::CaseInsensitive)) {
                qWarning() << "[DB] SQL note:" << query.lastError().text() << "in stmt:" << stmt.left(120);
            }
        }
    }
    m_db.commit();
    return true;
}

QList<RegionItem> DatabaseManager::getRegionsByParent(const QString& parentCode) const {
    if (!m_regionsCached && m_db.isOpen()) {
        m_regionChildrenCache.clear();
        QSqlQuery q("SELECT code, parent_code, name, level FROM regions WHERE is_deleted = 0 ORDER BY sort_order ASC, code ASC;", m_db);
        while (q.next()) {
            RegionItem item;
            item.code = q.value(0).toString();
            QString pCode = q.value(1).toString().trimmed();
            item.name = q.value(2).toString();
            item.level = q.value(3).toInt();
            if (item.level == 1) {
                m_regionChildrenCache[""].append(item);
            }
            if (!pCode.isEmpty()) {
                m_regionChildrenCache[pCode].append(item);
            }
        }
        m_regionsCached = true;
    }
    return m_regionChildrenCache.value(parentCode.trimmed());
}

UserProfile DatabaseManager::getUserProfile() {
    UserProfile profile;
    QSqlQuery q("SELECT name, phone, birth_date, province, city, district, detail_address, industry, title, company, ai_rules FROM user_profile WHERE id = 1;", m_db);
    if (q.next()) {
        profile.name = q.value("name").toString();
        QString ph = q.value("phone").toString().trimmed();
        if (!ph.isEmpty()) profile.phone = ph;
        profile.birthDate = q.value("birth_date").toString().trimmed();
        profile.province = q.value("province").toString().trimmed();
        profile.city = q.value("city").toString().trimmed();
        profile.district = q.value("district").toString().trimmed();
        profile.detailAddress = q.value("detail_address").toString().trimmed();
        profile.address = profile.province + profile.city + profile.district + profile.detailAddress;
        if (profile.address.isEmpty()) profile.address = "浙江省台州市路桥区";
        profile.industry = q.value("industry").toString().trimmed();
        profile.title = q.value("title").toString().trimmed();
        profile.company = q.value("company").toString().trimmed();
        profile.aiRules = q.value("ai_rules").toString();
    }
    profile.companyCount = getCompaniesCount();
    profile.contactCount = getContactsCount();
    return profile;
}

bool DatabaseManager::saveUserProfile(const UserProfile& profile) {
    QSqlQuery q(m_db);
    q.prepare("UPDATE user_profile SET name = :name, phone = :phone, birth_date = :bdate, "
              "province = :prov, city = :city, district = :dist, detail_address = :daddr, "
              "industry = :ind, company = :comp, title = :title, ai_rules = :rules, "
              "updated_at = CURRENT_TIMESTAMP WHERE id = 1;");
    q.bindValue(":name", profile.name);
    q.bindValue(":phone", profile.phone);
    q.bindValue(":bdate", profile.birthDate);
    q.bindValue(":prov", profile.province);
    q.bindValue(":city", profile.city);
    q.bindValue(":dist", profile.district);
    q.bindValue(":daddr", profile.detailAddress);
    q.bindValue(":ind", profile.industry);
    q.bindValue(":comp", profile.company);
    q.bindValue(":title", profile.title);
    q.bindValue(":rules", profile.aiRules);
    if (q.exec()) {
        if (!profile.industry.trimmed().isEmpty()) {
            addDictionaryValue("INDUSTRY", profile.industry.trimmed());
        }
        if (!profile.company.trimmed().isEmpty()) {
            addDictionaryValue("UNIT", profile.company.trimmed());
            QSqlQuery checkComp(m_db);
            checkComp.prepare("SELECT id FROM companies WHERE name = :n;");
            checkComp.bindValue(":n", profile.company.trimmed());
            if (checkComp.exec() && !checkComp.next()) {
                QSqlQuery insComp(m_db);
                insComp.prepare("INSERT INTO companies (name, role_type, industry, share_ratio, registered_capital, notes) "
                                "VALUES (:n, 'OWNER', :ind, 100.0, 1000000.0, '个人名片所属主体');");
                insComp.bindValue(":n", profile.company.trimmed());
                insComp.bindValue(":ind", profile.industry.trimmed());
                insComp.exec();
            }
        }
        if (!profile.title.trimmed().isEmpty()) {
            addDictionaryValue("TITLE", profile.title.trimmed());
        }
        return true;
    }
    return false;
}

int DatabaseManager::getCompaniesCount() {
    QSqlQuery q("SELECT COUNT(*) FROM companies WHERE status = 'ACTIVE' OR status IS NULL;", m_db);
    if (q.next()) return q.value(0).toInt();
    return 0;
}

QList<QVariantMap> DatabaseManager::getCompaniesList() {
    QList<QVariantMap> list;
    QSqlQuery q("SELECT id, name, tax_no, registered_capital, role_type, share_ratio, industry, notes, legal_rep_contact_id, legal_rep_name, is_my_company FROM companies ORDER BY id ASC;", m_db);
    while (q.next()) {
        QVariantMap row;
        row["id"] = q.value("id");
        row["name"] = q.value("name");
        row["tax_no"] = q.value("tax_no");
        row["registered_capital"] = q.value("registered_capital");
        row["role_type"] = q.value("role_type");
        row["share_ratio"] = q.value("share_ratio");
        row["industry"] = q.value("industry");
        row["notes"] = q.value("notes");
        row["remark"] = q.value("notes");
        row["legal_rep_contact_id"] = q.value("legal_rep_contact_id");
        row["legal_rep_name"] = q.value("legal_rep_name");
        row["is_my_company"] = q.value("is_my_company");
        list.append(row);
    }
    return list;
}

bool DatabaseManager::addCompany(const QString& name, const QString& roleType, const QString& industry,
                                 const QString& remark, const QString& taxNo,
                                 double registeredCapital, double shareRatio,
                                 int legalRepContactId, const QString& legalRepName) {
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO companies (name, tax_no, registered_capital, role_type, share_ratio, industry, notes, legal_rep_contact_id, legal_rep_name) "
              "VALUES (:name, :tax, :cap, :role, :share, :ind, :rem, :lrid, :lrname);");
    q.bindValue(":name", name);
    q.bindValue(":tax", taxNo.trimmed());
    q.bindValue(":cap", registeredCapital);
    q.bindValue(":role", roleType.isEmpty() ? "OWNER" : roleType);
    q.bindValue(":share", shareRatio);
    q.bindValue(":ind", industry);
    q.bindValue(":rem", remark);
    q.bindValue(":lrid", legalRepContactId);
    q.bindValue(":lrname", legalRepName.trimmed());
    if (q.exec()) {
        if (!industry.trimmed().isEmpty()) {
            addDictionaryValue("INDUSTRY", industry.trimmed());
        }
        addDictionaryValue("UNIT", name.trimmed());
        return true;
    }
    return false;
}

bool DatabaseManager::updateCompany(int id, const QString& name, const QString& roleType,
                                    const QString& industry, const QString& remark,
                                    const QString& taxNo, double registeredCapital,
                                    double shareRatio, bool clearAssociatedData,
                                    int legalRepContactId, const QString& legalRepName) {
    if (id <= 0) return false;
    m_db.transaction();

    if (clearAssociatedData) {
        QSqlQuery qTrans(m_db);
        qTrans.prepare("DELETE FROM transactions WHERE company_id = :cid;");
        qTrans.bindValue(":cid", id);
        qTrans.exec();

        QSqlQuery qAsset(m_db);
        qAsset.prepare("DELETE FROM assets WHERE company_id = :cid;");
        qAsset.bindValue(":cid", id);
        qAsset.exec();

        QSqlQuery qPlan(m_db);
        qPlan.prepare("DELETE FROM social_plans WHERE company_id = :cid;");
        qPlan.bindValue(":cid", id);
        qPlan.exec();

        QSqlQuery qEvent(m_db);
        qEvent.prepare("DELETE FROM social_events WHERE company_id = :cid;");
        qEvent.bindValue(":cid", id);
        qEvent.exec();
    }

    QSqlQuery q(m_db);
    q.prepare("UPDATE companies SET name = :name, tax_no = :tax, registered_capital = :cap, "
              "role_type = :role, share_ratio = :share, industry = :ind, notes = :rem, "
              "legal_rep_contact_id = :lrid, legal_rep_name = :lrname WHERE id = :id;");
    q.bindValue(":name", name);
    q.bindValue(":tax", taxNo.trimmed());
    q.bindValue(":cap", registeredCapital);
    q.bindValue(":role", roleType.isEmpty() ? "OWNER" : roleType);
    q.bindValue(":share", shareRatio);
    q.bindValue(":ind", industry);
    q.bindValue(":rem", remark);
    q.bindValue(":lrid", legalRepContactId);
    q.bindValue(":lrname", legalRepName.trimmed());
    q.bindValue(":id", id);

    if (q.exec()) {
        if (!industry.trimmed().isEmpty()) {
            addDictionaryValue("INDUSTRY", industry.trimmed());
        }
        addDictionaryValue("UNIT", name.trimmed());
        m_db.commit();
        syncEncryptedDatabaseToDisk();
        return true;
    }

    m_db.rollback();
    return false;
}

bool DatabaseManager::saveOrUpdateCompany(int& companyId,
                                         const QString& originalName,
                                         const QString& newName,
                                         const QString& roleType,
                                         const QString& industry,
                                         const QString& remark,
                                         const QString& taxNo,
                                         double registeredCapital,
                                         double shareRatio,
                                         int legalRepContactId,
                                         const QString& legalRepName,
                                         int isMyCompany) {
    if (newName.trimmed().isEmpty()) return false;
    QString cleanNewName = newName.trimmed();
    QString cleanOrigName = originalName.trimmed();

    UserProfile myProfile = getUserProfile();
    QString myCorp = myProfile.company.trimmed();

    // 严谨计算公司归属：我的企业 vs 外部企业
    int finalIsMine = 0;
    if (isMyCompany >= 0) {
        finalIsMine = (isMyCompany == 1) ? 1 : 0;
    } else {
        bool isMineRole = (roleType != "EXTERNAL" && (roleType == "OWNER" || roleType == "PARTNER" || roleType == "EMPLOYEE"));
        if (!myCorp.isEmpty() && cleanNewName == myCorp) {
            finalIsMine = 1;
        } else if (isMineRole) {
            finalIsMine = 1;
        } else {
            finalIsMine = 0;
        }
    }

    QString finalRole = roleType.trimmed();
    if (finalRole.isEmpty()) {
        finalRole = (finalIsMine == 1) ? "OWNER" : "EXTERNAL";
    }

    m_db.transaction();

    // 1. 若 companyId <= 0，尝试按旧名称或新名称在 companies 表中查找已有记录
    if (companyId <= 0) {
        QSqlQuery qFind(m_db);
        if (!cleanOrigName.isEmpty()) {
            qFind.prepare("SELECT id FROM companies WHERE name = :orig OR TRIM(name) = :orig;");
            qFind.bindValue(":orig", cleanOrigName);
            if (qFind.exec() && qFind.next()) {
                companyId = qFind.value(0).toInt();
            }
        }
        if (companyId <= 0) {
            qFind.prepare("SELECT id FROM companies WHERE name = :newName OR TRIM(name) = :newName;");
            qFind.bindValue(":newName", cleanNewName);
            if (qFind.exec() && qFind.next()) {
                companyId = qFind.value(0).toInt();
            }
        }
    }

    // 2. 更新已存在的公司或插入新记录
    if (companyId > 0) {
        QSqlQuery qUp(m_db);
        qUp.prepare("UPDATE companies SET name = :name, tax_no = :tax, registered_capital = :cap, "
                    "role_type = :role, share_ratio = :share, industry = :ind, notes = :rem, "
                    "legal_rep_contact_id = :lrid, legal_rep_name = :lrname, is_my_company = :ismine "
                    "WHERE id = :id;");
        qUp.bindValue(":name", cleanNewName);
        qUp.bindValue(":tax", taxNo.trimmed());
        qUp.bindValue(":cap", registeredCapital);
        qUp.bindValue(":role", finalRole);
        qUp.bindValue(":share", shareRatio);
        qUp.bindValue(":ind", industry.trimmed());
        qUp.bindValue(":rem", remark.trimmed());
        qUp.bindValue(":lrid", legalRepContactId);
        qUp.bindValue(":lrname", legalRepName.trimmed());
        qUp.bindValue(":ismine", finalIsMine);
        qUp.bindValue(":id", companyId);
        if (!qUp.exec()) {
            // 若因同名冲突导致失败，尝试定位并合并已有同名记录
            QSqlQuery qConflict(m_db);
            qConflict.prepare("SELECT id FROM companies WHERE (name = :name OR TRIM(name) = :name) AND id != :id;");
            qConflict.bindValue(":name", cleanNewName);
            qConflict.bindValue(":id", companyId);
            if (qConflict.exec() && qConflict.next()) {
                int conflictId = qConflict.value(0).toInt();
                qWarning() << "[DB] update company conflict with id:" << conflictId << "merging into existing record.";
                companyId = conflictId;
            } else {
                qWarning() << "[DB] update company failed:" << qUp.lastError().text();
                m_db.rollback();
                return false;
            }
        }
    } else {
        QSqlQuery qIns(m_db);
        qIns.prepare("INSERT INTO companies (name, tax_no, registered_capital, role_type, share_ratio, industry, notes, legal_rep_contact_id, legal_rep_name, is_my_company) "
                     "VALUES (:name, :tax, :cap, :role, :share, :ind, :rem, :lrid, :lrname, :ismine);");
        qIns.bindValue(":name", cleanNewName);
        qIns.bindValue(":tax", taxNo.trimmed());
        qIns.bindValue(":cap", registeredCapital);
        qIns.bindValue(":role", finalRole);
        qIns.bindValue(":share", shareRatio);
        qIns.bindValue(":ind", industry.trimmed());
        qIns.bindValue(":rem", remark.trimmed());
        qIns.bindValue(":lrid", legalRepContactId);
        qIns.bindValue(":lrname", legalRepName.trimmed());
        qIns.bindValue(":ismine", finalIsMine);
        if (!qIns.exec()) {
            qWarning() << "[DB] insert company failed:" << qIns.lastError().text();
            m_db.rollback();
            return false;
        }
        companyId = qIns.lastInsertId().toInt();
    }

    // 2.1 强制消除旧名称重影：若 companies 表中仍存在非当前 ID 但同名的旧主体，予以安全彻底清理，杜绝改名后新旧两个主体并存
    if (!cleanOrigName.isEmpty() && cleanOrigName != cleanNewName) {
        QSqlQuery qDelOld(m_db);
        qDelOld.prepare("DELETE FROM companies WHERE id != :cid AND (name = :orig OR TRIM(name) = :orig);");
        qDelOld.bindValue(":cid", companyId);
        qDelOld.bindValue(":orig", cleanOrigName);
        qDelOld.exec();
    }

    // 3. 核心修改逻辑：将所有原属于该公司的联系人、或单位等于旧名称的联系人，全量无损同步至新公司名与 companyId，绝不脱钩，确保原公司人员完整修改
    if (!cleanOrigName.isEmpty()) {
        QSqlQuery qContacts(m_db);
        qContacts.prepare("UPDATE contacts SET unit = :newName, company_id = :cid "
                          "WHERE company_id = :cid OR TRIM(unit) = :origName OR unit = :origName OR TRIM(unit) = :newName;");
        qContacts.bindValue(":newName", cleanNewName);
        qContacts.bindValue(":cid", companyId);
        qContacts.bindValue(":origName", cleanOrigName);
        qContacts.exec();
    } else {
        QSqlQuery qContacts(m_db);
        qContacts.prepare("UPDATE contacts SET unit = :newName, company_id = :cid "
                          "WHERE company_id = :cid OR TRIM(unit) = :newName OR unit = :newName;");
        qContacts.bindValue(":newName", cleanNewName);
        qContacts.bindValue(":cid", companyId);
        qContacts.exec();
    }

    // 4. 若指定了具体的法人联系人，将其关联到当前公司
    if (legalRepContactId > 0) {
        QSqlQuery qRep(m_db);
        qRep.prepare("UPDATE contacts SET company_id = :cid, unit = :cname "
                     "WHERE id = :rid;");
        qRep.bindValue(":cid", companyId);
        qRep.bindValue(":cname", cleanNewName);
        qRep.bindValue(":rid", legalRepContactId);
        qRep.exec();

        // 若其职务为空或仅为普通员工，自动设为“法定代表人”
        QSqlQuery qRepTitle(m_db);
        qRepTitle.prepare("UPDATE contacts SET title = '法定代表人' "
                          "WHERE id = :rid AND (title IS NULL OR TRIM(title) = '' OR title = '员工');");
        qRepTitle.bindValue(":rid", legalRepContactId);
        qRepTitle.exec();
    }

    // 5. 同步与清理字典表中的公司单位字典
    if (!cleanOrigName.isEmpty() && cleanOrigName != cleanNewName) {
        // 先删除旧名称字典项，确保不留死条目，也避免 UNIQUE 约束报错
        QSqlQuery qDelDict(m_db);
        qDelDict.prepare("DELETE FROM dictionary WHERE category IN ('UNIT', 'COMPANY') AND (value = :origName OR TRIM(value) = :origName);");
        qDelDict.bindValue(":origName", cleanOrigName);
        qDelDict.exec();

        // 同步更新流水中的项目/公司名称
        QSqlQuery qTrans(m_db);
        qTrans.prepare("UPDATE transactions SET project_name = :newName WHERE project_name = :origName OR TRIM(project_name) = :origName;");
        qTrans.bindValue(":newName", cleanNewName);
        qTrans.bindValue(":origName", cleanOrigName);
        qTrans.exec();

        QSqlQuery qCounter(m_db);
        qCounter.prepare("UPDATE transactions SET counterparty = :newName WHERE counterparty = :origName OR TRIM(counterparty) = :origName;");
        qCounter.bindValue(":newName", cleanNewName);
        qCounter.bindValue(":origName", cleanOrigName);
        qCounter.exec();
    }
    addDictionaryValue("UNIT", cleanNewName);
    if (!industry.trimmed().isEmpty()) {
        addDictionaryValue("INDUSTRY", industry.trimmed());
    }

    // 6. 若修改的是个人名片中的“我的公司”，且新主体确实为我的公司，同步名片中的公司名称
    if (finalIsMine == 1) {
        if (!cleanOrigName.isEmpty() && myProfile.company.trimmed() == cleanOrigName) {
            myProfile.company = cleanNewName;
            saveUserProfile(myProfile);
        }
    }

    m_db.commit();
    syncEncryptedDatabaseToDisk();
    return true;
}

int DatabaseManager::ensureCompanyExists(const QString& name, const QString& roleType, int isMyCompany) {
    QString cleanName = name.trimmed();
    if (cleanName.isEmpty()) return 0;

    // 1. 查找是否已存在该名称的主体
    {
        QSqlQuery q(m_db);
        q.prepare("SELECT id FROM companies WHERE name = :name OR TRIM(name) = :name;");
        q.bindValue(":name", cleanName);
        if (q.exec() && q.next()) {
            int cid = q.value(0).toInt();
            // 确保已录入联系人中属于该公司的均关联至此 company_id
            QSqlQuery qUp(m_db);
            qUp.prepare("UPDATE contacts SET company_id = :cid WHERE (TRIM(unit) = :name OR unit = :name) AND (company_id IS NULL OR company_id = 0);");
            qUp.bindValue(":cid", cid);
            qUp.bindValue(":name", cleanName);
            qUp.exec();
            return cid;
        }
    }

    // 2. 若不存在，插入新主体记录
    UserProfile prof = getUserProfile();
    bool isMine = (isMyCompany == 1) || (!prof.company.trimmed().isEmpty() && cleanName == prof.company.trimmed());
    QString finalRole = isMine ? (roleType == "EXTERNAL" ? "OWNER" : roleType) : "EXTERNAL";

    QSqlQuery qIns(m_db);
    qIns.prepare("INSERT INTO companies (name, role_type, is_my_company, status) VALUES (:name, :role, :ismine, 'ACTIVE');");
    qIns.bindValue(":name", cleanName);
    qIns.bindValue(":role", finalRole);
    qIns.bindValue(":ismine", isMine ? 1 : 0);
    if (qIns.exec()) {
        int newId = qIns.lastInsertId().toInt();
        addDictionaryValue("UNIT", cleanName);
        // 同步关联联系人
        QSqlQuery qUp(m_db);
        qUp.prepare("UPDATE contacts SET company_id = :cid WHERE TRIM(unit) = :name OR unit = :name;");
        qUp.bindValue(":cid", newId);
        qUp.bindValue(":name", cleanName);
        qUp.exec();
        syncEncryptedDatabaseToDisk();
        return newId;
    }
    return 0;
}

bool DatabaseManager::deleteCompany(int id) {
    if (id <= 0) return false;

    QString compName;
    {
        QSqlQuery qGetName(m_db);
        qGetName.prepare("SELECT name FROM companies WHERE id = :id;");
        qGetName.bindValue(":id", id);
        if (qGetName.exec() && qGetName.next()) {
            compName = qGetName.value(0).toString().trimmed();
        }
    }

    m_db.transaction();

    // 彻底清除该公司关联的所有流水、资产与走动数据
    QSqlQuery qTrans(m_db);
    qTrans.prepare("DELETE FROM transactions WHERE company_id = :cid;");
    qTrans.bindValue(":cid", id);
    qTrans.exec();

    QSqlQuery qAsset(m_db);
    qAsset.prepare("DELETE FROM assets WHERE company_id = :cid;");
    qAsset.bindValue(":cid", id);
    qAsset.exec();

    QSqlQuery qPlan(m_db);
    qPlan.prepare("DELETE FROM social_plans WHERE company_id = :cid;");
    qPlan.bindValue(":cid", id);
    qPlan.exec();

    QSqlQuery qEvent(m_db);
    qEvent.prepare("DELETE FROM social_events WHERE company_id = :cid;");
    qEvent.bindValue(":cid", id);
    qEvent.exec();

    QSqlQuery qContact(m_db);
    qContact.prepare("UPDATE contacts SET company_id = NULL WHERE company_id = :cid;");
    qContact.bindValue(":cid", id);
    qContact.exec();

    if (!compName.isEmpty()) {
        QSqlQuery qDelDict(m_db);
        qDelDict.prepare("DELETE FROM dictionary WHERE category IN ('UNIT', 'COMPANY') AND value = :cname;");
        qDelDict.bindValue(":cname", compName);
        qDelDict.exec();
    }

    QSqlQuery qComp(m_db);
    qComp.prepare("DELETE FROM companies WHERE id = :id;");
    qComp.bindValue(":id", id);
    if (qComp.exec()) {
        m_db.commit();
        syncEncryptedDatabaseToDisk();

        // 同步清理该公司的 AI 点评 JSON 缓存文件
        QString jsonPath = resolveAssetPath(QString("assets/uppic/json/%1.json").arg(id));
        if (QFile::exists(jsonPath)) {
            QFile::remove(jsonPath);
        }

        return true;
    }

    m_db.rollback();
    return false;
}

FinancialSnapshot DatabaseManager::getFinancialSnapshot() {
    FinancialSnapshot snapshot;
    QSqlQuery query(m_db);

    // 个人总进资金与总出资金 -> 当前资金 = 总进资金 - 总出资金
    query.exec("SELECT IFNULL(SUM(amount), 0) FROM transactions WHERE (book_type = 'PERSONAL' OR book_type = 'BOTH') AND io_type = 'INCOME';");
    if (query.next()) snapshot.personalTotalIncome = query.value(0).toDouble();

    query.exec("SELECT IFNULL(SUM(amount), 0) FROM transactions WHERE (book_type = 'PERSONAL' OR book_type = 'BOTH') AND io_type = 'EXPENSE';");
    if (query.next()) snapshot.personalTotalExpense = query.value(0).toDouble();

    snapshot.personalBalance = snapshot.personalTotalIncome - snapshot.personalTotalExpense;

    // 公司总进资金与总出资金 -> 当前资金 = 总进资金 - 总出资金
    query.exec("SELECT IFNULL(SUM(amount), 0) FROM transactions WHERE (book_type = 'COMPANY' OR book_type = 'BOTH') AND io_type = 'INCOME';");
    if (query.next()) snapshot.companyTotalIncome = query.value(0).toDouble();

    query.exec("SELECT IFNULL(SUM(amount), 0) FROM transactions WHERE (book_type = 'COMPANY' OR book_type = 'BOTH') AND io_type = 'EXPENSE';");
    if (query.next()) snapshot.companyTotalExpense = query.value(0).toDouble();

    snapshot.companyBalance = snapshot.companyTotalIncome - snapshot.companyTotalExpense;

    // 个人负债 (欠款): 借入 - 还款 (规则1: 还网贷/还银行/还个人是还款；规则6: 借入是负债；规则7: 认领是负债；严禁扣减'借个人')
    query.exec(
        "SELECT "
        "(SELECT IFNULL(SUM(amount), 0) FROM transactions WHERE (book_type = 'PERSONAL' OR book_type = 'BOTH') AND io_type = 'INCOME' "
        " AND (category_sub IN ('借入', '认领') OR (category_parent IN ('押抵借贷', '抵押借贷', '借贷') AND (category_sub IN ('借入', '认领', '贷款', '抵押') OR category_sub LIKE '%借入%' OR category_sub LIKE '%认领%')))) - "
        "(SELECT IFNULL(SUM(amount), 0) FROM transactions WHERE (book_type = 'PERSONAL' OR book_type = 'BOTH') AND io_type = 'EXPENSE' "
        " AND (category_sub IN ('还网贷', '还银行', '还个人') OR category_parent IN ('还网贷', '还银行', '还个人') "
        "      OR (category_parent IN ('押抵借贷', '抵押借贷', '借贷') AND (category_sub IN ('还网贷', '还银行', '还个人', '还贷', '还息', '偿还') OR category_sub LIKE '%还%'))));"
    );
    if (query.next()) snapshot.personalBorrowIn = qMax(0.0, query.value(0).toDouble());

    // 个人应收: 借出 - 收回 (规则2: 支出->个人->押抵借贷'借个人'是应收；规则8: 收入->个人->押抵借贷'收回'是还款冲减)
    query.exec(
        "SELECT "
        "(SELECT IFNULL(SUM(amount), 0) FROM transactions WHERE (book_type = 'PERSONAL' OR book_type = 'BOTH') AND io_type = 'EXPENSE' "
        " AND (category_sub IN ('借个人', '借出', '拆借') "
        "      OR (category_parent IN ('押抵借贷', '抵押借贷', '借贷') AND (category_sub IN ('借个人', '借出', '拆借') OR category_sub LIKE '%借%')))) - "
        "(SELECT IFNULL(SUM(amount), 0) FROM transactions WHERE (book_type = 'PERSONAL' OR book_type = 'BOTH') AND io_type = 'INCOME' "
        " AND (category_sub IN ('收回', '赎回') "
        "      OR (category_parent IN ('押抵借贷', '抵押借贷', '借贷', '投资理财', '资产管理') AND (category_sub IN ('收回', '赎回') OR category_sub LIKE '%收回%'))));"
    );
    if (query.next()) snapshot.personalLendOut = qMax(0.0, query.value(0).toDouble());

    // 公司负债: 借入 - 还贷 (规则3: 支出->工作->押抵借贷'还贷'、'还息'全部是还款)
    query.exec(
        "SELECT "
        "(SELECT IFNULL(SUM(amount), 0) FROM transactions WHERE (book_type = 'COMPANY' OR book_type = 'BOTH') AND io_type = 'INCOME' "
        " AND category_parent IN ('押抵借贷', '抵押借贷', '借贷') AND (category_sub IN ('借入', '贷款', '抵押', '贴现', '拆借') OR category_sub LIKE '%借入%' OR category_sub LIKE '%贷款%')) - "
        "(SELECT IFNULL(SUM(amount), 0) FROM transactions WHERE (book_type = 'COMPANY' OR book_type = 'BOTH') AND io_type = 'EXPENSE' "
        " AND ((category_parent IN ('押抵借贷', '抵押借贷', '借贷') AND (category_sub IN ('还贷', '还息', '偿还', '还个人') OR category_sub LIKE '%还%')) "
        "      OR category_sub IN ('还贷', '还息')));"
    );
    if (query.next()) snapshot.companyBorrowIn = qMax(0.0, query.value(0).toDouble());

    // 公司应收: 按项目/联系人归集的未结算款 (规则4: 押抵借贷'借出'是应收；规则5: 业务经营'进度合同'、'账期合同'是应收；规则9: 投资理财'收回'是还款冲减)
    query.exec(
        "SELECT IFNULL(SUM(CASE WHEN net_rec > 0 THEN net_rec ELSE 0 END), 0) FROM ("
        "    SELECT t.contact_id, "
        "           SUM(CASE WHEN t.io_type = 'EXPENSE' AND (t.category_sub IN ('进度合同', '账期合同', '借出') OR (t.category_parent = '业务经营' AND t.category_sub LIKE '%合同%') OR (t.category_parent IN ('押抵借贷', '抵押借贷', '借贷') AND t.category_sub = '借出')) THEN t.amount ELSE 0 END) - "
        "           SUM(CASE WHEN t.io_type = 'INCOME' AND ((t.category_parent IN ('业务经营', '投资理财', '押抵借贷', '抵押借贷') AND t.category_sub IN ('收回', '赎回', '尾款', '项目结算')) OR t.category_sub IN ('收回', '尾款', '项目结算')) "
        "                    AND EXISTS (SELECT 1 FROM transactions t2 WHERE (t2.book_type = 'COMPANY' OR t2.book_type = 'BOTH') AND t2.io_type = 'EXPENSE' AND (t2.contact_id = t.contact_id OR (t2.contact_id IS NULL AND t.contact_id IS NULL)) AND (t2.category_sub IN ('进度合同', '账期合同', '借出') OR (t2.category_parent = '业务经营' AND t2.category_sub LIKE '%合同%') OR (t2.category_parent IN ('押抵借贷', '抵押借贷', '借贷') AND t2.category_sub = '借出')) AND t2.trans_date <= t.trans_date) "
        "                    THEN t.amount ELSE 0 END) AS net_rec "
        "    FROM transactions t "
        "    WHERE (t.book_type = 'COMPANY' OR t.book_type = 'BOTH') "
        "    GROUP BY t.contact_id "
        ");"
    );
    if (query.next()) snapshot.companyLendOut = qMax(0.0, query.value(0).toDouble());

    // 流动资金 = 当前资金 - 负债
    snapshot.personalLiquidFund = snapshot.personalBalance - snapshot.personalBorrowIn;
    snapshot.companyLiquidFund = snapshot.companyBalance - snapshot.companyBorrowIn;

    snapshot.totalBorrowIn = snapshot.personalBorrowIn + snapshot.companyBorrowIn;
    snapshot.totalLendOut = snapshot.personalLendOut + snapshot.companyLendOut;

    // 月均收支计算 (按最近12个月滚动年化月均，或实际有效月份平均；收入严格剔除借款/借入周转资金):
    // 1. 个人月均
    double pYearIncome = 0.0, pYearBorrow = 0.0, pYearExpense = 0.0;
    query.exec("SELECT IFNULL(SUM(amount), 0) FROM transactions WHERE (book_type = 'PERSONAL' OR book_type = 'BOTH') AND io_type = 'INCOME' AND trans_date >= date('now', '-1 year');");
    if (query.next()) pYearIncome = query.value(0).toDouble();

    query.exec("SELECT IFNULL(SUM(amount), 0) FROM transactions WHERE (book_type = 'PERSONAL' OR book_type = 'BOTH') AND io_type = 'INCOME' AND trans_date >= date('now', '-1 year') "
               "  AND (category_sub IN ('借入', '认领') OR (category_parent IN ('押抵借贷', '抵押借贷', '借贷') AND (category_sub IN ('借入', '认领', '贷款', '抵押') OR category_sub LIKE '%借入%' OR category_sub LIKE '%认领%')));");
    if (query.next()) pYearBorrow = query.value(0).toDouble();

    query.exec("SELECT IFNULL(SUM(amount), 0) FROM transactions WHERE (book_type = 'PERSONAL' OR book_type = 'BOTH') AND io_type = 'EXPENSE' AND trans_date >= date('now', '-1 year');");
    if (query.next()) pYearExpense = query.value(0).toDouble();

    snapshot.monthlyPersonalIncome = qMax(0.0, (pYearIncome - pYearBorrow) / 12.0);
    snapshot.monthlyPersonalExpense = qMax(0.0, pYearExpense / 12.0);

    if (pYearIncome <= 0.0 && snapshot.personalTotalIncome > 0.0) {
        double allBorrow = 0.0;
        query.exec("SELECT IFNULL(SUM(amount), 0) FROM transactions WHERE (book_type = 'PERSONAL' OR book_type = 'BOTH') AND io_type = 'INCOME' "
                   "  AND (category_sub IN ('借入', '认领') OR (category_parent IN ('押抵借贷', '抵押借贷', '借贷') AND (category_sub IN ('借入', '认领', '贷款', '抵押') OR category_sub LIKE '%借入%' OR category_sub LIKE '%认领%')));");
        if (query.next()) allBorrow = query.value(0).toDouble();
        int distinctMonths = 1;
        query.exec("SELECT COUNT(DISTINCT strftime('%Y-%m', trans_date)) FROM transactions WHERE (book_type = 'PERSONAL' OR book_type = 'BOTH');");
        if (query.next()) distinctMonths = qMax(1, query.value(0).toInt());
        snapshot.monthlyPersonalIncome = qMax(0.0, (snapshot.personalTotalIncome - allBorrow) / distinctMonths);
        snapshot.monthlyPersonalExpense = qMax(0.0, snapshot.personalTotalExpense / distinctMonths);
    }

    // 2. 公司月均
    double cYearIncome = 0.0, cYearBorrow = 0.0, cYearExpense = 0.0;
    query.exec("SELECT IFNULL(SUM(amount), 0) FROM transactions WHERE (book_type = 'COMPANY' OR book_type = 'BOTH') AND io_type = 'INCOME' AND trans_date >= date('now', '-1 year');");
    if (query.next()) cYearIncome = query.value(0).toDouble();

    query.exec("SELECT IFNULL(SUM(amount), 0) FROM transactions WHERE (book_type = 'COMPANY' OR book_type = 'BOTH') AND io_type = 'INCOME' AND trans_date >= date('now', '-1 year') "
               "  AND category_parent IN ('押抵借贷', '抵押借贷', '借贷') AND (category_sub IN ('借入', '贷款', '抵押', '贴现', '拆借') OR category_sub LIKE '%借入%' OR category_sub LIKE '%贷款%');");
    if (query.next()) cYearBorrow = query.value(0).toDouble();

    query.exec("SELECT IFNULL(SUM(amount), 0) FROM transactions WHERE (book_type = 'COMPANY' OR book_type = 'BOTH') AND io_type = 'EXPENSE' AND trans_date >= date('now', '-1 year');");
    if (query.next()) cYearExpense = query.value(0).toDouble();

    snapshot.monthlyCompanyIncome = qMax(0.0, (cYearIncome - cYearBorrow) / 12.0);
    snapshot.monthlyCompanyExpense = qMax(0.0, cYearExpense / 12.0);

    if (cYearIncome <= 0.0 && snapshot.companyTotalIncome > 0.0) {
        int distinctMonths = 1;
        query.exec("SELECT COUNT(DISTINCT strftime('%Y-%m', trans_date)) FROM transactions WHERE (book_type = 'COMPANY' OR book_type = 'BOTH');");
        if (query.next()) distinctMonths = qMax(1, query.value(0).toInt());
        snapshot.monthlyCompanyIncome = qMax(0.0, snapshot.companyTotalIncome / distinctMonths);
        snapshot.monthlyCompanyExpense = qMax(0.0, snapshot.companyTotalExpense / distinctMonths);
    }

    double totalMonthlyExpense = snapshot.monthlyCompanyExpense + snapshot.monthlyPersonalExpense;
    if (totalMonthlyExpense > 0.0) {
        double dailyBurn = totalMonthlyExpense / 30.0;
        double totalBalance = qMax(0.0, snapshot.companyBalance + snapshot.personalBalance);
        snapshot.runwayDays = static_cast<int>(totalBalance / dailyBurn);
    } else {
        snapshot.runwayDays = 365;
    }

    return snapshot;
}

QList<CategoryBreakdownItem> DatabaseManager::getMonthlyCategoryBreakdown(const QString& transType, int year, int month) {
    if (year <= 0 || month <= 0) {
        QDate today = QDate::currentDate();
        year = today.year();
        month = today.month();
    }

    QList<CategoryBreakdownItem> list;
    QString ymStr = QString("%1-%2").arg(year).arg(month, 2, 10, QChar('0'));
    QString sql = "SELECT category_sub, SUM(amount) AS total, COUNT(*) AS cnt FROM transactions WHERE io_type = :tt "
                  "AND strftime('%Y-%m', trans_date) = :ym "
                  "GROUP BY category_sub ORDER BY total DESC;";

    QSqlQuery q(m_db);
    q.prepare(sql);
    q.bindValue(":tt", transType);
    q.bindValue(":ym", ymStr);
    q.exec();

    double grandTotal = 0.0;
    while (q.next()) {
        CategoryBreakdownItem item;
        item.category = q.value(0).toString();
        item.amount = q.value(1).toDouble();
        item.count = q.value(2).toInt();
        grandTotal += item.amount;
        list.append(item);
    }

    if (list.isEmpty()) {
        QSqlQuery qAll(m_db);
        qAll.prepare("SELECT category_sub, SUM(amount) AS total, COUNT(*) AS cnt FROM transactions WHERE io_type = :tt GROUP BY category_sub ORDER BY total DESC;");
        qAll.bindValue(":tt", transType);
        qAll.exec();
        while (qAll.next()) {
            CategoryBreakdownItem item;
            item.category = qAll.value(0).toString();
            item.amount = qAll.value(1).toDouble();
            item.count = qAll.value(2).toInt();
            grandTotal += item.amount;
            list.append(item);
        }
    }

    if (grandTotal > 0.0) {
        for (auto& item : list) {
            item.percentage = (item.amount / grandTotal) * 100.0;
        }
    }
    return list;
}

QList<CategoryHierarchyItem> DatabaseManager::getCategoryHierarchy(const QString& transType, const QString& scope) {
    QList<CategoryHierarchyItem> result;
    QSqlQuery q(m_db);
    QString sql = "SELECT parent_name, sub_name FROM transaction_categories WHERE io_type = :tt ";
    if (!scope.isEmpty()) {
        sql += "AND (book_type = :sc OR book_type = 'BOTH') ";
    }
    sql += "ORDER BY id ASC, parent_name, sub_name;";
    q.prepare(sql);
    q.bindValue(":tt", transType);
    if (!scope.isEmpty()) {
        q.bindValue(":sc", scope);
    }
    q.exec();

    QMap<QString, QStringList> map;
    QStringList parentOrder;
    while (q.next()) {
        QString p = q.value(0).toString();
        QString s = q.value(1).toString();
        if (!map.contains(p)) {
            parentOrder.append(p);
        }
        if (!map[p].contains(s)) {
            map[p].append(s);
        }
    }

    for (const auto& p : parentOrder) {
        CategoryHierarchyItem item;
        item.parentCategory = p;
        item.subCategories = map[p];
        result.append(item);
    }
    return result;
}

QStringList DatabaseManager::getSubCategoriesByParent(const QString& bookType, const QString& ioType, const QString& parentCategory) {
    QStringList list;
    QSqlQuery q(m_db);
    q.prepare("SELECT sub_name FROM transaction_categories "
              "WHERE (book_type = :bt OR book_type = 'BOTH') AND io_type = :io AND parent_name = :p "
              "ORDER BY id ASC;");
    q.bindValue(":bt", bookType);
    q.bindValue(":io", ioType);
    q.bindValue(":p", parentCategory);
    if (q.exec()) {
        while (q.next()) {
            QString s = q.value(0).toString().trimmed();
            if (!s.isEmpty() && !list.contains(s)) list.append(s);
        }
    }

    // 过滤排除遗留英文系统代号及被删除的分类
    static const QSet<QString> kExcludedSubs = {
        "GIFT", "GIFT_OUT", "GIFT_IN", "HELP", "HELP_GIVEN", "HELP_RECEIVED",
        "DINNER", "TREAT", "ENTERTAIN", "MEETING", "BUSINESS", "业务", "FINANCIAL"
    };
    list.removeIf([](const QString& s) {
        return kExcludedSubs.contains(s.trimmed().toUpper()) || kExcludedSubs.contains(s.trimmed());
    });

    return list;
}

bool DatabaseManager::addCustomCategory(const QString& transType, const QString& parentCategory, const QString& subCategory, const QString& scope) {
    if (parentCategory.trimmed().isEmpty() || subCategory.trimmed().isEmpty()) return false;
    QString sc = scope.isEmpty() ? "PERSONAL" : scope;
    QString p = parentCategory.trimmed();
    QString s = subCategory.trimmed();

    QSqlQuery checkQ(m_db);
    checkQ.prepare("SELECT id FROM transaction_categories WHERE book_type = :sc AND io_type = :tt AND parent_name = :p AND sub_name = :s LIMIT 1;");
    checkQ.bindValue(":sc", sc);
    checkQ.bindValue(":tt", transType);
    checkQ.bindValue(":p", p);
    checkQ.bindValue(":s", s);
    if (checkQ.exec() && checkQ.next()) {
        return true;
    }

    QSqlQuery q(m_db);
    q.prepare("INSERT OR IGNORE INTO transaction_categories (book_type, io_type, parent_name, sub_name, is_system) VALUES (:sc, :tt, :p, :s, 0);");
    q.bindValue(":sc", sc);
    q.bindValue(":tt", transType);
    q.bindValue(":p", p);
    q.bindValue(":s", s);
    return q.exec();
}

QList<QVariantMap> DatabaseManager::getRecentTransactions(int limit, const QString& bookType, int year, int month) {
    QList<QVariantMap> list;
    QString sql = "SELECT t.id, t.book_type, t.io_type AS trans_type, t.category_sub AS sub_type, "
                  "t.amount, t.category_parent AS category, t.trans_date AS trans_time, "
                  "t.remark, t.project_name, t.voucher_path, c.name AS contact_name, comp.name AS company_name "
                  "FROM transactions t "
                  "LEFT JOIN contacts c ON t.contact_id = c.id "
                  "LEFT JOIN companies comp ON t.company_id = comp.id WHERE 1=1 ";
    if (!bookType.isEmpty()) {
        sql += "AND t.book_type = '" + bookType + "' ";
    }
    if (year > 0 && month > 0) {
        sql += QString("AND strftime('%Y-%m', t.trans_date) = '%1-%2' ")
                   .arg(year)
                   .arg(month, 2, 10, QChar('0'));
    }
    sql += QString("ORDER BY t.trans_date DESC LIMIT %1;").arg(limit);

    QSqlQuery q(sql, m_db);
    while (q.next()) {
        QVariantMap row;
        row["id"] = q.value("id");
        row["book_type"] = q.value("book_type");
        row["trans_type"] = q.value("trans_type");
        row["sub_type"] = q.value("sub_type");
        row["amount"] = q.value("amount");
        row["category"] = q.value("category");
        row["trans_time"] = q.value("trans_time");
        row["remark"] = q.value("remark");
        row["project_name"] = q.value("project_name");
        row["voucher_path"] = q.value("voucher_path");
        row["contact_name"] = q.value("contact_name");
        row["company_name"] = q.value("company_name");
        list.append(row);
    }
    return list;
}

bool DatabaseManager::addTransaction(const QString& bookType, int accountId, const QString& transType,
                                    double amount, const QString& category, const QDateTime& time,
                                    int contactId, const QString& remark,
                                    const QString& subType, int companyId,
                                    const QString& voucherPath) {
    m_db.transaction();
    QSqlQuery q(m_db);
    // 注意参数含义: category 为小类(或主分类)，subType 若传入大类则作 parent，这里智能识别
    QString parentCat = subType.isEmpty() ? "日常收支" : subType;
    QString subCat = category.isEmpty() ? parentCat : category;

    q.prepare("INSERT INTO transactions (book_type, company_id, account_id, io_type, category_parent, category_sub, "
              "amount, trans_date, contact_id, remark, project_name, voucher_path) "
              "VALUES (:book, :cid_comp, :acc, :type, :cat, :subtype, :amt, :time, :cid, :rem, :proj, :vpath);");
    q.bindValue(":book", bookType.isEmpty() ? "PERSONAL" : bookType);
    if (companyId > 0) q.bindValue(":cid_comp", companyId);
    else q.bindValue(":cid_comp", QVariant());
    if (accountId > 0) q.bindValue(":acc", accountId);
    else q.bindValue(":acc", QVariant());
    q.bindValue(":type", transType);
    q.bindValue(":cat", parentCat);
    q.bindValue(":subtype", subCat);
    q.bindValue(":amt", amount);
    q.bindValue(":time", time.toString("yyyy-MM-dd HH:mm:ss"));
    if (contactId > 0) q.bindValue(":cid", contactId);
    else q.bindValue(":cid", QVariant());
    q.bindValue(":rem", remark);
    q.bindValue(":proj", remark.isEmpty() ? subCat : remark);
    if (!voucherPath.isEmpty()) q.bindValue(":vpath", voucherPath);
    else q.bindValue(":vpath", QVariant());

    if (!q.exec()) {
        qWarning() << "[DB] addTransaction failed:" << q.lastError().text();
        m_db.rollback();
        return false;
    }

    if (contactId > 0) {
        QSqlQuery updateContact(m_db);
        updateContact.prepare("UPDATE contacts SET last_contact_date = :dt, updated_at = CURRENT_TIMESTAMP WHERE id = :cid;");
        updateContact.bindValue(":dt", time.date().toString("yyyy-MM-dd"));
        updateContact.bindValue(":cid", contactId);
        updateContact.exec();
    }

    m_db.commit();
    return true;
}

QStringList DatabaseManager::getDictionaryValues(const QString& category) {
    QStringList values;
    QSqlQuery q(m_db);
    QString cat = (category == "COMPANY") ? "UNIT" : category;
    q.prepare("SELECT value FROM dictionary WHERE category = :cat ORDER BY id ASC;");
    q.bindValue(":cat", cat);
    if (q.exec()) {
        while (q.next()) {
            values.append(q.value(0).toString());
        }
    }
    return values;
}

bool DatabaseManager::addDictionaryValue(const QString& category, const QString& value) {
    if (value.trimmed().isEmpty()) return false;
    QString cat = (category == "COMPANY") ? "UNIT" : category;
    QSqlQuery q(m_db);
    q.prepare("INSERT OR IGNORE INTO dictionary (category, value) VALUES (:cat, :val);");
    q.bindValue(":cat", cat);
    q.bindValue(":val", value.trimmed());
    return q.exec();
}

QList<QVariantMap> DatabaseManager::getAccounts(const QString& bookType) {
    Q_UNUSED(bookType);
    return {};
}

int DatabaseManager::getContactsCount() {
    QSqlQuery q("SELECT COUNT(*) FROM contacts;", m_db);
    if (q.next()) return q.value(0).toInt();
    return 0;
}

static int scoreToStars(int score) {
    if (score <= 5) return qBound(1, score, 5);
    return qBound(1, (score + 19) / 20, 5);
}

static int starsToScore(int stars) {
    if (stars > 5) return qBound(0, stars, 100);
    return qBound(20, stars * 20, 100);
}

static ContactSummary parseContactSummaryRow(const QSqlQuery& q) {
    ContactSummary cs;
    cs.id = q.value("id").toInt();
    cs.name = q.value("name").toString();
    cs.phone = q.value("phone").toString();
    cs.relationType = q.value("relation_type").toString();
    cs.company = q.value("unit").toString();
    cs.companyId = q.value("company_id").toInt();
    cs.title = q.value("title").toString();
    cs.industry = q.value("industry").toString();
    cs.closenessLevel = scoreToStars(q.value("closeness_score").toInt());
    cs.influenceLevel = scoreToStars(q.value("inf_score").toInt());
    cs.creditRating = scoreToStars(q.value("cred_score").toInt());
    cs.preferences = q.value("preferences").toString();
    cs.taboos = q.value("taboos").toString();
    cs.province = q.value("province").toString();
    cs.city = q.value("city").toString();
    cs.district = q.value("district").toString();
    QString detailAddr = q.value("detail_address").toString();
    cs.address = detailAddr.isEmpty() ? (cs.province + cs.city + cs.district) : (cs.province + cs.city + cs.district + detailAddr);
    cs.notes = q.value("notes").toString();
    cs.avatarPath = q.value("avatar_path").toString();
    cs.introducerId = q.value("introducer_id").toInt();
    cs.introducerName = q.value("intro_name").toString();
    cs.totalSpent = q.value("calc_spent").toDouble();
    cs.totalReturn = q.value("calc_return").toDouble();
    cs.meetDate = q.value("known_since").toString();
    if (cs.meetDate.isEmpty()) cs.meetDate = "2024-05-18";
    cs.helpCount = q.value("calc_help").toInt();
    cs.entertainmentCount = q.value("calc_ent").toInt();
    cs.diningCount = q.value("calc_din").toInt();
    cs.cooperationCount = q.value("calc_coop").toInt();
    cs.aiReview = q.value("ai_review").toString();
    return cs;
}

static QString contactSummarySelectSql() {
    return "SELECT c.id, c.name, c.phone, c.relation_type, c.unit, c.company_id, c.title, c.industry, "
           "c.closeness_score, IFNULL(c.influence_score, c.closeness_score) AS inf_score, "
           "IFNULL(c.credit_score, 90) AS cred_score, c.preferences, IFNULL(c.taboos, '') AS taboos, "
           "c.province, c.city, c.district, c.detail_address, c.notes, "
           "c.avatar_path, c.introducer_id, intro.name AS intro_name, c.known_since, c.ai_review, "
           "IFNULL((SELECT SUM(amount) FROM transactions t WHERE t.contact_id = c.id AND t.io_type = 'EXPENSE'), 0.0) + "
           "IFNULL((SELECT SUM(my_cost) FROM social_events e WHERE e.contact_id = c.id), 0.0) AS calc_spent, "
           "IFNULL((SELECT SUM(amount) FROM transactions t WHERE t.contact_id = c.id AND t.io_type = 'INCOME'), 0.0) + "
           "IFNULL((SELECT SUM(their_value) FROM social_events e WHERE e.contact_id = c.id), 0.0) AS calc_return, "
           "IFNULL((SELECT COUNT(*) FROM social_events e WHERE e.contact_id = c.id AND (e.event_category IN ('HELP_GIVEN', 'HELP_RECEIVED') OR e.event_category LIKE '%帮扶%')), 0) AS calc_help, "
           "IFNULL((SELECT COUNT(*) FROM social_events e WHERE e.contact_id = c.id AND (e.event_category IN ('ENTERTAIN', 'MEETING') OR e.event_category LIKE '%应酬%' OR e.event_category LIKE '%招待%')), 0) AS calc_ent, "
           "IFNULL((SELECT COUNT(*) FROM social_events e WHERE e.contact_id = c.id AND (e.event_category IN ('TREAT', 'GIFT_OUT', 'GIFT_IN') OR e.event_category LIKE '%吃饭%' OR e.event_category LIKE '%送礼%' OR e.event_category LIKE '%宴请%')), 0) AS calc_din, "
           "IFNULL((SELECT COUNT(*) FROM social_events e WHERE e.contact_id = c.id AND (e.event_category = 'BUSINESS' OR e.event_category LIKE '%合作%')), 0) AS calc_coop "
           "FROM contacts c "
           "LEFT JOIN contacts intro ON c.introducer_id = intro.id ";
}

QList<ContactSummary> DatabaseManager::getContacts(const QString& keyword) {
    QList<ContactSummary> list;
    QString sql = contactSummarySelectSql();
    if (!keyword.isEmpty()) {
        sql += "WHERE c.name LIKE '%" + keyword + "%' OR c.unit LIKE '%" + keyword + "%' OR c.phone LIKE '%" + keyword + "%' OR c.industry LIKE '%" + keyword + "%' ";
    }
    sql += "ORDER BY c.closeness_score DESC, c.id ASC;";

    QSqlQuery q(sql, m_db);
    while (q.next()) {
        list.append(parseContactSummaryRow(q));
    }
    return list;
}

ContactSummary DatabaseManager::getContactDetails(int contactId) {
    if (contactId <= 0) return ContactSummary{};
    QSqlQuery q(m_db);
    q.prepare(contactSummarySelectSql() + "WHERE c.id = :cid LIMIT 1;");
    q.bindValue(":cid", contactId);
    if (q.exec() && q.next()) {
        return parseContactSummaryRow(q);
    }
    return ContactSummary{};
}

int DatabaseManager::getContactDirectFriendCount(int contactId) {
    if (contactId <= 0) return 0;
    QSet<int> friendIds;
    QSqlQuery q(m_db);

    // 1. relationships 表双向直接关联
    q.prepare("SELECT to_contact_id FROM relationships WHERE from_contact_id = :cid "
              "UNION "
              "SELECT from_contact_id FROM relationships WHERE to_contact_id = :cid;");
    q.bindValue(":cid", contactId);
    if (q.exec()) {
        while (q.next()) {
            int fid = q.value(0).toInt();
            if (fid > 0 && fid != contactId) friendIds.insert(fid);
        }
    }

    // 2. contacts.introducer_id 引荐关系双向关联
    QSqlQuery q2(m_db);
    q2.prepare("SELECT id FROM contacts WHERE introducer_id = :cid "
               "UNION "
               "SELECT introducer_id FROM contacts WHERE id = :cid AND introducer_id > 0;");
    q2.bindValue(":cid", contactId);
    if (q2.exec()) {
        while (q2.next()) {
            int fid = q2.value(0).toInt();
            if (fid > 0 && fid != contactId) friendIds.insert(fid);
        }
    }

    return friendIds.size();
}

QList<CategoryBreakdownItem> DatabaseManager::getContactSocialBreakdown(int contactId) {
    QList<CategoryBreakdownItem> result;
    if (contactId <= 0) return result;

    auto mapEventCodeToName = [](const QString& raw) -> QString {
        if (raw == "TREAT" || raw == "DINNER") return "请客吃饭";
        if (raw == "GIFT_OUT" || raw == "GIFT") return "送礼随份子";
        if (raw == "GIFT_IN") return "受赠回礼";
        if (raw == "ENTERTAIN") return "客户招待应酬";
        if (raw == "MEETING") return "差旅拜访";
        if (raw == "BUSINESS") return "生意合作";
        if (raw == "HELP_GIVEN" || raw == "HELP_RECEIVED" || raw == "HELP") return "互相帮扶";
        return raw;
    };

    QMap<QString, int> countMap;
    QMap<QString, double> amountMap;
    QStringList orderList;

    auto addStat = [&](const QString& name, int cnt, double amt) {
        if (cnt <= 0) return;
        QString key = mapEventCodeToName(name.trimmed());
        if (key.isEmpty()) return;
        if (!countMap.contains(key)) {
            orderList.append(key);
            countMap[key] = 0;
            amountMap[key] = 0.0;
        }
        countMap[key] += cnt;
        amountMap[key] += amt;
    };

    // 1. 查询该联系人在 social_events 表中的真实互动与往来记录
    QSqlQuery qEv(m_db);
    qEv.prepare("SELECT event_category, COUNT(*), SUM(my_cost + their_value) "
                "FROM social_events WHERE contact_id = :cid GROUP BY event_category;");
    qEv.bindValue(":cid", contactId);
    if (qEv.exec()) {
        while (qEv.next()) {
            addStat(qEv.value(0).toString(), qEv.value(1).toInt(), qEv.value(2).toDouble());
        }
    }

    // 2. 查询该联系人在 transactions 表中 人脉社交 / 商务社交 / 商务拓展 下的真实流水记录
    QSqlQuery qTr(m_db);
    qTr.prepare("SELECT category_sub, COUNT(*), SUM(amount) "
                "FROM transactions "
                "WHERE contact_id = :cid AND category_parent IN ('人脉社交', '商务社交', '商务拓展', '人情往来') "
                "GROUP BY category_sub;");
    qTr.bindValue(":cid", contactId);
    if (qTr.exec()) {
        while (qTr.next()) {
            addStat(qTr.value(0).toString(), qTr.value(1).toInt(), qTr.value(2).toDouble());
        }
    }

    if (orderList.isEmpty()) {
        return result;
    }

    int maxCount = 1;
    for (const auto& k : orderList) {
        if (countMap[k] > maxCount) maxCount = countMap[k];
    }

    for (const auto& k : orderList) {
        CategoryBreakdownItem item;
        item.category = k;
        item.count = countMap[k];
        item.amount = amountMap[k];
        item.percentage = (maxCount > 0) ? (item.count * 100.0 / maxCount) : 0.0;
        result.append(item);
    }

    return result;
}

bool DatabaseManager::addContact(const QString& name, const QString& phone, const QString& relationType,
                                 const QString& company, const QString& title, const QString& industry,
                                 int closeness, int influence, int credit, int introducerId,
                                 const QString& preferences, const QString& notes) {
    return addContactFull(name, phone, relationType, company, title, industry, closeness, influence, credit, introducerId, preferences, notes, "", "", "", "", "");
}

bool DatabaseManager::addContactFull(const QString& name, const QString& phone, const QString& relationType,
                                     const QString& company, const QString& title, const QString& industry,
                                     int closeness, int influence, int credit, int introducerId,
                                     const QString& preferences, const QString& notes,
                                     const QString& province, const QString& city, const QString& district,
                                     const QString& address, const QString& taboos, const QString& knownSince) {
    QSqlQuery q(m_db);
    int cScore = starsToScore(closeness);
    int iScore = starsToScore(influence);
    int crScore = starsToScore(credit);
    QString status = (cScore >= 85) ? "INTIMATE" : (cScore >= 70 ? "CLOSE" : "NORMAL");
    q.prepare("INSERT INTO contacts (name, phone, relation_type, unit, title, industry, closeness_score, influence_score, credit_score, closeness_Status, "
              "known_since, last_contact_date, introducer_id, preferences, taboos, notes, province, city, district, detail_address) "
              "VALUES (:name, :phone, :rel, :unit, :title, :ind, :score, :iscore, :crscore, :status, :since, :last, :intro, :pref, :taboos, :notes, :prov, :city, :dist, :daddr);");
    q.bindValue(":name", name);
    q.bindValue(":phone", phone);
    q.bindValue(":rel", relationType.isEmpty() ? "朋友" : relationType);
    q.bindValue(":unit", company);
    q.bindValue(":title", title);
    q.bindValue(":ind", industry);
    q.bindValue(":score", cScore);
    q.bindValue(":iscore", iScore);
    q.bindValue(":crscore", crScore);
    q.bindValue(":status", status);
    QString sinceStr = knownSince.trimmed();
    if (sinceStr.isEmpty()) sinceStr = QDate::currentDate().toString("yyyy-MM-dd");
    q.bindValue(":since", sinceStr);
    q.bindValue(":last", sinceStr);
    q.bindValue(":intro", introducerId > 0 ? introducerId : 0);
    q.bindValue(":pref", preferences);
    q.bindValue(":taboos", taboos);
    q.bindValue(":notes", notes);
    q.bindValue(":prov", province);
    q.bindValue(":city", city);
    q.bindValue(":dist", district);
    q.bindValue(":daddr", address);

    if (q.exec()) {
        addDictionaryValue("RELATION_TYPE", relationType);
        addDictionaryValue("UNIT", company);
        addDictionaryValue("TITLE", title);
        addDictionaryValue("INDUSTRY", industry);
        return true;
    }
    qWarning() << "[DB] addContactFull failed:" << q.lastError().text();
    return false;
}

bool DatabaseManager::updateContact(int contactId, const QString& name, const QString& phone, const QString& relationType,
                                    const QString& company, const QString& title, const QString& industry,
                                    int closeness, int influence, int credit, const QString& preferences) {
    return updateContactFull(contactId, name, phone, relationType, company, title, industry, closeness, influence, credit, preferences, "", "", "", "", "", "", "");
}

bool DatabaseManager::updateContactFull(int contactId, const QString& name, const QString& phone, const QString& relationType,
                                        const QString& company, const QString& title, const QString& industry,
                                        int closeness, int influence, int credit, const QString& preferences,
                                        const QString& province, const QString& city, const QString& district,
                                        const QString& address, const QString& notes, const QString& taboos,
                                        const QString& knownSince) {
    if (contactId <= 0) return false;
    QSqlQuery q(m_db);
    int cScore = starsToScore(closeness);
    int iScore = starsToScore(influence);
    int crScore = starsToScore(credit);
    QString status = (cScore >= 85) ? "INTIMATE" : (cScore >= 70 ? "CLOSE" : "NORMAL");
    QString sinceStr = knownSince.trimmed();
    if (sinceStr.isEmpty()) sinceStr = QDate::currentDate().toString("yyyy-MM-dd");

    q.prepare("UPDATE contacts SET name = :name, phone = :phone, relation_type = :rel, unit = :unit, "
              "title = :title, industry = :ind, closeness_score = :score, influence_score = :iscore, credit_score = :crscore, "
              "closeness_Status = :status, known_since = :since, preferences = :pref, taboos = :taboos, province = :prov, city = :city, district = :dist, detail_address = :daddr, notes = :notes, "
              "updated_at = CURRENT_TIMESTAMP WHERE id = :id;");
    q.bindValue(":name", name);
    q.bindValue(":phone", phone);
    q.bindValue(":rel", relationType);
    q.bindValue(":unit", company);
    q.bindValue(":title", title);
    q.bindValue(":ind", industry);
    q.bindValue(":score", cScore);
    q.bindValue(":iscore", iScore);
    q.bindValue(":crscore", crScore);
    q.bindValue(":status", status);
    q.bindValue(":since", sinceStr);
    q.bindValue(":pref", preferences);
    q.bindValue(":taboos", taboos);
    q.bindValue(":prov", province);
    q.bindValue(":city", city);
    q.bindValue(":dist", district);
    q.bindValue(":daddr", address);
    q.bindValue(":notes", notes);
    q.bindValue(":id", contactId);

    if (q.exec()) {
        addDictionaryValue("RELATION_TYPE", relationType);
        addDictionaryValue("UNIT", company);
        addDictionaryValue("TITLE", title);
        addDictionaryValue("INDUSTRY", industry);
        return true;
    }
    qWarning() << "[DB] updateContactFull failed:" << q.lastError().text();
    return false;
}

bool DatabaseManager::updateContactAvatar(int contactId, const QString& avatarPath) {
    if (contactId <= 0) return false;
    QSqlQuery q(m_db);
    q.prepare("UPDATE contacts SET avatar_path = :av, updated_at = CURRENT_TIMESTAMP WHERE id = :id;");
    q.bindValue(":av", avatarPath);
    q.bindValue(":id", contactId);
    return q.exec();
}

bool DatabaseManager::saveContactAiReview(int contactId, const QString& reviewText) {
    if (contactId <= 0) return false;
    QSqlQuery q(m_db);
    q.prepare("UPDATE contacts SET ai_review = :rev, updated_at = CURRENT_TIMESTAMP WHERE id = :id;");
    q.bindValue(":rev", reviewText);
    q.bindValue(":id", contactId);
    return q.exec();
}

ContactInteractionStats DatabaseManager::getContactInteractionStats(int contactId) {
    ContactInteractionStats stats;
    ContactSummary cs = getContactDetails(contactId);
    stats.contactName = cs.name;
    stats.meetDate = QDate::fromString(cs.meetDate, "yyyy-MM-dd");
    if (!stats.meetDate.isValid()) stats.meetDate = QDate::currentDate().addYears(-1);
    stats.entertainCount = cs.entertainmentCount;
    stats.dinnerCount = cs.diningCount;
    stats.businessCount = cs.cooperationCount;
    stats.helpCount = cs.helpCount;
    stats.myTotalCost = cs.totalSpent;
    stats.theirTotalValue = cs.totalReturn;
    return stats;
}

QList<QVariantMap> DatabaseManager::getContactBusinessTransactions(int contactId) {
    QList<QVariantMap> list;
    if (contactId <= 0) return list;

    QString sql =
        "SELECT t.id, t.book_type, t.io_type, t.category_parent, t.category_sub, "
        "t.amount, t.trans_date, t.remark, t.project_name, comp.name AS company_name "
        "FROM transactions t "
        "LEFT JOIN companies comp ON t.company_id = comp.id "
        "WHERE (t.contact_id = :cid OR (t.contact_id IS NULL AND t.account_id = :cid)) "
        "ORDER BY t.trans_date DESC, t.id DESC;";

    QSqlQuery q(m_db);
    q.prepare(sql);
    q.bindValue(":cid", contactId);
    if (q.exec()) {
        while (q.next()) {
            QVariantMap row;
            row["id"] = q.value("id");
            row["book_type"] = q.value("book_type");
            row["io_type"] = q.value("io_type");
            row["trans_type"] = q.value("io_type");
            row["category_parent"] = q.value("category_parent");
            row["category"] = q.value("category_parent");
            row["category_sub"] = q.value("category_sub");
            row["sub_type"] = q.value("category_sub");
            row["amount"] = q.value("amount");
            row["trans_date"] = q.value("trans_date");
            row["trans_time"] = q.value("trans_date");
            row["remark"] = q.value("remark");
            row["project_name"] = q.value("project_name");
            row["company_name"] = q.value("company_name");
            list.append(row);
        }
    }
    return list;
}

QList<TimelineNode> DatabaseManager::getRelationshipTimeline(int contactId) {
    QList<TimelineNode> list;
    QSqlQuery q(m_db);
    q.prepare("SELECT id, contact_id, stage_name, event_date, description, rating_after "
              "FROM relationship_timeline WHERE contact_id = :id ORDER BY event_date ASC, id ASC;");
    q.bindValue(":id", contactId);
    if (q.exec()) {
        while (q.next()) {
            TimelineNode node;
            node.id = q.value("id").toInt();
            node.contactId = q.value("contact_id").toInt();
            node.stageName = q.value("stage_name").toString();
            node.eventDate = q.value("event_date").toDate();
            node.description = q.value("description").toString();
            node.ratingAfter = q.value("rating_after").toInt();
            list.append(node);
        }
    }
    return list;
}

bool DatabaseManager::addTimelineNode(int contactId, const QString& stageName, const QDate& eventDate,
                                     const QString& description, int ratingAfter) {
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO relationship_timeline (contact_id, stage_name, event_date, description, rating_after) "
              "VALUES (:cid, :stage, :date, :desc, :rating);");
    q.bindValue(":cid", contactId);
    q.bindValue(":stage", stageName);
    q.bindValue(":date", eventDate.toString("yyyy-MM-dd"));
    q.bindValue(":desc", description);
    int clampedStars = qBound(1, ratingAfter, 5);
    q.bindValue(":rating", clampedStars);
    if (q.exec()) {
        // 同步更新联系人最新亲疏星级
        QSqlQuery uq(m_db);
        int cScore = starsToScore(clampedStars);
        QString status = (cScore >= 85) ? "INTIMATE" : (cScore >= 70 ? "CLOSE" : "NORMAL");
        uq.prepare("UPDATE contacts SET closeness_score = :sc, closeness_Status = :st, updated_at = CURRENT_TIMESTAMP WHERE id = :cid;");
        uq.bindValue(":sc", cScore);
        uq.bindValue(":st", status);
        uq.bindValue(":cid", contactId);
        uq.exec();
        return true;
    }
    return false;
}

QList<QVariantMap> DatabaseManager::getSocialEvents(int contactId, bool onlyPending) {
    QList<QVariantMap> list;
    QString sql = "SELECT e.id, e.contact_id, e.scope, e.company_id, e.event_category, "
                  "e.title, e.detail, e.event_date, e.my_cost, e.their_value, e.status, "
                  "c.name AS contact_name, comp.name AS company_name "
                  "FROM social_events e "
                  "LEFT JOIN contacts c ON e.contact_id = c.id "
                  "LEFT JOIN companies comp ON e.company_id = comp.id WHERE 1=1 ";
    if (contactId > 0) {
        sql += QString("AND e.contact_id = %1 ").arg(contactId);
    }
    if (onlyPending) {
        sql += "AND e.status = 'PENDING_RETURN' ";
    }
    sql += "ORDER BY e.event_date DESC, e.id DESC;";

    QSqlQuery q(sql, m_db);
    while (q.next()) {
        QVariantMap row;
        row["id"] = q.value("id");
        row["contact_id"] = q.value("contact_id");
        row["scope"] = q.value("scope");
        row["company_id"] = q.value("company_id");
        row["event_category"] = q.value("event_category");
        row["title"] = q.value("title");
        row["detail"] = q.value("detail");
        row["event_date"] = q.value("event_date");
        row["my_cost"] = q.value("my_cost");
        row["their_value"] = q.value("their_value");
        row["status"] = q.value("status");
        row["contact_name"] = q.value("contact_name");
        row["company_name"] = q.value("company_name");
        list.append(row);
    }
    return list;
}

bool DatabaseManager::addSocialEvent(int contactId, const QString& eventType, const QString& title,
                                     const QString& detail, double estimatedValue, const QDate& eventDate,
                                     const QString& status, int transactionId) {
    Q_UNUSED(transactionId);
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO social_events (contact_id, scope, event_category, title, detail, event_date, my_cost, status) "
              "VALUES (:cid, 'PRIVATE', :cat, :title, :detail, :date, :cost, :status);");
    q.bindValue(":cid", contactId);
    q.bindValue(":cat", eventType.isEmpty() ? "请客吃饭" : eventType);
    q.bindValue(":title", title);
    q.bindValue(":detail", detail);
    q.bindValue(":date", eventDate.toString("yyyy-MM-dd"));
    q.bindValue(":cost", estimatedValue);
    q.bindValue(":status", status.isEmpty() ? "CLOSED" : status);
    return q.exec();
}

QList<QVariantMap> DatabaseManager::getDiaries(const QString& bookType, int limit) {
    QList<QVariantMap> list;
    QString sql = "SELECT d.id, d.diary_date, d.book_type, d.related_contact_id AS contact_id, "
                  "d.title, d.mood, d.content, c.name AS contact_name "
                  "FROM diaries d "
                  "LEFT JOIN contacts c ON d.related_contact_id = c.id WHERE 1=1 ";
    if (!bookType.isEmpty()) {
        sql += "AND d.book_type = '" + bookType + "' ";
    }
    sql += QString("ORDER BY d.diary_date DESC, d.id DESC LIMIT %1;").arg(limit);

    QSqlQuery q(sql, m_db);
    while (q.next()) {
        QVariantMap row;
        row["id"] = q.value("id");
        row["diary_date"] = q.value("diary_date");
        row["book_type"] = q.value("book_type");
        row["contact_id"] = q.value("contact_id");
        row["contact_name"] = q.value("contact_name");
        row["title"] = q.value("title");
        row["mood"] = q.value("mood");
        row["content"] = q.value("content");
        list.append(row);
    }
    return list;
}

QList<QVariantMap> DatabaseManager::getDiariesFiltered(const QString& bookType, int year, int month, int day, const QString& search) {
    QList<QVariantMap> list;
    QString filter = "WHERE 1=1 ";
    if (!bookType.isEmpty()) {
        filter += QString("AND d.book_type = '%1' ").arg(bookType);
    }
    if (year > 0) {
        filter += QString("AND strftime('%Y', d.diary_date) = '%1' ").arg(year);
    }
    if (month > 0) {
        filter += QString("AND strftime('%m', d.diary_date) = '%1' ").arg(month, 2, 10, QChar('0'));
    }
    if (day > 0) {
        filter += QString("AND strftime('%d', d.diary_date) = '%1' ").arg(day, 2, 10, QChar('0'));
    }
    if (!search.trimmed().isEmpty()) {
        QString s = search.trimmed();
        filter += QString("AND (d.content LIKE '%%1%' OR d.title LIKE '%%1%' OR d.mood LIKE '%%1%' OR c.name LIKE '%%1%') ").arg(s);
    }

    QString sql = QString("SELECT d.id, d.diary_date, d.book_type, d.related_contact_id AS contact_id, "
                          "d.title, d.mood, d.content, c.name AS contact_name "
                          "FROM diaries d "
                          "LEFT JOIN contacts c ON d.related_contact_id = c.id "
                          "%1 ORDER BY d.diary_date DESC, d.id DESC;").arg(filter);
    QSqlQuery q(sql, m_db);
    while (q.next()) {
        QVariantMap row;
        row["id"] = q.value("id");
        row["diary_date"] = q.value("diary_date");
        row["book_type"] = q.value("book_type");
        row["contact_id"] = q.value("contact_id");
        row["contact_name"] = q.value("contact_name");
        row["title"] = q.value("title");
        row["mood"] = q.value("mood");
        row["content"] = q.value("content");
        list.append(row);
    }
    return list;
}

bool DatabaseManager::addDiary(const QString& dateStr, const QString& content, const QString& mood,
                               const QString& bookType, int contactId) {
    if (content.trimmed().isEmpty()) return false;
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO diaries (diary_date, book_type, related_contact_id, mood, content) "
              "VALUES (:dt, :bt, :cid, :mood, :cnt);");
    q.bindValue(":dt", dateStr.isEmpty() ? QDate::currentDate().toString("yyyy-MM-dd") : dateStr);
    q.bindValue(":bt", bookType.isEmpty() ? "PERSONAL" : bookType);
    if (contactId > 0) q.bindValue(":cid", contactId);
    else q.bindValue(":cid", QVariant());
    q.bindValue(":mood", mood.isEmpty() ? "平静" : mood);
    q.bindValue(":cnt", content.trimmed());
    return q.exec();
}

QList<QVariantMap> DatabaseManager::getRelationshipEdges() {
    QList<QVariantMap> list;
    QSqlQuery q("SELECT r.id, r.from_contact_id, r.to_contact_id, r.relation_type, r.description, "
                "c1.name AS from_name, c2.name AS to_name "
                "FROM relationships r "
                "JOIN contacts c1 ON r.from_contact_id = c1.id "
                "JOIN contacts c2 ON r.to_contact_id = c2.id;", m_db);
    while (q.next()) {
        QVariantMap row;
        row["id"] = q.value("id");
        row["from_id"] = q.value("from_contact_id");
        row["to_id"] = q.value("to_contact_id");
        row["relation_type"] = q.value("relation_type");
        row["description"] = q.value("description");
        row["from_name"] = q.value("from_name");
        row["to_name"] = q.value("to_name");
        list.append(row);
    }
    return list;
}

QList<DebtItem> DatabaseManager::getDebts(const QString& directionFilter) {
    Q_UNUSED(directionFilter);
    return {};
}

bool DatabaseManager::addDebt(const QString& borrowerType, const QString& direction, int contactId,
                             const QString& institutionName, double principal, double interestRate,
                             const QString& repayType, const QDate& startDate, const QDate& dueDate,
                             int monthlyPayDay, double monthlyPayment, const QString& remark) {
    Q_UNUSED(borrowerType); Q_UNUSED(direction); Q_UNUSED(contactId);
    Q_UNUSED(institutionName); Q_UNUSED(principal); Q_UNUSED(interestRate);
    Q_UNUSED(repayType); Q_UNUSED(startDate); Q_UNUSED(dueDate);
    Q_UNUSED(monthlyPayDay); Q_UNUSED(monthlyPayment); Q_UNUSED(remark);
    return true;
}

QList<DebtItem> DatabaseManager::getUpcomingDebts(int daysAhead) {
    Q_UNUSED(daysAhead);
    return {};
}

QList<SocialPlanItem> DatabaseManager::getSocialPlans(const QString& status) {
    QList<SocialPlanItem> list;
    QString sql = "SELECT p.id, p.contact_id, IFNULL(p.scope, 'PERSONAL') AS scope, p.company_id, "
                  "p.plan_type, p.title, p.detail, p.planned_date, "
                  "p.remind_days_before, IFNULL(p.budget, 0.0) AS budget, p.status, "
                  "c.name AS contact_name, comp.name AS company_name "
                  "FROM social_plans p "
                  "LEFT JOIN contacts c ON p.contact_id = c.id "
                  "LEFT JOIN companies comp ON p.company_id = comp.id WHERE 1=1 ";
    if (!status.isEmpty()) {
        if (status == "PENDING") {
            sql += "AND p.status = 'PENDING' ";
        } else {
            sql += "AND p.status = '" + status + "' ";
        }
    }
    sql += "ORDER BY p.planned_date ASC;";

    QSqlQuery q(sql, m_db);
    while (q.next()) {
        SocialPlanItem item;
        item.id = q.value("id").toInt();
        item.contactId = q.value("contact_id").toInt();
        item.contactName = q.value("contact_name").toString();
        item.scope = q.value("scope").toString();
        item.companyId = q.value("company_id").toInt();
        item.companyName = q.value("company_name").toString();
        item.planType = q.value("plan_type").toString();
        item.title = q.value("title").toString();
        QString dtStr = q.value("planned_date").toString();
        item.plannedDate = QDate::fromString(dtStr.left(10), "yyyy-MM-dd");
        item.remindDaysBefore = q.value("remind_days_before").toInt();
        item.budget = q.value("budget").toDouble();
        item.status = q.value("status").toString();
        item.remark = q.value("detail").toString();
        list.append(item);
    }
    return list;
}

bool DatabaseManager::addSocialPlan(int contactId, const QString& planType, const QString& title,
                                   const QDate& plannedDate, int remindDaysBefore, const QString& remark,
                                   const QString& scope, int companyId, double budget) {
    QSqlQuery q(m_db);
    QString cleanScope = (scope == "COMPANY") ? "COMPANY" : "PERSONAL";
    QString cleanType = planType.trimmed().isEmpty() ? "请客吃饭" : planType.trimmed();

    q.prepare("INSERT INTO social_plans (contact_id, scope, company_id, plan_type, title, detail, planned_date, remind_days_before, budget, status) "
              "VALUES (:cid, :scope, :compId, :ptype, :title, :detail, :pdate, :remdays, :budget, 'PENDING');");
    if (contactId > 0) q.bindValue(":cid", contactId);
    else q.bindValue(":cid", QVariant());
    q.bindValue(":scope", cleanScope);
    if (companyId > 0) q.bindValue(":compId", companyId);
    else q.bindValue(":compId", QVariant());
    q.bindValue(":ptype", cleanType);
    q.bindValue(":title", title);
    q.bindValue(":detail", remark);
    q.bindValue(":pdate", plannedDate.toString("yyyy-MM-dd") + " 10:00:00");
    q.bindValue(":remdays", remindDaysBefore);
    q.bindValue(":budget", budget);

    return q.exec();
}

bool DatabaseManager::updateSocialPlanStatus(int planId, const QString& status) {
    QSqlQuery q(m_db);
    QString st = (status == "COMPLETED") ? "DONE" : status;
    q.prepare("UPDATE social_plans SET status = :st WHERE id = :id;");
    q.bindValue(":st", st);
    q.bindValue(":id", planId);
    return q.exec();
}

bool DatabaseManager::completeSocialPlanWithExpense(int planId, double expenseAmount, const QString& category,
                                                  int accountId, const QString& remark) {
    Q_UNUSED(accountId);
    m_db.transaction();
    QSqlQuery q(m_db);
    q.prepare("SELECT contact_id, title, IFNULL(scope, 'PERSONAL'), IFNULL(company_id, 0) FROM social_plans WHERE id = :id;");
    q.bindValue(":id", planId);
    if (!q.exec() || !q.next()) {
        m_db.rollback();
        return false;
    }
    int contactId = q.value(0).toInt();
    QString planTitle = q.value(1).toString();
    QString scope = q.value(2).toString();
    int companyId = q.value(3).toInt();

    QSqlQuery updatePlan(m_db);
    updatePlan.prepare("UPDATE social_plans SET status = 'DONE', detail = :det WHERE id = :id;");
    updatePlan.bindValue(":det", remark);
    updatePlan.bindValue(":id", planId);
    if (!updatePlan.exec()) {
        m_db.rollback();
        return false;
    }

    QString bookType = (scope == "COMPANY") ? "COMPANY" : "PERSONAL";
    QString parentCat = (bookType == "COMPANY") ? "商务社交" : "人脉社交";
    QString subCat = category.trimmed().isEmpty() ? "请客吃饭" : category.trimmed();

    if (expenseAmount > 0.0) {
        QString transRemark = remark.isEmpty() ? planTitle : QString("%1 (%2)").arg(planTitle, remark);
        if (!addTransaction(bookType, 0, "EXPENSE", expenseAmount,
                            subCat, QDateTime::currentDateTime(), contactId, transRemark,
                            parentCat, companyId)) {
            m_db.rollback();
            return false;
        }
    }

    if (contactId > 0) {
        QSqlQuery insEvent(m_db);
        insEvent.prepare("INSERT INTO social_events (contact_id, scope, company_id, io_type, event_category, title, detail, event_date, my_cost, their_value, status) "
                         "VALUES (:cid, :sc, :comp, 'EXPENSE', :cat, :title, :det, :dt, :cost, 0.0, 'CLOSED');");
        insEvent.bindValue(":cid", contactId);
        insEvent.bindValue(":sc", bookType == "COMPANY" ? "COMPANY" : "PRIVATE");
        if (companyId > 0) insEvent.bindValue(":comp", companyId);
        else insEvent.bindValue(":comp", QVariant());
        insEvent.bindValue(":cat", subCat);
        insEvent.bindValue(":title", planTitle);
        insEvent.bindValue(":det", remark);
        insEvent.bindValue(":dt", QDate::currentDate().toString("yyyy-MM-dd"));
        insEvent.bindValue(":cost", expenseAmount);
        insEvent.exec();
    }

    m_db.commit();
    return true;
}

QList<SocialPlanItem> DatabaseManager::getUpcomingSocialPlans(int daysAhead) {
    QList<SocialPlanItem> all = getSocialPlans("PENDING");
    QList<SocialPlanItem> upcoming;
    QDate today = QDate::currentDate();
    QDate limitDate = today.addDays(daysAhead);

    for (const auto& item : all) {
        if (item.plannedDate <= limitDate) {
            upcoming.append(item);
        }
    }
    return upcoming;
}

QList<AssetItem> DatabaseManager::getAssets() {
    QList<AssetItem> list;
    QString sql = "SELECT a.id, a.asset_type, a.name, a.current_valuation, a.owner_type, IFNULL(a.company_id, 0) AS comp_id, "
                  "IFNULL(comp.name, '') AS company_name, a.purchase_date, a.related_contact_id, a.description, "
                  "IFNULL(a.status, 'HOLDING') AS status, c.name AS contact_name "
                  "FROM assets a "
                  "LEFT JOIN contacts c ON a.related_contact_id = c.id "
                  "LEFT JOIN companies comp ON a.company_id = comp.id "
                  "WHERE a.status IS NULL OR a.status != 'SOLD' "
                  "ORDER BY a.current_valuation DESC;";

    QSqlQuery q(sql, m_db);
    while (q.next()) {
        AssetItem item;
        item.id = q.value("id").toInt();
        item.assetType = q.value("asset_type").toString();
        item.name = q.value("name").toString();
        item.estimatedValue = q.value("current_valuation").toDouble();
        item.ownership = q.value("owner_type").toString();
        item.companyId = q.value("comp_id").toInt();
        item.companyName = q.value("company_name").toString();
        item.purchaseDate = q.value("purchase_date").toDate();
        item.linkedContactId = q.value("related_contact_id").toInt();
        item.linkedContactName = q.value("contact_name").toString();
        item.linkedDebtId = 0;
        item.remark = q.value("description").toString();
        item.status = q.value("status").toString();
        list.append(item);
    }
    return list;
}

bool DatabaseManager::addAsset(const QString& assetType, const QString& name, double estimatedValue,
                               const QString& ownership, const QDate& purchaseDate, int linkedContactId,
                               int linkedDebtId, const QString& remark, int companyId) {
    m_db.transaction();
    QSqlQuery q(m_db);
    QString validOwner = (ownership == "COMPANY" || ownership == "WORK") ? "COMPANY" : "PERSONAL";
    QString validType = assetType.trimmed().isEmpty() ? "其他资产" : assetType.trimmed();

    int resolvedCompId = companyId;
    if (validOwner == "COMPANY" && resolvedCompId <= 0) {
        auto comps = getCompaniesList();
        if (!comps.isEmpty()) {
            resolvedCompId = comps.first()["id"].toInt();
        }
    }

    q.prepare("INSERT INTO assets (owner_type, company_id, asset_type, name, purchase_date, purchase_price, current_valuation, "
              "related_contact_id, description, status) "
              "VALUES (:owner, :comp, :atype, :name, :pdate, :price, :val, :cid, :rem, 'HOLDING');");
    q.bindValue(":owner", validOwner);
    if (validOwner == "COMPANY" && resolvedCompId > 0) q.bindValue(":comp", resolvedCompId);
    else q.bindValue(":comp", QVariant());
    q.bindValue(":atype", validType);
    q.bindValue(":name", name);
    QDate effDate = purchaseDate.isValid() ? purchaseDate : QDate::currentDate();
    q.bindValue(":pdate", effDate.toString("yyyy-MM-dd"));
    q.bindValue(":price", estimatedValue);
    q.bindValue(":val", estimatedValue);
    if (linkedContactId > 0) q.bindValue(":cid", linkedContactId);
    else q.bindValue(":cid", QVariant());
    q.bindValue(":rem", remark);

    if (!q.exec()) {
        qWarning() << "[DB] addAsset insert into assets failed:" << q.lastError().text();
        m_db.rollback();
        return false;
    }

    int assetId = q.lastInsertId().toInt();

    // 1. 添加“个人”资产 (PERSONAL) -> 除 assets 外，在 transactions 中同步写入一份个人支出数据 (大类: 资产购置)
    // 2. 添加“工作”资产 (COMPANY) -> 除 assets 外，在 transactions 中同步写入一份工作支出数据 (大类: 投资理财)
    QString parentCat = (validOwner == "COMPANY") ? "投资理财" : "资产购置";
    addCustomCategory("EXPENSE", parentCat, validType, validOwner);

    QString transTimeStr = QDateTime(effDate, QTime::currentTime()).toString("yyyy-MM-dd HH:mm:ss");
    QString transRemark = remark.trimmed().isEmpty()
                              ? QString("【资产购置】%1").arg(name)
                              : QString("【资产购置】%1 - %2").arg(name, remark.trimmed());

    QSqlQuery qt(m_db);
    qt.prepare("INSERT INTO transactions (book_type, company_id, account_id, io_type, category_parent, category_sub, "
               "amount, trans_date, contact_id, debt_id, asset_id, payment_method, remark, project_name) "
               "VALUES (:book, :comp, NULL, 'EXPENSE', :cparent, :csub, :amt, :tdate, :cid, :did, :aid, '银行转账', :rem, :proj);");
    qt.bindValue(":book", validOwner);
    if (validOwner == "COMPANY" && resolvedCompId > 0) qt.bindValue(":comp", resolvedCompId);
    else qt.bindValue(":comp", QVariant());
    qt.bindValue(":cparent", parentCat);
    qt.bindValue(":csub", validType);
    qt.bindValue(":amt", estimatedValue);
    qt.bindValue(":tdate", transTimeStr);
    if (linkedContactId > 0) qt.bindValue(":cid", linkedContactId);
    else qt.bindValue(":cid", QVariant());
    if (linkedDebtId > 0) qt.bindValue(":did", linkedDebtId);
    else qt.bindValue(":did", QVariant());
    if (assetId > 0) qt.bindValue(":aid", assetId);
    else qt.bindValue(":aid", QVariant());
    qt.bindValue(":rem", transRemark);
    qt.bindValue(":proj", name);

    if (!qt.exec()) {
        qWarning() << "[DB] addAsset insert into transactions failed:" << qt.lastError().text();
        m_db.rollback();
        return false;
    }

    m_db.commit();
    return true;
}

bool DatabaseManager::updateAsset(int assetId, const QString& assetType, const QString& name,
                                  double estimatedValue, const QString& ownership, int companyId, const QString& remark) {
    QSqlQuery q(m_db);
    QString validOwner = (ownership == "COMPANY") ? "COMPANY" : "PERSONAL";
    QString validType = assetType.trimmed().isEmpty() ? "其他资产" : assetType.trimmed();

    q.prepare("UPDATE assets SET owner_type = :owner, company_id = :comp, asset_type = :atype, "
              "name = :name, current_valuation = :val, description = :rem WHERE id = :id;");
    q.bindValue(":owner", validOwner);
    if (companyId > 0) q.bindValue(":comp", companyId);
    else q.bindValue(":comp", QVariant());
    q.bindValue(":atype", validType);
    q.bindValue(":name", name);
    q.bindValue(":val", estimatedValue);
    q.bindValue(":rem", remark);
    q.bindValue(":id", assetId);
    return q.exec();
}

bool DatabaseManager::sellAsset(int assetId, const QString& assetType, const QString& name,
                                double salePrice, const QString& ownership, int companyId, const QString& remark) {
    m_db.transaction();
    QString validOwner = (ownership == "COMPANY") ? "COMPANY" : "PERSONAL";
    QString validType = assetType.trimmed().isEmpty() ? "资产变现" : assetType.trimmed();

    QSqlQuery q(m_db);
    q.prepare("UPDATE assets SET owner_type = :owner, company_id = :comp, asset_type = :atype, "
              "name = :name, current_valuation = :val, description = :rem, status = 'SOLD' WHERE id = :id;");
    q.bindValue(":owner", validOwner);
    if (companyId > 0) q.bindValue(":comp", companyId);
    else q.bindValue(":comp", QVariant());
    q.bindValue(":atype", validType);
    q.bindValue(":name", name);
    q.bindValue(":val", salePrice);
    q.bindValue(":rem", remark);
    q.bindValue(":id", assetId);

    if (!q.exec()) {
        m_db.rollback();
        return false;
    }

    // 个人资产出售 -> 个人收入 (大类: 资产变现)
    // 工作资产出售 -> 工作收入 (大类: 投资理财)
    QString parentCat = (validOwner == "COMPANY") ? "投资理财" : "资产变现";
    QString subCat = validType;
    addCustomCategory("INCOME", parentCat, subCat, validOwner);
    addCustomCategory("EXPENSE", parentCat, subCat, validOwner);

    QString transRemark = QString("【资产出售变现】%1%2").arg(name, remark.isEmpty() ? "" : (" - " + remark));
    if (!addTransaction(validOwner, 0, "INCOME", salePrice, subCat, QDateTime::currentDateTime(), 0, transRemark, parentCat, companyId)) {
        m_db.rollback();
        return false;
    }

    m_db.commit();
    return true;
}

QString DatabaseManager::getSecurityConfig(const QString& key) {
    QSqlQuery q(m_db);
    q.prepare("SELECT value FROM security_config WHERE key = :k;");
    q.bindValue(":k", key);
    if (q.exec() && q.next()) {
        return q.value(0).toString();
    }
    return "";
}

bool DatabaseManager::setSecurityConfig(const QString& key, const QString& value) {
    QSqlQuery q(m_db);
    q.prepare("INSERT OR REPLACE INTO security_config (key, value, updated_at) VALUES (:k, :v, CURRENT_TIMESTAMP);");
    q.bindValue(":k", key);
    q.bindValue(":v", value);
    bool ok = q.exec();
    if (ok && m_isUnlocked) {
        syncEncryptedDatabaseToDisk();
    }
    return ok;
}

bool DatabaseManager::exportDatabaseBackup(const QString& destinationPath) {
    if (m_isUnlocked) {
        syncEncryptedDatabaseToDisk();
    }

    if (QFile::exists(destinationPath)) {
        QFile::remove(destinationPath);
    }
    return QFile::copy(m_dbPath, destinationPath);
}

LedgerMetrics DatabaseManager::getPeriodMetrics(const QString& bookType, int year, int month, int day) {
    LedgerMetrics m;
    QSqlQuery q(m_db);

    QString filter = "WHERE 1=1 ";
    if (!bookType.isEmpty()) {
        filter += QString("AND (book_type = '%1' OR book_type = 'BOTH') ").arg(bookType);
    }
    if (year > 0) {
        filter += QString("AND strftime('%Y', trans_date) = '%1' ").arg(year);
    }
    if (month > 0) {
        filter += QString("AND strftime('%m', trans_date) = '%1' ").arg(month, 2, 10, QChar('0'));
    }
    if (day > 0) {
        filter += QString("AND strftime('%d', trans_date) = '%1' ").arg(day, 2, 10, QChar('0'));
    }

    // 1. 周期内账面全量收入 (包含借贷进账，用户指示：“总收入 的账单里 包含 借来的钱”)
    q.exec(QString("SELECT IFNULL(SUM(amount), 0) FROM transactions %1 AND io_type = 'INCOME';").arg(filter));
    if (q.next()) m.income = q.value(0).toDouble();

    // 2. 周期内账面全量支出
    q.exec(QString("SELECT IFNULL(SUM(amount), 0) FROM transactions %1 AND io_type = 'EXPENSE';").arg(filter));
    if (q.next()) m.expense = q.value(0).toDouble();

    // 3. 盈收
    m.surplus = m.income - m.expense;

    // 4. 资产负债存量指标：应收与欠款 (反映截至该周期节点或总账的实际存量余额)
    QString dateCutoff = "";
    QString dateCutoffT = "";
    if (year > 0) {
        if (month > 0) {
            if (day > 0) {
                QString dtStr = QString("%1-%2-%3 23:59:59").arg(year).arg(month, 2, 10, QChar('0')).arg(day, 2, 10, QChar('0'));
                dateCutoff = QString("AND trans_date <= '%1' ").arg(dtStr);
                dateCutoffT = QString("AND t.trans_date <= '%1' ").arg(dtStr);
            } else {
                QDate lastDay(year, month, QDate(year, month, 1).daysInMonth());
                dateCutoff = QString("AND trans_date <= '%1 23:59:59' ").arg(lastDay.toString("yyyy-MM-dd"));
                dateCutoffT = QString("AND t.trans_date <= '%1 23:59:59' ").arg(lastDay.toString("yyyy-MM-dd"));
            }
        } else {
            dateCutoff = QString("AND trans_date <= '%1-12-31 23:59:59' ").arg(year);
            dateCutoffT = QString("AND t.trans_date <= '%1-12-31 23:59:59' ").arg(year);
        }
    }

    if (bookType == "COMPANY") {
        // 公司应收: 按项目/联系人归集的未结算款 (规则4: 押抵借贷'借出'是应收；规则5: 业务经营'进度合同'、'账期合同'是应收；规则9: 投资理财'收回'是还款冲减)
        q.exec(QString(
            "SELECT IFNULL(SUM(CASE WHEN net_rec > 0 THEN net_rec ELSE 0 END), 0) FROM ("
            "    SELECT t.contact_id, "
            "           SUM(CASE WHEN t.io_type = 'EXPENSE' AND (t.category_sub IN ('进度合同', '账期合同', '借出') OR (t.category_parent = '业务经营' AND t.category_sub LIKE '%合同%') OR (t.category_parent IN ('押抵借贷', '抵押借贷', '借贷') AND t.category_sub = '借出')) THEN t.amount ELSE 0 END) - "
            "           SUM(CASE WHEN t.io_type = 'INCOME' AND ((t.category_parent IN ('业务经营', '投资理财', '押抵借贷', '抵押借贷') AND t.category_sub IN ('收回', '赎回', '尾款', '项目结算')) OR t.category_sub IN ('收回', '尾款', '项目结算')) "
            "                    AND EXISTS (SELECT 1 FROM transactions t2 WHERE (t2.book_type = 'COMPANY' OR t2.book_type = 'BOTH') AND t2.io_type = 'EXPENSE' AND (t2.contact_id = t.contact_id OR (t2.contact_id IS NULL AND t.contact_id IS NULL)) AND (t2.category_sub IN ('进度合同', '账期合同', '借出') OR (t2.category_parent = '业务经营' AND t2.category_sub LIKE '%合同%') OR (t2.category_parent IN ('押抵借贷', '抵押借贷', '借贷') AND t2.category_sub = '借出')) AND t2.trans_date <= t.trans_date %1) "
            "                    THEN t.amount ELSE 0 END) AS net_rec "
            "    FROM transactions t "
            "    WHERE (t.book_type = 'COMPANY' OR t.book_type = 'BOTH') %1 "
            "    GROUP BY t.contact_id "
            ");"
        ).arg(dateCutoffT));
        if (q.next()) m.receivable = qMax(0.0, q.value(0).toDouble());

        // 公司欠款/负债 (规则3: 支出->工作->押抵借贷'还贷'、'还息'全部是还款)
        q.exec(QString(
            "SELECT "
            "(SELECT IFNULL(SUM(amount), 0) FROM transactions WHERE (book_type = 'COMPANY' OR book_type = 'BOTH') AND io_type = 'INCOME' %1 "
            " AND category_parent IN ('押抵借贷', '抵押借贷', '借贷') AND (category_sub IN ('借入', '贷款', '抵押', '贴现', '拆借') OR category_sub LIKE '%借入%' OR category_sub LIKE '%贷款%')) - "
            "(SELECT IFNULL(SUM(amount), 0) FROM transactions WHERE (book_type = 'COMPANY' OR book_type = 'BOTH') AND io_type = 'EXPENSE' %1 "
            " AND ((category_parent IN ('押抵借贷', '抵押借贷', '借贷') AND (category_sub IN ('还贷', '还息', '偿还', '还个人') OR category_sub LIKE '%还%')) "
            "      OR category_sub IN ('还贷', '还息')));"
        ).arg(dateCutoff));
        if (q.next()) m.debt = qMax(0.0, q.value(0).toDouble());
    } else if (bookType == "PERSONAL") {
        // 个人应收 (规则2: 支出->个人->押抵借贷'借个人'是应收；规则8: 收入->个人->押抵借贷'收回'是还款冲减)
        q.exec(QString(
            "SELECT "
            "(SELECT IFNULL(SUM(amount), 0) FROM transactions WHERE (book_type = 'PERSONAL' OR book_type = 'BOTH') AND io_type = 'EXPENSE' %1 "
            " AND (category_sub IN ('借个人', '借出', '拆借') "
            "      OR (category_parent IN ('押抵借贷', '抵押借贷', '借贷') AND (category_sub IN ('借个人', '借出', '拆借') OR category_sub LIKE '%借%')))) - "
            "(SELECT IFNULL(SUM(amount), 0) FROM transactions WHERE (book_type = 'PERSONAL' OR book_type = 'BOTH') AND io_type = 'INCOME' %1 "
            " AND (category_sub IN ('收回', '赎回') "
            "      OR (category_parent IN ('押抵借贷', '抵押借贷', '借贷', '投资理财', '资产管理') AND (category_sub IN ('收回', '赎回') OR category_sub LIKE '%收回%'))));"
        ).arg(dateCutoff));
        if (q.next()) m.receivable = qMax(0.0, q.value(0).toDouble());

        // 个人欠款/负债 (规则1: 还网贷/还银行/还个人是还款；规则6: 借入是负债；规则7: 认领是负债；严禁扣减'借个人')
        q.exec(QString(
            "SELECT "
            "(SELECT IFNULL(SUM(amount), 0) FROM transactions WHERE (book_type = 'PERSONAL' OR book_type = 'BOTH') AND io_type = 'INCOME' %1 "
            " AND (category_sub IN ('借入', '认领') OR (category_parent IN ('押抵借贷', '抵押借贷', '借贷') AND (category_sub IN ('借入', '认领', '贷款', '抵押') OR category_sub LIKE '%借入%' OR category_sub LIKE '%认领%')))) - "
            "(SELECT IFNULL(SUM(amount), 0) FROM transactions WHERE (book_type = 'PERSONAL' OR book_type = 'BOTH') AND io_type = 'EXPENSE' %1 "
            " AND (category_sub IN ('还网贷', '还银行', '还个人') OR category_parent IN ('还网贷', '还银行', '还个人') "
            "      OR (category_parent IN ('押抵借贷', '抵押借贷', '借贷') AND (category_sub IN ('还网贷', '还银行', '还个人', '还贷', '还息', '偿还') OR category_sub LIKE '%还%'))));"
        ).arg(dateCutoff));
        if (q.next()) m.debt = qMax(0.0, q.value(0).toDouble());
    } else {
        // 双账本/全部
        // 应收 = 个人应收 + 公司应收
        double pRec = 0.0, cRec = 0.0;
        q.exec(QString(
            "SELECT "
            "(SELECT IFNULL(SUM(amount), 0) FROM transactions WHERE (book_type = 'PERSONAL' OR book_type = 'BOTH') AND io_type = 'EXPENSE' %1 "
            " AND (category_sub IN ('借个人', '借出', '拆借') "
            "      OR (category_parent IN ('押抵借贷', '抵押借贷', '借贷') AND (category_sub IN ('借个人', '借出', '拆借') OR category_sub LIKE '%借%')))) - "
            "(SELECT IFNULL(SUM(amount), 0) FROM transactions WHERE (book_type = 'PERSONAL' OR book_type = 'BOTH') AND io_type = 'INCOME' %1 "
            " AND (category_sub IN ('收回', '赎回') "
            "      OR (category_parent IN ('押抵借贷', '抵押借贷', '借贷', '投资理财', '资产管理') AND (category_sub IN ('收回', '赎回') OR category_sub LIKE '%收回%'))));"
        ).arg(dateCutoff));
        if (q.next()) pRec = qMax(0.0, q.value(0).toDouble());

        q.exec(QString(
            "SELECT IFNULL(SUM(CASE WHEN net_rec > 0 THEN net_rec ELSE 0 END), 0) FROM ("
            "    SELECT t.contact_id, "
            "           SUM(CASE WHEN t.io_type = 'EXPENSE' AND (t.category_sub IN ('进度合同', '账期合同', '借出') OR (t.category_parent = '业务经营' AND t.category_sub LIKE '%合同%') OR (t.category_parent IN ('押抵借贷', '抵押借贷', '借贷') AND t.category_sub = '借出')) THEN t.amount ELSE 0 END) - "
            "           SUM(CASE WHEN t.io_type = 'INCOME' AND ((t.category_parent IN ('业务经营', '投资理财', '押抵借贷', '抵押借贷') AND t.category_sub IN ('收回', '赎回', '尾款', '项目结算')) OR t.category_sub IN ('收回', '尾款', '项目结算')) "
            "                    AND EXISTS (SELECT 1 FROM transactions t2 WHERE (t2.book_type = 'COMPANY' OR t2.book_type = 'BOTH') AND t2.io_type = 'EXPENSE' AND (t2.contact_id = t.contact_id OR (t2.contact_id IS NULL AND t.contact_id IS NULL)) AND (t2.category_sub IN ('进度合同', '账期合同', '借出') OR (t2.category_parent = '业务经营' AND t2.category_sub LIKE '%合同%') OR (t2.category_parent IN ('押抵借贷', '抵押借贷', '借贷') AND t2.category_sub = '借出')) AND t2.trans_date <= t.trans_date %1) "
            "                    THEN t.amount ELSE 0 END) AS net_rec "
            "    FROM transactions t "
            "    WHERE (t.book_type = 'COMPANY' OR t.book_type = 'BOTH') %1 "
            "    GROUP BY t.contact_id "
            ");"
        ).arg(dateCutoffT));
        if (q.next()) cRec = qMax(0.0, q.value(0).toDouble());

        m.receivable = pRec + cRec;

        // 欠款 = 个人欠款 + 公司欠款
        double pDebt = 0.0, cDebt = 0.0;
        q.exec(QString(
            "SELECT "
            "(SELECT IFNULL(SUM(amount), 0) FROM transactions WHERE (book_type = 'PERSONAL' OR book_type = 'BOTH') AND io_type = 'INCOME' %1 "
            " AND (category_sub IN ('借入', '认领') OR (category_parent IN ('押抵借贷', '抵押借贷', '借贷') AND (category_sub IN ('借入', '认领', '贷款', '抵押') OR category_sub LIKE '%借入%' OR category_sub LIKE '%认领%')))) - "
            "(SELECT IFNULL(SUM(amount), 0) FROM transactions WHERE (book_type = 'PERSONAL' OR book_type = 'BOTH') AND io_type = 'EXPENSE' %1 "
            " AND (category_sub IN ('还网贷', '还银行', '还个人') OR category_parent IN ('还网贷', '还银行', '还个人') "
            "      OR (category_parent IN ('押抵借贷', '抵押借贷', '借贷') AND (category_sub IN ('还网贷', '还银行', '还个人', '还贷', '还息', '偿还') OR category_sub LIKE '%还%'))));"
        ).arg(dateCutoff));
        if (q.next()) pDebt = qMax(0.0, q.value(0).toDouble());

        q.exec(QString(
            "SELECT "
            "(SELECT IFNULL(SUM(amount), 0) FROM transactions WHERE (book_type = 'COMPANY' OR book_type = 'BOTH') AND io_type = 'INCOME' %1 "
            " AND category_parent IN ('押抵借贷', '抵押借贷', '借贷') AND (category_sub IN ('借入', '贷款', '抵押', '贴现', '拆借') OR category_sub LIKE '%借入%' OR category_sub LIKE '%贷款%')) - "
            "(SELECT IFNULL(SUM(amount), 0) FROM transactions WHERE (book_type = 'COMPANY' OR book_type = 'BOTH') AND io_type = 'EXPENSE' %1 "
            " AND ((category_parent IN ('押抵借贷', '抵押借贷', '借贷') AND (category_sub IN ('还贷', '还息', '偿还', '还个人') OR category_sub LIKE '%还%')) "
            "      OR category_sub IN ('还贷', '还息')));"
        ).arg(dateCutoff));
        if (q.next()) cDebt = qMax(0.0, q.value(0).toDouble());

        m.debt = pDebt + cDebt;
    }

    q.exec(QString("SELECT COUNT(DISTINCT contact_id) FROM transactions %1 AND contact_id IS NOT NULL AND contact_id > 0;").arg(filter));
    if (q.next()) m.friendCount = q.value(0).toInt();

    return m;
}

QList<QVariantMap> DatabaseManager::getLedgerTransactions(const QString& bookType, int year, int month, int day, const QString& search) {
    QList<QVariantMap> list;
    QString filter = "WHERE 1=1 ";
    if (!bookType.isEmpty()) {
        filter += QString("AND (t.book_type = '%1' OR t.book_type = 'BOTH') ").arg(bookType);
    }
    if (year > 0) {
        filter += QString("AND strftime('%Y', t.trans_date) = '%1' ").arg(year);
    }
    if (month > 0) {
        filter += QString("AND strftime('%m', t.trans_date) = '%1' ").arg(month, 2, 10, QChar('0'));
    }
    if (day > 0) {
        filter += QString("AND strftime('%d', t.trans_date) = '%1' ").arg(day, 2, 10, QChar('0'));
    }
    if (!search.trimmed().isEmpty()) {
        QString s = search.trimmed();
        filter += QString("AND (t.category_parent LIKE '%%1%' OR t.category_sub LIKE '%%1%' OR t.project_name LIKE '%%1%' OR t.remark LIKE '%%1%' OR c.name LIKE '%%1%') ").arg(s);
    }

    QString sql = QString("SELECT t.id, t.book_type, t.io_type AS trans_type, t.category_sub AS sub_type, "
                          "t.amount, t.category_parent AS category, t.trans_date AS trans_time, "
                          "t.project_name, t.remark, t.voucher_path, "
                          "c.name AS contact_name, comp.name AS company_name "
                          "FROM transactions t "
                          "LEFT JOIN contacts c ON t.contact_id = c.id "
                          "LEFT JOIN companies comp ON t.company_id = comp.id "
                          "%1 ORDER BY t.trans_date DESC, t.id DESC;").arg(filter);

    QSqlQuery q(sql, m_db);
    while (q.next()) {
        QVariantMap row;
        row["id"] = q.value("id");
        row["book_type"] = q.value("book_type");
        row["trans_type"] = q.value("trans_type");
        row["sub_type"] = q.value("sub_type");
        row["amount"] = q.value("amount");
        row["category"] = q.value("category");
        row["trans_time"] = q.value("trans_time");
        row["project_name"] = q.value("project_name");
        row["remark"] = q.value("remark");
        row["voucher_path"] = q.value("voucher_path");
        row["contact_name"] = q.value("contact_name");
        row["company_name"] = q.value("company_name");
        list.append(row);
    }
    return list;
}

bool DatabaseManager::selfDestructDatabase() {
    close();
    shredAndRemoveFile(m_dbPath);
    shredAndRemoveFile(m_runtimeDbPath);
    cleanupStaleRuntimeFiles();
    QFile::remove(m_dbPath + "-wal");
    QFile::remove(m_dbPath + "-shm");
    QFile::remove(m_dbPath + "-journal");
    return true;
}

bool DatabaseManager::deleteContact(int contactId) {
    if (contactId <= 0) return false;

    // 1. 删除所有双向关系网关联
    QSqlQuery qRel(m_db);
    qRel.prepare("DELETE FROM relationships WHERE from_contact_id = :id OR to_contact_id = :id;");
    qRel.bindValue(":id", contactId);
    qRel.exec();

    // 2. 删除亲疏轨迹
    QSqlQuery qTl(m_db);
    qTl.prepare("DELETE FROM relationship_timeline WHERE contact_id = :id;");
    qTl.bindValue(":id", contactId);
    qTl.exec();

    // 3. 删除礼尚往来
    QSqlQuery qEv(m_db);
    qEv.prepare("DELETE FROM social_events WHERE contact_id = :id;");
    qEv.bindValue(":id", contactId);
    qEv.exec();

    // 4. 删除走动计划
    QSqlQuery qPl(m_db);
    qPl.prepare("DELETE FROM social_plans WHERE contact_id = :id;");
    qPl.bindValue(":id", contactId);
    qPl.exec();

    // 5. 删除与该联系人绑定的所有业务往来及收支流水
    QSqlQuery qTr(m_db);
    qTr.prepare("DELETE FROM transactions WHERE contact_id = :id;");
    qTr.bindValue(":id", contactId);
    qTr.exec();

    // 6. 删除借贷往来
    QSqlQuery qDb(m_db);
    qDb.prepare("DELETE FROM debts WHERE contact_id = :id;");
    qDb.bindValue(":id", contactId);
    qDb.exec();

    // 7. 删除与该联系人关联的日记
    QSqlQuery qDi(m_db);
    qDi.prepare("DELETE FROM diaries WHERE related_contact_id = :id;");
    qDi.bindValue(":id", contactId);
    qDi.exec();

    // 8. 引荐人关联置空
    QSqlQuery qIntro(m_db);
    qIntro.prepare("UPDATE contacts SET introducer_id = 0 WHERE introducer_id = :id;");
    qIntro.bindValue(":id", contactId);
    qIntro.exec();

    // 9. 资产关联置空
    QSqlQuery qAss(m_db);
    qAss.prepare("UPDATE assets SET related_contact_id = NULL WHERE related_contact_id = :id;");
    qAss.bindValue(":id", contactId);
    qAss.exec();

    // 10. 删除联系人主表
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM contacts WHERE id = :id;");
    q.bindValue(":id", contactId);
    return q.exec();
}

bool DatabaseManager::deleteTransaction(int transactionId) {
    if (transactionId <= 0) return false;
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM transactions WHERE id = :id;");
    q.bindValue(":id", transactionId);
    return q.exec();
}

bool DatabaseManager::updateTransaction(int id, const QString& bookType, int accountId, const QString& transType,
                                       double amount, const QString& category, const QDateTime& transTime,
                                       int contactId, const QString& remark, const QString& parentCategory,
                                       int companyId, const QString& voucherPath) {
    Q_UNUSED(accountId);
    if (id <= 0) return false;
    QSqlQuery q(m_db);
    q.prepare("UPDATE transactions SET book_type = :bt, io_type = :tt, amount = :amt, "
              "category_sub = :cat, category_parent = :pcat, trans_date = :dt, "
              "contact_id = :cid, company_id = :comp, remark = :rem, voucher_path = :vouch "
              "WHERE id = :id;");
    q.bindValue(":bt", bookType.isEmpty() ? "PERSONAL" : bookType);
    q.bindValue(":tt", transType);
    q.bindValue(":amt", amount);
    q.bindValue(":cat", category);
    q.bindValue(":pcat", parentCategory.isEmpty() ? category : parentCategory);
    q.bindValue(":dt", transTime.toString("yyyy-MM-dd HH:mm:ss"));
    if (contactId > 0) q.bindValue(":cid", contactId);
    else q.bindValue(":cid", QVariant());
    if (companyId > 0) q.bindValue(":comp", companyId);
    else q.bindValue(":comp", QVariant());
    q.bindValue(":rem", remark);
    q.bindValue(":vouch", voucherPath);
    q.bindValue(":id", id);
    return q.exec();
}

QVariantMap DatabaseManager::getTransactionById(int id) {
    QVariantMap res;
    if (id <= 0) return res;
    QSqlQuery q(m_db);
    q.prepare("SELECT t.*, c.name AS contact_name, comp.name AS company_name "
              "FROM transactions t "
              "LEFT JOIN contacts c ON t.contact_id = c.id "
              "LEFT JOIN companies comp ON t.company_id = comp.id "
              "WHERE t.id = :id;");
    q.bindValue(":id", id);
    if (q.exec() && q.next()) {
        res["id"] = q.value("id");
        res["book_type"] = q.value("book_type");
        res["trans_type"] = q.value("io_type");
        res["amount"] = q.value("amount");
        res["category_sub"] = q.value("category_sub");
        res["category_parent"] = q.value("category_parent");
        res["trans_time"] = q.value("trans_date");
        res["contact_id"] = q.value("contact_id");
        res["contact_name"] = q.value("contact_name");
        res["company_id"] = q.value("company_id");
        res["company_name"] = q.value("company_name");
        res["remark"] = q.value("remark");
        res["voucher_path"] = q.value("voucher_path");
    }
    return res;
}

QList<QVariantMap> DatabaseManager::getRelationshipsForContact(int contactId) {
    QList<QVariantMap> list;
    if (contactId <= 0) return list;
    QSqlQuery q(m_db);
    q.prepare("SELECT r.id, r.from_contact_id, r.to_contact_id, r.relation_type, r.description, "
              "c1.name AS from_name, c2.name AS to_name "
              "FROM relationships r "
              "JOIN contacts c1 ON r.from_contact_id = c1.id "
              "JOIN contacts c2 ON r.to_contact_id = c2.id "
              "WHERE r.from_contact_id = :cid OR r.to_contact_id = :cid "
              "ORDER BY r.id DESC;");
    q.bindValue(":cid", contactId);
    if (q.exec()) {
        while (q.next()) {
            QVariantMap row;
            int fId = q.value("from_contact_id").toInt();
            int tId = q.value("to_contact_id").toInt();
            row["id"] = q.value("id");
            row["relation_type"] = q.value("relation_type");
            row["description"] = q.value("description");
            if (fId == contactId) {
                row["other_id"] = tId;
                row["other_name"] = q.value("to_name");
            } else {
                row["other_id"] = fId;
                row["other_name"] = q.value("from_name");
            }
            list.append(row);
        }
    }
    return list;
}

bool DatabaseManager::updateRelationship(int id, const QString& relationType, const QString& description) {
    if (id <= 0) return false;
    QSqlQuery q(m_db);
    q.prepare("UPDATE relationships SET relation_type = :r, description = :desc WHERE id = :id;");
    q.bindValue(":r", relationType);
    q.bindValue(":desc", description);
    q.bindValue(":id", id);
    return q.exec();
}

bool DatabaseManager::deleteRelationship(int id) {
    if (id <= 0) return false;
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM relationships WHERE id = :id;");
    q.bindValue(":id", id);
    return q.exec();
}

bool DatabaseManager::updateTimelineNode(int id, const QString& stageName, const QDate& eventDate,
                                        const QString& description, int ratingAfter) {
    if (id <= 0) return false;
    QSqlQuery q(m_db);
    q.prepare("UPDATE relationship_timeline SET stage_name = :stg, event_date = :dt, "
              "description = :desc, rating_after = :rt WHERE id = :id;");
    q.bindValue(":stg", stageName);
    q.bindValue(":dt", eventDate.toString("yyyy-MM-dd"));
    q.bindValue(":desc", description);
    q.bindValue(":rt", ratingAfter);
    q.bindValue(":id", id);
    return q.exec();
}

bool DatabaseManager::deleteTimelineNode(int id) {
    if (id <= 0) return false;
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM relationship_timeline WHERE id = :id;");
    q.bindValue(":id", id);
    return q.exec();
}

bool DatabaseManager::updateSocialEvent(int id, const QString& category, const QString& title,
                                       double myCost, double theirValue, const QString& detail, const QDate& eventDate) {
    if (id <= 0) return false;
    QSqlQuery q(m_db);
    q.prepare("UPDATE social_events SET event_category = :cat, title = :title, my_cost = :cost, "
              "their_value = :val, detail = :det, event_date = :dt WHERE id = :id;");
    q.bindValue(":cat", category);
    q.bindValue(":title", title);
    q.bindValue(":cost", myCost);
    q.bindValue(":val", theirValue);
    q.bindValue(":det", detail);
    q.bindValue(":dt", eventDate.toString("yyyy-MM-dd"));
    q.bindValue(":id", id);
    return q.exec();
}

bool DatabaseManager::deleteSocialEvent(int id) {
    if (id <= 0) return false;
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM social_events WHERE id = :id;");
    q.bindValue(":id", id);
    return q.exec();
}

bool DatabaseManager::updateSocialPlan(int id, const QString& planType, const QString& title,
                                      double budget, const QString& detail, const QDate& plannedDate,
                                      int remindDaysBefore) {
    if (id <= 0) return false;
    QSqlQuery q(m_db);
    q.prepare("UPDATE social_plans SET plan_type = :pt, title = :title, budget = :bdg, "
              "detail = :det, planned_date = :dt, remind_days_before = :rmd WHERE id = :id;");
    q.bindValue(":pt", planType);
    q.bindValue(":title", title);
    q.bindValue(":bdg", budget);
    q.bindValue(":det", detail);
    q.bindValue(":dt", plannedDate.toString("yyyy-MM-dd"));
    q.bindValue(":rmd", remindDaysBefore);
    q.bindValue(":id", id);
    return q.exec();
}

bool DatabaseManager::deleteSocialPlan(int id) {
    if (id <= 0) return false;
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM social_plans WHERE id = :id;");
    q.bindValue(":id", id);
    return q.exec();
}

bool DatabaseManager::updateDiary(int id, const QString& bookType, const QDate& diaryDate,
                                 const QString& mood, int contactId, const QString& content) {
    if (id <= 0) return false;
    QSqlQuery q(m_db);
    q.prepare("UPDATE diaries SET book_type = :bt, diary_date = :dt, mood = :mood, "
              "related_contact_id = :cid, content = :cnt WHERE id = :id;");
    q.bindValue(":bt", bookType.isEmpty() ? "PERSONAL" : bookType);
    q.bindValue(":dt", diaryDate.toString("yyyy-MM-dd"));
    q.bindValue(":mood", mood.isEmpty() ? "平静" : mood);
    if (contactId > 0) q.bindValue(":cid", contactId);
    else q.bindValue(":cid", QVariant());
    q.bindValue(":cnt", content.trimmed());
    q.bindValue(":id", id);
    return q.exec();
}

bool DatabaseManager::deleteDiary(int id) {
    if (id <= 0) return false;
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM diaries WHERE id = :id;");
    q.bindValue(":id", id);
    return q.exec();
}

QVariantMap DatabaseManager::getSocialEventById(int id) {
    QVariantMap map;
    if (id <= 0) return map;
    QSqlQuery q(m_db);
    q.prepare("SELECT id, contact_id, scope, company_id, io_type, event_category, title, detail, event_date, my_cost, their_value, status "
              "FROM social_events WHERE id = :id LIMIT 1;");
    q.bindValue(":id", id);
    if (q.exec() && q.next()) {
        map["id"] = q.value("id");
        map["contact_id"] = q.value("contact_id");
        map["scope"] = q.value("scope");
        map["company_id"] = q.value("company_id");
        map["io_type"] = q.value("io_type");
        map["event_category"] = q.value("event_category");
        map["title"] = q.value("title");
        map["detail"] = q.value("detail");
        map["event_date"] = q.value("event_date");
        map["my_cost"] = q.value("my_cost");
        map["their_value"] = q.value("their_value");
        map["status"] = q.value("status");
    }
    return map;
}

bool DatabaseManager::updateSocialEventFull(int id, int contactId, const QString& scope, int companyId,
                                           const QString& ioType, const QString& category, const QString& title,
                                           double myCost, double theirValue, const QString& detail,
                                           const QDate& eventDate) {
    if (id <= 0) return false;
    QSqlQuery q(m_db);
    q.prepare("UPDATE social_events SET contact_id = :cid, scope = :sc, company_id = :comp, io_type = :io, "
              "event_category = :cat, title = :title, my_cost = :cost, their_value = :val, detail = :det, "
              "event_date = :dt WHERE id = :id;");
    q.bindValue(":cid", contactId);
    q.bindValue(":sc", scope == "COMPANY" ? "COMPANY" : "PRIVATE");
    if (companyId > 0) q.bindValue(":comp", companyId);
    else q.bindValue(":comp", QVariant());
    q.bindValue(":io", ioType);
    q.bindValue(":cat", category);
    q.bindValue(":title", title);
    q.bindValue(":cost", myCost);
    q.bindValue(":val", theirValue);
    q.bindValue(":det", detail);
    q.bindValue(":dt", eventDate.toString("yyyy-MM-dd"));
    q.bindValue(":id", id);
    return q.exec();
}

QList<int> DatabaseManager::getAvailableTransactionYears() {
    QSet<int> yearSet;

    QSqlQuery q(m_db);
    if (q.exec("SELECT DISTINCT CAST(strftime('%Y', trans_date) AS INTEGER) AS yr FROM transactions WHERE trans_date IS NOT NULL AND trans_date != '' "
               "UNION "
               "SELECT DISTINCT CAST(strftime('%Y', diary_date) AS INTEGER) AS yr FROM diaries WHERE diary_date IS NOT NULL AND diary_date != '';")) {
        while (q.next()) {
            int y = q.value(0).toInt();
            if (y >= 1990 && y <= 2100) {
                yearSet.insert(y);
            }
        }
    }

    if (yearSet.isEmpty()) {
        yearSet.insert(QDate::currentDate().year());
    }

    QList<int> years = yearSet.values();
    std::sort(years.begin(), years.end(), std::greater<int>());
    return years;
}


