// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "file_cache.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QSaveFile>
#include <QtCore/QSet>
#include <QtGui/QImage>

#include <nx/utils/log/log_main.h>
#include <nx/vms/client/core/common/utils/thread_pool.h>

namespace nx::vms::client::core {

FileCache::FileCache(QObject* parent):
    QObject(parent)
{
}

FileCache::~FileCache() = default;

bool FileCache::hasAllowedImageExtension(const QString& filename)
{
    static const QSet<QString> kAllowedImageExtensions{"png", "jpg", "jpeg"};
    return kAllowedImageExtensions.contains(QFileInfo(filename).suffix());
}

ThreadPool* FileCache::ioThreadPool()
{
    static const QString kThreadPoolId = "FileCache_thread_pool";
    return ThreadPool::instance(kThreadPoolId);
}

bool FileCache::ensureCacheFolder() const
{
    if (QDir().mkpath(cacheFolder()))
        return true;

    NX_WARNING(this, "Impossible to create folder: %1", cacheFolder());
    return false;
}

FileCache::OperationResult FileCache::storeImage(
    const QString& unsafeFilename, const QImage& image)
{
    if (image.isNull())
    {
        NX_WARNING(this, "Rejecting cache write with null image: %1", unsafeFilename);
        return OperationResult::invalidOperation;
    }

    return writeImageFile(unsafeFilename,
        [&image](QSaveFile& file) { return image.save(&file); });
}

FileCache::OperationResult FileCache::storeImageData(
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

FileCache::OperationResult FileCache::writeImageFile(
    const QString& unsafeFilename,
    const std::function<bool(QSaveFile&)>& writer)
{
    if (!hasAllowedImageExtension(unsafeFilename))
    {
        NX_WARNING(this, "Rejecting cache write with non-image extension: %1", unsafeFilename);
        return OperationResult::invalidOperation;
    }

    const auto path = absoluteFilePath(unsafeFilename);
    if (path.isEmpty())
    {
        NX_WARNING(this, "Rejecting unsafe filename: %1", unsafeFilename);
        return OperationResult::invalidOperation;
    }

    if (!ensureCacheFolder())
        return OperationResult::fileSystemError;

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        NX_WARNING(this, "Failed to open cache file for writing: %1 (%2)",
            path, file.errorString());
        return OperationResult::fileSystemError;
    }

    if (!writer(file))
    {
        NX_WARNING(this, "Failed to write cache file: %1 (%2)", path, file.errorString());
        // QSaveFile destructor discards the temporary file when commit() is not called.
        return OperationResult::fileSystemError;
    }

    if (!file.commit())
    {
        NX_WARNING(this, "Failed to commit cache file: %1 (%2)", path, file.errorString());
        return OperationResult::fileSystemError;
    }

    return OperationResult::ok;
}

} // namespace nx::vms::client::core
