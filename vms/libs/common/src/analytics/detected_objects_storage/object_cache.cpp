#include "object_cache.h"

namespace nx::analytics::storage {

ObjectCache::ObjectCache(
    std::chrono::milliseconds aggregationPeriod,
    std::chrono::milliseconds maxObjectLifetime)
    :
    m_aggregationPeriod(aggregationPeriod),
    m_maxObjectLifetime(maxObjectLifetime),
    m_timerPool([this](const QnUuid& objectGuid) { removeObject(objectGuid); })
{
}

void ObjectCache::add(const common::metadata::ConstDetectionMetadataPacketPtr& packet)
{
    QnMutexLocker lock(&m_mutex);

    for (const auto& detectedObject: packet->objects)
        updateObject(detectedObject, *packet);
}

std::vector<DetectedObject> ObjectCache::getObjectsToInsert(bool flush)
{
    QnMutexLocker lock(&m_mutex);

    const auto currentClock = nx::utils::monotonicTime();

    std::vector<DetectedObject> result;
    for (auto& [objectId, ctx]: m_objectsById)
    {
        if ((!flush && currentClock - ctx.lastReportTime < m_aggregationPeriod) ||
            ctx.insertionReported)
        {
            continue;
        }
        ctx.insertionReported = true;

        ctx.lastReportTime = currentClock;
        m_timerPool.addTimer(
            ctx.object.objectAppearanceId,
            m_maxObjectLifetime);

        result.push_back(ctx.object);
        ctx.object.track.clear();
        ctx.newAttributesSinceLastUpdate.clear();
    }

    return result;
}

std::optional<DetectedObject> ObjectCache::getObjectToInsertForced(const QnUuid& objectGuid)
{
    QnMutexLocker lock(&m_mutex);

    auto it = m_objectsById.find(objectGuid);
    if (it == m_objectsById.end())
        return std::nullopt;

    auto& ctx = it->second;

    if (ctx.insertionReported)
        return std::nullopt;
    ctx.insertionReported = true;

    ctx.lastReportTime = nx::utils::monotonicTime();
    m_timerPool.addTimer(
        ctx.object.objectAppearanceId,
        m_maxObjectLifetime);

    auto result = ctx.object;
    ctx.object.track.clear();
    ctx.newAttributesSinceLastUpdate.clear();

    return result;
}

std::vector<ObjectUpdate> ObjectCache::getObjectsToUpdate(bool flush)
{
    QnMutexLocker lock(&m_mutex);

    const auto currentClock = nx::utils::monotonicTime();

    std::vector<ObjectUpdate> result;
    for (auto& [objectId, ctx]: m_objectsById)
    {
        if ((!flush && currentClock - ctx.lastReportTime < m_aggregationPeriod) ||
            !ctx.insertionReported ||
            ctx.object.track.empty())
        {
            continue;
        }

        ctx.lastReportTime = currentClock;
        m_timerPool.addTimer(
            ctx.object.objectAppearanceId,
            m_maxObjectLifetime);

        ObjectUpdate objectUpdate;
        objectUpdate.objectId = objectId;
        objectUpdate.appendedTrack = std::exchange(ctx.object.track, {});
        objectUpdate.appendedAttributes = std::exchange(ctx.newAttributesSinceLastUpdate, {});
        objectUpdate.allAttributes = ctx.object.attributes;

        result.push_back(std::move(objectUpdate));
    }

    return result;
}

std::optional<DetectedObject> ObjectCache::getObjectById(const QnUuid& objectGuid) const
{
    QnMutexLocker lock(&m_mutex);

    if (auto it = m_objectsById.find(objectGuid); it != m_objectsById.end())
        return it->second.object;

    return std::nullopt;
}

void ObjectCache::setObjectIdInDb(const QnUuid& objectId, long long dbId)
{
    QnMutexLocker lock(&m_mutex);

    if (auto it = m_objectsById.find(objectId);
        it != m_objectsById.end())
    {
        it->second.dbId = dbId;
    }
}

long long ObjectCache::dbIdFromObjectId(const QnUuid& objectId) const
{
    QnMutexLocker lock(&m_mutex);

    if (auto it = m_objectsById.find(objectId);
        it != m_objectsById.end())
    {
        return it->second.dbId;
    }

    return -1;
}

void ObjectCache::saveObjectGuidToAttributesId(
    const QnUuid& objectId,
    long long attributesId)
{
    QnMutexLocker lock(&m_mutex);

    if (auto it = m_objectsById.find(objectId);
        it != m_objectsById.end())
    {
        it->second.attributesDbId = attributesId;
    }
}

long long ObjectCache::getAttributesIdByObjectGuid(const QnUuid& objectId) const
{
    QnMutexLocker lock(&m_mutex);

    if (auto it = m_objectsById.find(objectId);
        it != m_objectsById.end())
    {
        return it->second.attributesDbId;
    }

    return -1;
}

void ObjectCache::removeExpiredData()
{
    QnMutexLocker lock(&m_mutex);

    m_timerPool.processTimers();
}

void ObjectCache::updateObject(
    const nx::common::metadata::DetectedObject& detectedObject,
    const nx::common::metadata::DetectionMetadataPacket& packet)
{
    auto [objectIter, inserted] = m_objectsById.emplace(
        detectedObject.objectId,
        ObjectContext());
    auto& objectContext = objectIter->second;

    if (inserted)
    {
        objectContext.object.deviceId = packet.deviceId;
        objectContext.object.objectAppearanceId = detectedObject.objectId;
        objectContext.object.objectTypeId = detectedObject.objectTypeId;
        objectContext.object.firstAppearanceTimeUsec = packet.timestampUsec;

        objectContext.lastReportTime = nx::utils::monotonicTime();

        m_timerPool.addTimer(
            objectContext.object.objectAppearanceId,
            m_maxObjectLifetime);
    }

    if (detectedObject.bestShot)
    {
        objectContext.object.bestShot.timestampUsec = packet.timestampUsec;
        objectContext.object.bestShot.rect = detectedObject.boundingBox;
    }

    objectContext.object.lastAppearanceTimeUsec = packet.timestampUsec;

    addNewAttributes(detectedObject.labels, &objectContext);

    ObjectPosition objectPosition;
    objectPosition.deviceId = packet.deviceId;
    objectPosition.timestampUsec = packet.timestampUsec;
    objectPosition.durationUsec = packet.durationUsec;
    objectPosition.boundingBox = detectedObject.boundingBox;
    objectPosition.attributes = detectedObject.labels;

    objectContext.object.track.push_back(std::move(objectPosition));
}

void ObjectCache::addNewAttributes(
    const std::vector<common::metadata::Attribute>& attributes,
    ObjectContext* objectContext)
{
    for (const auto& attribute: attributes)
    {
        auto it = objectContext->allAttributes.find(attribute.name);
        if (it == objectContext->allAttributes.end())
        {
            objectContext->object.attributes.push_back(attribute);
            objectContext->newAttributesSinceLastUpdate.push_back(attribute);
            objectContext->allAttributes[attribute.name] = attribute.value;
            continue;
        }

        if (it->second == attribute.value)
            continue;

        it->second = attribute.value;
        for (auto& existingAttribute: objectContext->object.attributes)
        {
            if (existingAttribute.name == attribute.name)
                existingAttribute.value = attribute.value;
        }

        objectContext->newAttributesSinceLastUpdate.push_back(attribute);
    }
}

void ObjectCache::removeObject(const QnUuid& objectGuid)
{
    m_objectsById.erase(objectGuid);
}

} // namespace nx::analytics::storage
