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
#include <nx/system_commands.h>

int digitsInNumber(unsigned num)
{
    int digits = 1;
    while(num /= 10)
        digits++;

    return digits;
}


QString getParamFromString(const QString& str, const QString& param)
{
    if (!str.contains(param))
        return QString();

    int param_index = str.indexOf(param);
    param_index += param.length();

    int first_index = str.indexOf(QLatin1Char('\"'), param_index);
    if (first_index == -1)
        return QString();

    int second_index = str.indexOf(QLatin1Char('\"'), first_index + 1);

    return str.mid(first_index+1, second_index - (first_index+1));
}

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

#ifdef Q_OS_WIN32

qint64 getDiskFreeSpace(const QString& root)
{
    return nx::SystemCommands().freeSpace(root.toStdString());
};

qint64 getDiskTotalSpace(const QString& root)
{
    return nx::SystemCommands().totalSpace(root.toStdString());
};

#elif defined(Q_OS_ANDROID)

qint64 getDiskFreeSpace(const QString& root) {
    return 0; // TODO: #android
}

qint64 getDiskTotalSpace(const QString& root) {
    return 0; // TODO: #android
}

#else

//TODO #ak introduce single function for getting partition info
    //and place platform-specific code in a single pace

#ifdef __APPLE__
#define statvfs64 statvfs
#endif

qint64 getDiskFreeSpace(const QString& root) {
    struct statvfs64 buf;
    if (statvfs64(root.toUtf8().data(), &buf) == 0)
    {
        //qint64 disk_size = buf.f_blocks * (qint64) buf.f_bsize;
        //TODO #ak if we run under root, MUST use buf.f_bfree, else buf.f_bavail
        qint64 free = buf.f_bavail * (qint64) buf.f_bsize;
        //qint64 used = disk_size - free;

        return free;
    }
    else {
        return -1;
    }
}

qint64 getDiskTotalSpace(const QString& root) {
    struct statvfs64 buf;
    if (statvfs64(root.toUtf8().data(), &buf) == 0)
    {
        qint64 disk_size = buf.f_blocks * (qint64) buf.f_frsize;
        //qint64 free = buf.f_bavail * (qint64) buf.f_bsize;
        //qint64 used = disk_size - free;

        return disk_size;
    }
    else {
        return -1;
    }
}

#endif


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

#ifdef _DEBUG
QString debugTime(qint64 timeMSec, const QString &fmt) {
    QString format = fmt.isEmpty() ? lit("hh:mm:ss") : fmt;
    return QDateTime::fromMSecsSinceEpoch(timeMSec).toString(format);
}
#endif

QString mksecToDateTime(qint64 valueUsec)
{
    if (valueUsec == DATETIME_NOW)
        return lit("NOW");
    if (valueUsec == DATETIME_INVALID)
        return lit("No value");
    return QDateTime::fromMSecsSinceEpoch(valueUsec / 1000).toString(lit("dd.MM.yyyy hh:mm:ss.zzz"));
}
