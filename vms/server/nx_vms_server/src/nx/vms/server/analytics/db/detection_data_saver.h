#pragma once

#include <QtGui/QRegion>

#include <nx/sql/query_context.h>

#include <analytics/db/analytics_db_types.h>

#include "object_cache.h"
#include "object_track_aggregator.h"

namespace nx::analytics::db {

class AttributesDao;
class DeviceDao;
class ObjectTypeDao;
class AnalyticsArchiveDirectory;

class DetectionDataSaver
{
public:
    DetectionDataSaver(
        AttributesDao* attributesDao,
        DeviceDao* deviceDao,
        ObjectTypeDao* objectTypeDao,
        ObjectCache* objectCache,
        AnalyticsArchiveDirectory* analyticsArchive);

    DetectionDataSaver(const DetectionDataSaver&) = delete;
    DetectionDataSaver& operator=(const DetectionDataSaver&) = delete;
    DetectionDataSaver(DetectionDataSaver&&) = default;
    DetectionDataSaver& operator=(DetectionDataSaver&&) = default;

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
    struct AnalArchiveItem
    {
        QnUuid deviceId;
        int objectType = -1;
        std::chrono::milliseconds timestamp = std::chrono::milliseconds::zero();
        /**
         * This region is given with object search grid resolution.
         */
        QRegion region;
        long long combinedAttributesId = -1;
    };

    struct ObjectDbAttributes
    {
        QnUuid deviceId;
        int objectTypeId = -1;
        long long attributesDbId = -1;
    };

    AttributesDao* m_attributesDao = nullptr;
    DeviceDao* m_deviceDao = nullptr;
    ObjectTypeDao* m_objectTypeDao = nullptr;
    ObjectCache* m_objectCache = nullptr;
    AnalyticsArchiveDirectory* m_analyticsArchive = nullptr;

    std::vector<DetectedObject> m_objectsToInsert;
    std::vector<ObjectUpdate> m_objectsToUpdate;
    std::vector<AggregatedTrackData> m_objectSearchData;
    std::map<QnUuid, long long> m_objectGuidToId;

    void resolveObjectIds();

    void insertObjects(nx::sql::QueryContext* queryContext);

    std::pair<qint64, qint64> findMinMaxTimestamp(
        const std::vector<ObjectPosition>& track);

    void updateObjects(nx::sql::QueryContext* queryContext);

    void saveObjectSearchData(nx::sql::QueryContext* queryContext);

    void saveToAnalyticsArchive(nx::sql::QueryContext* queryContext);
    std::vector<AnalArchiveItem> prepareArchiveData(nx::sql::QueryContext* queryContext);
    ObjectDbAttributes getObjectDbDataById(const QnUuid& objectId);
    
    long long combineAttributes(
        nx::sql::QueryContext* queryContext,
        const std::vector<long long>& attributesIds);
};

} // namespace nx::analytics::db
