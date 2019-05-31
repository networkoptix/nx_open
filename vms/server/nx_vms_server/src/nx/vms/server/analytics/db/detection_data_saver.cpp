#include "detection_data_saver.h"

#include <nx/fusion/serialization/sql_functions.h>

#include <analytics/db/config.h>

#include "analytics_archive_directory.h"
#include "attributes_dao.h"
#include "device_dao.h"
#include "object_type_dao.h"
#include "object_group_dao.h"
#include "serializers.h"

namespace nx::analytics::db {

DetectionDataSaver::DetectionDataSaver(
    AttributesDao* attributesDao,
    DeviceDao* deviceDao,
    ObjectTypeDao* objectTypeDao,
    ObjectGroupDao* objectGroupDao,
    ObjectCache* objectCache,
    AnalyticsArchiveDirectory* analyticsArchive)
    :
    m_attributesDao(attributesDao),
    m_deviceDao(deviceDao),
    m_objectTypeDao(objectTypeDao),
    m_objectGroupDao(objectGroupDao),
    m_objectCache(objectCache),
    m_analyticsArchive(analyticsArchive)
{
}

void DetectionDataSaver::load(
    ObjectTrackAggregator* trackAggregator,
    bool flush)
{
    m_objectsToInsert = m_objectCache->getObjectsToInsert(flush);
    m_objectsToUpdate = m_objectCache->getObjectsToUpdate(flush);
    m_objectSearchData = trackAggregator->getAggregatedData(flush);

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
    if (m_analyticsArchive)
        saveToAnalyticsArchive(queryContext);
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
                        // The object insertion task may already be in the query queue.
                        // So, the dbId will be available when we need to insert.
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
        INSERT INTO object (device_id, object_type_id, guid,
            track_start_ms, track_end_ms, track_detail, attributes_id,
            best_shot_timestamp_ms, best_shot_rect)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    )sql");

    for (const auto& object: m_objectsToInsert)
    {
        const auto deviceDbId = m_deviceDao->insertOrFetch(queryContext, object.deviceId);
        const auto objectTypeDbId = m_objectTypeDao->insertOrFetch(queryContext, object.objectTypeId);
        const auto attributesId = m_attributesDao->insertOrFetchAttributes(
            queryContext, object.attributes);

        auto [trackMinTimestamp, trackMaxTimestamp] = findMinMaxTimestamp(object.track);

        query->bindValue(0, deviceDbId);
        query->bindValue(1, (long long) objectTypeDbId);
        query->bindValue(2, QnSql::serialized_field(object.objectAppearanceId));
        query->bindValue(3, trackMinTimestamp / kUsecInMs);
        query->bindValue(4, trackMaxTimestamp / kUsecInMs);
        query->bindValue(5, TrackSerializer::serialized(object.track));
        query->bindValue(6, (long long) attributesId);
        query->bindValue(7, object.bestShot.initialized()
            ? object.bestShot.timestampUsec / kUsecInMs
            : 0);
        query->bindValue(8, object.bestShot.initialized()
            ? TrackSerializer::serialized(object.bestShot.rect)
            : QByteArray());

        query->exec();

        const auto objectDbId = query->lastInsertId().toLongLong();
        m_objectGuidToId[object.objectAppearanceId] = objectDbId;
        m_objectCache->setObjectIdInDb(object.objectAppearanceId, objectDbId);

        m_objectCache->saveObjectGuidToAttributesId(
            object.objectAppearanceId, attributesId);
    }
}

std::pair<qint64, qint64> DetectionDataSaver::findMinMaxTimestamp(
    const std::vector<ObjectPosition>& track)
{
    auto timestamps = std::make_pair<qint64, qint64>(
        std::numeric_limits<qint64>::max(),
        std::numeric_limits<qint64>::min());

    for (const auto& pos: track)
    {
        if (pos.timestampUsec < timestamps.first)
            timestamps.first = pos.timestampUsec;
        if (pos.timestampUsec > timestamps.second)
            timestamps.second = pos.timestampUsec;
    }

    return timestamps;
}

