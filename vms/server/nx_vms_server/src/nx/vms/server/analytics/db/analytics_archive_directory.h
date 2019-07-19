#pragma once

#include <vector>

#include <nx/utils/thread/mutex.h>

#include <nx/vms/server/metadata/analytics_archive.h>
#include <analytics/db/analytics_db_types.h>

#include "media_server/media_server_module.h"

namespace nx::analytics::db {

class ObjectTypeDao;

using ArchiveFilter = nx::vms::server::metadata::AnalyticsArchive::AnalyticsFilter;
using AnalyticsArchiveImpl = nx::vms::server::metadata::AnalyticsArchive;

class AnalyticsArchiveDirectory
{
public:
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
        std::vector<QnUuid> deviceIds,
        ArchiveFilter filter);

    QnTimePeriodList matchPeriods(
        const QnUuid& deviceId,
        const ArchiveFilter& filter);

    /**
     * NOTE: This method selects object groups by filter.
     * Object groups have to be converted to objects (and filtered) later.
     */
    ObjectTrackMatchResult matchObjects(
        std::vector<QnUuid> deviceIds,
        ArchiveFilter filter);

    static ArchiveFilter prepareArchiveFilter(
        const db::Filter& filter,
        const ObjectTypeDao& objectTypeDao);

private:
    QnMediaServerModule* m_mediaServerModule = nullptr;
    const QString m_dataDir;
    std::map<QnUuid, std::unique_ptr<AnalyticsArchiveImpl>> m_deviceIdToArchive;
    QnMutex m_mutex;

    AnalyticsArchiveImpl* openOrGetArchive(const QnUuid& deviceId);

    void fixFilterRegion(ArchiveFilter* filter);

    nx::vms::server::metadata::AnalyticsArchive::MatchObjectsResult matchObjects(
        const QnUuid& deviceId,
        const ArchiveFilter& filter);

    using TrackGroups =
        std::vector<std::pair<std::chrono::milliseconds /*timestamp*/, int64_t /*trackGroupId*/>>;

    ObjectTrackMatchResult toObjectTrackMatchResult(
        const ArchiveFilter& filter,
        TrackGroups trackGroups);
};

} // namespace nx::analytics::db
