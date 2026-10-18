#pragma once

#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QFontDatabase>
#include <QFont>
#include <QColor>
#include <QString>
#include <QFile>
#include "../core/DatabaseManager.h"

namespace UiIcons {

inline QString iconFontFamily() {
    static QString family;
    if (family.isEmpty()) {
        QString fontPath = DatabaseManager::instance().resolveAssetPath("assets/font/iconfont.ttf");
        int id = -1;
        if (QFile::exists(fontPath)) {
            id = QFontDatabase::addApplicationFont(fontPath);
        }
        if (id < 0) {
            id = QFontDatabase::addApplicationFont(":/fonts/assets/font/iconfont.ttf");
        }
        if (id >= 0) {
            QStringList families = QFontDatabase::applicationFontFamilies(id);
            if (!families.isEmpty()) {
                family = families.first();
            }
        }
    }
    return family;
}

inline QPixmap pixmap(QChar codepoint, int pixelSize = 12, const QColor& color = QColor("#8e8e93")) {
    QPixmap px(pixelSize + 2, pixelSize + 2);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    QString fam = iconFontFamily();
    QFont f = fam.isEmpty() ? QFont("Microsoft YaHei UI") : QFont(fam);
    f.setPixelSize(pixelSize);
    p.setFont(f);
    p.setPen(color);
    p.drawText(px.rect(), Qt::AlignCenter, QString(codepoint));
    p.end();
    return px;
}

inline QIcon icon(QChar codepoint, int pixelSize = 12, const QColor& color = QColor("#8e8e93")) {
    return QIcon(pixmap(codepoint, pixelSize, color));
}

// 常用单色图标代码点 (来自 assets/font/iconfont.ttf)
namespace Code {
    constexpr QChar Dashboard  = QChar(0xe8da); // 综合栏目
    constexpr QChar Relation   = QChar(0xe73e); // 关系人脉
    constexpr QChar Finance    = QChar(0xe69f); // 财务账单
    constexpr QChar Plan       = QChar(0xe62b); // 走动计划
    constexpr QChar Calendar   = QChar(0xe63a); // 日历日期
    constexpr QChar AiBrain    = QChar(0xe680); // AI大脑
    constexpr QChar Asset      = QChar(0xe657); // 企业/资产
    constexpr QChar Export     = QChar(0xea6a); // 导出
    constexpr QChar Import     = QChar(0xe622); // 导入
    constexpr QChar Security   = QChar(0xe627); // 安全设置
    constexpr QChar Search     = QChar(0xe60d); // 搜索
    constexpr QChar Diary      = QChar(0xe602); // 日记/编辑
    constexpr QChar Link       = QChar(0xe696); // 关联
    constexpr QChar Image      = QChar(0xe681); // 头像/凭证图片
    constexpr QChar Add        = QChar(0xe61f); // 添加
    constexpr QChar Delete     = QChar(0xe6ba); // 删除
    constexpr QChar User       = QChar(0xe643); // 个人/用户
    constexpr QChar Company    = QChar(0xe91d); // 公司
    constexpr QChar Network    = QChar(0xe676); // 关系网拓扑
}

} // namespace UiIcons
