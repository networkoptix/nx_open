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
Q_DECLARE_OPERATORS_FOR_FLAGS(Options)

QString NX_UTILS_API symLinkTarget(const QString& linkPath);

Result NX_UTILS_API copy(const QString& sourcePath, const QString& targetPath,
    Options options = NoOption);

bool NX_UTILS_API ensureDir(const QDir& dir);

// Functions below are intended for usage in situations where QCoreApplication is unavailable
QString NX_UTILS_API applicationFilePath(const QString& defaultFilePath);
QString NX_UTILS_API applicationDirPath(const QString& defaultFilePath);

QString NX_UTILS_API applicationFilePath(int argc, char** argv);
QString NX_UTILS_API applicationDirPath(int argc, char** argv);

#ifdef Q_OS_WIN
QString applicationFileNameInternal(const QString& defaultFileName);

struct WinDriveInfo
{
    enum Access
    {
        NoAccess = 0,
        Readable = 1,
        Writable = 2,
    };

    QString path;
    unsigned long type;
    int access;

    WinDriveInfo() :
        type(0),
        access(NoAccess)
    {
    }
};

/** @param driveString - any path, starting with drive letter */
bool NX_UTILS_API mediaIsInserted(const QString& driveString);

using WinDriveInfoList = QList<WinDriveInfo>;

WinDriveInfoList NX_UTILS_API getWinDrivesInfo();
#endif

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)

bool NX_UTILS_API isUsb(const QString& devName);

#endif

bool NX_UTILS_API isSafeRelativePath(const QString& path);

} // namespace file_system
} // namespace utils
} // namespace nx
