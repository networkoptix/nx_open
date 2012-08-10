#include "utils/common/util.h"

#ifndef Q_OS_WIN
#include <sys/statvfs.h>
#include <sys/time.h>
#endif

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

QString fromNativePath(QString path)
{
    path = QDir::cleanPath(QDir::fromNativeSeparators(path));

    if (!path.isEmpty() && path.endsWith(QLatin1Char('/')))
        path.chop(1);

    return path;
}

QString getMoviesDirectory()
{
    return QDesktopServices::storageLocation(QDesktopServices::MoviesLocation);
}

QString formatDuration(unsigned position, unsigned total)
{
    unsigned hours = position / 3600;
    unsigned minutes = (position % 3600) / 60;
    unsigned seconds = position % 60;

    if (total == 0)
    {
        if (hours == 0)
            return QString(QLatin1String("%1:%2")).arg(minutes, 2, 10, QLatin1Char('0')).arg(seconds, 2, 10, QLatin1Char('0'));

        return QString(QLatin1String("%1:%2:%3")).arg(hours).arg(minutes, 2, 10, QLatin1Char('0')).arg(seconds, 2, 10, QLatin1Char('0'));
    }
    else
    {
        unsigned totalHours = total / 3600;
        unsigned totalMinutes = (total % 3600) / 60;
        unsigned totalSeconds = total % 60;

        QString secondsString, totalString;

        if (totalHours == 0)
        {
            secondsString = QString(QLatin1String("%1:%2")).arg(minutes, 2, 10, QLatin1Char('0')).arg(seconds, 2, 10, QLatin1Char('0'));
            totalString = QString(QLatin1String("%1:%2")).arg(totalMinutes, 2, 10, QLatin1Char('0')).arg(totalSeconds, 2, 10, QLatin1Char('0'));
        }
        else
        {
            secondsString = QString(QLatin1String("%1:%2:%3")).arg(hours).arg(minutes, 2, 10, QLatin1Char('0')).arg(seconds, 2, 10, QLatin1Char('0'));
            totalString = QString(QLatin1String("%1:%2:%3")).arg(totalHours).arg(totalMinutes, 2, 10, QLatin1Char('0')).arg(totalSeconds, 2, 10, QLatin1Char('0'));
        }

        return secondsString + QLatin1Char('/') + totalString;
    }
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
#else 
qint64 getDiskFreeSpace(const QString& root) {
    struct statvfs buf;
    if (statvfs(root.toUtf8().data(), &buf) == 0)
    {
        qint64 disk_size = buf.f_blocks * (qint64) buf.f_bsize;
        qint64 free = buf.f_bavail * (qint64) buf.f_bsize;
        qint64 used = disk_size - free;

        return free;
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
