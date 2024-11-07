// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QDir>
#include <QtCore/QFlags>
#include <QtCore/QString>

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
        sourceAndTargetAreSame,
        targetFolderDoesNotExist,
        cannotMove,
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

/**
 * Moves a file or a folder to the new location. Non-recursive. This function behavior is meant to
 * be close to the Unix move utility.
 *
 * 1 If `sourcePath` is a file
 *    1.1 If `targetPath` exists
 *        1.1.1 If `targetPath` is a file
 *            a) If `options` does not have OverwriteExisting  - returns alreadyExists
 *            b) Otherwise removes `targetPath` and tries to move or copy/remove the original.
 *        1.1.2 If `targetPath` is a folder
 *            1.1.2.1 If a file with the name `targetPath` + `fileName(sourcePath)` already exists
 *                a) If `options` does not have OverwriteExisting  - returns alreadyExists
 *                b) Otherwise removes `targetPath` + `fileName(sourcePath)` and tries to move or copy/remove the original.
 *            1.1.2.2 If a file with the name `fileName(sourcePath)` does not exist - moves the original to `targetPath`
 *    1.2 If `targetPath` does not exist
 *        1.2.1 If an enclosing folder for `targetPath` exists (`targetPath` = /tmp/111, enclosing = /tmp) -
 *            rename `sourcePath` to `targetPath`
 *        1.2.2 Otherwise return targetFolderDoesNotExist
 * 2 If `sourcePath` is a folder
 *    2.1 If `targetPath` exists
 *        2.1.1 If `targetPath` is a file - returns cannotMove
 *        2.1.2 If `targetPath` is a folder - moves the source folder inside the target folder
 *    2.2 If `targetPath` does not exist
 *        2.2.1 If an enclosing folder for `targetPath` exists (`targetPath` = /tmp/111, enclosing = /tmp) -
 *            rename `sourcePath` to `targetPath`
 *        2.2.2 Otherwise return targetFolderDoesNotExist
 */
Result NX_UTILS_API move(
    const QString& sourcePath, const QString& targetPath, bool replaceExisting = false);

/**
 * Move `sourcePath` folder contents to `targetPath` folder recursively. Both source and target
 * folders must exist for the function to succeed. Moving is performed recursively.
 */
Result NX_UTILS_API moveFolderContents(
    const QString& sourcePath, const QString& targetPath, bool replaceExisting);

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
