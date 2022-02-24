// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCache>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtGui/QImage>

namespace nx::vms::client::core {

/**
 * Thumbnail image cache with a capability to emit a signal upon each write into.
 * Intended to be used mostly with AbstractCachingResourceThumbnail descendants.
 */
class ThumbnailCache: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    ThumbnailCache(QObject* parent = nullptr);
    virtual ~ThumbnailCache() override = default;

    void insert(const QString& key, const QImage& image);
    void remove(const QString& key);
    void clear();

    QImage* image(const QString& key) const;

    int maxCostPixels() const;
    void setMaxCostPixels(int value);

signals:
    /**
     * Emitted after an image is inserted into the cache.
     * Is NOT emitted when an image is removed or discarded.
     */
    void updated(const QString& key, const QImage& image);

private:
    QCache<QString, QImage> m_cache;
};

} // namespace nx::vms::client::core
