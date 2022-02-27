// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils/common/util.h"

#if !defined(Q_OS_WIN) && !defined(Q_OS_ANDROID)
    #include <sys/statvfs.h>
    #include <sys/time.h>
#endif

#ifdef Q_OS_WIN32
    #include <windows.h>
    #include <mmsystem.h>
#endif

#include <QtCore/QDateTime>
#include <QtCore/QStandardPaths>
#include <QtGui/QDesktopServices>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtNetwork/QHostInfo>

#include <common/common_globals.h>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/datetime.h>
#include <nx/utils/time.h>

QString strPadLeft(const QString &str, int len, char ch)
{
    int diff = len - str.length();
    if (diff > 0)
        return QString(diff, QLatin1Char(ch)) + str;
    return str;
}

QString getPathSeparator(const QString& path)
{
    return path.contains("\\") ? "\\" : "/";
}

QString closeDirPath(const QString& value)
{
    QString separator = getPathSeparator(value);
    if (value.endsWith(separator))
        return value;
    else
        return value + separator;
}

QString getValueFromString(const QString& line)
{

    int index = line.indexOf(QLatin1Char('='));

    if (index < 1)
        return QString();

    return line.mid(index+1);
}

int timeZone(QDateTime dt1)
{
    QDateTime dt2 = dt1.toUTC();
    dt1.setTimeSpec(Qt::UTC);
    int res = dt2.secsTo(dt1);
    return res;
}

int currentTimeZone()
{
    return timeZone(QDateTime::fromMSecsSinceEpoch(nx::utils::millisSinceEpoch().count()));
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
