// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

#include <utils/common/aspect_ratio.h>

#include "file_cache.h"

namespace nx::vms::client::desktop {

/**
 * Asynchronously decodes a source image file and stores it in a `FileCache` under a
 * hash-derived filename. The image is downscaled to fit the renderer's texture limit.
 */
class ImageImporter: public QObject
{
    Q_OBJECT

public:
    ImageImporter(FileCache* targetCache, QObject* parent = nullptr);
    virtual ~ImageImporter() override;

    void importFromFile(
        const QString& sourcePath,
        const QnAspectRatio& aspectRatio = QnAspectRatio());

signals:
    void imported(const QString& filename, FileCache::OperationResult status);

private:
    FileCache* const m_targetCache;
};

} // namespace nx::vms::client::desktop
