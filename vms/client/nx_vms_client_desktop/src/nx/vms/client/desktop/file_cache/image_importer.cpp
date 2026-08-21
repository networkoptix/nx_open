// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "image_importer.h"

#include <QtCore/QFileInfo>
#include <QtGui/QImageReader>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/utils/filename_utils.h>
#include <nx/vms/client/desktop/file_cache/file_cache_utils.h>
#include <nx/vms/client/desktop/image_providers/threaded_image_loader.h>

using nx::vms::client::core::FileCache;

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
    const QnAspectRatio& aspectRatio,
    const QString& cachedImageFilename)
{
    if (!NX_ASSERT(m_targetCache))
    {
        emit imported(/*filename*/ {}, FileCache::OperationResult::invalidOperation);
        return;
    }

    auto importImage =
        [this, sourcePath, aspectRatio](const QString& cachedImageFilename)
        {
            if (cachedImageFilename.isEmpty())
            {
                NX_WARNING(this, "Cannot read image file: %1", sourcePath);
                emit imported(/*filename*/ {}, FileCache::OperationResult::fileSystemError);
                return;
            }

            if (!NX_ASSERT(core::isFilenameSafe(cachedImageFilename),
                "Cached image file name must be safe: %1", sourcePath))
            {
                emit imported(cachedImageFilename, FileCache::OperationResult::invalidOperation);
                return;
            }

            const auto targetPath = m_targetCache
                ? m_targetCache->absoluteFilePath(cachedImageFilename)
                : QString();

            if (targetPath.isEmpty())
            {
                emit imported(cachedImageFilename, FileCache::OperationResult::invalidOperation);
                return;
            }

            QImageReader reader(sourcePath);
            reader.setDecideFormatFromContent(true);
            const QSize sourceSize = reader.size();

            // The cache file name is derived from the file content, so the source file is copied
            // as is unless the loader actually has to change the image.
            if (!aspectRatio.isValid()
                && sourceSize.isValid()
                && sourceSize.boundedTo(file_cache::maxBackgroundImageSize()) == sourceSize)
            {
                if (QFileInfo::exists(targetPath))
                {
                    emit imported(cachedImageFilename, FileCache::OperationResult::ok);
                    return;
                }

                file_cache::copyFileAsync(sourcePath, targetPath,
                    [this, cachedImageFilename](bool success)
                    {
                        emit imported(cachedImageFilename, success
                            ? FileCache::OperationResult::ok
                            : FileCache::OperationResult::fileSystemError);
                    },
                    this);
                return;
            }

            transformImage(sourcePath, aspectRatio, targetPath);
        };

    if (!cachedImageFilename.isEmpty())
    {
        importImage(cachedImageFilename);
        return;
    }

    file_cache::cachedImageFilenameAsync(sourcePath, std::move(importImage), this);
}

void ImageImporter::transformImage(
    const QString& sourcePath,
    const QnAspectRatio& aspectRatio,
    const QString& targetPath)
{
    auto loader = new ThreadedImageLoader(this);
    loader->setInput(sourcePath);
    loader->setSize(file_cache::maxBackgroundImageSize());
    loader->setAspectRatio(aspectRatio);
    connect(loader, &ThreadedImageLoader::imageLoaded, this,
        [this, loader, sourcePath, targetPath](const QImage& image)
        {
            loader->deleteLater();

            if (image.isNull())
            {
                NX_WARNING(this, "Cannot read image file: %1", sourcePath);
                emit imported(/*filename*/ {}, FileCache::OperationResult::fileSystemError);
                return;
            }

            // The transformed image differs from the source, so its own content defines the name.
            const QFileInfo targetInfo(targetPath);
            file_cache::storeImageAsync(targetInfo.absolutePath(), image, targetInfo.suffix(),
                [this](const QString& filename)
                {
                    emit imported(filename, filename.isEmpty()
                        ? FileCache::OperationResult::fileSystemError
                        : FileCache::OperationResult::ok);
                },
                this);
        });
    loader->start();
}

} // namespace nx::vms::client::desktop
