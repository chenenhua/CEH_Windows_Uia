#include "SecurityManager.h"
#include "DatabaseManager.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QSysInfo>
#include <QRandomGenerator>
#include <QMessageAuthenticationCode>
#include <QtEndian>
#include <cstring>

namespace {

constexpr const char kWoodMagic[8] = {'W', 'O', 'O', 'D', 'E', 'N', 'C', '2'};
constexpr int kHeaderSize = 216;
// Header layout (216 bytes):
// [0..7]     8B: Magic "WOODENC2"
// [8]        1B: Flags (0x01 = password configured, 0x00 = unconfigured initial container)
// [9..11]    3B: Reserved (0)
// [12..43]  32B: Floating random salt (regenerated on every save/open)
// [44..55]  12B: ChaCha20 96-bit payload nonce (regenerated on every save/open)
// [56..87]  32B: Slot 1 wrapped DEK (bound to MasterPassword + Current Machine Fingerprint + FloatingSalt)
// [88..119] 32B: Slot 1 HMAC-SHA256 tag
// [120..151]32B: Slot 2 wrapped DEK (bound to MasterPassword + Portable Fallback + FloatingSalt)
// [152..183]32B: Slot 2 HMAC-SHA256 tag
// [184..215]32B: Payload HMAC-SHA256 (key = floatingDek, message = header[0..183] + ciphertext)

inline quint32 rotl32(quint32 x, int n) {
    return (x << n) | (x >> (32 - n));
}

inline void chachaQuarterRound(quint32& a, quint32& b, quint32& c, quint32& d) {
    a += b; d ^= a; d = rotl32(d, 16);
    c += d; b ^= c; b = rotl32(b, 12);
    a += b; d ^= a; d = rotl32(d, 8);
    c += d; b ^= c; b = rotl32(b, 7);
}

// RFC 8439 ChaCha20 256-bit Stream Cipher
void chacha20Xor(const uchar key32[32], const uchar nonce12[12], quint32 initialCounter,
                 const uchar* inBuf, uchar* outBuf, qsizetype len) {
    quint32 state[16];
    state[0] = 0x61707865u;
    state[1] = 0x3320646eu;
    state[2] = 0x79622d32u;
    state[3] = 0x6b206574u;
    for (int i = 0; i < 8; ++i) {
        state[4 + i] = qFromLittleEndian<quint32>(key32 + i * 4);
    }
    state[12] = initialCounter;
    for (int i = 0; i < 3; ++i) {
        state[13 + i] = qFromLittleEndian<quint32>(nonce12 + i * 4);
    }

    quint32 working[16];
    uchar block[64];
    qsizetype offset = 0;

    while (offset < len) {
        std::memcpy(working, state, sizeof(state));
        for (int r = 0; r < 10; ++r) {
            chachaQuarterRound(working[0], working[4], working[8],  working[12]);
            chachaQuarterRound(working[1], working[5], working[9],  working[13]);
            chachaQuarterRound(working[2], working[6], working[10], working[14]);
            chachaQuarterRound(working[3], working[7], working[11], working[15]);
            chachaQuarterRound(working[0], working[5], working[10], working[15]);
            chachaQuarterRound(working[1], working[6], working[11], working[12]);
            chachaQuarterRound(working[2], working[7], working[8],  working[13]);
            chachaQuarterRound(working[3], working[4], working[9],  working[14]);
        }
        for (int i = 0; i < 16; ++i) {
            quint32 word = working[i] + state[i];
            qToLittleEndian<quint32>(word, block + i * 4);
        }

        qsizetype chunk = qMin<qsizetype>(64, len - offset);
        for (qsizetype i = 0; i < chunk; ++i) {
            outBuf[offset + i] = inBuf[offset + i] ^ block[i];
        }

        state[12] += 1;
        offset += chunk;
    }
    std::memset(working, 0, sizeof(working));
    std::memset(block, 0, sizeof(block));
}

QByteArray generateRandomBytes(int count) {
    QByteArray buf(count, Qt::Uninitialized);
    auto* rng = QRandomGenerator::system();
    int offset = 0;
    while (offset < count) {
        quint32 val = rng->generate();
        int copyLen = qMin(4, count - offset);
        std::memcpy(buf.data() + offset, &val, copyLen);
        offset += copyLen;
    }
    return buf;
}

QByteArray deriveSlotWrapKey(const QString& passwordHash,
                             const QByteArray& hardwareOrDomainBinding,
                             const QByteArray& floatingSalt,
                             const QByteArray& slotLabel) {
    QByteArray normalizedPwd = passwordHash.trimmed().toLower().toUtf8();
    if (normalizedPwd.isEmpty()) {
        normalizedPwd = QByteArrayLiteral("WOOD_UNCONFIGURED_INITIAL_STATE_2026");
    }

    QByteArray seed = normalizedPwd + "|" + hardwareOrDomainBinding + "|" + floatingSalt + "|" + slotLabel;
    QByteArray key = QCryptographicHash::hash(seed, QCryptographicHash::Sha256);
    // 多轮迭代强化派生密钥 (抗暴力破解)
    for (int i = 0; i < 2048; ++i) {
        QCryptographicHash hasher(QCryptographicHash::Sha256);
        hasher.addData(key);
        hasher.addData(floatingSalt);
        hasher.addData(reinterpret_cast<const char*>(&i), sizeof(i));
        key = hasher.result();
    }
    return key;
}

void wrapDekForSlot(const QByteArray& floatingDek,
                    const QByteArray& wrapKey,
                    const QByteArray& floatingSalt,
                    QByteArray& outEncryptedDek,
                    QByteArray& outSlotMac) {
    QByteArray mask = QCryptographicHash::hash(wrapKey + "|DEK_XOR_MASK|" + floatingSalt, QCryptographicHash::Sha256);
    outEncryptedDek.resize(32);
    for (int i = 0; i < 32; ++i) {
        outEncryptedDek[i] = floatingDek[i] ^ mask[i];
    }
    QByteArray macData = floatingDek + floatingSalt + QByteArrayLiteral("WOOD_SLOT_AUTH_V2");
    outSlotMac = QMessageAuthenticationCode::hash(macData, wrapKey, QCryptographicHash::Sha256);
}

bool unwrapDekFromSlot(const QByteArray& encryptedDek,
                       const QByteArray& expectedSlotMac,
                       const QByteArray& wrapKey,
                       const QByteArray& floatingSalt,
                       QByteArray& outFloatingDek) {
    if (encryptedDek.size() != 32 || expectedSlotMac.size() != 32) return false;
    QByteArray mask = QCryptographicHash::hash(wrapKey + "|DEK_XOR_MASK|" + floatingSalt, QCryptographicHash::Sha256);
    QByteArray candidateDek(32, Qt::Uninitialized);
    for (int i = 0; i < 32; ++i) {
        candidateDek[i] = encryptedDek[i] ^ mask[i];
    }
    QByteArray macData = candidateDek + floatingSalt + QByteArrayLiteral("WOOD_SLOT_AUTH_V2");
    QByteArray actualMac = QMessageAuthenticationCode::hash(macData, wrapKey, QCryptographicHash::Sha256);

    // 常数时间比较防止时序侧信道
    uchar diff = 0;
    for (int i = 0; i < 32; ++i) {
        diff |= static_cast<uchar>(actualMac[i] ^ expectedSlotMac[i]);
    }
    if (diff != 0) {
        candidateDek.fill('\0');
        return false;
    }
    outFloatingDek = candidateDek;
    return true;
}

} // namespace

