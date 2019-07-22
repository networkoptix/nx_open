#pragma once

//#define USE_IN_MEMORY_ARCHIVE

#include <vector>

#include <nx/utils/thread/mutex.h>

#include <nx/vms/server/metadata/analytics_archive.h>
#include <analytics/db/analytics_db_types.h>

#include "analytics_archive.h"
#include "media_server/media_server_module.h"

namespace nx::analytics::db {

#ifdef USE_IN_MEMORY_ARCHIVE
using AnalyticsArchiveImpl = AnalyticsArchive;
#else
using AnalyticsArchiveImpl = nx::vms::server::metadata::AnalyticsArchive;
#endif

class ObjectTypeDao;

class AnalyticsArchiveDirectory
{
public:
    using Filter = nx::vms::server::metadata::AnalyticsArchive::AnalyticsFilter;

    struct ObjectTrackMatchResult
    {
        std::vector<std::int64_t> trackGroups;
        QnTimePeriod timePeriod;
    };

    AnalyticsArchiveDirectory(
        QnMediaServerModule* mediaServerModule,
        const QString& dataDir);

    virtual ~AnalyticsArchiveDirectory() = default;

    /**
     * @param region Region on the search grid. (Qn::kMotionGridWidth * Qn::kMotionGridHeight).
     */
    bool saveToArchive(
        const QnUuid& deviceId,
        std::chrono::milliseconds timestamp,
        const std::vector<QRect>& region,
        uint32_t objectTrackGroupId,
        uint32_t objectType,
        int64_t allAttributesHash);

    QnTimePeriodList matchPeriods(
        const std::vector<QnUuid>& deviceIds,
        Filter filter);

    QnTimePeriodList matchPeriods(
        const QnUuid& deviceId,
        const Filter& filter);

    /**
     * NOTE: This method selects object groups by filter.
     * Object groups have to be converted to objects (and filtered) later.
     */
    ObjectTrackMatchResult matchObjects(
        const std::vector<QnUuid>& deviceIds,
        Filter filter);

    static AnalyticsArchive::Filter prepareArchiveFilter(
        const db::Filter& filter,
        const ObjectTypeDao& objectTypeDao);

private:
    QnMediaServerModule* m_mediaServerModule = nullptr;
    const QString m_dataDir;
    std::map<QnUuid, std::unique_ptr<AnalyticsArchiveImpl>> m_deviceIdToArchive;
    QnMutex m_mutex;

    AnalyticsArchiveImpl* openOrGetArchive(const QnUuid& deviceId);

    void fixFilterRegion(Filter* filter);

    nx::vms::server::metadata::AnalyticsArchive::MatchObjectsResult matchObjects(
        const QnUuid& deviceId,
        const Filter& filter);

    using TrackGroups =
        std::vector<std::pair<std::chrono::milliseconds /*timestamp*/, int64_t /*trackGroupId*/>>;

    ObjectTrackMatchResult toObjectTrackMatchResult(
        const Filter& filter,
        TrackGroups trackGroups);
};

} // namespace nx::analytics::db
