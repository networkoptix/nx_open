#include "object_cache.h"

namespace nx::analytics::storage {

ObjectCache::ObjectCache(
    std::chrono::milliseconds aggregationPeriod,
    std::chrono::milliseconds maxObjectLifetime)
    :
    m_aggregationPeriod(aggregationPeriod),
    m_maxObjectLifetime(maxObjectLifetime)
{
}

void ObjectCache::add(const common::metadata::ConstDetectionMetadataPacketPtr& /*packet*/)
{
    // TODO
}

std::vector<DetectedObject> ObjectCache::getObjectsToInsert()
{
    // TODO
    return {};
}

std::vector<ObjectUpdate> ObjectCache::getObjectsToUpdate()
{
    // TODO
    return {};
}

void ObjectCache::setObjectIdInDb(const QnUuid& /*objectId*/, long long /*dbId*/)
{
    // TODO
}

long long ObjectCache::dbIdFromObjectId(const QnUuid& /*objectId*/) const
{
    // TODO
    return -1;
}

void ObjectCache::removeExpiredData()
{
    // TODO
}

} // namespace nx::analytics::storage
