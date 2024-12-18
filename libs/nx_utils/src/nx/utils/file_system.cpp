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

#include <nx/utils/log/log.h>

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

nx::log::Tag kLogTag{QString{"FileSystemUtils"}};

static Result moveToExisting(
    const QString& sourcePath, const QString& targetPath, bool replaceExisting)
{
    QFileInfo tgtFileInfo(targetPath);
    QFileInfo srcFileInfo(sourcePath);
    const auto targetFilePath = tgtFileInfo.absoluteFilePath() + "/" + srcFileInfo.fileName();
    NX_VERBOSE(
        kLogTag, "%1: src: %2, tgt: %3, tgt file path: %4",
        __func__, sourcePath, targetPath, targetFilePath);
    if (QFileInfo(targetFilePath).exists())
    {
        NX_VERBOSE(kLogTag, "%1: tgt file path %2 exists", __func__, targetFilePath);
        if (replaceExisting)
        {
            if (!QFile::remove(targetFilePath))
            {
                NX_VERBOSE(
                    kLogTag, "%1: ReplaceExisting is true but removing %2 failed",
                    __func__, targetFilePath);
                return Result::cannotMove;
            }

            if (QFile::rename(sourcePath, targetFilePath))
            {
                NX_VERBOSE(
                    kLogTag, "%1: Sucessfully renamed %2 to %3",
                    __func__, sourcePath, targetFilePath);
                return Result::ok;
            }

            NX_DEBUG(
                kLogTag, "%1: Failed to rename %2 to %3",
                __func__, sourcePath, targetFilePath);
            return Result::cannotMove;
        }

        NX_VERBOSE(
            kLogTag, "%1: ReplaceExisting is false but tgt %2 exists",
            __func__, targetFilePath);
        return Result::alreadyExists;
    }

    if (QFile::rename(sourcePath, targetFilePath))
    {
        NX_VERBOSE(
            kLogTag, "%1: Target path does not exists. Sucessfully renamed %2 to %3",
            __func__, sourcePath, targetFilePath);
        return Result::ok;
    }

    NX_DEBUG(
        kLogTag, "%1: Target path does not exist. Failed to rename %2 to %3",
        __func__, sourcePath, targetFilePath);
    return Result::cannotMove;
}

static Result moveIfTargetDoesNotExist(const QString& sourcePath, const QString& targetPath)
{
    QFileInfo tgtFileInfo(targetPath);
    QFileInfo srcFileInfo(sourcePath);
    const auto targetParentDir = tgtFileInfo.absoluteDir();
    NX_VERBOSE(
        kLogTag, "%1: src: %2, tgt: %3, parent %4",
        __func__, sourcePath, targetPath, targetParentDir);
    if (targetParentDir.exists())
    {
        if (QFile::rename(sourcePath, targetPath))
        {
            NX_VERBOSE(
                kLogTag, "%1: Rename %2 to %3 succeeded",
                __func__, sourcePath, targetPath);
            return Result::ok;
        }

        NX_DEBUG(
            kLogTag, "%1: Failed to rename %2 to %3",
            __func__, sourcePath, targetPath);
        return Result::cannotMove;
    }

    NX_DEBUG(
        kLogTag, "%1: tgt %2 parent dir %3 does not exist",
        __func__, targetPath, targetParentDir);
    return Result::targetFolderDoesNotExist;
}

