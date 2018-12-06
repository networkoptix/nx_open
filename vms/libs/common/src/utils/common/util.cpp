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

#include <utils/common/app_info.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/datetime.h>

QString strPadLeft(const QString &str, int len, char ch)
{
    int diff = len - str.length();
    if (diff > 0)
        return QString(diff, QLatin1Char(ch)) + str;
    return str;
}

QString getPathSeparator(const QString& path)
{
    return path.contains(lit("\\")) ? lit("\\") : lit("/");
}

QString closeDirPath(const QString& value)
{
    QString separator = getPathSeparator(value);
    if (value.endsWith(separator))
        return value;
    else
        return value + separator;
}

quint64 getUsecTimer()
{
#if defined(Q_OS_WIN32)
    LARGE_INTEGER timer, freq;
    QueryPerformanceCounter(&timer);
    QueryPerformanceFrequency(&freq);
    return (double) timer.QuadPart / (double) freq.QuadPart * 1000000.0;

    static quint32 prevTics = 0;
    static quint64 cycleCount = 0;
    static QnMutex timeMutex;
    QnMutexLocker lock( &timeMutex );
    quint32 tics = (qint32) timeGetTime();
    if (tics < prevTics)
        cycleCount+= 0x100000000ull;
    prevTics = tics;
    return ((quint64) tics + cycleCount) * 1000ull;
#else
    // POSIX
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec * 1000000ull + tv.tv_usec;
#endif
}

QString getValueFromString(const QString& line)
{

    int index = line.indexOf(QLatin1Char('='));

    if (index < 1)
        return QString();

    return line.mid(index+1);
}

int currentTimeZone()
{
    QDateTime dt1(QDateTime::currentDateTime());
    QDateTime dt2 = dt1.toUTC();
    dt1.setTimeSpec(Qt::UTC);
    int res = dt2.secsTo(dt1);
    return res;
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
        return lit("NOW");
    if (valueUsec == DATETIME_INVALID)
        return lit("No value");
    return QDateTime::fromMSecsSinceEpoch(valueUsec / 1000).toString(lit("dd.MM.yyyy hh:mm:ss.zzz"));
}
