#pragma once

#include <set>

#include <QtGui/QRegion>

#include <nx/sql/query_context.h>

#include <analytics/db/analytics_db_types.h>

#include "object_track_cache.h"
#include "object_track_aggregator.h"

namespace nx::analytics::db {

class AttributesDao;
class DeviceDao;
class ObjectTypeDao;
class ObjectTrackGroupDao;
class AnalyticsArchiveDirectory;

class ObjectTrackDataSaver
{
public:
    ObjectTrackDataSaver(
        AttributesDao* attributesDao,
        DeviceDao* deviceDao,
        ObjectTypeDao* objectTypeDao,
        ObjectTrackGroupDao* objectGroupDao,
        ObjectTrackCache* trackCache,
        AnalyticsArchiveDirectory* analyticsArchive);

    ObjectTrackDataSaver(const ObjectTrackDataSaver&) = delete;
    ObjectTrackDataSaver& operator=(const ObjectTrackDataSaver&) = delete;
    ObjectTrackDataSaver(ObjectTrackDataSaver&&) = default;
    ObjectTrackDataSaver& operator=(ObjectTrackDataSaver&&) = default;

    /**
     * @param flush If true then all available data is loaded, the aggregation period is ignored.
     */
    void load(ObjectTrackAggregator* trackAggregator, bool flush);

    bool empty() const;

    /**
     * NOTE: Throws on failure.
     */
    void save(nx::sql::QueryContext* queryContext);

private:
    struct AnalyticsArchiveItem
    {
        QnUuid deviceId;
        uint32_t trackGroupId = 0;
        int objectType = -1;
        std::chrono::milliseconds timestamp = std::chrono::milliseconds::zero();
        /**
         * This region is given with object search grid resolution.
         */
        QRegion region;
        int64_t combinedAttributesId = -1;
    };

    struct ObjectTrackDbAttributes
    {
        QnUuid deviceId;
        int objectTypeId = -1;
        int64_t attributesDbId = -1;
    };

    AttributesDao* m_attributesDao = nullptr;
    DeviceDao* m_deviceDao = nullptr;
    ObjectTypeDao* m_objectTypeDao = nullptr;
    ObjectTrackGroupDao* m_trackGroupDao = nullptr;
    ObjectTrackCache* m_objectTrackCache = nullptr;
    AnalyticsArchiveDirectory* m_analyticsArchive = nullptr;

    std::vector<ObjectTrack> m_tracksToInsert;
    std::vector<ObjectTrackUpdate> m_tracksToUpdate;
    std::vector<AggregatedTrackData> m_trackSearchData;
    std::map<QnUuid, int64_t> m_trackGuidToId;

    void resolveTrackIds();

    void insertObjects(nx::sql::QueryContext* queryContext);

    std::pair<qint64, qint64> findMinMaxTimestamp(const std::vector<ObjectPosition>& track);

    void updateObjects(nx::sql::QueryContext* queryContext);

    void saveObjectSearchData(nx::sql::QueryContext* queryContext);

    void saveToAnalyticsArchive(nx::sql::QueryContext* queryContext);
    std::vector<AnalyticsArchiveItem> prepareArchiveData(nx::sql::QueryContext* queryContext);
    ObjectTrackDbAttributes getTrackDbDataById(const QnUuid& trackId);
};

} // namespace nx::analytics::db
