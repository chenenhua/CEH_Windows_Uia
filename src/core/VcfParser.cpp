#include "VcfParser.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QByteArray>

QList<VcfContact> VcfParser::parseFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    return parseContent(content);
}

QString VcfParser::decodeQuotedPrintable(const QString& input, const QString& /*charset*/) {
    QByteArray bytes;
    int i = 0;
    while (i < input.length()) {
        if (input[i] == '=' && i + 2 < input.length()) {
            bool ok = false;
            int hexVal = input.mid(i + 1, 2).toInt(&ok, 16);
            if (ok) {
                bytes.append(static_cast<char>(hexVal));
                i += 3;
                continue;
            }
        }
        bytes.append(input[i].toLatin1());
        i++;
    }
    return QString::fromUtf8(bytes);
}

QList<VcfContact> VcfParser::parseContent(const QString& content) {
    QList<VcfContact> list;
    const QStringList lines = content.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);

    VcfContact current;
    bool inCard = false;

    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.startsWith("BEGIN:VCARD", Qt::CaseInsensitive)) {
            inCard = true;
            current = VcfContact();
            continue;
        }

        if (trimmed.startsWith("END:VCARD", Qt::CaseInsensitive)) {
            if (inCard && !current.name.isEmpty()) {
                list.append(current);
            }
            inCard = false;
            continue;
        }

        if (!inCard) continue;

        // 解析名字 (FN)
        if (trimmed.startsWith("FN", Qt::CaseInsensitive)) {
            int colonIdx = trimmed.indexOf(':');
            if (colonIdx != -1) {
                QString val = trimmed.mid(colonIdx + 1).trimmed();
                if (trimmed.contains("ENCODING=QUOTED-PRINTABLE", Qt::CaseInsensitive)) {
                    current.name = decodeQuotedPrintable(val);
                } else {
                    current.name = val;
                }
            }
        }
        // 解析电话 (TEL)
        else if (trimmed.startsWith("TEL", Qt::CaseInsensitive)) {
            int colonIdx = trimmed.indexOf(':');
            if (colonIdx != -1) {
                QString phone = trimmed.mid(colonIdx + 1).trimmed();
                phone.remove(QRegularExpression("[^0-9+]"));
                if (current.phone.isEmpty()) {
                    current.phone = phone;
                }
            }
        }
        // 解析单位 (ORG)
        else if (trimmed.startsWith("ORG", Qt::CaseInsensitive)) {
            int colonIdx = trimmed.indexOf(':');
            if (colonIdx != -1) {
                QString val = trimmed.mid(colonIdx + 1).trimmed();
                val.remove(';');
                if (trimmed.contains("ENCODING=QUOTED-PRINTABLE", Qt::CaseInsensitive)) {
                    current.company = decodeQuotedPrintable(val);
                } else {
                    current.company = val;
                }
            }
        }
        // 解析职务 (TITLE)
        else if (trimmed.startsWith("TITLE", Qt::CaseInsensitive)) {
            int colonIdx = trimmed.indexOf(':');
            if (colonIdx != -1) {
                QString val = trimmed.mid(colonIdx + 1).trimmed();
                if (trimmed.contains("ENCODING=QUOTED-PRINTABLE", Qt::CaseInsensitive)) {
                    current.title = decodeQuotedPrintable(val);
                } else {
                    current.title = val;
                }
            }
        }
        // 解析备注 (NOTE)
        else if (trimmed.startsWith("NOTE", Qt::CaseInsensitive)) {
            int colonIdx = trimmed.indexOf(':');
            if (colonIdx != -1) {
                QString val = trimmed.mid(colonIdx + 1).trimmed();
                if (trimmed.contains("ENCODING=QUOTED-PRINTABLE", Qt::CaseInsensitive)) {
                    current.note = decodeQuotedPrintable(val);
                } else {
                    current.note = val;
                }
            }
        }
    }

    return list;
}
