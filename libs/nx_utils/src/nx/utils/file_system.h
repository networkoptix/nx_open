// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)

bool NX_UTILS_API isUsb(const QString& devName);

#endif

bool NX_UTILS_API isRelativePathSafe(const QString& path);

bool NX_UTILS_API reserveSpace(QFile& file, qint64 size);
bool NX_UTILS_API reserveSpace(const QString& fileName, qint64 size);

} // namespace file_system
} // namespace utils
} // namespace nx
