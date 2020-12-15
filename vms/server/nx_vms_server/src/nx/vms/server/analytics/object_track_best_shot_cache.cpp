#include "object_track_best_shot_cache.h"
#include <utils/common/synctime.h>

namespace nx::vms::server::analytics {

using namespace std::chrono;
using namespace std::literals::chrono_literals;

using Image = nx::analytics::db::Image;

static const seconds kMaxBestShotWaitTime = 10s;

ObjectTrackBestShotCache::~ObjectTrackBestShotCache()
{
}

void ObjectTrackBestShotCache::promiseBestShot(QnUuid trackId)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_imageByTrackId[trackId] = ImageInfo();
    m_imageByTrackId[trackId].timer.restart();

    const std::chrono::microseconds currentTime = m_timeProvider->currentTime();

    m_trackIdByTimestamp.emplace(currentTime + kMaxBestShotWaitTime, trackId);
}

void ObjectTrackBestShotCache::insert(QnUuid trackId, nx::analytics::db::Image image)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    const std::chrono::microseconds currentTime = m_timeProvider->currentTime();

    m_imageByTrackId[trackId] = {nx::utils::ElapsedTimer(), std::move(image)};
    m_trackIdByTimestamp.emplace(currentTime, trackId);

    removeOldEntries();

    m_waitCondition.wakeAll();
}

std::optional<nx::analytics::db::Image> ObjectTrackBestShotCache::fetch(QnUuid trackId) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    for (;;)
    {
        const auto imageIt = m_imageByTrackId.find(trackId);
        if (imageIt == m_imageByTrackId.cend())
            return std::nullopt;

        ImageInfo& imageInfo = imageIt->second;

        if (imageInfo.image || !imageInfo.timer.isValid())
            return imageInfo.image;

        const milliseconds elapsed = imageIt->second.timer.elapsed();
        if (elapsed >= kMaxBestShotWaitTime)
        {
            imageInfo.timer.invalidate();
            return imageInfo.image;
        }

        m_waitCondition.wait(&m_mutex, kMaxBestShotWaitTime - elapsed);
    }

    return std::nullopt;
}

void ObjectTrackBestShotCache::removeOldEntries()
{
    const std::chrono::microseconds currentTime = m_timeProvider->currentTime();
    const std::chrono::microseconds timestampToDeleteUpTo = currentTime - m_maxImageLifetime;

    const auto rangeToDeleteEndIt = m_trackIdByTimestamp.upper_bound(timestampToDeleteUpTo);
    for (auto it = m_trackIdByTimestamp.cbegin(); it != rangeToDeleteEndIt; ++it)
        m_imageByTrackId.erase(it->second);

    m_trackIdByTimestamp.erase(m_trackIdByTimestamp.cbegin(), rangeToDeleteEndIt);
}

} // namespace nx::vms::server::analytics
