#include "caching_metadata_consumer.h"

#include <QtCore/QVector>
#include <QtCore/QQueue>
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

enum class SearchPolicy
{
    exact,
    closestBefore,
    closestAfter
};

class MetadataCache
{
public:
    MetadataCache(int cacheSize = -1):
        m_maxItemsCount(std::max(1, cacheSize >= 0 ? cacheSize : ini().metadataCacheSize))
    {
        m_metadataCache.reserve(m_maxItemsCount);
    }

    void insertMetadata(const QnAbstractCompressedMetadataPtr& metadata)
    {
        QnMutexLocker lock(&m_mutex);

        if (m_metadataCache.size() == m_maxItemsCount)
        {
            const auto oldestMetadata = m_metadataCache.dequeue();
            const auto it = m_metadataByTimestamp.find(oldestMetadata->timestamp);
            // Check the value equality is necessary because the stored value can be replaced by
            // other metadata with the same timestamp.
            NX_ASSERT(it != m_metadataByTimestamp.end());
            if (it != m_metadataByTimestamp.end() && *it == oldestMetadata)
                m_metadataByTimestamp.erase(it);
        }

        m_metadataCache.enqueue(metadata);
        m_metadataByTimestamp[metadata->timestamp] = metadata;
    }

    QnAbstractCompressedMetadataPtr findMetadata(const qint64 timestamp) const
    {
        QnMutexLocker lock(&m_mutex);

        const auto it = findMetadataIterator(timestamp, SearchPolicy::exact);
        if (it == m_metadataByTimestamp.end())
            return {};

        NX_ASSERT(*it, "Metadata cache should not hold null metadata pointers.");

        return *it;
    }

    QList<QnAbstractCompressedMetadataPtr> findMetadataInRange(
        const qint64 startTimestamp, const qint64 endTimestamp, int maxCount) const
    {
        QnMutexLocker lock(&m_mutex);

        const auto startIt = findMetadataIterator(startTimestamp, SearchPolicy::closestAfter);
        if (startIt == m_metadataByTimestamp.end())
            return {};

        const auto endIt = findMetadataIterator(endTimestamp, SearchPolicy::closestBefore);

        QList<QnAbstractCompressedMetadataPtr> result;
        auto itemsLeft = maxCount;
        for (auto it = startIt; itemsLeft != 0 && it != endIt; ++it, --itemsLeft)
        {
            NX_ASSERT(*it, "Metadata cache should not hold null metadata pointers.");
            if (*it)
                result.append(*it);
        }
        return result;
    }

private:
    QMap<qint64, QnAbstractCompressedMetadataPtr>::const_iterator findMetadataIterator(
        const qint64 timestamp, SearchPolicy searchPolicy) const
    {
        if (m_metadataByTimestamp.isEmpty())
            return m_metadataByTimestamp.end();

        switch (searchPolicy)
        {
            case SearchPolicy::exact:
            {
                // Check metadata at lower_bound first (will match when only when timestamps are
                // equal). Then if it fails check the previous item (may match by duration). Since
                // we check the previous item anyway we can exclude the first item from
                // lower_bound. Thus explicit check for possibility to decrement the iterator
                // can be omitted.

                auto it = std::lower_bound(
                    std::next(m_metadataByTimestamp.keyBegin()),
                    m_metadataByTimestamp.keyEnd(),
                    timestamp).base();

                if (it != m_metadataByTimestamp.end() && metadataContainsTime(*it, timestamp))
                    return it;

                --it;

                if (metadataContainsTime(*it, timestamp))
                    return it;

                return m_metadataByTimestamp.end();
            }

            case SearchPolicy::closestAfter:
                return std::lower_bound(
                    m_metadataByTimestamp.keyBegin(),
                    m_metadataByTimestamp.keyEnd(),
                    timestamp).base();

            case SearchPolicy::closestBefore:
                return std::upper_bound(
                    m_metadataByTimestamp.keyBegin(),
                    m_metadataByTimestamp.keyEnd(),
                    timestamp).base();
        }

        return m_metadataByTimestamp.end();
    }

    mutable QnMutex m_mutex;
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
        cache.reset(new MetadataCache());

    cache->insertMetadata(metadata);
}

} // namespace media
} // namespace nx
