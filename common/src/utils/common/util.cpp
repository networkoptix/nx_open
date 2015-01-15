#include "utils/common/util.h"

#if !defined(Q_OS_WIN) && !defined(Q_OS_ANDROID)
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
#include <utils/mac_utils.h>
#include <utils/common/app_info.h>


bool removeDir(const QString &dirName)
{
    bool result = true;
    QDir dir(dirName);

    if (dir.exists(dirName))
    {
        for(const QFileInfo& info: dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden  | QDir::AllDirs | QDir::Files, QDir::DirsFirst))
        {
            if (info.isDir())
                result &= removeDir(info.absoluteFilePath());
            else
                result &= QFile::remove(info.absoluteFilePath());
        }

        result &= dir.rmdir(dirName);
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
#ifdef Q_OS_MACX
    QString moviesDir = mac_getMoviesDir();
    return moviesDir.isEmpty() ? QString() : moviesDir + QLatin1String("/") + QnAppInfo::mediaFolderName();
#else
    const QStringList& moviesDirs = QStandardPaths::standardLocations(QStandardPaths::MoviesLocation);
    return moviesDirs.isEmpty() ? QString() : (moviesDirs[0] + QLatin1String("/") + QnAppInfo::mediaFolderName()) ;
#endif
}

QString getBackgroundsDirectory() {
    const QStringList& pictureFolderList = QStandardPaths::standardLocations(QStandardPaths::PicturesLocation);
    QString baseDir = pictureFolderList.isEmpty() ? QString(): pictureFolderList[0];
#ifdef Q_OS_WIN
    QString productDir = baseDir + QDir::toNativeSeparators(QString(lit("/%1 Backgrounds")).arg(QnAppInfo::productNameLong()));
#else
    QString productDir = QDir::toNativeSeparators(QString(lit("/opt/%1/client/share/pictures/sample-backgrounds")).arg(QnAppInfo::linuxOrganizationName()));
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

bool isLocalPath(const QString& folder)
{
    return folder.length() >= 2 && folder[1] == L':';
}

QString getParentFolder(const QString& root)
{
    QString newRoot = QDir::toNativeSeparators(root);
    if (newRoot.endsWith(QDir::separator()))
        newRoot.chop(1);
    return newRoot.left(newRoot.lastIndexOf(QDir::separator())+1);
}

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
    if (!status && isLocalPath(root)) {
        QString newRoot = getParentFolder(root);
        if (!newRoot.isEmpty())
            return getDiskFreeSpace(newRoot); // try parent folder
    }
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
    if (!status && isLocalPath(root)) {
        QString newRoot = getParentFolder(root);
        if (!newRoot.isEmpty())
            return getDiskTotalSpace(newRoot); // try parent folder
    }
    return totalNumberOfBytes;
};

#elif defined(Q_OS_ANDROID)

qint64 getDiskFreeSpace(const QString& root) {
    return 0; // TODO: #android
}

qint64 getDiskTotalSpace(const QString& root) {
    return 0; // TODO: #android
}

#else
qint64 getDiskFreeSpace(const QString& root) {
    struct statvfs buf;
    if (statvfs(root.toUtf8().data(), &buf) == 0)
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
    struct statvfs buf;
    if (statvfs(root.toUtf8().data(), &buf) == 0)
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

#ifdef _DEBUG
QString debugTime(qint64 timeMSec, const QString &fmt) {
    QString format = fmt.isEmpty() ? lit("hh:mm:ss") : fmt;
    return QDateTime::fromMSecsSinceEpoch(timeMSec).toString(format);
}
#endif

int formatJSonStringInternal(const char* srcPtr, const char* srcEnd, char* dstPtr)
{
    static const int INDENT_SIZE = 4;           // how many space add to formatting
    static const char INDENT_SYMBOL = ' ';      // space filler
    static QByteArray OUTPUT_DELIMITER("\n");   // new line
    static const QByteArray INPUT_DELIMITERS("[]{},");  // delimiters in a input string
    static const int INDENTS[] = {1, -1, 1, -1, 0};     // indent offset for INPUT_DELIMITERS
    const char* dstPtrBase = dstPtr;
    const char* srcPtrBase = srcPtr;
    int indent = 0;
    bool quoted = false;
    for (; srcPtr < srcEnd; ++srcPtr)
    {
        if (*srcPtr == '"' && (srcPtr > srcPtrBase || srcPtr[-1] != '\\'))
            quoted = !quoted;

        int symbolIdx = INPUT_DELIMITERS.indexOf(*srcPtr);
        bool isDelimBefore = (symbolIdx >= 0 && INDENTS[symbolIdx] < 0);
        if (!dstPtrBase)
            dstPtr++;
        else if (!isDelimBefore)
            *dstPtr++ = *srcPtr;

        if (symbolIdx >= 0 && !quoted)
        {
            if (dstPtrBase) 
                memcpy(dstPtr, OUTPUT_DELIMITER.data(), OUTPUT_DELIMITER.size());
            dstPtr += OUTPUT_DELIMITER.size();
            indent += INDENT_SIZE * INDENTS[symbolIdx];
            if (dstPtrBase)
                memset(dstPtr, INDENT_SYMBOL, indent);
            dstPtr += indent;
        }

        if (dstPtrBase && isDelimBefore)
            *dstPtr++ = *srcPtr;
    }
    return dstPtr - dstPtrBase;
}

QByteArray formatJSonString(const QByteArray& data)
{
    QByteArray result;
    result.resize(formatJSonStringInternal(data.data(), data.data() + data.size(), 0));
    formatJSonStringInternal(data.data(), data.data() + data.size(), result.data());
    return result;
}
