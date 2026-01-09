// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "video_cache.h"

#include <nx/utils/log/log.h>

using namespace std::chrono;

namespace nx::vms::client::core {

namespace {

constexpr auto kDefaultCacheSize = 1s;
constexpr int kCacheHardLimit = 1800; //< 30 sec with 60 fps

} // namespace

const std::chrono::microseconds VideoCache::kNoTimestamp(-1);

VideoCache::VideoCache(QObject* parent):
    QObject(parent),
    m_cacheSize(kDefaultCacheSize)
{
}

void VideoCache::setCacheSize(microseconds value)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_cacheSize = value;
}

microseconds VideoCache::cacheSize() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_cacheSize;
}

int VideoCache::fpsLimit() const
{
    return m_fpsLimit;
}

void VideoCache::setFpsLimit(int value)
{
    m_fpsLimit = value;
    m_minDeltaUs = value > 0 ? (microseconds(1s).count() / value) : 0;
}

void VideoCache::setCachedDevices(intptr_t consumerId, const QSet<nx::Uuid>& value)
{
    m_cachedDevices[consumerId] = value;
    QSet<nx::Uuid> totalCachedDevices;

    for (const auto& subset: m_cachedDevices)
        totalCachedDevices += subset;

    setCachedDevices(totalCachedDevices);
}

void VideoCache::setCachedDevices(const QSet<nx::Uuid>& value)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
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
    const nx::Uuid& resourceId,
    std::chrono::microseconds timestamp,
    std::chrono::microseconds* outImageTimestamp) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (outImageTimestamp)
        *outImageTimestamp = kNoTimestamp;

    const auto queueItr = m_cache.find(resourceId);
    if (queueItr == m_cache.end() || queueItr->empty())
        return QImage();

    const auto logRange =
        [this](microseconds value, microseconds minimum, microseconds maximum)
        {
            const auto cacheLength =
                [=]() { return duration_cast<milliseconds>(maximum - minimum); };

            const auto verdict =
                [=]()
                {
                    return value < minimum ? "too old" : (value > maximum ? "too new" : "cached");
                };

            const auto delta =
                [=]()
                {
                    return value < minimum
                        ? duration_cast<milliseconds>(minimum - value)
                        : duration_cast<milliseconds>(std::chrono::abs(maximum - value));
                };

            NX_VERBOSE(this, "Cached from %1 to %2 (%3), requested %4 - %5 (%6 delta)",
                minimum, maximum, cacheLength(), value, verdict(), delta());
        };

    logRange(timestamp,
        microseconds(queueItr->front()->pkt_dts), microseconds(queueItr->back()->pkt_dts));

    auto itr = std::lower_bound(queueItr->begin(), queueItr->end(),
        timestamp,
        [](const CLConstVideoDecoderOutputPtr& left, const std::chrono::microseconds& right)
        {
            return left->pkt_dts < right.count();
        });

    if (itr == queueItr->end())
        return QImage();

    if (outImageTimestamp)
        *outImageTimestamp = std::chrono::microseconds((*itr)->pkt_dts);

    return (*itr)->toImage();
}

bool VideoCache::add(const nx::Uuid& resourceId, const CLConstVideoDecoderOutputPtr& frame)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    auto it = m_cache.find(resourceId);
    if (it == m_cache.end())
        return false;

    auto& queue = it.value();
    auto begIt = std::upper_bound(queue.begin(), queue.end(), frame->pkt_dts - m_cacheSize.count(),
        [](qint64 leftUs, const auto& right) { return leftUs < right->pkt_dts; });

    const auto numberToDrop = begIt - queue.cbegin();
    if (numberToDrop > 0)
    {
        NX_TRACE(this, "Dropping %1 outdated frame(s) from the queue", numberToDrop);
        queue.erase(queue.begin(), begIt);
    }

    NX_TRACE(this, "Queue duration=%1us, size=%2",
        queue.empty() ? 0 : frame->pkt_dts - queue.front()->pkt_dts,
        queue.size());

    auto posIt = std::upper_bound(queue.begin(), queue.end(), frame,
        [](const auto& left, const auto& right) { return left->pkt_dts < right->pkt_dts; });

    if (m_minDeltaUs > 0)
    {
        if (posIt != queue.cbegin() && (frame->pkt_dts - (*(posIt - 1))->pkt_dts) < m_minDeltaUs)
        {
            NX_TRACE(this, "Dropping frame %1 due to FPS limit (too close to %2)",
                frame->pkt_dts, (*(posIt - 1))->pkt_dts);
            return false;
        }

        if (posIt != queue.cend() && ((*posIt)->pkt_dts - frame->pkt_dts) < m_minDeltaUs)
        {
            NX_TRACE(this, "Dropping frame %1 due to FPS limit (too close to %2)",
                frame->pkt_dts, (*posIt)->pkt_dts);
            return false;
        }
    }

    queue.insert(posIt, frame);

    if (queue.size() > kCacheHardLimit)
    {
        NX_VERBOSE(this, "Dropping the oldest frame from the queue, hard limit has been exceeded");
        queue.pop_front();
    }

    return true;
}

} // namespace nx::vms::client::core
