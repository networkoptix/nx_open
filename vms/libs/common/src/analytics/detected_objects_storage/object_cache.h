#pragma once

#include <chrono>
#include <vector>

#include <nx/utils/thread/mutex.h>

#include <analytics/common/object_detection_metadata.h>

#include "analytics_events_storage_types.h"

namespace nx::analytics::storage {

static constexpr long long kInvalidDbId = -1;

struct ObjectUpdate
{
    long long dbId = kInvalidDbId;
    std::vector<ObjectPosition> trackToAppend;
    std::vector<common::metadata::Attribute> allAttributes;
};

/**
 * NOTE: All methods of this class are thread-safe.
 */
class ObjectCache
{
public:
    /**
     * Object is removed from the cache if:
     * - every change made to the object was reported using getObjectsToInsert and getObjectsToUpdate
     * - AND there was no new object data during the aggregationPeriod
     * OR
     * - there was no new object data during the maxObjectLifetime period regardless of previous conditions.
     */
    ObjectCache(
        std::chrono::milliseconds aggregationPeriod,
        std::chrono::milliseconds maxObjectLifetime);

    void add(const common::metadata::ConstDetectionMetadataPacketPtr& packet);

    /**
     * @return Objects that are to be inserted
     */
    std::vector<DetectedObject> getObjectsToInsert();

    /**
     * Provides data only for objects with known dbId.
     * Db id is set using ObjectCache::setObjectIdInDb.
     */
    std::vector<ObjectUpdate> getObjectsToUpdate();

    /**
     * MUST be invoked after inserting object to the DB.
     */
    void setObjectIdInDb(const QnUuid& objectId, long long dbId);

    /**
     * @return kInvalidDbId if object id is unknown.
     */
    long long dbIdFromObjectId(const QnUuid& objectId) const;

    /**
     * NOTE: Data removal happens inly in this method.
     * So, it MUST be called periodically.
     */
    void removeExpiredData();

private:
    const std::chrono::milliseconds m_aggregationPeriod;
    const std::chrono::milliseconds m_maxObjectLifetime;
};

} // namespace nx::analytics::storage
