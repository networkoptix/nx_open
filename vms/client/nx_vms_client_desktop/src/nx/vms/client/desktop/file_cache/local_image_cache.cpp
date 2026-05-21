// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "local_image_cache.h"

#include <QtCore/QDir>
#include <QtCore/QSaveFile>
#include <QtCore/QStandardPaths>

#include <common/common_globals.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/file_cache/file_cache_utils.h>

namespace nx::vms::client::desktop {

namespace {

bool isResourceFile(const QString& filename)
{
    return filename.startsWith(":/") || filename.startsWith("qrc://");
}

} // namespace

LocalImageCache::LocalImageCache(QObject* parent):
    base_type(parent)
{
}

LocalImageCache::~LocalImageCache() = default;

QString LocalImageCache::cacheFolder() const
{
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return QDir::toNativeSeparators(QString("%1/cache/local/%2")
        .arg(root)
        .arg(Qn::kWallpapersFolder));
}

QString LocalImageCache::absoluteFilePath(const QString& unsafeFilename) const
{
    if (isResourceFile(unsafeFilename))
    {
        // Resource files are always safe and don't live in the cache folder.
        return unsafeFilename;
    }

    const QString safeFilename = file_cache::sanitizeFilename(unsafeFilename);
    if (safeFilename.isEmpty())
        return {};

    return QDir::toNativeSeparators(cacheFolder() + QDir::separator() + safeFilename);
}

void LocalImageCache::clear()
{
    // Nothing to reset.
}

FileCache::OperationResult LocalImageCache::storeImageData(
    const QString& unsafeFilename, const QByteArray& imageData)
{
    if (imageData.isEmpty())
    {
        NX_WARNING(this, "Rejecting cache write with empty image data: %1", unsafeFilename);
        return OperationResult::invalidOperation;
    }

    return writeImageFile(unsafeFilename,
        [&imageData](QSaveFile& file)
        {
            return file.write(imageData) == imageData.size();
        });
}

} // namespace nx::vms::client::desktop
