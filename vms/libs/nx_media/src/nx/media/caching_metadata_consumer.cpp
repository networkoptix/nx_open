// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "caching_metadata_consumer.h"

#include <queue>

#include <boost/container/flat_map.hpp>

#include <QtCore/QVector>
#include <QtCore/QMultiMap>
#include <QtCore/QSharedPointer>

#include <analytics/common/object_metadata.h>

#include <nx/fusion/serialization/ubjson.h>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/log/log.h>

#include <nx/media/ini.h>

namespace nx::media {

using namespace std::chrono;

namespace {

template<typename T>
class MetadataCache
{
public:
    MetadataCache(size_t cacheSize = 1):
        m_cacheSize(cacheSize)
    {
    }

    void insertMetadata(const T& metadata)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        if (m_metadataCache.size() == m_cacheSize)
            removeOldestMetadataItem();

        m_metadataCache.push(metadata);
        m_metadataByTimestamp.insert({timestampUs(metadata), metadata});
    }

    QList<T> findMetadataInRange(
        const qint64 startTimestamp, const qint64 endTimestamp, bool isForward, int maxCount) const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        if (m_metadataByTimestamp.empty())
            return {};

        QList<T> result;

        if (isForward)
        {
            auto it = m_metadataByTimestamp.lower_bound(startTimestamp);

            while (maxCount > 0 && it != m_metadataByTimestamp.end()
                && timestampUs(it->second) < endTimestamp)
            {
                if (NX_ASSERT(it->second, "Metadata cache should not hold null metadata pointers."))
                    result.append(it->second);

                ++it;
                --maxCount;
            }
        }
        else
        {
            auto it = m_metadataByTimestamp.lower_bound(endTimestamp);

            while (maxCount > 0 && it != m_metadataByTimestamp.begin())
            {
                --it;
                if (NX_ASSERT(it->second, "Metadata cache should not hold null metadata pointers."))
                {
                    if (timestampUs(it->second) < startTimestamp)
                        break;

                    result.prepend(it->second);
                    --maxCount;
                }
            }
        }

        return result;
    }

    void setCacheSize(size_t cacheSize)
    {
        if (cacheSize == 0 || cacheSize == m_cacheSize)
            return;

        trimCacheToSize(cacheSize);
        m_cacheSize = cacheSize;
    }

    static T uncompress(const QnAbstractCompressedMetadataPtr& metadata);
    static qint64 timestampUs(const T& t);
    static bool containsTime(const T& t, const qint64 timestampUs);

private:
    void removeOldestMetadataItem() //< The oldest by arrival, not by timestamp.
    {
        const auto metadata = m_metadataCache.front();
        m_metadataCache.pop();

        const auto range = m_metadataByTimestamp.equal_range(timestampUs(metadata));
        NX_ASSERT(range.first != range.second);

        for (auto it = range.first; it != range.second; ++it)
        {
            if (it->second != metadata)
                continue;

            m_metadataByTimestamp.erase(it);
            break;
        }
    }

    void trimCacheToSize(size_t size)
    {
        while (m_metadataCache.size() > size)
            removeOldestMetadataItem();
    }

private:
    mutable nx::Mutex m_mutex;
    std::queue<T> m_metadataCache;

    // The most frequent operation we do is computing lower_bound and T is small (shared_ptr)
    // so a flat_* container is used here for better performance.
    boost::container::flat_multimap<qint64, T> m_metadataByTimestamp;
    size_t m_cacheSize;
};

QString metadataRepresentation(QnAbstractCompressedMetadataPtr metadata)
{
    if (metadata->metadataType != MetadataType::ObjectDetection)
    {
        return nx::format("type %1").arg(int(metadata->metadataType));
    }

    const auto compressedMetadata =
            std::dynamic_pointer_cast<QnCompressedMetadata>(metadata);
    if (!NX_ASSERT(compressedMetadata, "Unparseable object metadata"))
        return nx::format("Unparseable object metadata");

    using namespace nx::common::metadata;
    const auto objectDataPacket = QnUbjson::deserialized<ObjectMetadataPacket>(
        QByteArray::fromRawData(compressedMetadata->data(), int(compressedMetadata->dataSize())));

    QStringList result;
    for (const auto& objectData: objectDataPacket.objectMetadataList)
    {
        result.append(nx::format("Object type %1 track %2")
            .args(objectData.typeId, objectData.trackId));
        for (const auto& attribute: objectData.attributes)
            result.append(nx::format("    %1 = %2").args(attribute.name, attribute.value));
    }
    return result.join('\n');
}

template<>
QnMetaDataV1Ptr MetadataCache<QnMetaDataV1Ptr>::uncompress(
    const QnAbstractCompressedMetadataPtr& metadata)
{
    return std::dynamic_pointer_cast<QnMetaDataV1>(metadata);
}

