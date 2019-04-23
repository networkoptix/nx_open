#include "video_cache.h"

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
    m_cacheSize = value;
}

std::chrono::microseconds VideoCache::cacheSize() const
{
    return m_cacheSize;
}

void VideoCache::setCachedDevices(QSet<QnUuid>& value)
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
    std::chrono::microseconds* outImageTimestamp)
{
    QnMutexLocker lock(&m_mutex);
    if (outImageTimestamp)
        *outImageTimestamp = kNoTimestamp;
    const auto& queue = m_cache.value(resourceId);
    auto itr = std::lower_bound(queue.begin(), queue.end(),
        timestamp,
        [](const CLVideoDecoderOutputPtr& left, const std::chrono::microseconds& right)
        {
            return left->pkt_dts < right.count();
        });
    if (itr == queue.end())
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
        CLVideoDecoderOutput::copy(frame.get(), frameToAdd.get());
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
