// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "export_caching_metadata_consumer.h"

#include <algorithm>
#include <limits>
#include <queue>
#include <unordered_map>
#include <unordered_set>

#include <boost/container/flat_map.hpp>

#include <QtCore/QSharedPointer>
#include <QtCore/QVector>

#include <analytics/common/object_metadata.h>
#include <nx/fusion/serialization/ubjson.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

#include "metadata_cache.h"

namespace nx::analytics {

template<typename TMetadata>
class ExportCachingMetadataConsumer<TMetadata>::Private
{
public:
    QVector<QSharedPointer<MetadataCache<TMetadata>>> cachePerChannel;
    milliseconds cacheDuration = 10s;
};

template<typename TMetadata>
ExportCachingMetadataConsumer<TMetadata>::ExportCachingMetadataConsumer(
    const MetadataType metadataType,
    const milliseconds& cacheDuration):
    base_type(metadataType),
    d(new Private())
{
    d->cacheDuration = static_cast<milliseconds>(std::max(1ms, cacheDuration));
}

template<typename TMetadata>
ExportCachingMetadataConsumer<TMetadata>::~ExportCachingMetadataConsumer()
{
}

template<typename TMetadata>
milliseconds ExportCachingMetadataConsumer<TMetadata>::cacheDuration() const
{
    return d->cacheDuration;
}

template<typename TMetadata>
void ExportCachingMetadataConsumer<TMetadata>::setCacheDuration(const milliseconds& cacheDuration)
{
    if (cacheDuration < 1s || d->cacheDuration == cacheDuration)
        return;

    d->cacheDuration = cacheDuration;

    for (const auto& cache: d->cachePerChannel)
    {
        if (cache)
            cache->setCacheSize(d->cacheDuration);
    }
}

template<typename TMetadata>
std::vector<TMetadata> ExportCachingMetadataConsumer<TMetadata>::metadata(
    const milliseconds& timestamp, const int channel) const
{
    if (channel >= d->cachePerChannel.size())
        return {};

    const auto& cache = d->cachePerChannel[channel];
    if (!cache)
        return {};

    return cache->getMetadata(timestamp);
}

template<typename TMetadata>
void ExportCachingMetadataConsumer<TMetadata>::processMetadata(
    const QnConstAbstractCompressedMetadataPtr& metadata)
{
    const auto channel = static_cast<int>(metadata->channelNumber);
    if (d->cachePerChannel.size() <= channel)
        d->cachePerChannel.resize(channel + 1);

    auto& cache = d->cachePerChannel[channel];
    if (cache.isNull())
    {
        cache.reset(new MetadataCache<TMetadata>(metadataType(), d->cacheDuration));
    }

    cache->insertMetadata(metadata);
}

template class NX_VMS_COMMON_API ExportCachingMetadataConsumer<QnConstMetaDataV1Ptr>;

template class NX_VMS_COMMON_API
    ExportCachingMetadataConsumer<common::metadata::ObjectMetadataPacketPtr>;

} // namespace nx::analytics
