#include "video_cache.h"

#include <nx/utils/log/log.h>

namespace nx::vms::client::desktop {

const std::chrono::microseconds VideoCache::kNoTimestamp(-1);

static const std::chrono::milliseconds kDefaultCacheSize(2000);

VideoCache::VideoCache(QObject* parent):
    QObject(parent),
    m_cacheSize(kDefaultCacheSize)
{
}

void VideoCache::setCacheSize(std::chrono::microseconds value)
{
    QnMutexLocker lock(&m_mutex);
    m_cacheSize = value;
}

std::chrono::microseconds VideoCache::cacheSize() const
{
    QnMutexLocker lock(&m_mutex);
    return m_cacheSize;
}

void VideoCache::setCachedDevices(intptr_t consumerId, const QSet<QnUuid>& value)
{
    m_cachedDevices[consumerId] = value;
    QSet<QnUuid> totalCachedDevices;

    for (const auto& subset: m_cachedDevices)
        totalCachedDevices += subset;

    setCachedDevices(totalCachedDevices);
}

void VideoCache::setCachedDevices(const QSet<QnUuid>& value)
{
    QnMutexLocker lock(&m_mutex);
    for (auto itr = m_cache.begin(); itr != m_cache.end();)
    {
        if (value.contains(itr.key()))
            ++itr;
        else
            itr = m_cache.erase(itr);
    }
    for (const auto& id : value)
        m_cache[id]; //< Insert default values if still missing in map
}

QImage VideoCache::image(
    const QnUuid& resourceId,
    std::chrono::microseconds timestamp,
    std::chrono::microseconds* outImageTimestamp) const
{
    QnMutexLocker lock(&m_mutex);
    if (outImageTimestamp)
        *outImageTimestamp = kNoTimestamp;

    const auto queueItr = m_cache.find(resourceId);
    if (queueItr == m_cache.end() || queueItr->empty())
        return QImage();

    const auto logRange =
        [this](qint64 value, qint64 minimum, qint64 maximum)
        {
            NX_VERBOSE(this, "Cached from %1 to %2, requested %3 - %4", minimum, maximum, value,
                ((value < minimum) ? "TOO OLD" : ((value > maximum) ? "TOO NEW" : "CACHED")));
        };

    logRange(timestamp.count(), queueItr->front()->pkt_dts, queueItr->back()->pkt_dts);

    auto itr = std::lower_bound(queueItr->begin(), queueItr->end(),
        timestamp,
        [](const CLVideoDecoderOutputPtr& left, const std::chrono::microseconds& right)
        {
            return left->pkt_dts < right.count();
        });

    if (itr == queueItr->end())
        return QImage();

    if (outImageTimestamp)
        *outImageTimestamp = std::chrono::microseconds((*itr)->pkt_dts);

    return (*itr)->toImage();
}

void VideoCache::add(const QnUuid& resourceId, const CLVideoDecoderOutputPtr& frame)
{
    QnMutexLocker lock(&m_mutex);
    auto itr = m_cache.find(resourceId);
    if (itr == m_cache.end())
        return;

    auto& queue = itr.value();
    while (!queue.empty() && frame->pkt_dts - queue.front()->pkt_dts >= m_cacheSize.count())
        queue.pop_front();

    CLVideoDecoderOutputPtr frameToAdd(frame);
    if (frame->isExternalData())
    {
        frameToAdd.reset(new CLVideoDecoderOutput());
        frame->copyFrom(frameToAdd.get());
    }

    auto posItr = std::upper_bound(queue.begin(), queue.end(),
        frame,
        [](const CLVideoDecoderOutputPtr& left, const CLVideoDecoderOutputPtr& right)
    {
        return left->pkt_dts < right->pkt_dts;
    });
    queue.insert(posItr, frameToAdd);
}

} // namespace nx::vms::client::desktop
