// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/utils/log/log.h>

#include "file_system.h"

#if defined(Q_OS_UNIX)
    #include <unistd.h>
    #include <fcntl.h>
#endif

#if defined(Q_OS_MAC)
    #include <sys/syslimits.h>
    #include <CoreFoundation/CoreFoundation.h>
#endif

#if defined(Q_OS_WIN)
    #include <windows.h>
    #include <io.h>
#endif

#include <QtCore/QFileInfo>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QStandardPaths>

namespace {

#ifdef Q_OS_MAC

QString fromCFStringToQString(CFStringRef str)
{
    if (!str)
        return QString();

    CFIndex length = CFStringGetLength(str);
    if (length == 0)
        return QString();

    QString string((int) length, Qt::Uninitialized);
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
        auto len = readlink(path, target, PATH_MAX);
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

bool isRelativePathSafe(const QString& path)
{
    if (path.contains(".."))
        return false; //< May be a path traversal attempt.

    if (path.startsWith('/'))
        return false; //< UNIX root.

    if (path.contains('*') || path.contains('?') || path.contains('[') || path.contains(']'))
        return false; //< UNIX shell wildcards.

    if (path.startsWith('-') || path.startsWith('~'))
        return false; //< UNIX shell special characters.

    if (path.size() > 1 && path[1] == ':')
        return false; //< Windows mount point.

    if (path.startsWith('\\'))
        return false; //< Windows SMB drive.

    // TODO: It makes sense to add some more security cheks e.g. symlinks, etc.
    return true;
}

bool reserveSpace(QFile& file, qint64 size)
{
    if (size <= 0)
        return true;

    #if defined(Q_OS_WIN)
        return _chsize_s(file.handle(), size) == 0;
    #elif defined(Q_OS_MAC)
        fstore_t store = {F_ALLOCATEALL, F_PEOFPOSMODE, 0, size, 0};
        return fcntl(file.handle(), F_PREALLOCATE, &store) == 0;
    #else
        return posix_fallocate(file.handle(), 0, size) == 0;
    #endif
}

bool reserveSpace(const QString& fileName, qint64 size)
{
    QFile file(fileName);
    if (!file.open(QFile::WriteOnly))
        return false;

    return reserveSpace(file, size);
}

} // namespace file_system
} // namespace utils
} // namespace nx
