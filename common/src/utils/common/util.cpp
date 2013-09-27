#include "utils/common/util.h"

#ifndef Q_OS_WIN
#   include <sys/statvfs.h>
#   include <sys/time.h>
#endif

#include <QtCore/QDateTime>
#include <QtCore/QStandardPaths>
#include <QtGui/QDesktopServices>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtNetwork/QHostInfo>

#include <common/common_globals.h>
#include "version.h"


bool removeDir(const QString &dirName)
{
    bool result = true;
    QDir dir(dirName);

    if (dir.exists(dirName))
    {
        foreach(QFileInfo info, dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden  | QDir::AllDirs | QDir::Files, QDir::DirsFirst))
        {
            if (info.isDir())
                result = removeDir(info.absoluteFilePath());
            else
                result = QFile::remove(info.absoluteFilePath());

            if (!result)
                return result;
        }

        result = dir.rmdir(dirName);
    }

    return result;
}

QString fromNativePath(const QString &path)
{
    QString result = QDir::cleanPath(QDir::fromNativeSeparators(path));

    if (!result.isEmpty() && result.endsWith(QLatin1Char('/')))
        result.chop(1);

    return result;
}

QString getMoviesDirectory()
{
    const QStringList& moviesDirs = QStandardPaths::standardLocations(QStandardPaths::MoviesLocation);
    return moviesDirs.isEmpty() ? QString() : (moviesDirs[0] + QLatin1String("/") + QLatin1String(QN_MEDIA_FOLDER_NAME) );
}

QString getBackgroundsDirectory() {
    const QStringList& pictureFolderList = QStandardPaths::standardLocations(QStandardPaths::PicturesLocation);
    QString baseDir = pictureFolderList.isEmpty() ? QString(): pictureFolderList[0];
#ifdef Q_OS_WIN
    QString productDir = baseDir + QDir::toNativeSeparators(QString(lit("/%1 Backgrounds")).arg(lit(QN_PRODUCT_NAME_LONG)));
#else
    QString productDir = QDir::toNativeSeparators(QString(lit("/opt/%1/client/share/pictures/sample-backgrounds")).arg(lit(VER_LINUX_ORGANIZATION_NAME)));
#endif

    return QDir(productDir).exists()
            ? productDir
            : baseDir;
}

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

QString closeDirPath(const QString& value)
{
    QString tmp = value;
    for (int i = 0; i < tmp.size(); ++i) {
        if (tmp[i] == QLatin1Char('\\'))
            tmp[i] = QLatin1Char('/');
    }
    if (tmp.endsWith(QLatin1Char('/')))
        return tmp;
    else
        return tmp + QLatin1Char('/');
}
#ifdef Q_OS_WIN32
qint64 getDiskFreeSpace(const QString& root)
{
    quint64 freeBytesAvailableToCaller = -1;
    quint64 totalNumberOfBytes = -1;
    quint64 totalNumberOfFreeBytes = -1;
    BOOL status = GetDiskFreeSpaceEx(
        (LPCWSTR) root.data(), // pointer to the directory name
        (PULARGE_INTEGER) &freeBytesAvailableToCaller, // receives the number of bytes on disk available to the caller
        (PULARGE_INTEGER) &totalNumberOfBytes, // receives the number of bytes on disk
        (PULARGE_INTEGER) &totalNumberOfFreeBytes // receives the free bytes on disk
    );
    Q_UNUSED(status);
    return totalNumberOfFreeBytes;
};

qint64 getDiskTotalSpace(const QString& root)
{
    quint64 freeBytesAvailableToCaller = -1;
    quint64 totalNumberOfBytes = -1;
    quint64 totalNumberOfFreeBytes = -1;
    BOOL status = GetDiskFreeSpaceEx(
        (LPCWSTR) root.data(), // pointer to the directory name
        (PULARGE_INTEGER) &freeBytesAvailableToCaller, // receives the number of bytes on disk available to the caller
        (PULARGE_INTEGER) &totalNumberOfBytes, // receives the number of bytes on disk
        (PULARGE_INTEGER) &totalNumberOfFreeBytes // receives the free bytes on disk
        );
    Q_UNUSED(status);
    return totalNumberOfBytes;
};

#else 
qint64 getDiskFreeSpace(const QString& root) {
    struct statvfs buf;
    if (statvfs(root.toUtf8().data(), &buf) == 0)
    {
        //qint64 disk_size = buf.f_blocks * (qint64) buf.f_bsize;
        qint64 free = buf.f_bavail * (qint64) buf.f_bsize;
        //qint64 used = disk_size - free;

        return free;
    }
    else {
        return -1;
    }
}

qint64 getDiskTotalSpace(const QString& root) {
    struct statvfs buf;
    if (statvfs(root.toUtf8().data(), &buf) == 0)
    {
        qint64 disk_size = buf.f_blocks * (qint64) buf.f_bsize;
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
    static QMutex timeMutex;
    QMutexLocker lock(&timeMutex);
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

int random(int min, int max) {
    return min + static_cast<int>(static_cast<qint64>(max - min) * qrand() / (static_cast<qint64>(RAND_MAX) + 1));
}

qreal frandom() {
    return qrand() / (RAND_MAX + 1.0);
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