Result move(const QString& sourcePath, const QString& targetPath, bool replaceExisting)
{
    const QFileInfo srcFileInfo(sourcePath);
    if (!srcFileInfo.exists())
    {
        NX_DEBUG(kLogTag, "%1: Src %2 does not exists", __func__, sourcePath);
        return Result(Result::sourceDoesNotExist, sourcePath);
    }

    const QFileInfo tgtFileInfo(targetPath);
    if (srcFileInfo.isFile())
    {
        NX_VERBOSE(kLogTag, "%1: Src %2 is file", __func__, sourcePath);
        if (!tgtFileInfo.exists())
        {
            NX_VERBOSE(kLogTag, "%1: Tgt %2 does not exist", __func__, targetPath);
            return moveIfTargetDoesNotExist(sourcePath, targetPath);
        }

        if (tgtFileInfo.isFile())
        {
            NX_VERBOSE(kLogTag, "%1: Tgt %2 is existing file", __func__, targetPath);
            if (replaceExisting)
            {
                if (!QFile::remove(tgtFileInfo.absoluteFilePath()))
                {
                    NX_DEBUG(
                        kLogTag, "%1: ReplaceExisting is true but removing of %2 failed",
                        __func__, targetPath);
                    return Result::cannotMove;
                }

                if (QFile::rename(sourcePath, targetPath))
                {
                    NX_VERBOSE(
                        kLogTag, "%1: Successfully renamed %2 to %3",
                        __func__, sourcePath, targetPath);
                    return Result::ok;
                }

                NX_DEBUG(
                    kLogTag, "%1: Failed to rename %2 to %3",
                    __func__, sourcePath, targetPath);
                return Result::cannotMove;
            }

            NX_DEBUG(
                kLogTag, "%1: ReplaceExisting is false but target %2 exists",
                __func__, targetPath);
            return Result::alreadyExists;
        }

        // targetPath is a folder
        return moveToExisting(sourcePath, targetPath, replaceExisting);
    }

    // sourcePath is a folder
    if (!tgtFileInfo.exists())
    {
        NX_VERBOSE(
            kLogTag, "%1: Src %2 is dir and tgt %3 does not exist",
            __func__, sourcePath, targetPath);
        return moveIfTargetDoesNotExist(sourcePath, targetPath);
    }

    if (tgtFileInfo.isFile())
    {
        NX_DEBUG(
            kLogTag, "%1: Src %2 is dir but tgt %3 is file",
            __func__, sourcePath, targetPath);
        return Result::cannotMove;
    }

    // targetPath is a folder
    return moveToExisting(sourcePath, targetPath, replaceExisting);
}

Result moveFolderContents(
    const QString& sourcePath, const QString& targetPath, bool replaceExisting)
{
    const QFileInfo srcFileInfo(sourcePath);
    const QFileInfo tgtFileInfo(targetPath);
    if (!srcFileInfo.exists() || !tgtFileInfo.exists() || !srcFileInfo.isDir() || !tgtFileInfo.isDir())
    {
        NX_DEBUG(kLogTag,
            "Failed. src %1 exists: %2, tgt %3 exists %4, src is Dir: %5, tgt is Dir: %6",
            sourcePath, srcFileInfo.exists(), targetPath, tgtFileInfo.exists(),
            srcFileInfo.isDir(), tgtFileInfo.isDir());
        return Result::cannotMove;
    }

    const auto contents = QDir(sourcePath).entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    for (const auto& fi: contents)
    {
        NX_VERBOSE(kLogTag,
            "Moving %1 to %2",
            fi.absoluteFilePath(), QDir(targetPath).absoluteFilePath(fi.fileName()));
        Result::ResultCode moveResult = Result::cannotMove;
        if (fi.isFile())
        {
            moveResult = move(
                fi.absoluteFilePath(),
                QDir(targetPath).absoluteFilePath(fi.fileName()),
                replaceExisting).code;

            NX_VERBOSE(kLogTag, "%1 is File. Move result: %2", fi.absoluteFilePath(), moveResult);
        }
        else if (fi.isDir())
        {
            const auto targetDirPath = QDir(targetPath).absoluteFilePath(fi.fileName());
            auto targetDir = QDir(targetDirPath);
            NX_VERBOSE(kLogTag, "%1 is Dir. Exists: %2", fi.absoluteFilePath(), targetDir.exists());
            if (targetDir.exists())
            {
                const auto srcDirPath = fi.absoluteFilePath();
                moveResult = moveFolderContents(
                    srcDirPath, targetDirPath, replaceExisting).code;
                NX_VERBOSE(
                    kLogTag, "%1 is existing Dir. Recursive move result: %2",
                    fi.absoluteFilePath(), moveResult);
                if (moveResult == Result::ok)
                {
                    const auto srcDirContents = QDir(srcDirPath).entryInfoList(
                        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
                    if (srcDirContents.empty())
                        QDir(srcDirPath).removeRecursively();
                }
            }
            else
            {
                moveResult = move(fi.absoluteFilePath(), targetDirPath, replaceExisting).code;
                NX_VERBOSE(
                    kLogTag, "%1 is NOT existing Dir. Move result: %2",
                    fi.absoluteFilePath(), moveResult);
            }
        }
        else
        {
            continue;
        }

        switch (moveResult)
        {
            case Result::cannotMove:
                return Result::cannotMove;
            case Result::ok:
                break;
            case Result::alreadyExists:
                if (replaceExisting)
                    return Result::cannotMove;
                break;
            default:
                NX_ASSERT(false, nx::format("Unexpected state %1", (int) moveResult));
                return Result::cannotMove;
        }
    }

    return Result::ok;
}

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

    // TODO: It makes sense to add some more security checks e.g. symlinks, etc.
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
