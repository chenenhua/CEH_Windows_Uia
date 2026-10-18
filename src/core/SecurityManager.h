#pragma once

#include <QString>
#include <QByteArray>
#include <QCryptographicHash>

class SecurityManager {
public:
    static SecurityManager& instance();

    bool isSecurityConfigured();
    bool setupMasterPassword(const QString& password);
    bool verifyMasterPassword(const QString& password);
    bool changeMasterPassword(const QString& oldPassword, const QString& newPassword);

    QString hashPassword(const QString& password) const;
    QByteArray machineFingerprint() const;

    // WOODENC2 动态浮动密钥加密容器 (ChaCha20-256 + HMAC-SHA256 + 硬件指纹双槽保护)
    static bool isEncryptedContainer(const QString& filePath, bool* outHasPassword = nullptr);
    bool encryptDatabaseBytes(const QByteArray& plainSqliteBytes,
                              const QString& passwordHash,
                              QByteArray& outEncryptedBytes) const;
    bool decryptDatabaseBytes(const QByteArray& encryptedBytes,
                              const QString& passwordHash,
                              QByteArray& outPlainSqliteBytes,
                              bool* outUsedPortableSlot = nullptr) const;
    bool encryptDatabaseFile(const QString& filePath,
                             const QByteArray& plainSqliteBytes,
                             const QString& passwordHash) const;
    bool decryptDatabaseFile(const QString& filePath,
                             const QString& passwordHash,
                             QByteArray& outPlainSqliteBytes,
                             bool* outUsedPortableSlot = nullptr) const;

private:
    SecurityManager() = default;
    ~SecurityManager() = default;
};

