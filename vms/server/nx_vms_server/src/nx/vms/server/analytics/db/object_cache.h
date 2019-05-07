#pragma once

#include <chrono>
#include <map>
#include <vector>

#include <nx/utils/elapsed_timer_pool.h>
#include <nx/utils/thread/mutex.h>

#include <analytics/common/object_detection_metadata.h>
#include <analytics/db/analytics_events_storage_types.h>

namespace nx::analytics::db {

static constexpr long long kInvalidDbId = -1;

struct ObjectUpdate
{
    long long dbId = kInvalidDbId;
    QnUuid objectId;
    std::vector<ObjectPosition> appendedTrack;
    /** Attributes that were added since the last insert/update. */
    std::vector<common::metadata::Attribute> appendedAttributes;
    /** All object's attributes. */
    std::vector<common::metadata::Attribute> allAttributes;
};

/**
 * NOTE: All methods of this class are thread-safe.
 */
class ObjectCache
{
public:
    /**
     * @param aggregationPeriod Object is reported with this delay to minimize the number of
     * inserts to the DB.
     *
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
     * Objects are provided only after the aggregation period passes.
     * @param flush If true, all available data is provided.
     * @return Objects that are to be inserted.
     */
    std::vector<DetectedObject> getObjectsToInsert(bool flush = false);

    /**
     * Provides new object regardless of the aggregation period.
     * NOTE: Should be avoided when possible.
     */
    std::optional<DetectedObject> getObjectToInsertForced(const QnUuid& objectGuid);

    /**
     * Provides data only for objects with known dbId.
     * Db id is set using ObjectCache::setObjectIdInDb.
     */
    std::vector<ObjectUpdate> getObjectsToUpdate(bool flush = false);

    std::optional<DetectedObject> getObjectById(const QnUuid& objectGuid) const;

    /**
     * MUST be invoked after inserting object to the DB.
     */
    void setObjectIdInDb(const QnUuid& objectId, long long dbId);

    /**
     * @return kInvalidDbId if object id is unknown.
     */
    long long dbIdFromObjectId(const QnUuid& objectId) const;

    void saveObjectGuidToAttributesId(const QnUuid& guid, long long attributesId);

    long long getAttributesIdByObjectGuid(const QnUuid& guid) const;

    /**
     * NOTE: Data removal happens only in this method.
     * So, it MUST be called periodically.
     */
    void removeExpiredData();

private:
    struct ObjectContext
    {
        long long dbId = -1;
        long long attributesDbId = -1;

        DetectedObject object;
        std::vector<common::metadata::Attribute> newAttributesSinceLastUpdate;

        std::chrono::steady_clock::time_point lastReportTime;
        bool insertionReported = false;

        std::map<QString, QString> allAttributes;
    };

    const std::chrono::milliseconds m_aggregationPeriod;
    const std::chrono::milliseconds m_maxObjectLifetime;
    mutable QnMutex m_mutex;
    std::map<QnUuid, ObjectContext> m_objectsById;
    nx::utils::ElapsedTimerPool<QnUuid> m_timerPool;

    void updateObject(
        const nx::common::metadata::DetectedObject& detectedObject,
        const nx::common::metadata::DetectionMetadataPacket& packet);

    void addNewAttributes(
        const std::vector<common::metadata::Attribute>& attributes,
        ObjectContext* objectContext);

    void removeObject(const QnUuid& objectGuid);
};

} // namespace nx::analytics::db
