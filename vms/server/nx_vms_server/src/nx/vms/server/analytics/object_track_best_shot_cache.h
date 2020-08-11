#pragma once

#include <map>
#include <optional>
#include <chrono>

#include <QtCore/QString>

#include <nx/utils/uuid.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/time/abstract_time_provider.h>
#include <analytics/common/object_metadata.h>
#include <nx/analytics/db/abstract_object_track_best_shot_cache.h>

namespace nx::vms::server::analytics {

class ObjectTrackBestShotCache:
    public QObject,
    public nx::analytics::db::AbstractObjectTrackBestShotCache
{
    Q_OBJECT
public:
    ObjectTrackBestShotCache(
        std::chrono::seconds maxImageLifetime,
        nx::utils::time::AbstractTimeProvider* timeProvider,
        QObject* parent = nullptr)
        :
        QObject(parent),
        m_maxImageLifetime(maxImageLifetime),
        m_timeProvider(timeProvider)
    {
    }

    virtual ~ObjectTrackBestShotCache() override;

    virtual void insert(QnUuid trackId, nx::analytics::db::Image image) override;

    virtual std::optional<nx::analytics::db::Image> fetch(QnUuid trackId) const override;

private:
    void removeOldEntries();

private:
    const std::chrono::microseconds m_maxImageLifetime;
    const nx::utils::time::AbstractTimeProvider* m_timeProvider = nullptr;

    std::map<QnUuid, nx::analytics::db::Image> m_imageByTrackId;
    std::multimap<std::chrono::microseconds, QnUuid> m_trackIdByTimestamp;

    mutable nx::Mutex m_mutex;
};

} // namespace nx::vms::server::analytics
