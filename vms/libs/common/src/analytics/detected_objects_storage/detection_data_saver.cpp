#include "detection_data_saver.h"

#include <nx/fusion/serialization/sql_functions.h>

#include "attributes_dao.h"
#include "device_dao.h"
#include "object_type_dao.h"

namespace nx::analytics::storage {

static constexpr int kMsInUsec = 1000;

DetectionDataSaver::DetectionDataSaver(
    AttributesDao* attributesDao,
    DeviceDao* deviceDao,
    ObjectTypeDao* objectTypeDao)
    :
    m_attributesDao(attributesDao),
    m_deviceDao(deviceDao),
    m_objectTypeDao(objectTypeDao)
{
}

void DetectionDataSaver::load(
    ObjectCache* objectCache,
    ObjectTrackAggregator* trackAggregator)
{
    m_objectsToInsert = objectCache->getObjectsToInsert();
    m_objectsToUpdate = objectCache->getObjectsToUpdate();
    m_objectSearchData = trackAggregator->getAggregatedData();

    resolveObjectIds(*objectCache);
}

bool DetectionDataSaver::empty() const
{
    return m_objectsToInsert.empty()
        && m_objectsToUpdate.empty()
        && m_objectSearchData.empty();
}

void DetectionDataSaver::save(nx::sql::QueryContext* queryContext)
{
    insertObjects(queryContext);
    updateObjects(queryContext);
    saveObjectSearchData(queryContext);
}

void DetectionDataSaver::resolveObjectIds(const ObjectCache& objectCache)
{
    for (auto& objectSearchGridCell: m_objectSearchData)
    {
        for (auto it = objectSearchGridCell.objectIds.begin();
            it != objectSearchGridCell.objectIds.end();
            )
        {
            const auto& objectId = *it;

            auto newObjectIter = std::find_if(
                m_objectsToInsert.begin(), m_objectsToInsert.end(),
                [objectId](const DetectedObject& detectedObject)
                { return detectedObject.objectAppearanceId == objectId; });

            if (newObjectIter == m_objectsToInsert.end())
            {
                // Object MUST be known.
                const auto dbId = objectCache.dbIdFromObjectId(objectId);
                NX_ASSERT(dbId != kInvalidDbId);
                if (dbId == kInvalidDbId)
                {
                    it = objectSearchGridCell.objectIds.erase(it);
                    continue;
                }

                m_objectGuidToId[objectId] = dbId;
            }

            ++it;
        }
    }
}

void DetectionDataSaver::insertObjects(nx::sql::QueryContext* queryContext)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(R"sql(
        INSERT INTO object (device_id, object_type_id, guid, track_start_timestamp_ms, track_detail, attributes_id)
        VALUES (?, ?, ?, ?, ?, ?)
    )sql");

    for (const auto& object: m_objectsToInsert)
    {
        const auto attributesId = m_attributesDao->insertOrFetchAttributes(
            queryContext, object.attributes);

        const auto deviceDbId = m_deviceDao->insertOrFetch(queryContext, object.deviceId);
        const auto objectTypeDbId = m_objectTypeDao->insertOrFetch(queryContext, object.objectTypeId);

        query->bindValue(0, deviceDbId);
        query->bindValue(1, objectTypeDbId);
        query->bindValue(2, QnSql::serialized_field(object.objectAppearanceId));
        query->bindValue(3, object.track.front().timestampUsec / kMsInUsec); //< TODO
        query->bindValue(4, serializeTrack(object.track));
        query->bindValue(5, attributesId);

        query->exec();

        const auto objectDbId = query->lastInsertId().toLongLong();
        m_objectGuidToId[object.objectAppearanceId] = objectDbId;
    }
}

void DetectionDataSaver::updateObjects(nx::sql::QueryContext* queryContext)
{
    // TODO: Updating existing object.
    // TODO: Updating attributes for the full-text search.
}

void DetectionDataSaver::saveObjectSearchData(nx::sql::QueryContext* queryContext)
{
    // TODO
}

} // namespace nx::analytics::storage
