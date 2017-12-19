#include "caching_metadata_consumer.h"

#include <QtCore/QVector>
#include <QtCore/QQueue>
#include <QtCore/QMap>
#include <QtCore/QSharedPointer>

#include <nx/media/ini.h>

namespace nx {
namespace media {

namespace {

static constexpr int kDefaultMaxCacheItemsCount = 60;

bool metadataContainsTime(const QnAbstractCompressedMetadataPtr& metadata, const qint64 timestamp)
{
    const auto allowedDelay = (metadata->metadataType == MetadataType::ObjectDetection)
        ? ini().allowedAnalyticsMetadataDelayMs * 1000
        : 0;

    const auto duration = metadata->duration() + allowedDelay;

    if (duration == 0)
        return timestamp == metadata->timestamp;

    return timestamp >= metadata->timestamp && timestamp < metadata->timestamp + duration;
}

class MetadataCache
{
public:
    MetadataCache(int cacheSize = kDefaultMaxCacheItemsCount):
        m_maxItemsCount(std::max(cacheSize, 1))
    {
        m_metadataCache.reserve(m_maxItemsCount);
    }

    void insertMetadata(const QnAbstractCompressedMetadataPtr& metadata)
    {
        if (m_metadataCache.size() == m_maxItemsCount)
        {
            const auto metadata = m_metadataCache.dequeue();
            const auto it = m_metadataByTimestamp.find(metadata->timestamp);
            // Check the value equality is necessary because the stored value can be replaced by
            // other metadata with the same timestamp.
            if (it != m_metadataByTimestamp.end() && *it == metadata)
                m_metadataByTimestamp.erase(it);
        }

        m_metadataCache.enqueue(metadata);
        m_metadataByTimestamp[metadata->timestamp] = metadata;
    }

    QnAbstractCompressedMetadataPtr findMetadata(const qint64 timestamp) const
    {
        if (m_metadataByTimestamp.isEmpty())
            return QnAbstractCompressedMetadataPtr();

        // Check metadata at lower_bound first (will match when only when timestamps are equal).
        // Then if it fails check the previous item (may match by duration). Since we check the
        // previous item anyway we can exclude the first item from lower_bound search. Thus
        // explicit check for possibility to decrement the iterator can be omitted.

        auto it = std::lower_bound(
            std::next(m_metadataByTimestamp.keyBegin()),
            m_metadataByTimestamp.keyEnd(),
            timestamp);

        if (it != m_metadataByTimestamp.keyEnd() && metadataContainsTime(*it.base(), timestamp))
            return *it.base();

        --it;

        if (metadataContainsTime(*it.base(), timestamp))
            return *it.base();

        return QnAbstractCompressedMetadataPtr();
    }

private:
public:
    QQueue<QnAbstractCompressedMetadataPtr> m_metadataCache;
    QMap<qint64, QnAbstractCompressedMetadataPtr> m_metadataByTimestamp;
    const int m_maxItemsCount;
};

using MetadataCachePtr = QSharedPointer<MetadataCache>;

} // namespace

class CachingMetadataConsumer::Private
{
public:
    QVector<MetadataCachePtr> cachePerChannel;
};

CachingMetadataConsumer::CachingMetadataConsumer(MetadataType metadataType):
    base_type(metadataType),
    d(new Private())
{
}

CachingMetadataConsumer::~CachingMetadataConsumer()
{
}

QnAbstractCompressedMetadataPtr CachingMetadataConsumer::metadata(
    qint64 timestamp, int channel) const
{
    if (channel >= d->cachePerChannel.size())
        return QnAbstractCompressedMetadataPtr();

    const auto& cache = d->cachePerChannel[channel];
    if (!cache)
        return QnAbstractCompressedMetadataPtr();

    return cache->findMetadata(timestamp);
}

void CachingMetadataConsumer::processMetadata(const QnAbstractCompressedMetadataPtr& metadata)
{
    const auto channel = static_cast<int>(metadata->channelNumber);
    if (d->cachePerChannel.size() <= channel)
        d->cachePerChannel.resize(channel + 1);

    auto& cache = d->cachePerChannel[channel];
    if (cache.isNull())
        cache.reset(new MetadataCache());

    cache->insertMetadata(metadata);
}

} // namespace media
} // namespace nx
