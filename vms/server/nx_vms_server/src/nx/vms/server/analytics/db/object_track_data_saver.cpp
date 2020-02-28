#include "object_track_data_saver.h"

#include <nx/fusion/serialization/sql_functions.h>

#include <analytics/db/config.h>

#include "analytics_archive_directory.h"
#include "attributes_dao.h"
#include "device_dao.h"
#include "object_type_dao.h"
#include "object_track_group_dao.h"
#include "serializers.h"

namespace nx::analytics::db {

ObjectTrackDataSaver::ObjectTrackDataSaver(
    AttributesDao* attributesDao,
    DeviceDao* deviceDao,
    ObjectTypeDao* objectTypeDao,
    ObjectTrackGroupDao* trackGroupDao,
    ObjectTrackCache* trackCache,
    AnalyticsArchiveDirectory* analyticsArchive)
    :
    m_attributesDao(attributesDao),
    m_deviceDao(deviceDao),
    m_objectTypeDao(objectTypeDao),
    m_trackGroupDao(trackGroupDao),
    m_objectTrackCache(trackCache),
    m_analyticsArchive(analyticsArchive)
{
}

void ObjectTrackDataSaver::load(
    ObjectTrackAggregator* trackAggregator,
    bool flush)
{
    m_tracksToInsert = m_objectTrackCache->getTracksToInsert(flush);
    m_tracksToUpdate = m_objectTrackCache->getTracksToUpdate(flush);
    m_trackSearchData = trackAggregator->getAggregatedData(flush);

    resolveTrackIds();
}

bool ObjectTrackDataSaver::empty() const
{
    return m_tracksToInsert.empty()
        && m_tracksToUpdate.empty()
        && m_trackSearchData.empty();
}

void ObjectTrackDataSaver::save(nx::sql::QueryContext* queryContext)
{
    insertObjects(queryContext);

    resolveUnknownTrackIdsThroughDb(queryContext);

    updateObjects(queryContext);

    if (m_analyticsArchive)
        saveToAnalyticsArchive(queryContext);
}

void ObjectTrackDataSaver::resolveTrackIds()
{
    for (auto& trackSearchGridCell: m_trackSearchData)
    {
        for (auto it = trackSearchGridCell.trackIds.begin();
            it != trackSearchGridCell.trackIds.end();
            )
        {
            const auto& trackId = *it;

            auto newObjectIter = std::find_if(
                m_tracksToInsert.begin(), m_tracksToInsert.end(),
                [trackId](const ObjectTrack& track) { return track.id == trackId; });

            if (newObjectIter == m_tracksToInsert.end())
            {
                // Object MUST be known.
                const auto dbId = m_objectTrackCache->dbIdFromTrackId(trackId);
                if (dbId == kInvalidDbId)
                {
                    if (auto object = m_objectTrackCache->getTrackToInsertForced(trackId);
                        object)
                    {
                        m_tracksToInsert.push_back(std::move(*object));
                    }
                    else
                    {
                        // The track insertion task may already be in the query queue.
                        // So, the dbId will be available when we need to insert.
                    }
                }
            }

            ++it;
        }
    }
}

void ObjectTrackDataSaver::insertObjects(nx::sql::QueryContext* queryContext)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(R"sql(
        INSERT INTO track (device_id, object_type_id, guid,
            track_start_ms, track_end_ms, track_detail, attributes_id,
            best_shot_timestamp_ms, best_shot_rect, stream_index)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )sql");

    for (const auto& track: m_tracksToInsert)
    {
        const auto deviceDbId = m_deviceDao->insertOrFetch(queryContext, track.deviceId);
        const auto objectTypeDbId = m_objectTypeDao->insertOrFetch(queryContext, track.objectTypeId);
        const auto attributesId = m_attributesDao->insertOrFetchAttributes(
            queryContext, track.attributes);

        auto trackMinTimestamp = track.firstAppearanceTimeUs;
        auto trackMaxTimestamp = track.lastAppearanceTimeUs;

        query->addBindValue(deviceDbId);
        query->addBindValue((long long) objectTypeDbId);
        query->addBindValue(QnSql::serialized_field(track.id));
        query->addBindValue(trackMinTimestamp / kUsecInMs);
        query->addBindValue(trackMaxTimestamp / kUsecInMs);
        query->addBindValue(TrackSerializer::serialized(track.objectPosition));

        query->addBindValue((long long) attributesId);
        query->addBindValue(track.bestShot.initialized()
            ? track.bestShot.timestampUs / kUsecInMs
            : 0);
        query->addBindValue(track.bestShot.initialized()
            ? TrackSerializer::serialized(track.bestShot.rect)
            : QByteArray());

        query->addBindValue((int) track.bestShot.streamIndex);

        query->exec();

        const auto trackDbId = query->lastInsertId().toLongLong();
        m_trackDbAttributes[track.id] = {trackDbId, track.deviceId, objectTypeDbId};
        m_objectTrackCache->setTrackIdInDb(track.id, trackDbId);

        m_objectTrackCache->saveTrackIdToAttributesId(
            track.id, attributesId);
    }
}

