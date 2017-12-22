#include <nx/utils/log/log.h>
#include "file_system.h"

#if defined(Q_OS_UNIX)
    #include <unistd.h>
    #include <QtCore/QStandardPaths>
#endif

#if defined(Q_OS_MAC)
    #include <sys/syslimits.h>
    #include <CoreFoundation/CoreFoundation.h>
#endif

#if defined(Q_OS_WIN)
    #include <windows.h>
#endif

#include <QtCore/QFileInfo>
#include <QtCore/QFile>
#include <QtCore/QDir>

namespace {

#ifdef Q_OS_MAC

QString fromCFStringToQString(CFStringRef str)
{
    if (!str)
        return QString();

    CFIndex length = CFStringGetLength(str);
    if (length == 0)
        return QString();

    QString string(length, Qt::Uninitialized);
    CFStringGetCharacters(str, CFRangeMake(0, length), reinterpret_cast<UniChar *>
        (const_cast<QChar *>(string.unicode())));
    return string;
}

#endif

} // namespace

namespace nx {
namespace utils {
namespace file_system {

Result copy(const QString& sourcePath, const QString& targetPath, Options options)
{
    const QFileInfo srcFileInfo(sourcePath);
    if (!srcFileInfo.exists())
        return Result(Result::sourceDoesNotExist, sourcePath);

    QFileInfo tgtFileInfo(targetPath);

    {
        auto fullTargetPath = targetPath;
        if (tgtFileInfo.exists() && tgtFileInfo.isDir())
            fullTargetPath = QDir(targetPath).absoluteFilePath(srcFileInfo.fileName());

        tgtFileInfo = QFileInfo(fullTargetPath);
    }

    if (srcFileInfo.absoluteFilePath() == tgtFileInfo.absoluteFilePath())
        return Result(Result::sourceAndTargetAreSame, sourcePath);

    {
        auto targetDir = tgtFileInfo.dir();
        if (!targetDir.exists())
        {
            if (!options.testFlag(CreateTargetPath) || !targetDir.mkpath("."))
                return Result(Result::cannotCreateDirectory, targetDir.path());
        }
    }

    if (srcFileInfo.isSymLink()
        && srcFileInfo.symLinkTarget() == tgtFileInfo.absoluteFilePath())
    {
        return Result(Result::sourceAndTargetAreSame, sourcePath);
    }

    if (srcFileInfo.isDir() && (!srcFileInfo.isSymLink() || options.testFlag(FollowSymLinks)))
    {
        if (!tgtFileInfo.exists())
        {
            auto targetDir = tgtFileInfo.dir();
            if (!targetDir.mkdir(tgtFileInfo.fileName()))
                return Result(Result::cannotCreateDirectory, targetPath);
        }
        else if (!tgtFileInfo.isDir())
        {
            return Result(Result::cannotCreateDirectory, targetPath);
        }

        const auto sourceDir = QDir(sourcePath);

        const auto entries = sourceDir.entryList(
            QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        for (const auto& entry: entries)
        {
            const auto newSrcFilePath = sourceDir.absoluteFilePath(entry);

            const auto result = copy(newSrcFilePath, tgtFileInfo.absoluteFilePath(), options);
            if (!result)
                return result;
        }
    }
    else
    {
        const auto targetFilePath = tgtFileInfo.absoluteFilePath();
        if (tgtFileInfo.exists())
        {
            if (options.testFlag(SkipExisting))
                return Result(Result::ok);

            if (!options.testFlag(OverwriteExisting) || !QFile::remove(targetFilePath))
                return Result(Result::alreadyExists, targetFilePath);
        }

        if (srcFileInfo.isSymLink() && !options.testFlag(FollowSymLinks))
        {
            if (!QFile::link(symLinkTarget(srcFileInfo.absoluteFilePath()), targetFilePath))
                return Result(Result::cannotCopy, sourcePath);
        }
        else
        {
            if (!QFile::copy(sourcePath, targetFilePath))
                return Result(Result::cannotCopy, sourcePath);
        }
    }

    return Result(Result::ok);
}

QString symLinkTarget(const QString& linkPath)
{
    const QFileInfo linkInfo(linkPath);

    if (!linkInfo.isSymLink())
        return QString();

    #if defined(Q_OS_UNIX)
        char target[PATH_MAX + 1];
        const auto path = QFile::encodeName(linkPath);
        int len = readlink(path, target, PATH_MAX);
        if (len <= 0)
            return QString();
        target[len] = '\0';
        return QFile::decodeName(QByteArray(target));
    #else
        return linkInfo.symLinkTarget();
    #endif
}

bool ensureDir(const QDir& dir)
{
    return dir.exists() || dir.mkpath(".");
}

#ifdef Q_OS_WIN
#if defined(Q_OS_WINRT) && _MSC_VER < 1900
QString applicationFileNameInternal(const QString& defaultFileName)
{
    return defaultFileName;
}
#else
QString applicationFileNameInternal(const QString& /*defaultFileName*/)
{
    // We do MAX_PATH + 2 here, and request with MAX_PATH + 1, so we can handle all paths
    // up to, and including MAX_PATH size perfectly fine with string termination, as well
    // as easily detect if the file path is indeed larger than MAX_PATH, in which case we
    // need to use the heap instead. This is a work-around, since contrary to what the
    // MSDN documentation states, GetModuleFileName sometimes doesn't set the
    // ERROR_INSUFFICIENT_BUFFER error number, and we thus cannot rely on this value if
    // GetModuleFileName(0, buffer, MAX_PATH) == MAX_PATH.
    // GetModuleFileName(0, buffer, MAX_PATH + 1) == MAX_PATH just means we hit the normal
    // file path limit, and we handle it normally, if the result is MAX_PATH + 1, we use
    // heap (even if the result _might_ be exactly MAX_PATH + 1, but that's ok).
    wchar_t buffer[MAX_PATH + 2];
    DWORD v = GetModuleFileName(0, buffer, MAX_PATH + 1);
    buffer[MAX_PATH + 1] = 0;

    if (v == 0)
        return QString();
    else if (v <= MAX_PATH)
        return QString::fromWCharArray(buffer);

    // MAX_PATH sized buffer wasn't large enough to contain the full path, use heap
    wchar_t *b = 0;
    int i = 1;
    size_t size;
    do
    {
        ++i;
        size = MAX_PATH * i;
        b = reinterpret_cast<wchar_t *>(realloc(b, (size + 1) * sizeof(wchar_t)));
        if (b)
            v = GetModuleFileName(NULL, b, DWORD(size));
    } while (b && v == size);

    if (b)
        *(b + size) = 0;
    QString res = QString::fromWCharArray(b);
    free(b);

    return res;
}
#endif // #if defined(Q_OS_WINRT) && _MSC_VER < 1900

#endif // #ifdef Q_OS_WIN

QString applicationDirPath(const QString& defaultFilePath)
{
    return QFileInfo(applicationFilePath(defaultFilePath)).path();
}

QString applicationDirPath(int argc, char** argv)
{
    return QFileInfo(applicationFilePath(argc, argv)).path();
}

QString applicationFilePath(const QString& defaultFilePath)
{

#if defined(Q_OS_WIN)
    return QFileInfo(applicationFileNameInternal(defaultFilePath)).filePath();
#elif defined(Q_OS_MAC)
    auto bundleUrl = CFBundleCopyExecutableURL(CFBundleGetMainBundle());
    if (bundleUrl)
    {
        auto path = CFURLCopyFileSystemPath(bundleUrl, kCFURLPOSIXPathStyle);
        if (path)
            return fromCFStringToQString(path);
    }
#endif
#if defined( Q_OS_UNIX )
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    // Try looking for a /proc/<pid>/exe symlink first which points to
    // the absolute path of the executable.
    QFileInfo pfi(QString::fromLatin1("/proc/%1/exe").arg(getpid()));
    if (pfi.exists() && pfi.isSymLink())
        return pfi.canonicalFilePath();

#endif
    QString argv0 = QFile::decodeName(defaultFilePath.toLocal8Bit());
    QString absPath;

    if (!argv0.isEmpty() && argv0.at(0) == QLatin1Char('/'))
    {
        // If argv0 starts with a slash, it is already an absolute
        // file path.
        absPath = argv0;
    }
    else if (argv0.contains(QLatin1Char('/')))
    {
        // If argv0 contains one or more slashes, it is a file path
        // relative to the current directory.
        absPath = QDir::current().absoluteFilePath(argv0);
    }
    else
    {
        // Otherwise, the file path has to be determined using the
        // PATH environment variable.
        absPath = QStandardPaths::findExecutable(argv0);
    }

    absPath = QDir::cleanPath(absPath);

    QFileInfo fi(absPath);
    if (fi.exists())
        fi.canonicalFilePath();

#endif
    return QString();
}

QString applicationFilePath(int argc, char** argv)
{
    QString defaultFilePath;
    if (argc > 0)
        defaultFilePath = QString::fromUtf8(argv[0]);

    return applicationFilePath(defaultFilePath);
}

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)

bool isUsb(const QString& devName)
{
    for (const QFileInfo& info: QDir("/dev/disk/by-id").entryInfoList(QDir::System))
    {
        if (info.fileName().contains("usb-") && info.symLinkTarget().contains(devName))
            return true;
    }

    return false;
}

#endif

} // namespace file_system
} // namespace utils
} // namespace nx
