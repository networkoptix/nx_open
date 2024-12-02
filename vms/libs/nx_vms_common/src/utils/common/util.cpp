// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "util.h"

#include <common/common_globals.h>
#include <nx/utils/datetime.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/time.h>

QString strPadLeft(const QString &str, int len, char ch)
{
    int diff = len - str.length();
    if (diff > 0)
        return QString(diff, QLatin1Char(ch)) + str;
    return str;
}

QString toUnixSeparator(QString path)
{
    return path.replace("\\", "/");
}

QString getPathSeparator(const QString& path)
{
    return path.contains("\\") ? "\\" : "/";
}

QString closeDirPath(QString path)
{
    QString separator = getPathSeparator(path);
    if (path.endsWith(separator))
        return path;
    else
        return path + separator;
}

QString closeDirPathUnix(QString path)
{
    path = toUnixSeparator(std::move(path));
    return closeDirPath(std::move(path));
}

QString stripLastSeparator(QString path)
{
    while (path.endsWith("/") || path.endsWith("\\"))
        path.chop(1);

    return path;
}

QString getValueFromString(const QString& line)
{

    int index = line.indexOf(QLatin1Char('='));

    if (index < 1)
        return QString();

    return line.mid(index+1);
}

static uint hash(const QChar *p, int n)
{
    uint h = 0;

    while (n--) {
        h = (h << 4) + (*p++).unicode();
        h ^= (h & 0xf0000000) >> 23;
        h &= 0x0fffffff;
    }
    return h;
}

uint qt4Hash(const QString &key)
{
    return hash(key.unicode(), key.size());
}

QString mksecToDateTime(qint64 valueUsec)
{
    if (valueUsec == DATETIME_NOW)
        return QStringLiteral("NOW");
    if (valueUsec == DATETIME_INVALID)
        return QStringLiteral("No value");
    return QDateTime::fromMSecsSinceEpoch(valueUsec / 1000).toString(
        QStringLiteral("dd.MM.yyyy hh:mm:ss.zzz"));
}
