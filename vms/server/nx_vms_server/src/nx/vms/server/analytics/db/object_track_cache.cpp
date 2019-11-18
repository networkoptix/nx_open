#include "object_track_cache.h"

namespace nx::analytics::db {

ObjectTrackCache::ObjectTrackCache(
    std::chrono::milliseconds aggregationPeriod,
    std::chrono::milliseconds maxObjectLifetime)
    :
    m_aggregationPeriod(aggregationPeriod),
    m_maxObjectLifetime(maxObjectLifetime),
    m_timerPool([this](const QnUuid& trackId) { removeTrack(trackId); })
{
}

void ObjectTrackCache::add(const common::metadata::ConstObjectMetadataPacketPtr& packet)
{
    QnMutexLocker lock(&m_mutex);

    for (const auto& objectMetadata: packet->objectMetadataList)
        updateObject(objectMetadata, *packet);
}

std::vector<ObjectTrack> ObjectTrackCache::getTracksToInsert(bool flush)
{
    QnMutexLocker lock(&m_mutex);

    const auto currentClock = nx::utils::monotonicTime();

    std::vector<ObjectTrack> result;
    for (auto& [trackId, ctx]: m_tracksById)
    {
        if ((!flush && currentClock - ctx.lastReportTime < m_aggregationPeriod) ||
            ctx.insertionReported)
        {
            continue;
        }
        ctx.insertionReported = true;
        ctx.modified = false;

        ctx.lastReportTime = currentClock;
        m_timerPool.addTimer(
            ctx.track.id,
            m_maxObjectLifetime);

        result.push_back(ctx.track);
        ctx.track.objectPosition.clear();
        ctx.newAttributesSinceLastUpdate.clear();
    }

    return result;
}

std::optional<ObjectTrack> ObjectTrackCache::getTrackToInsertForced(const QnUuid& trackId)
{
    QnMutexLocker lock(&m_mutex);

    auto it = m_tracksById.find(trackId);
    if (it == m_tracksById.end())
        return std::nullopt;

    auto& ctx = it->second;

    if (ctx.insertionReported)
        return std::nullopt;
    ctx.insertionReported = true;
    ctx.modified = false;

    ctx.lastReportTime = nx::utils::monotonicTime();
    m_timerPool.addTimer(
        ctx.track.id,
        m_maxObjectLifetime);

    auto result = ctx.track;
    ctx.track.objectPosition.clear();
    ctx.newAttributesSinceLastUpdate.clear();

    return result;
}

std::vector<ObjectTrackUpdate> ObjectTrackCache::getTracksToUpdate(bool flush)
{
    QnMutexLocker lock(&m_mutex);

    const auto currentClock = nx::utils::monotonicTime();

    std::vector<ObjectTrackUpdate> result;
    for (auto& [trackId, ctx]: m_tracksById)
    {
        if ((!flush && currentClock - ctx.lastReportTime < m_aggregationPeriod) ||
            !ctx.insertionReported ||
            ctx.track.objectPosition.isEmpty())
        {
            continue;
        }

        ctx.lastReportTime = currentClock;
        ctx.modified = false;
        m_timerPool.addTimer(
            ctx.track.id,
            m_maxObjectLifetime);

        ObjectTrackUpdate trackUpdate;
        trackUpdate.trackId = trackId;
        trackUpdate.appendedTrack = std::exchange(ctx.track.objectPosition, {});
        trackUpdate.appendedAttributes = std::exchange(ctx.newAttributesSinceLastUpdate, {});
        trackUpdate.allAttributes = ctx.track.attributes;

        result.push_back(std::move(trackUpdate));
    }

    return result;
}

std::optional<ObjectTrack> ObjectTrackCache::getTrackById(const QnUuid& trackId) const
{
    QnMutexLocker lock(&m_mutex);

    if (auto it = m_tracksById.find(trackId); it != m_tracksById.end())
        return it->second.track;

    return std::nullopt;
}

void ObjectTrackCache::setTrackIdInDb(const QnUuid& trackId, int64_t dbId)
{
    QnMutexLocker lock(&m_mutex);

    if (auto it = m_tracksById.find(trackId);
        it != m_tracksById.end())
    {
        it->second.dbId = dbId;
    }
}

int64_t ObjectTrackCache::dbIdFromTrackId(const QnUuid& trackId) const
{
    QnMutexLocker lock(&m_mutex);

    if (auto it = m_tracksById.find(trackId);
        it != m_tracksById.end())
    {
        return it->second.dbId;
    }

    return -1;
}

void ObjectTrackCache::saveTrackIdToAttributesId(
    const QnUuid& trackId,
    int64_t attributesId)
{
    QnMutexLocker lock(&m_mutex);

    if (auto it = m_tracksById.find(trackId);
        it != m_tracksById.end())
    {
        it->second.attributesDbId = attributesId;
    }
}

int64_t ObjectTrackCache::getAttributesIdByTrackId(const QnUuid& trackId) const
{
    QnMutexLocker lock(&m_mutex);

    if (auto it = m_tracksById.find(trackId);
        it != m_tracksById.end())
    {
        return it->second.attributesDbId;
    }

    return -1;
}

void ObjectTrackCache::removeExpiredData()
{
    QnMutexLocker lock(&m_mutex);

    m_timerPool.processTimers();
}

void ObjectTrackCache::updateObject(
    const nx::common::metadata::ObjectMetadata& objectMetadata,
    const nx::common::metadata::ObjectMetadataPacket& packet)
{
    auto [trackIter, inserted] = m_tracksById.emplace(
        objectMetadata.trackId,
        ObjectTrackContext());
    auto& trackContext = trackIter->second;

    if (inserted)
    {
        trackContext.track.deviceId = packet.deviceId;
        trackContext.track.id = objectMetadata.trackId;
        trackContext.track.firstAppearanceTimeUs = packet.timestampUs;

        trackContext.lastReportTime = nx::utils::monotonicTime();
    }
    else
    {
        trackContext.modified = true;
        m_timerPool.removeTimer(trackContext.track.id);
    }

    if (trackContext.track.objectTypeId.isEmpty())
        trackContext.track.objectTypeId = objectMetadata.typeId;

    if (objectMetadata.bestShot)
    {
        // "Best shot" packet contains only information about the best shot, not a real object movement.
        trackContext.track.bestShot.timestampUs = packet.timestampUs;
        trackContext.track.bestShot.rect = objectMetadata.boundingBox;
        return;
    }

    trackContext.track.lastAppearanceTimeUs = packet.timestampUs;

    addNewAttributes(objectMetadata.attributes, &trackContext);

    trackContext.track.objectPosition.add(objectMetadata.boundingBox);
}

void ObjectTrackCache::addNewAttributes(
    const std::vector<common::metadata::Attribute>& attributes,
    ObjectTrackContext* trackContext)
{
    for (const auto& attribute: attributes)
    {
        auto it = trackContext->allAttributes.find(attribute.name);
        if (it == trackContext->allAttributes.end())
        {
            trackContext->track.attributes.push_back(attribute);
            trackContext->newAttributesSinceLastUpdate.push_back(attribute);
            trackContext->allAttributes[attribute.name] = attribute.value;
            continue;
        }

        if (it->second == attribute.value)
            continue;

        it->second = attribute.value;
        for (auto& existingAttribute: trackContext->track.attributes)
        {
            if (existingAttribute.name == attribute.name)
                existingAttribute.value = attribute.value;
        }

        trackContext->newAttributesSinceLastUpdate.push_back(attribute);
    }
}

void ObjectTrackCache::removeTrack(const QnUuid& trackId)
{
    auto it = m_tracksById.find(trackId);
    if (it == m_tracksById.end())
        return;

    if (!it->second.insertionReported || it->second.modified)
        return; //< Never dropping unsaved data.

    m_tracksById.erase(it);
}

} // namespace nx::analytics::db