void ObjectTrackDataSaver::resolveUnknownTrackIdsThroughDb(
    nx::sql::QueryContext* queryContext)
{
    for (const auto& trackUpdate: m_tracksToUpdate)
    {
        if (trackUpdate.dbId == kInvalidDbId)
            makeSureTrackIdIsResolved(queryContext, trackUpdate.trackId);
    }

    for (const auto& aggregatedTrackData: m_trackSearchData)
    {
        for (const auto& trackId: aggregatedTrackData.trackIds)
            makeSureTrackIdIsResolved(queryContext, trackId);
    }
}

void ObjectTrackDataSaver::makeSureTrackIdIsResolved(
    nx::sql::QueryContext* queryContext,
    const QnUuid& trackId)
{
    if (m_trackDbAttributes.count(trackId) > 0)
        return;

    const auto cachedDbId = m_objectTrackCache->dbIdFromTrackId(trackId);
    const auto track = m_objectTrackCache->getTrackById(trackId);

    if (cachedDbId != kInvalidDbId && track)
    {
        m_trackDbAttributes[trackId] = {
            cachedDbId,
            track->deviceId,
            m_objectTypeDao->objectTypeIdFromName(track->objectTypeId)};
        return;
    }

    m_trackDbAttributes[trackId] = fetchTrackDbAttributes(queryContext, trackId);
}

ObjectTrackDataSaver::ObjectTrackDbAttributes ObjectTrackDataSaver::fetchTrackDbAttributes(
    nx::sql::QueryContext* queryContext,
    const QnUuid& trackId)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(R"sql(
        SELECT t.id, d.guid, t.object_type_id
        FROM track t, device d
        WHERE t.guid=? AND t.device_id = d.id
    )sql");
    query->addBindValue(QnSql::serialized_field(trackId));

    query->exec();
    if (!query->next())
        return {};

    return {
        query->value(0).toLongLong(),
        QnSql::deserialized_field<QnUuid>(query->value(1)),
        query->value(2).toInt()};
}

void ObjectTrackDataSaver::updateObjects(nx::sql::QueryContext* queryContext)
{
    auto loadExistingRegion =
        [queryContext](const int64_t& id)
        {
            auto query = queryContext->connection()->createQuery();
            query->prepare(R"sql(SELECT track_detail FROM track WHERE id = ?)sql");
            query->addBindValue((long long) id);
            query->exec();
            if (query->next())
                return ObjectRegion{query->value(0).toByteArray()};
            return ObjectRegion();
        };

    auto updateObjectQuery = queryContext->connection()->createQuery();
    updateObjectQuery->prepare(R"sql(
        UPDATE track
        SET track_detail = ?,
            attributes_id = ?,
            track_start_ms = min(track_start_ms, ?),
            track_end_ms = max(track_end_ms, ?),
            best_shot_timestamp_ms = coalesce(?, best_shot_timestamp_ms),
            best_shot_rect = coalesce(?, best_shot_rect),
            stream_index = ?
        WHERE id = ?
    )sql");

    for (const auto& trackUpdate: m_tracksToUpdate)
    {
        const auto newAttributesId = m_attributesDao->insertOrFetchAttributes(
            queryContext, trackUpdate.allAttributes);

        auto trackMinTimestamp = trackUpdate.firstAppearanceTimeUs;
        auto trackMaxTimestamp = trackUpdate.lastAppearanceTimeUs;

        auto dbId = trackUpdate.dbId != -1
            ? (long long)trackUpdate.dbId
            : (long long)m_trackDbAttributes.at(trackUpdate.trackId).dbId;

        ObjectRegion region = loadExistingRegion(dbId);
        region.add(trackUpdate.appendedTrack);

        updateObjectQuery->addBindValue(region.boundingBoxGrid);
        updateObjectQuery->addBindValue((long long) newAttributesId);
        updateObjectQuery->addBindValue(trackMinTimestamp / kUsecInMs);
        updateObjectQuery->addBindValue(trackMaxTimestamp / kUsecInMs);
        if (trackUpdate.bestShot.initialized())
        {
            updateObjectQuery->addBindValue(trackUpdate.bestShot.timestampUs / kUsecInMs);
            updateObjectQuery->addBindValue(TrackSerializer::serialized(trackUpdate.bestShot.rect));
        }
        else
        {
            updateObjectQuery->addBindValue(QVariant());
            updateObjectQuery->addBindValue(QVariant());
        }

        NX_ASSERT(trackUpdate.bestShot.streamIndex == nx::vms::api::StreamIndex::primary
            || trackUpdate.bestShot.streamIndex == nx::vms::api::StreamIndex::secondary);

        updateObjectQuery->addBindValue((int) trackUpdate.bestShot.streamIndex);
        updateObjectQuery->addBindValue(dbId);

        updateObjectQuery->exec();
        m_objectTrackCache->saveTrackIdToAttributesId(trackUpdate.trackId, newAttributesId);
    }
}