SecurityManager& SecurityManager::instance() {
    static SecurityManager s_instance;
    return s_instance;
}

QString SecurityManager::hashPassword(const QString& password) const {
    if (password.isEmpty()) return QString();
    QByteArray salted = password.toUtf8() + "_WOOD_SECRET_SALT_2026_";
    return QCryptographicHash::hash(salted, QCryptographicHash::Sha256).toHex();
}

QByteArray SecurityManager::machineFingerprint() const {
    QByteArray hwId = QSysInfo::machineUniqueId();
    if (hwId.isEmpty()) {
        hwId = (QSysInfo::prettyProductName() + "|" + QSysInfo::currentCpuArchitecture() + "|" + QSysInfo::machineHostName()).toUtf8();
    }
    return QCryptographicHash::hash(hwId + "|WOOD_MACHINE_FINGERPRINT_2026", QCryptographicHash::Sha256);
}

bool SecurityManager::isEncryptedContainer(const QString& filePath, bool* outHasPassword) {
    QFile f(filePath);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
        if (outHasPassword) *outHasPassword = false;
        return false;
    }
    QByteArray head = f.read(16);
    f.close();
    if (head.size() < 12 || std::memcmp(head.constData(), kWoodMagic, 8) != 0) {
        if (outHasPassword) *outHasPassword = false;
        return false;
    }
    if (outHasPassword) {
        *outHasPassword = (static_cast<uchar>(head[8]) & 0x01) != 0;
    }
    return true;
}

