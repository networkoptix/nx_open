#include "caching_metadata_consumer.h"

#include <queue>

#include <QtCore/QVector>
#include <QtCore/QMultiMap>
#include <QtCore/QSharedPointer>

#include <nx/utils/thread/mutex.h>

#include <nx/media/ini.h>

namespace nx::media {

using namespace std::chrono;

namespace {

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
        m_metadataByTimestamp.insert(metadata->timestamp, metadata);
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

        QList<QnAbstractCompressedMetadataPtr> result;

        auto it = std::upper_bound(
            m_metadataByTimestamp.keyBegin(),
            m_metadataByTimestamp.keyEnd(),
            startTimestamp).base();

        if (it != m_metadataByTimestamp.begin())
        {
            --it;
            if ((*it)->duration() > 0 && it.key() + (*it)->duration() < startTimestamp)
                ++it;
        }

        while (maxCount > 0 && it != m_metadataByTimestamp.end()
            && (*it)->timestamp < endTimestamp)
        {
            if (NX_ASSERT(*it, "Metadata cache should not hold null metadata pointers."))
                result.append(*it);

            ++it;
            --maxCount;
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
    void removeOldestMetadataItem() //< The oldest by arrival, not by timestamp.
    {
        const auto metadata = m_metadataCache.front();
        m_metadataCache.pop();

        const auto range = m_metadataByTimestamp.equal_range(metadata->timestamp);
        NX_ASSERT(range.first != range.second);

        for (auto it = range.first; it != range.second; ++it)
        {
            if (*it != metadata)
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
    mutable QnMutex m_mutex;
    std::queue<QnAbstractCompressedMetadataPtr> m_metadataCache;
    QMultiMap<qint64, QnAbstractCompressedMetadataPtr> m_metadataByTimestamp;
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
    microseconds timestamp, int channel) const
{
    if (channel >= d->cachePerChannel.size())
        return QnAbstractCompressedMetadataPtr();

    const auto& cache = d->cachePerChannel[channel];
    if (!cache)
        return QnAbstractCompressedMetadataPtr();

    return cache->findMetadata(timestamp.count());
}

QList<QnAbstractCompressedMetadataPtr> CachingMetadataConsumer::metadataRange(
    microseconds startTimestamp,
    microseconds endTimestamp,
    int channel,
    int maximumCount) const
{
    if (channel >= d->cachePerChannel.size())
        return QList<QnAbstractCompressedMetadataPtr>();

    const auto& cache = d->cachePerChannel[channel];
    if (!cache)
        return QList<QnAbstractCompressedMetadataPtr>();

    return cache->findMetadataInRange(startTimestamp.count(), endTimestamp.count(), maximumCount);
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

} // namespace nx::media
