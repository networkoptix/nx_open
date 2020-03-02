#pragma once

#include <vector>

#include <nx/utils/thread/mutex.h>

#include <analytics/db/analytics_db_types.h>
#include <common/common_module.h>

#include "analytics_archive.h"

namespace nx::sql { class QueryContext; }

namespace nx::analytics::db {

class AttributesDao;
class ObjectTypeDao;

using ArchiveFilter = AnalyticsArchive::AnalyticsFilter;
using AnalyticsArchiveImpl = AnalyticsArchive;

class NX_ANALYTICS_DB_API AnalyticsArchiveDirectory
{
public:
    struct ObjectTrackMatchResult
    {
        std::vector<std::int64_t> trackGroups;
        QnTimePeriod timePeriod;
    };

    AnalyticsArchiveDirectory(
        QnCommonModule* commonModule,
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

    /**
     * @return std::nullopt if filter specifies an empty data set.
     * I.e., no lookup makes sense.
     */
    static std::optional<ArchiveFilter> prepareArchiveFilter(
        nx::sql::QueryContext* queryContext,
        const db::Filter& filter,
        const ObjectTypeDao& objectTypeDao,
        AttributesDao* attributesDao);

private:
    QnCommonModule* m_commonModule = nullptr;
    const QString m_dataDir;
    std::map<QnUuid, std::unique_ptr<AnalyticsArchiveImpl>> m_deviceIdToArchive;
    QnMutex m_mutex;

    AnalyticsArchiveImpl* openOrGetArchive(const QnUuid& deviceId);

    void fixFilter(ArchiveFilter* filter);

    AnalyticsArchive::MatchObjectsResult matchObjects(
        const QnUuid& deviceId,
        const ArchiveFilter& filter);

    using TrackGroups =
        std::vector<std::pair<std::chrono::milliseconds /*timestamp*/, int64_t /*trackGroupId*/>>;

    ObjectTrackMatchResult toObjectTrackMatchResult(
        const ArchiveFilter& filter,
        TrackGroups trackGroups);

    void copyAllDeviceIds(std::vector<QnUuid>* deviceIds);
};

} // namespace nx::analytics::db
