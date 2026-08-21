// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QSize>
#include <QtCore/QString>

#include <core/resource/resource_fwd.h>
#include <nx/utils/async_handler_executor.h>

class QImage;

namespace nx::vms::client::core { class FileCache; }

namespace nx::vms::client::desktop::file_cache {

/**
 * Asynchronously derives the cache filename from the image file content, so edits to the
 * source produce a new cache entry. The whole source file is read and hashed in a separate thread.
 * @param sourcePath Path to the source image file.
 * @param handler Receives the derived name, a pure leaf name that can be safely used in the
 *     cache directory, or an empty string if the file cannot be read.
 * @param handlerExecutor Executor the handler is invoked through. Pass a pointer to QObject to
 *     invoke the handler in that object's thread and to drop it if the object is destroyed.
 */
void cachedImageFilenameAsync(
    const QString& sourcePath,
    std::function<void(const QString& filename)> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor = {});

/**
 * Asynchronously copies a file in a separate thread, creating the target directory if needed.
 * @param handler Receives false if the source cannot be read or the target cannot be written.
 * @param handlerExecutor Executor the handler is invoked through. Pass a pointer to QObject to
 *     invoke the handler in that object's thread and to drop it if the object is destroyed.
 */
void copyFileAsync(
    const QString& sourcePath,
    const QString& targetPath,
    std::function<void(bool success)> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor = {});

/**
 * Asynchronously encodes the image in a separate thread and stores it in the folder under the name
 * derived from the encoded data, so the name of a transformed image matches its own content.
 * @param extension Image format to encode to, also the extension of the resulting file.
 * @param handler Receives the name of the stored file, or an empty string if the image cannot be
 *     encoded or written.
 * @param handlerExecutor Executor the handler is invoked through. Pass a pointer to QObject to
 *     invoke the handler in that object's thread and to drop it if the object is destroyed.
 */
void storeImageAsync(
    const QString& folder,
    const QImage& image,
    const QString& extension,
    std::function<void(const QString& filename)> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor = {});

/** Remove the entire local cache directory tree under `<AppLocalData>/cache`. */
void clearLocalCache();

/** Maximum image dimension the renderer can upload as a single texture. */
QSize maxBackgroundImageSize();

/**
 * The cache storing the given layout's background image: the cloud image cache for cross-system
 * layouts, the local image cache for exported layouts, or the server file cache of the
 * layout's system.
 */
core::FileCache* backgroundImageCache(const QnLayoutResourcePtr& layout);

} // namespace nx::vms::client::desktop::file_cache