bool SecurityManager::encryptDatabaseBytes(const QByteArray& plainSqliteBytes,
                                           const QString& passwordHash,
                                           QByteArray& outEncryptedBytes) const {
    if (plainSqliteBytes.isEmpty()) return false;

    // 每次保存/打开均重新生成全新的浮动盐值 (Floating Salt)、浮动数据密钥 (Floating DEK) 与随机 Nonce
    QByteArray floatingSalt = generateRandomBytes(32);
    QByteArray payloadNonce = generateRandomBytes(12);
    QByteArray floatingDek  = generateRandomBytes(32);

    // 槽位 1: 严格绑定当前电脑硬件指纹 + 主密码 + 浮动盐
    QByteArray wrapKey1 = deriveSlotWrapKey(passwordHash, machineFingerprint(), floatingSalt, "SLOT1_MACHINE_FLOAT");
    QByteArray slot1Dek, slot1Mac;
    wrapDekForSlot(floatingDek, wrapKey1, floatingSalt, slot1Dek, slot1Mac);

    // 槽位 2: 跨机打包/迁移恢复槽位 (仍需输入正确主密码 + 浮动盐，打开后自动重绑定到新电脑槽位 1)
    QByteArray portableDomain = QCryptographicHash::hash("WOOD_PORTABLE_PACKAGE_BINDING_2026", QCryptographicHash::Sha256);
    QByteArray wrapKey2 = deriveSlotWrapKey(passwordHash, portableDomain, floatingSalt, "SLOT2_PORTABLE_FLOAT");
    QByteArray slot2Dek, slot2Mac;
    wrapDekForSlot(floatingDek, wrapKey2, floatingSalt, slot2Dek, slot2Mac);

    QByteArray header(kHeaderSize, '\0');
    std::memcpy(header.data(), kWoodMagic, 8);
    header[8] = passwordHash.trimmed().isEmpty() ? 0x00 : 0x01;
    std::memcpy(header.data() + 12,  floatingSalt.constData(), 32);
    std::memcpy(header.data() + 44,  payloadNonce.constData(), 12);
    std::memcpy(header.data() + 56,  slot1Dek.constData(),     32);
    std::memcpy(header.data() + 88,  slot1Mac.constData(),     32);
    std::memcpy(header.data() + 120, slot2Dek.constData(),     32);
    std::memcpy(header.data() + 152, slot2Mac.constData(),     32);

    outEncryptedBytes.resize(kHeaderSize + plainSqliteBytes.size());
    std::memcpy(outEncryptedBytes.data(), header.constData(), kHeaderSize);

    uchar* cipherPtr = reinterpret_cast<uchar*>(outEncryptedBytes.data() + kHeaderSize);
    chacha20Xor(reinterpret_cast<const uchar*>(floatingDek.constData()),
                reinterpret_cast<const uchar*>(payloadNonce.constData()),
                1u,
                reinterpret_cast<const uchar*>(plainSqliteBytes.constData()),
                cipherPtr,
                plainSqliteBytes.size());

    // 计算 Encrypt-then-MAC 完整性认证标签 (覆盖 Header 前 184 字节 + 全部密文)
    QMessageAuthenticationCode payloadMac(QCryptographicHash::Sha256, floatingDek);
    payloadMac.addData(QByteArrayView(outEncryptedBytes.constData(), 184));
    payloadMac.addData(QByteArrayView(outEncryptedBytes.constData() + kHeaderSize, plainSqliteBytes.size()));
    QByteArray hmacTag = payloadMac.result();
    std::memcpy(outEncryptedBytes.data() + 184, hmacTag.constData(), 32);

    floatingDek.fill('\0');
    wrapKey1.fill('\0');
    wrapKey2.fill('\0');
    return true;
}

