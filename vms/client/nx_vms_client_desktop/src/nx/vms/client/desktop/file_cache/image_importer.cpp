// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "image_importer.h"

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/file_cache/file_cache_utils.h>
#include <nx/vms/client/desktop/image_providers/threaded_image_loader.h>

namespace nx::vms::client::desktop {

ImageImporter::ImageImporter(FileCache* targetCache, QObject* parent):
    QObject(parent),
    m_targetCache(targetCache)
{
    NX_ASSERT(m_targetCache);
}

ImageImporter::~ImageImporter() = default;

void ImageImporter::importFromFile(
    const QString& sourcePath,
    const QnAspectRatio& aspectRatio)
{
    if (!NX_ASSERT(m_targetCache))
    {
        emit imported({}, FileCache::OperationResult::invalidOperation);
        return;
    }

    const auto filename = file_cache::cachedImageFilename(sourcePath);
    if (!NX_ASSERT(file_cache::isFilenameSafe(filename),
        "Cached image file name must be safe: %1", sourcePath))
    {
        emit imported(filename, FileCache::OperationResult::invalidOperation);
        return;
    }

    const auto targetPath = m_targetCache->absoluteFilePath(filename);
    if (targetPath.isEmpty())
    {
        emit imported(filename, FileCache::OperationResult::invalidOperation);
        return;
    }

    auto loader = new ThreadedImageLoader(this);
    loader->setInput(sourcePath);
    loader->setSize(file_cache::maxBackgroundImageSize());
    loader->setAspectRatio(aspectRatio);
    // Encode and write happen inside the loader's worker thread.
    loader->setOutput(targetPath);
    connect(loader, &ThreadedImageLoader::imageSaved, this,
        [this, loader, filename](const QString& savedPath)
        {
            loader->deleteLater();
            emit imported(filename, savedPath.isEmpty()
                ? FileCache::OperationResult::fileSystemError
                : FileCache::OperationResult::ok);
        });
    loader->start();
}

} // namespace nx::vms::client::desktop