template<>
qint64 MetadataCache<QnMetaDataV1Ptr>::timestampUs(const QnMetaDataV1Ptr& t)
{
    return t->timestamp;
}

template<>
bool MetadataCache<QnMetaDataV1Ptr>::containsTime(
    const QnMetaDataV1Ptr& t, const qint64 timestampUs)
{
    return t->containTime(timestampUs);
}

using namespace nx::common::metadata;

template<>
ObjectMetadataPacketPtr MetadataCache<ObjectMetadataPacketPtr>::uncompress(
    const QnAbstractCompressedMetadataPtr& metadata)
{
    const auto compressedMetadata = std::dynamic_pointer_cast<QnCompressedMetadata>(metadata);
    return fromCompressedMetadataPacket(compressedMetadata);
}

template<>
qint64 MetadataCache<ObjectMetadataPacketPtr>::timestampUs(const ObjectMetadataPacketPtr& t)
{
    return t->timestampUs;
}

template<>
bool MetadataCache<ObjectMetadataPacketPtr>::containsTime(
    const ObjectMetadataPacketPtr& t,
    const qint64 timestampUs)
{
    if (t->durationUs == 0)
        return t->timestampUs == timestampUs;

    return (t->timestampUs <= timestampUs) && (timestampUs < (t->timestampUs + t->durationUs));
}

} // namespace

template<typename T>
class CachingMetadataConsumer<T>::Private
{
public:
    QVector<QSharedPointer<MetadataCache<T>>> cachePerChannel;
    size_t cacheSize = 1;
};

template<typename T>
CachingMetadataConsumer<T>::CachingMetadataConsumer(MetadataType metadataType):
    base_type(metadataType),
    d(new Private())
{
    d->cacheSize = static_cast<size_t>(std::max(1, ini().metadataCacheSize));
}

template<typename T>
CachingMetadataConsumer<T>::~CachingMetadataConsumer()
{
}

template<typename T>
size_t CachingMetadataConsumer<T>::cacheSize() const
{
    return d->cacheSize;
}

template<typename T>
void CachingMetadataConsumer<T>::setCacheSize(size_t cacheSize)
{
    if (cacheSize < 1 || d->cacheSize == cacheSize)
        return;

    d->cacheSize = cacheSize;

    for (const auto& cache: d->cachePerChannel)
    {
        if (cache)
            cache->setCacheSize(d->cacheSize);
    }
}

template<typename T>
T CachingMetadataConsumer<T>::metadata(
    microseconds timestamp, int channel) const
{
    if (channel >= d->cachePerChannel.size())
        return T();

    const auto& cache = d->cachePerChannel[channel];
    if (!cache)
        return T();

    auto list = cache->findMetadataInRange(
        0, timestamp.count() + 1, //< Look for the data in a range from the beginning to timestamp.
        false, 1); //< Take the newest one of them (the one that's in the back of the queue buffer).

    if (!list.empty())
    {
        const auto& ptr = list.first();
        if (MetadataCache<T>::containsTime(ptr, timestamp.count()))
            return ptr;
    }

    return {};
}

template<typename T>
QList<T> CachingMetadataConsumer<T>::metadataRange(
    microseconds startTimestamp,
    microseconds endTimestamp,
    int channel,
    PickingPolicy pickingPolicy,
    int maximumCount) const
{
    if (channel >= d->cachePerChannel.size())
        return QList<T>();

    const auto& cache = d->cachePerChannel[channel];
    if (!cache)
        return QList<T>();

    return cache->findMetadataInRange(
        startTimestamp.count(), endTimestamp.count(),
        pickingPolicy == PickingPolicy::TakeFirst, maximumCount);
}

template<typename T>
void CachingMetadataConsumer<T>::processMetadata(const QnAbstractCompressedMetadataPtr& metadata)
{
    NX_VERBOSE(this, "Metadata packet received, %1, timestamp %2",
        metadataRepresentation(metadata),
        metadata->timestamp);

    const auto channel = static_cast<int>(metadata->channelNumber);
    if (d->cachePerChannel.size() <= channel)
        d->cachePerChannel.resize(channel + 1);

    auto& cache = d->cachePerChannel[channel];
    if (cache.isNull())
        cache.reset(new MetadataCache<T>(d->cacheSize));

    const auto uncompressedMetadata = MetadataCache<T>::uncompress(metadata);
    cache->insertMetadata(uncompressedMetadata);
}

template class CachingMetadataConsumer<QnMetaDataV1Ptr>;
template class CachingMetadataConsumer<nx::common::metadata::ObjectMetadataPacketPtr>;

} // namespace nx::media
