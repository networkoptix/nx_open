#pragma once

#include <QtCore/QString>
#include <QtCore/QFlags>
#include <QtCore/QDir>

namespace nx {
namespace utils {
namespace file_system {

struct NX_UTILS_API Result
{
    enum ResultCode
    {
        ok,
        sourceDoesNotExist,
        alreadyExists,
        cannotCopy,
        cannotCreateDirectory,
        sourceAndTargetAreSame
    };

    ResultCode code;
    QString path;

    Result(ResultCode code, const QString& path = QString()): code(code), path(path) {}

    explicit operator bool() const { return code == ok; }
};

enum Option
{
    NoOption = 0,
    OverwriteExisting = 0x01,
    SkipExisting = 0x02,
    CreateTargetPath = 0x04,
    FollowSymLinks = 0x08
};
Q_DECLARE_FLAGS(Options, Option)

QString NX_UTILS_API symLinkTarget(const QString& linkPath);

Result NX_UTILS_API copy(const QString& sourcePath, const QString& targetPath,
    Options options = NoOption);

bool NX_UTILS_API ensureDir(const QDir& dir);

// The functions below are "inspired" by Qt. Please look at the Qt source code
// if there are any questions.
QString NX_UTILS_API applicationFilePath(const QString& defaultFilePath);
QString NX_UTILS_API applicationDirPath(const QString& defaultFilePath);

#ifdef Q_OS_WIN
QString applicationFileNameInternal(const QString& defaultFileName);
#endif

} // namespace file_system
} // namespace utils
} // namespace nx
