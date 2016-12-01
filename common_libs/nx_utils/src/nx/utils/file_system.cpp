#include "file_system.h"

#if defined(Q_OS_UNIX)
    #include <unistd.h>
#endif

#include <QtCore/QFileInfo>
#include <QtCore/QFile>
#include <QtCore/QDir>

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

    {
        auto targetDir = tgtFileInfo.dir();
        if (!targetDir.exists())
        {
            if (!options.testFlag(CreateTargetPath) || !targetDir.mkpath("."))
                return Result(Result::cannotCreateDirectory, targetDir.path());
        }
    }

    if (srcFileInfo.isDir() && (!srcFileInfo.isSymLink() || options.testFlag(FollowSymLinks)))
    {
        if (!tgtFileInfo.exists())
        {
            auto targetDir = tgtFileInfo.dir();
            if (!targetDir.mkdir(tgtFileInfo.fileName()))
                return Result(Result::cannotCreateDirectory, targetPath);
        }

        const auto sourceDir = QDir(sourcePath);

        const auto entries = sourceDir.entryList(
            QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        foreach (const auto& entry, entries)
        {
            const auto newSrcFilePath = sourceDir.absoluteFilePath(entry);

            const auto result = copy(newSrcFilePath, tgtFileInfo.absoluteFilePath(), options);
            if (!result.isOk())
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
    #if defined(Q_OS_UNIX)
        if (!QFileInfo(linkPath).isSymLink())
            return QString();

        char target[PATH_MAX + 1];
        const auto path = QFile::encodeName(linkPath);
        int len = readlink(path, target, PATH_MAX);
        if (len <= 0)
            return QString();
        target[len] = '\0';
        return QFile::decodeName(QByteArray(target));
    #else
        return QString();
    #endif
}

} // namespace file_system
} // namespace utils
} // namespace nx
