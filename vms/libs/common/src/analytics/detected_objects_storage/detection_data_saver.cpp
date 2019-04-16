#include "detection_data_saver.h"

#include <nx/fusion/serialization/sql_functions.h>

#include "attributes_dao.h"
#include "device_dao.h"
#include "object_type_dao.h"
#include "serializers.h"

namespace nx::analytics::storage {

static constexpr int kMsInUsec = 1000;

DetectionDataSaver::DetectionDataSaver(
    AttributesDao* attributesDao,
    DeviceDao* deviceDao,
    ObjectTypeDao* objectTypeDao,
    ObjectCache* objectCache)
    :
    m_attributesDao(attributesDao),
    m_deviceDao(deviceDao),
    m_objectTypeDao(objectTypeDao),
    m_objectCache(objectCache)
{
}

void DetectionDataSaver::load(ObjectTrackAggregator* trackAggregator)
{
    m_objectsToInsert = m_objectCache->getObjectsToInsert();
    m_objectsToUpdate = m_objectCache->getObjectsToUpdate();
    m_objectSearchData = trackAggregator->getAggregatedData();

    resolveObjectIds();
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

void DetectionDataSaver::resolveObjectIds()
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
                const auto dbId = m_objectCache->dbIdFromObjectId(objectId);
                if (dbId == kInvalidDbId)
                {
                    if (auto object = m_objectCache->getObjectToInsertForced(objectId);
                        object)
                    {
                        m_objectsToInsert.push_back(std::move(*object));
                    }
                    else
                    {
                        NX_ASSERT(false);
                        it = objectSearchGridCell.objectIds.erase(it);
                        continue;
                    }
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
        INSERT INTO object (device_id, object_type_id, guid, track_start_timestamp_ms,
            track_detail, attributes_id)
        VALUES (?, ?, ?, ?, ?, ?)
    )sql");

    for (const auto& object: m_objectsToInsert)
    {
        const auto deviceDbId = m_deviceDao->insertOrFetch(queryContext, object.deviceId);
        const auto objectTypeDbId = m_objectTypeDao->insertOrFetch(queryContext, object.objectTypeId);
        const auto attributesId = m_attributesDao->insertOrFetchAttributes(
            queryContext, object.attributes);

        query->bindValue(0, deviceDbId);
        query->bindValue(1, objectTypeDbId);
        query->bindValue(2, QnSql::serialized_field(object.objectAppearanceId));
        query->bindValue(3, object.track.front().timestampUsec / kMsInUsec);
        query->bindValue(4, TrackSerializer::serialize(object.track));
        query->bindValue(5, attributesId);

        query->exec();

        const auto objectDbId = query->lastInsertId().toLongLong();
        m_objectGuidToId[object.objectAppearanceId] = objectDbId;

        m_objectCache->saveObjectGuidToAttributesId(
            object.objectAppearanceId, attributesId);
    }
}

void DetectionDataSaver::updateObjects(nx::sql::QueryContext* queryContext)
{
    auto updateObjectQuery = queryContext->connection()->createQuery();
    updateObjectQuery->prepare(R"sql(
        UPDATE object
        SET track_detail = track_detail || ?, attributes_id = ?
        WHERE id = ?
    )sql");

    for (const auto& objectUpdate: m_objectsToUpdate)
    {
        const auto newAttributesId = m_attributesDao->insertOrFetchAttributes(
            queryContext, objectUpdate.allAttributes);

        updateObjectQuery->bindValue(0, TrackSerializer::serialize(objectUpdate.appendedTrack));
        updateObjectQuery->bindValue(1, newAttributesId);
        updateObjectQuery->bindValue(2, objectUpdate.dbId);
        updateObjectQuery->exec();

        m_objectCache->saveObjectGuidToAttributesId(objectUpdate.objectId, newAttributesId);
    }
}

void DetectionDataSaver::saveObjectSearchData(nx::sql::QueryContext* queryContext)
{
    using namespace std::chrono;

    auto insertObjectSearchCell = queryContext->connection()->createQuery();
    insertObjectSearchCell->prepare(R"sql(
        INSERT INTO object_search(timestamp_seconds_utc,
            box_top_left_x, box_top_left_y, box_bottom_right_x, box_bottom_right_y,
            object_id_list)
        VALUES (?, ?, ?, ?, ?, ?)
    )sql");

    auto insertObjectSearchToAttributesBinding = queryContext->connection()->createQuery();
    insertObjectSearchToAttributesBinding->prepare(R"sql(
        INSERT OR IGNORE INTO unique_attributes_to_object_search(attributes_id, object_search_id)
        VALUES (?, ?)
    )sql");

    for (const auto& objectSearchGridCell: m_objectSearchData)
    {
        insertObjectSearchCell->bindValue(0,
            (long long) duration_cast<seconds>(objectSearchGridCell.timestamp).count());

        insertObjectSearchCell->bindValue(1, objectSearchGridCell.boundingBox.topLeft().x());
        insertObjectSearchCell->bindValue(2, objectSearchGridCell.boundingBox.topLeft().y());
        insertObjectSearchCell->bindValue(3, objectSearchGridCell.boundingBox.bottomRight().x());
        insertObjectSearchCell->bindValue(4, objectSearchGridCell.boundingBox.bottomRight().y());

        insertObjectSearchCell->bindValue(5,
            compact_int::serialized(toDbIds(objectSearchGridCell.objectIds)));

        insertObjectSearchCell->exec();
        const auto objectSearchCellId = insertObjectSearchCell->lastInsertId().toLongLong();

        for (const auto& objectId: objectSearchGridCell.objectIds)
        {
            const auto attributesId = m_objectCache->getAttributesIdByObjectGuid(objectId);

            insertObjectSearchToAttributesBinding->bindValue(0, attributesId);
            insertObjectSearchToAttributesBinding->bindValue(1, objectSearchCellId);
            insertObjectSearchToAttributesBinding->exec();
        }
    }
}

std::vector<long long> DetectionDataSaver::toDbIds(
    const std::set<QnUuid>& objectIds)
{
    std::vector<long long> dbIds;
    for (const auto& objectId: objectIds)
    {
        if (auto it = m_objectGuidToId.find(objectId);
            it != m_objectGuidToId.end())
        {
            dbIds.push_back(it->second);
        }
    }

    return dbIds;
}

} // namespace nx::analytics::storage
