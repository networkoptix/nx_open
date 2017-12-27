#include "caching_metadata_consumer.h"

#include <queue>

#include <QtCore/QVector>
#include <QtCore/QMap>
#include <QtCore/QSharedPointer>

#include <nx/utils/thread/mutex.h>

#include <nx/media/ini.h>

namespace nx {
namespace media {

namespace {

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
    MetadataCache(size_t cacheSize = 1):
        m_cacheSize(cacheSize)
    {
    }

    void insertMetadata(const QnAbstractCompressedMetadataPtr& metadata)
    {
        QnMutexLocker lock(&m_mutex);

        if (m_metadataCache.size() == m_cacheSize)
            removeOldestMetadataItem();

        m_metadataCache.push(metadata);
        m_metadataByTimestamp[metadata->timestamp] = metadata;
    }

    QnAbstractCompressedMetadataPtr findMetadata(const qint64 timestamp) const
    {
        const auto& metadataList = findMetadataInRange(timestamp, timestamp + 1, 1);
        if (metadataList.isEmpty())
            return {};

        return metadataList.first();
    }

    QList<QnAbstractCompressedMetadataPtr> findMetadataInRange(
        const qint64 startTimestamp, const qint64 endTimestamp, int maxCount) const
    {
        QnMutexLocker lock(&m_mutex);

        if (m_metadataByTimestamp.isEmpty())
            return {};

        // Check metadata at lower_bound first (will match when only when timestamps are
        // equal). Then if it fails check the previous item (may match by duration). Since
        // we check the previous item anyway we can exclude the first item from
        // lower_bound. Thus explicit check for possibility to decrement the iterator
        // can be omitted.

        auto startIt = std::lower_bound(
            std::next(m_metadataByTimestamp.keyBegin()),
            m_metadataByTimestamp.keyEnd(),
            startTimestamp).base();

        if (startIt == m_metadataByTimestamp.end() || startTimestamp < (*startIt)->timestamp)
        {
            --startIt;
            if (!metadataContainsTime(*startIt, startTimestamp))
                ++startIt;
        }

        QList<QnAbstractCompressedMetadataPtr> result;

        auto itemsLeft = maxCount;
        auto it = startIt;
        while (itemsLeft != 0 && it != m_metadataByTimestamp.end()
            && (*it)->timestamp < endTimestamp)
        {
            NX_ASSERT(*it, "Metadata cache should not hold null metadata pointers.");
            if (*it)
                result.append(*it);

            ++it;
            --itemsLeft;
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

private:
    void removeOldestMetadataItem()
    {
        const auto metadata = m_metadataCache.front();
        m_metadataCache.pop();

        const auto it = m_metadataByTimestamp.find(metadata->timestamp);
        // Check the value equality is necessary because the stored value can be replaced by
        // other metadata with the same timestamp.
        NX_ASSERT(it != m_metadataByTimestamp.end());
        if (it != m_metadataByTimestamp.end() && *it == metadata)
            m_metadataByTimestamp.erase(it);
    }

    void trimCacheToSize(size_t size)
    {
        while (m_metadataCache.size() > size)
            removeOldestMetadataItem();
    }

private:
    mutable QnMutex m_mutex;
    std::queue<QnAbstractCompressedMetadataPtr> m_metadataCache;
    QMap<qint64, QnAbstractCompressedMetadataPtr> m_metadataByTimestamp;
    size_t m_cacheSize;
};

using MetadataCachePtr = QSharedPointer<MetadataCache>;

} // namespace

class CachingMetadataConsumer::Private
{
public:
    QVector<MetadataCachePtr> cachePerChannel;
    size_t cacheSize = 1;
};

CachingMetadataConsumer::CachingMetadataConsumer(MetadataType metadataType):
    base_type(metadataType),
    d(new Private())
{
    d->cacheSize = static_cast<size_t>(std::max(1, ini().metadataCacheSize));
}

CachingMetadataConsumer::~CachingMetadataConsumer()
{
}

size_t CachingMetadataConsumer::cacheSize() const
{
    return d->cacheSize;
}

void CachingMetadataConsumer::setCacheSize(size_t cacheSize)
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

QList<QnAbstractCompressedMetadataPtr> CachingMetadataConsumer::metadataRange(
    qint64 startTimestamp, qint64 endTimestamp, int channel, int maximumCount) const
{
    if (channel >= d->cachePerChannel.size())
        return QList<QnAbstractCompressedMetadataPtr>();

    const auto& cache = d->cachePerChannel[channel];
    if (!cache)
        return QList<QnAbstractCompressedMetadataPtr>();

    return cache->findMetadataInRange(startTimestamp, endTimestamp, maximumCount);
}

void CachingMetadataConsumer::processMetadata(const QnAbstractCompressedMetadataPtr& metadata)
{
    const auto channel = static_cast<int>(metadata->channelNumber);
    if (d->cachePerChannel.size() <= channel)
        d->cachePerChannel.resize(channel + 1);

    auto& cache = d->cachePerChannel[channel];
    if (cache.isNull())
        cache.reset(new MetadataCache(d->cacheSize));

    cache->insertMetadata(metadata);
}

} // namespace media
} // namespace nx
