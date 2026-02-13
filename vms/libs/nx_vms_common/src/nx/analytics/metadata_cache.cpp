// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "metadata_cache.h"

#include <algorithm>
#include <stack>

#include <boost/container/flat_map.hpp>

#include <analytics/common/object_metadata.h>
#include <nx/fusion/serialization/ubjson.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

#include "nx/vms/api/data/secure_cookie_storage_data.h"
#include "private/max_duration_counter.h"

namespace nx::analytics {

using namespace std::chrono;

namespace {

const std::unordered_map<MetadataType, milliseconds> kDefaultDurationByType({
    { MetadataType::Motion, 3000ms },
    { MetadataType::ObjectDetection, 100ms },
    { MetadataType::MediaStreamEvent, 1ms },
    { MetadataType::InStream, 1ms }});

milliseconds fromMicrosecondQint(qint64 microsecond)
{
    return duration_cast<milliseconds>(microseconds(microsecond));
}

} // namespace

template<typename T>
MetadataCache<T>::MetadataCache(const MetadataType metadataType, const milliseconds& cacheDuration):
    m_cacheDuration(cacheDuration),
    m_metadataType(metadataType),
    m_maxDurationCounter(new MaxDurationCounter())
{
}

template<typename T>
void MetadataCache<T>::insertMetadata(const QnConstAbstractCompressedMetadataPtr& metadata)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (!m_orderedMetadata.empty()
        && m_orderedMetadata.begin()->first + m_cacheDuration
            < fromMicrosecondQint(metadata->timestamp))
    {
        removeOldestMetadataItemUnsafe(fromMicrosecondQint(metadata->timestamp) - m_cacheDuration);
    }

    const auto& unpackedMetadata = uncompress(metadata);

    m_orderedMetadata.insert({fromMicrosecondQint(metadata->timestamp), unpackedMetadata});

    // Store duration in milliseconds as well
    if (m_metadataType == MetadataType::ObjectDetection)
    {
        m_maxDurationCounter->push(metadata->duration() > 0
           ? metadata->duration() / 1000
           : kDefaultDurationByType.at(MetadataType::ObjectDetection).count());
    }
}

template<typename T>
void MetadataCache<T>::setCacheSize(const milliseconds& cacheDuration)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (cacheDuration == milliseconds::zero() || cacheDuration == m_cacheDuration)
        return;

    m_cacheDuration = cacheDuration;
    removeOldestMetadataItemUnsafe(m_orderedMetadata.rbegin()->first - m_cacheDuration);
}

template<typename T>
std::vector<T> MetadataCache<T>::getMetadata(const milliseconds& timestamp)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    const milliseconds maxDuration = m_metadataType == MetadataType::ObjectDetection
        ? milliseconds(m_maxDurationCounter->max())
        : kDefaultDurationByType.at(m_metadataType);

    auto left = m_orderedMetadata.lower_bound(timestamp - maxDuration);
    auto right = m_orderedMetadata.upper_bound(timestamp);
    std::vector<T> result;
    for (auto it = right; it != left && it != m_orderedMetadata.begin();)
    {
        --it;
        if (timestampUs(it->second) + durationUs(it->second) < timestamp.count() * 1000)
            continue;

        result.push_back(it->second);
    }

    return result;
}

template<typename T>
void MetadataCache<T>::removeOldestMetadataItemUnsafe(const milliseconds& rightBorder)
{
    if (m_orderedMetadata.empty())
        return;

    const auto right = m_orderedMetadata.upper_bound(rightBorder);
    if (m_metadataType == MetadataType::ObjectDetection)
        m_maxDurationCounter->pop(right - m_orderedMetadata.begin());

    m_orderedMetadata.erase(m_orderedMetadata.begin(), right);
}

template<>
QnConstMetaDataV1Ptr MetadataCache<QnConstMetaDataV1Ptr>::uncompress(
    const QnConstAbstractCompressedMetadataPtr& metadata)
{
    return QnConstMetaDataV1Ptr(std::dynamic_pointer_cast<const QnMetaDataV1>(metadata)->clone());
}

template<>
nx::common::metadata::ObjectMetadataPacketPtr MetadataCache<
    nx::common::metadata::ObjectMetadataPacketPtr>::uncompress(
    const QnConstAbstractCompressedMetadataPtr& metadata)
{
    const auto compressedMetadata =
        std::dynamic_pointer_cast<const QnCompressedMetadata>(metadata);
    if (!compressedMetadata || compressedMetadata->dataSize() > std::numeric_limits<int>::max())
        return {};

    return std::make_shared<nx::common::metadata::ObjectMetadataPacket>(
        QnUbjson::deserialized<nx::common::metadata::ObjectMetadataPacket>(QByteArray::fromRawData(
            compressedMetadata->data(),
            int(compressedMetadata->dataSize()))));
}

template<>
qint64 MetadataCache<QnConstMetaDataV1Ptr>::timestampUs(const QnConstMetaDataV1Ptr& metadata)
{
    return metadata->timestamp;
}

template<>
qint64 MetadataCache<QnConstMetaDataV1Ptr>::durationUs(const QnConstMetaDataV1Ptr& metadata)
{
    if (metadata->duration() < 1)
        return kDefaultDurationByType.at(MetadataType::Motion).count();

    return metadata->duration();
}

template<>
qint64 MetadataCache<nx::common::metadata::ObjectMetadataPacketPtr>::timestampUs(
    const nx::common::metadata::ObjectMetadataPacketPtr& metadata)
{
    return metadata->timestampUs;
}

template<>
qint64 MetadataCache<nx::common::metadata::ObjectMetadataPacketPtr>::durationUs(
    const nx::common::metadata::ObjectMetadataPacketPtr& metadata)
{
    if (metadata->durationUs < 1)
        return kDefaultDurationByType.at(MetadataType::ObjectDetection).count() * 1000;

    return metadata->durationUs;
}

template class MetadataCache<QnConstMetaDataV1Ptr>;
template class MetadataCache<nx::common::metadata::ObjectMetadataPacketPtr>;

} // namespace nx::analytics
