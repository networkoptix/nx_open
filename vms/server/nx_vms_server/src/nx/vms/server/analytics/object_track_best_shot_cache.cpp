#include "object_track_best_shot_cache.h"
#include <utils/common/synctime.h>

namespace nx::vms::server::analytics {

using namespace std::chrono;

ObjectTrackBestShotCache::~ObjectTrackBestShotCache()
{
}

void ObjectTrackBestShotCache::insert(QnUuid trackId, nx::analytics::db::Image image)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    const std::chrono::microseconds currentTime = m_timeProvider->currentTime();

    m_imageByTrackId[trackId] = std::move(image);
    m_trackIdByTimestamp.emplace(currentTime, trackId);

    removeOldEntries();
}

std::optional<nx::analytics::db::Image> ObjectTrackBestShotCache::fetch(QnUuid trackId) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    const auto imageIt = m_imageByTrackId.find(trackId);
    if (imageIt == m_imageByTrackId.cend())
        return std::nullopt;

    return imageIt->second;
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
