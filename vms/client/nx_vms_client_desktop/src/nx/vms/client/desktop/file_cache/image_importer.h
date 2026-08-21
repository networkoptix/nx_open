// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>

#include <nx/vms/client/core/file_cache/file_cache.h>
#include <utils/common/aspect_ratio.h>

namespace nx::vms::client::desktop {

/**
 * Asynchronously stores a source image file in a `FileCache` under a filename derived from the
 * stored file content. An image is copied as is, unless it has to be cropped to an aspect ratio
 * or downscaled to the renderer's texture limit: such an image is stored under the name of the
 * transformation result, so images transformed differently do not share a cache file.
 */
class ImageImporter: public QObject
{
    Q_OBJECT

public:
    ImageImporter(core::FileCache* targetCache, QObject* parent = nullptr);
    virtual ~ImageImporter() override;

    /**
     * @param cachedImageFilename Cache filename derived from the source file content, when the
     *     caller already knows it. It is the resulting name only if the image is stored as is.
     */
    void importFromFile(
        const QString& sourcePath,
        const QnAspectRatio& aspectRatio = QnAspectRatio(),
        const QString& cachedImageFilename = {});

signals:
    void imported(const QString& filename, core::FileCache::OperationResult status);

private:
    /** @param targetPath Provides the cache folder and the image format to store the result. */
    void transformImage(
        const QString& sourcePath,
        const QnAspectRatio& aspectRatio,
        const QString& targetPath);

private:
    QPointer<core::FileCache> const m_targetCache;
};

} // namespace nx::vms::client::desktop
