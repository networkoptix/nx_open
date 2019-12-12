#pragma once

#include <chrono>
#include <map>
#include <vector>

#include <nx/utils/elapsed_timer_pool.h>
#include <nx/utils/thread/mutex.h>

#include <analytics/common/object_metadata.h>
#include <analytics/db/analytics_db_types.h>

namespace nx::analytics::db {

static constexpr int64_t kInvalidDbId = -1;

struct ObjectTrackUpdate
{
    int64_t dbId = kInvalidDbId;
    QnUuid trackId;
    std::vector<ObjectPosition> appendedTrack;
    /** Attributes that were added since the last insert/update. */
    std::vector<common::metadata::Attribute> appendedAttributes;
    /** All object's attributes. */
    std::vector<common::metadata::Attribute> allAttributes;
};

/**
 * NOTE: All methods of this class are thread-safe.
 */
class ObjectTrackCache
{
public:
    /**
     * @param aggregationPeriod Object is reported with this delay to minimize the number of
     * inserts to the DB.
     *
     * Object is removed from the cache if:
     * - every change made to the object was reported using getTracksToInsert and getTracksToUpdate
     * - AND there was no new object data during the aggregationPeriod
     * OR
     * - there was no new object data during the maxObjectLifetime period regardless of previous
     * conditions.
     */
    ObjectTrackCache(
        std::chrono::milliseconds aggregationPeriod,
        std::chrono::milliseconds maxObjectLifetime);

    void add(const common::metadata::ConstObjectMetadataPacketPtr& packet);

    /**
     * Objects are provided only after the aggregation period passes.
     * @param flush If true, all available data is provided.
     * @return Objects that are to be inserted.
     */
    std::vector<ObjectTrack> getTracksToInsert(bool flush = false);

    /**
     * Provides new object regardless of the aggregation period.
     * NOTE: Should be avoided when possible.
     */
    std::optional<ObjectTrack> getTrackToInsertForced(const QnUuid& trackId);

    /**
     * Provides data only for objects with known dbId.
     * Db id is set using ObjectTrackCache::setTrackIdInDb.
     */
    std::vector<ObjectTrackUpdate> getTracksToUpdate(bool flush = false);

    std::optional<ObjectTrack> getTrackById(const QnUuid& trackId) const;

    /**
     * MUST be invoked after inserting track to the DB.
     */
    void setTrackIdInDb(const QnUuid& trackId, int64_t dbId);

    /**
     * @return kInvalidDbId if track id is unknown.
     */
    int64_t dbIdFromTrackId(const QnUuid& trackId) const;

    void saveTrackIdToAttributesId(const QnUuid& trackId, int64_t attributesId);

    int64_t getAttributesIdByTrackId(const QnUuid& trackId) const;

    /**
     * NOTE: Data removal happens only in this method.
     * So, it MUST be called periodically.
     */
    void removeExpiredData();

private:
    struct ObjectTrackContext
    {
        int64_t dbId = -1;
        int64_t attributesDbId = -1;

        ObjectTrack track;
        std::vector<common::metadata::Attribute> newAttributesSinceLastUpdate;

        std::chrono::steady_clock::time_point lastReportTime;
        bool insertionReported = false;
        bool modified = false;

        std::set<std::pair<QString, QString>> allAttributes;
    };

    const std::chrono::milliseconds m_aggregationPeriod;
    const std::chrono::milliseconds m_maxObjectLifetime;
    mutable QnMutex m_mutex;
    std::map<QnUuid, ObjectTrackContext> m_tracksById;
    nx::utils::ElapsedTimerPool<QnUuid> m_timerPool;

    void updateObject(
        const nx::common::metadata::ObjectMetadata& objectMetadata,
        const nx::common::metadata::ObjectMetadataPacket& packet);

    void addNewAttributes(
        const std::vector<common::metadata::Attribute>& attributes,
        ObjectTrackContext* trackContext);

    void removeTrack(const QnUuid& trackId);
};

} // namespace nx::analytics::db
