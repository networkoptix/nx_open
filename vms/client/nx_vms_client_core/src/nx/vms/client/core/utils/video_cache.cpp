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

void VideoCache::setCacheSize(std::chrono::microseconds value)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_cacheSize = value;
}

std::chrono::microseconds VideoCache::cacheSize() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_cacheSize;
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
        [this](qint64 value, qint64 minimum, qint64 maximum)
        {
            NX_VERBOSE(this, "Cached from %1 to %2, requested %3 - %4", minimum, maximum, value,
                ((value < minimum) ? "TOO OLD" : ((value > maximum) ? "TOO NEW" : "CACHED")));
        };

    logRange(timestamp.count(), queueItr->front()->pkt_dts, queueItr->back()->pkt_dts);

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

void VideoCache::add(const nx::Uuid& resourceId, const CLConstVideoDecoderOutputPtr& frame)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    auto itr = m_cache.find(resourceId);
    if (itr == m_cache.end())
        return;

    auto& queue = itr.value();
    while (!queue.empty() && frame->pkt_dts - queue.front()->pkt_dts >= m_cacheSize.count())
        queue.pop_front();

    NX_VERBOSE(this, "queue duration=%1us, size=%2",
        queue.empty() ? 0 : frame->pkt_dts - queue.front()->pkt_dts,
        queue.size());

    auto posItr = std::upper_bound(queue.begin(), queue.end(),
        frame,
        [](const CLConstVideoDecoderOutputPtr& left, const CLConstVideoDecoderOutputPtr& right)
    {
        return left->pkt_dts < right->pkt_dts;
    });
    queue.insert(posItr, frame);

    if (queue.size() > kCacheHardLimit)
    {
        NX_DEBUG(this, "Drop image from video cache, the hard limit has been exceeded");
        queue.pop_front();
    }
}

} // namespace nx::vms::client::core
