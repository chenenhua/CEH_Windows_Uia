#pragma once

#include <QString>
#include <QList>

struct VcfContact {
    QString name;
    QString phone;
    QString company;
    QString title;
    QString note;
};

class VcfParser {
public:
    static QList<VcfContact> parseFile(const QString& filePath);
    static QList<VcfContact> parseContent(const QString& content);

private:
    static QString decodeQuotedPrintable(const QString& input, const QString& charset = "UTF-8");
};
