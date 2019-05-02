#pragma once

#include <nx/sql/query_context.h>

#include "analytics_events_storage_types.h"
#include "object_cache.h"
#include "object_track_aggregator.h"

namespace nx::analytics::storage {

class AttributesDao;
class DeviceDao;
class ObjectTypeDao;

class DetectionDataSaver
{
public:
    DetectionDataSaver(
        AttributesDao* attributesDao,
        DeviceDao* deviceDao,
        ObjectTypeDao* objectTypeDao);

    void load(
        ObjectCache* objectCache,
        ObjectTrackAggregator* trackAggregator);

    bool empty() const;

    /**
     * NOTE: Throws on failure.
     */
    void save(nx::sql::QueryContext* queryContext);

private:
    AttributesDao* m_attributesDao = nullptr;
    DeviceDao* m_deviceDao = nullptr;
    ObjectTypeDao* m_objectTypeDao = nullptr;

    std::vector<DetectedObject> m_objectsToInsert;
    std::vector<ObjectUpdate> m_objectsToUpdate;
    std::vector<AggregatedTrackData> m_objectSearchData;
    std::map<QnUuid, long long> m_objectGuidToId;

    void resolveObjectIds(const ObjectCache& objectCache);

    void insertObjects(nx::sql::QueryContext* queryContext);
    void updateObjects(nx::sql::QueryContext* queryContext);
    void saveObjectSearchData(nx::sql::QueryContext* queryContext);
};

} // namespace nx::analytics::storage