bool SecurityManager::decryptDatabaseBytes(const QByteArray& encryptedBytes,
                                           const QString& passwordHash,
                                           QByteArray& outPlainSqliteBytes,
                                           bool* outUsedPortableSlot) const {
    if (outUsedPortableSlot) *outUsedPortableSlot = false;
    if (encryptedBytes.size() <= kHeaderSize) return false;
    if (std::memcmp(encryptedBytes.constData(), kWoodMagic, 8) != 0) return false;

    QByteArray floatingSalt(encryptedBytes.constData() + 12, 32);
    QByteArray payloadNonce(encryptedBytes.constData() + 44, 12);
    QByteArray slot1Dek(encryptedBytes.constData() + 56, 32);
    QByteArray slot1Mac(encryptedBytes.constData() + 88, 32);
    QByteArray slot2Dek(encryptedBytes.constData() + 120, 32);
    QByteArray slot2Mac(encryptedBytes.constData() + 152, 32);
    QByteArray expectedPayloadHmac(encryptedBytes.constData() + 184, 32);

    QByteArray floatingDek;
    // 1. 优先使用当前电脑硬件指纹 + 主密码 + 浮动盐 解锁槽位 1
    QByteArray wrapKey1 = deriveSlotWrapKey(passwordHash, machineFingerprint(), floatingSalt, "SLOT1_MACHINE_FLOAT");
    bool unlocked = unwrapDekFromSlot(slot1Dek, slot1Mac, wrapKey1, floatingSalt, floatingDek);
    wrapKey1.fill('\0');

    // 2. 若在客户新电脑首次打开打包数据库，验证主密码解锁槽位 2 (解锁后将立即重加密绑定到当前电脑硬件指纹)
    if (!unlocked) {
        QByteArray portableDomain = QCryptographicHash::hash("WOOD_PORTABLE_PACKAGE_BINDING_2026", QCryptographicHash::Sha256);
        QByteArray wrapKey2 = deriveSlotWrapKey(passwordHash, portableDomain, floatingSalt, "SLOT2_PORTABLE_FLOAT");
        unlocked = unwrapDekFromSlot(slot2Dek, slot2Mac, wrapKey2, floatingSalt, floatingDek);
        wrapKey2.fill('\0');
        if (unlocked && outUsedPortableSlot) {
            *outUsedPortableSlot = true;
        }
    }

    if (!unlocked) {
        return false;
    }

    qsizetype cipherLen = encryptedBytes.size() - kHeaderSize;
    QMessageAuthenticationCode payloadMac(QCryptographicHash::Sha256, floatingDek);
    payloadMac.addData(QByteArrayView(encryptedBytes.constData(), 184));
    payloadMac.addData(QByteArrayView(encryptedBytes.constData() + kHeaderSize, cipherLen));
    QByteArray actualPayloadHmac = payloadMac.result();

    uchar diff = 0;
    for (int i = 0; i < 32; ++i) {
        diff |= static_cast<uchar>(actualPayloadHmac[i] ^ expectedPayloadHmac[i]);
    }
    if (diff != 0) {
        floatingDek.fill('\0');
        return false;
    }

    outPlainSqliteBytes.resize(cipherLen);
    chacha20Xor(reinterpret_cast<const uchar*>(floatingDek.constData()),
                reinterpret_cast<const uchar*>(payloadNonce.constData()),
                1u,
                reinterpret_cast<const uchar*>(encryptedBytes.constData() + kHeaderSize),
                reinterpret_cast<uchar*>(outPlainSqliteBytes.data()),
                cipherLen);

    floatingDek.fill('\0');
    return true;
}

bool SecurityManager::encryptDatabaseFile(const QString& filePath,
                                          const QByteArray& plainSqliteBytes,
                                          const QString& passwordHash) const {
    QByteArray encryptedBytes;
    if (!encryptDatabaseBytes(plainSqliteBytes, passwordHash, encryptedBytes)) {
        return false;
    }

    QString tmpPath = filePath + ".enc_tmp";
    {
        QFile tmpFile(tmpPath);
        if (!tmpFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return false;
        }
        if (tmpFile.write(encryptedBytes) != encryptedBytes.size()) {
            tmpFile.close();
            QFile::remove(tmpPath);
            return false;
        }
        tmpFile.flush();
        tmpFile.close();
    }

    if (QFile::exists(filePath)) {
        if (!QFile::remove(filePath)) {
            // 若目标文件无法删除，直接覆盖写入
            QFile directFile(filePath);
            if (!directFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                QFile::remove(tmpPath);
                return false;
            }
            bool ok = (directFile.write(encryptedBytes) == encryptedBytes.size());
            directFile.flush();
            directFile.close();
            QFile::remove(tmpPath);
            return ok;
        }
    }
    return QFile::rename(tmpPath, filePath);
}

bool SecurityManager::decryptDatabaseFile(const QString& filePath,
                                          const QString& passwordHash,
                                          QByteArray& outPlainSqliteBytes,
                                          bool* outUsedPortableSlot) const {
    QFile f(filePath);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
        return false;
    }
    QByteArray encryptedBytes = f.readAll();
    f.close();
    return decryptDatabaseBytes(encryptedBytes, passwordHash, outPlainSqliteBytes, outUsedPortableSlot);
}

bool SecurityManager::isSecurityConfigured() {
    return DatabaseManager::instance().isPasswordConfiguredInStorage();
}

bool SecurityManager::setupMasterPassword(const QString& password) {
    if (password.length() < 4) return false;
    if (!DatabaseManager::instance().isUnlocked()) {
        return DatabaseManager::instance().unlockWithPassword(password, true);
    }
    return DatabaseManager::instance().updateEncryptionPassword(password);
}

bool SecurityManager::verifyMasterPassword(const QString& password) {
    if (!DatabaseManager::instance().isUnlocked()) {
        return DatabaseManager::instance().unlockWithPassword(password, false);
    }
    QString storedHash = DatabaseManager::instance().getSecurityConfig("master_password_hash");
    if (storedHash.isEmpty()) return true;
    return (hashPassword(password) == storedHash);
}

bool SecurityManager::changeMasterPassword(const QString& oldPassword, const QString& newPassword) {
    if (!verifyMasterPassword(oldPassword)) {
        return false;
    }
    return setupMasterPassword(newPassword);
}