void ObjectTrackDataSaver::saveToAnalyticsArchive(nx::sql::QueryContext* queryContext)
{
    if (m_trackSearchData.empty())
        return;

    const auto data = prepareArchiveData(queryContext);

    for (const auto& item: data)
    {
        const auto result = m_analyticsArchive->saveToArchive(
            item.deviceId,
            item.timestamp,
            std::vector<QRect>(item.region.begin(), item.region.end()),
            item.trackGroupId,
            item.objectType,
            item.combinedAttributesId);
        if (!result)
            NX_INFO(this, "Failed to save analytics data. device %1", item.deviceId);
    }
}

std::vector<ObjectTrackDataSaver::AnalyticsArchiveItem>
    ObjectTrackDataSaver::prepareArchiveData(nx::sql::QueryContext* queryContext)
{
    struct Item
    {
        QRegion region;
        std::set<int64_t> attributesIds;
        std::set<int64_t> trackIds;
    };

    std::map<std::pair<QnUuid /*deviceGuid*/, int /*objectType*/>, Item> regionByObjectType;

    std::chrono::milliseconds minTimestamp = m_trackSearchData.front().timestamp;
    for (const auto& aggregatedTrackData: m_trackSearchData)
    {
        // NOTE: All timestamp belong to the same period of size of kTrackAggregationPeriod.
        if (minTimestamp > aggregatedTrackData.timestamp)
            minTimestamp = aggregatedTrackData.timestamp;

        for (const auto& trackId: aggregatedTrackData.trackIds)
        {
            auto trackIter = m_trackDbAttributes.find(trackId);
            NX_ASSERT(trackIter != m_trackDbAttributes.end());
            const auto trackDbAttributes = trackIter != m_trackDbAttributes.end()
                ? trackIter->second
                : ObjectTrackDbAttributes();

            Item& item = regionByObjectType[
                {trackDbAttributes.deviceId, trackDbAttributes.objectTypeDbId}];
            item.region += aggregatedTrackData.boundingBox;
            item.attributesIds.insert(m_objectTrackCache->getAttributesIdByTrackId(trackId));
            item.trackIds.insert(trackDbAttributes.dbId);
        }
    }

    std::vector<ObjectTrackDataSaver::AnalyticsArchiveItem> result;
    for (const auto& [deviceIdAndObjectType, item]: regionByObjectType)
    {
        AnalyticsArchiveItem archiveItem;
        archiveItem.deviceId = deviceIdAndObjectType.first;
        archiveItem.trackGroupId =
            m_trackGroupDao->insertOrFetchGroup(queryContext, item.trackIds);
        archiveItem.combinedAttributesId =
            m_attributesDao->combineAttributes(queryContext, item.attributesIds);
        archiveItem.objectType = deviceIdAndObjectType.second;
        archiveItem.region = item.region;
        archiveItem.timestamp = minTimestamp;
        result.push_back(std::move(archiveItem));
    }

    return result;
}

} // namespace nx::analytics::db
