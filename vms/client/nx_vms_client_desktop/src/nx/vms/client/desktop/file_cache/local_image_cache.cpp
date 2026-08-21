// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "local_image_cache.h"

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>

#include <common/common_globals.h>
#include <nx/vms/client/core/utils/filename_utils.h>

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

    const QString safeFilename = core::sanitizeFilename(unsafeFilename);
    if (safeFilename.isEmpty())
        return {};

    return QDir::toNativeSeparators(cacheFolder() + QDir::separator() + safeFilename);
}

void LocalImageCache::clear()
{
    // Nothing to reset.
}

} // namespace nx::vms::client::desktop
