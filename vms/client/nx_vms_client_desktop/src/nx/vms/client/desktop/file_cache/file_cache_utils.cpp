// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "file_cache_utils.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QSet>
#include <QtCore/QStandardPaths>

#include <nx/utils/uuid.h>
#include <ui/graphics/opengl/gl_functions.h>
#include <utils/common/id.h>

namespace nx::vms::client::desktop::file_cache {

namespace {

const QSet<QString> kAllowedImageExtensions{"png", "jpg", "jpeg"};
const QString kDefaultImageExtension = "png";

} // namespace

bool isFilenameSafe(const QString& unsafeFilename)
{
    return !sanitizeFilename(unsafeFilename).isEmpty();
}

QString sanitizeFilename(const QString& unsafeFilename)
{
    // The last component of the path, which is expected to be the filename.
    // It can be empty if the path ends with a separator or is empty itself.
    QString filename = QFileInfo(unsafeFilename).fileName();

    if (filename.isEmpty()
        || filename == "."
        || filename == ".."
        || filename != unsafeFilename)
    {
        return {};
    }

    // Windows (NTFS) strips trailing whitespace and dots when resolving filenames, so `"foo "`
    // and `"foo."` collide with `"foo"`. Reject such names to keep cache keys unique.
    const QChar last = filename.back();
    if (last == '.' || last.isSpace())
        return {};

    return filename;
}

QString cachedImageFilename(const QString& sourcePath)
{
    return guidFromArbitraryData(sourcePath.toUtf8()).toSimpleString() + '.'
        + (hasAllowedImageExtension(sourcePath)
            ? QFileInfo(sourcePath).suffix()
            : kDefaultImageExtension);
}

bool hasAllowedImageExtension(const QString& filename)
{
    return kAllowedImageExtensions.contains(QFileInfo(filename).suffix());
}

void clearLocalCache()
{
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    if (dir.cd("cache"))
        dir.removeRecursively();
}

QSize maxBackgroundImageSize()
{
    // TODO: #dfisenko This is a rough estimation. Ideally, it should be connected to the chosen
    // graphics backend.
    const int value = QnGlFunctions::estimatedInteger(GL_MAX_TEXTURE_SIZE);
    return QSize(value, value);
}

} // namespace nx::vms::client::desktop::file_cache
