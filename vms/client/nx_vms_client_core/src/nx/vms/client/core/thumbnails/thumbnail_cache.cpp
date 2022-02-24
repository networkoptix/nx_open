// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "thumbnail_cache.h"

namespace nx::vms::client::core {

ThumbnailCache::ThumbnailCache(QObject* parent):
    base_type(parent)
{
    static const int kDefaultPixelLimit = 400 * 250 * 512;
    m_cache.setMaxCost(kDefaultPixelLimit);
}

void ThumbnailCache::insert(const QString& key, const QImage& image)
{
    static constexpr int kExtraCost = 100;
    m_cache.insert(key, new QImage(image), (image.width() * image.height()) + kExtraCost);
    emit updated(key, image);
}

void ThumbnailCache::remove(const QString& key)
{
    m_cache.remove(key);
}

void ThumbnailCache::clear()
{
    m_cache.clear();
}

QImage* ThumbnailCache::image(const QString& key) const
{
    return m_cache.object(key);
}

int ThumbnailCache::maxCostPixels() const
{
    return m_cache.maxCost();
}

void ThumbnailCache::setMaxCostPixels(int value)
{
    m_cache.setMaxCost(value);
}

} // namespace nx::vms::client::core
