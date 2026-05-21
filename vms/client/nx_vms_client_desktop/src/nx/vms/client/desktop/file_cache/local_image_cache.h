// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "file_cache.h"

namespace nx::vms::client::desktop {

/**
 * Image cache backed by the local filesystem only. All operations are synchronous.
 */
class LocalImageCache: public FileCache
{
    Q_OBJECT
    using base_type = FileCache;

public:
    explicit LocalImageCache(QObject* parent = nullptr);
    virtual ~LocalImageCache() override;

    /**
     * Accepts a Qt resource path (`:/...`, `qrc://...`) and returns it unchanged, as resource
     * files are inherently safe; otherwise delegates to the base sanitization.
     */
    virtual QString absoluteFilePath(const QString& unsafeFilename) const override;

    virtual void clear() override;

    OperationResult storeImageData(const QString& unsafeFilename, const QByteArray& imageData);

protected:
    virtual QString cacheFolder() const override;
};

} // namespace nx::vms::client::desktop