void DetectionDataSaver::updateObjects(nx::sql::QueryContext* queryContext)
{
    auto updateObjectQuery = queryContext->connection()->createQuery();
    updateObjectQuery->prepare(R"sql(
        UPDATE object
        SET track_detail = CAST(track_detail || CAST(? AS BLOB) AS BLOB),
            attributes_id = ?,
            track_start_ms = min(track_start_ms, ?),
            track_end_ms = max(track_end_ms, ?)
        WHERE id = ?
    )sql");

    for (const auto& objectUpdate: m_objectsToUpdate)
    {
        const auto newAttributesId = m_attributesDao->insertOrFetchAttributes(
            queryContext, objectUpdate.allAttributes);

        auto [trackMinTimestamp, trackMaxTimestamp] =
            findMinMaxTimestamp(objectUpdate.appendedTrack);

        updateObjectQuery->bindValue(0, TrackSerializer::serialized(objectUpdate.appendedTrack));
        updateObjectQuery->bindValue(1, (long long) newAttributesId);
        updateObjectQuery->bindValue(2, trackMinTimestamp / kUsecInMs);
        updateObjectQuery->bindValue(3, trackMaxTimestamp / kUsecInMs);
        updateObjectQuery->bindValue(4, objectUpdate.dbId != -1
            ? (long long) objectUpdate.dbId
            : (long long) m_objectCache->dbIdFromObjectId(objectUpdate.objectId));
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
            object_group_id)
        VALUES (?, ?, ?, ?, ?, ?)
    )sql");

    for (const auto& objectSearchGridCell: m_objectSearchData)
    {
        std::set<int64_t> objectDbIds;
        std::transform(
            objectSearchGridCell.objectIds.begin(), objectSearchGridCell.objectIds.end(),
            std::inserter(objectDbIds, objectDbIds.end()),
            [this](const QnUuid& objectId) { return m_objectCache->dbIdFromObjectId(objectId); });

        const auto objectGroupId = m_objectGroupDao->insertOrFetchGroup(queryContext, objectDbIds);

        insertObjectSearchCell->bindValue(0,
            (long long) duration_cast<seconds>(objectSearchGridCell.timestamp).count());

        insertObjectSearchCell->bindValue(1, objectSearchGridCell.boundingBox.topLeft().x());
        insertObjectSearchCell->bindValue(2, objectSearchGridCell.boundingBox.topLeft().y());
        insertObjectSearchCell->bindValue(3, objectSearchGridCell.boundingBox.bottomRight().x());
        insertObjectSearchCell->bindValue(4, objectSearchGridCell.boundingBox.bottomRight().y());
        insertObjectSearchCell->bindValue(5, (long long) objectGroupId);

        insertObjectSearchCell->exec();
    }
}

void DetectionDataSaver::saveToAnalyticsArchive(nx::sql::QueryContext* queryContext)
{
    if (m_objectSearchData.empty())
        return;

    const auto data = prepareArchiveData(queryContext);

    for (const auto& item: data)
    {
        const auto result = m_analyticsArchive->saveToArchive(
            item.deviceId,
            item.timestamp,
            std::vector<QRect>(item.region.begin(), item.region.end()),
            item.objectType,
            item.combinedAttributesId);
        if (!result)
            NX_INFO(this, "Failed to save analytics data. device %1", item.deviceId);
    }
}

std::vector<DetectionDataSaver::AnalArchiveItem>
    DetectionDataSaver::prepareArchiveData(nx::sql::QueryContext* queryContext)
{
    struct Item
    {
        QRegion region;
        std::set<int64_t> attributesIds;
    };

    std::map<std::pair<QnUuid, int /*objectType*/>, Item> regionByObjectType;

    std::chrono::milliseconds minTimestamp = m_objectSearchData.front().timestamp;
    for (const auto& aggregatedTrackData: m_objectSearchData)
    {
        // NOTE: All timestamp belong to the same period of size of kTrackAggregationPeriod.
        if (minTimestamp > aggregatedTrackData.timestamp)
            minTimestamp = aggregatedTrackData.timestamp;

        for (const auto& objectId: aggregatedTrackData.objectIds)
        {
            const auto objectAttributes = getObjectDbDataById(objectId);

            Item& item = regionByObjectType[
                {objectAttributes.deviceId, objectAttributes.objectTypeId}];
            item.region += aggregatedTrackData.boundingBox;
            item.attributesIds.insert(objectAttributes.attributesDbId);
        }
    }

    std::vector<DetectionDataSaver::AnalArchiveItem> result;
    for (const auto& [deviceIdAndObjectType, item]: regionByObjectType)
    {
        AnalArchiveItem archiveItem;
        archiveItem.deviceId = deviceIdAndObjectType.first;
        archiveItem.combinedAttributesId = 
            m_attributesDao->combineAttributes(queryContext, item.attributesIds);
        archiveItem.objectType = deviceIdAndObjectType.second;
        archiveItem.region = item.region;
        archiveItem.timestamp = minTimestamp;
        result.push_back(std::move(archiveItem));
    }

    return result;
}

DetectionDataSaver::ObjectDbAttributes
    DetectionDataSaver::getObjectDbDataById(const QnUuid& objectId)
{
    ObjectDbAttributes result;
    result.attributesDbId = m_objectCache->getAttributesIdByObjectGuid(objectId);
    if (auto object = m_objectCache->getObjectById(objectId); object)
    {
        result.objectTypeId = m_objectTypeDao->objectTypeIdFromName(object->objectTypeId);
        result.deviceId = object->deviceId;
    }

    return result;
}

} // namespace nx::analytics::db
