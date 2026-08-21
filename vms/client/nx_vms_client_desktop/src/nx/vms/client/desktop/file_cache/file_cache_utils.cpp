// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "file_cache_utils.h"

#include <QtCore/QBuffer>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QSaveFile>
#include <QtCore/QStandardPaths>
#include <QtGui/QImage>

#include <common/common_globals.h>
#include <core/resource/layout_resource.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/common/utils/thread_pool.h>
#include <nx/vms/client/core/cross_system/cloud_image_cache.h>
#include <nx/vms/client/core/cross_system/cloud_layouts_manager.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/utils/local_file_cache.h>
#include <nx/vms/client/desktop/utils/server_image_cache.h>
#include <nx/vms/client/desktop/system_context.h>
#include <ui/graphics/opengl/gl_functions.h>
#include <utils/common/id.h>

using nx::vms::client::core::FileCache;

namespace nx::vms::client::desktop::file_cache {

namespace {

const QString kDefaultImageExtension = "png";

/** Cache file name identifying the image data, so edits to the image produce a new cache entry. */
QString cachedImageFilename(const QByteArray& imageData, const QString& extension)
{
    return guidFromArbitraryData(imageData).toSimpleString() + '.' + extension;
}

} // namespace

void cachedImageFilenameAsync(
    const QString& sourcePath,
    std::function<void(const QString& filename)> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    FileCache::ioThreadPool()->start(
        [sourcePath, handler = handlerExecutor.bind(std::move(handler))]() mutable
        {
            QString result;
            QFile file(sourcePath);

            if (file.open(QIODevice::ReadOnly))
            {
                result = cachedImageFilename(file.readAll(),
                    FileCache::hasAllowedImageExtension(sourcePath)
                        ? QFileInfo(sourcePath).suffix()
                        : kDefaultImageExtension);
            }

            handler(result);
        });
}

void copyFileAsync(
    const QString& sourcePath,
    const QString& targetPath,
    std::function<void(bool success)> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    FileCache::ioThreadPool()->start(
        [sourcePath, targetPath, handler = handlerExecutor.bind(std::move(handler))]() mutable
        {
            QDir().mkpath(QFileInfo(targetPath).absolutePath());
            handler(QFile::copy(sourcePath, targetPath));
        });
}

void storeImageAsync(
    const QString& folder,
    const QImage& image,
    const QString& extension,
    std::function<void(const QString& filename)> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    FileCache::ioThreadPool()->start(
        [folder, image, extension, handler = handlerExecutor.bind(std::move(handler))]() mutable
        {
            QByteArray data;
            QBuffer buffer(&data);
            if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, extension.toUtf8()))
            {
                handler(QString());
                return;
            }

            const QString filename = cachedImageFilename(data, extension);

            QDir().mkpath(folder);
            QSaveFile file(folder + QDir::separator() + filename);
            const bool success = file.open(QIODevice::WriteOnly)
                && file.write(data) == data.size()
                && file.commit();

            handler(success ? filename : QString());
        });
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

FileCache* backgroundImageCache(const QnLayoutResourcePtr& layout)
{
    if (layout->hasFlags(Qn::cross_system))
        return appContext()->cloudLayoutsManager()->imageCache();

    auto systemContext = SystemContext::fromResource(layout);
    if (!NX_ASSERT(systemContext))
        systemContext = appContext()->currentSystemContext();

    return layout->isFile()
        ? static_cast<FileCache*>(systemContext->localFileCache())
        : static_cast<FileCache*>(systemContext->serverImageCache());
}

} // namespace nx::vms::client::desktop::file_cache
