#pragma once

#include <chrono>
#include <map>
#include <vector>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>

#include <nx/sql/filter.h>
#include <nx/sql/query.h>
#include <nx/utils/basic_factory.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/mutex.h>

#include <analytics/common/object_detection_metadata.h>
#include <recording/time_period_list.h>

#include "analytics_events_storage_cursor.h"
#include "analytics_events_storage_db_controller.h"
#include "analytics_events_storage_settings.h"
#include "analytics_events_storage_types.h"

#include "attributes_dao.h"
#include "device_dao.h"
#include "object_cache.h"
#include "object_track_aggregator.h"
#include "object_type_dao.h"

namespace nx::analytics::storage {

using StoreCompletionHandler =
    nx::utils::MoveOnlyFunc<void(ResultCode /*resultCode*/)>;

using LookupResult = std::vector<DetectedObject>;

using LookupCompletionHandler =
    nx::utils::MoveOnlyFunc<void(ResultCode /*resultCode*/, LookupResult)>;

using TimePeriodsLookupCompletionHandler =
    nx::utils::MoveOnlyFunc<void(ResultCode /*resultCode*/, QnTimePeriodList)>;

using CreateCursorCompletionHandler =
    nx::utils::MoveOnlyFunc<void(
        ResultCode /*resultCode*/,
        std::unique_ptr<AbstractCursor> /*cursor*/)>;

/**
 * NOTE: Every method of this class is asynchronous if other not specified.
 * Generally, any error does not mean storage cannot continue.
 * New operations may be completed successfully.
 */
class AbstractEventsStorage
{
public:
    virtual ~AbstractEventsStorage() = default;

    /**
     * Initializes internal database. MUST be called just after object instantiation.
     */
    virtual bool initialize() = 0;

    virtual void save(
        common::metadata::ConstDetectionMetadataPacketPtr packet,
        StoreCompletionHandler completionHandler) = 0;

    /**
     * Newly-created cursor points just before the first element.
     * So, AbstractCursor::next has to be called to get the first element.
     */
    virtual void createLookupCursor(
        Filter filter,
        CreateCursorCompletionHandler completionHandler) = 0;

    /**
     * Selects all objects with non-empty track that satisfy to the filter.
     * Output is sorted by timestamp with order defined by filter.sortOrder.
     */
    virtual void lookup(
        Filter filter,
        LookupCompletionHandler completionHandler) = 0;

    /**
     * Select periods of time which contain any data suitable for filter. If some periods in the
     * result period list have zero duration that means their durations are unspecified.
     */
    virtual void lookupTimePeriods(
        Filter filter,
        TimePeriodsLookupOptions options,
        TimePeriodsLookupCompletionHandler completionHandler) = 0;

    /**
     * In some undefined time after this call deprecated data will be removed from persistent storage.
     * NOTE: Call is non-blocking.
     * @param deviceId Can be null.
     */
    virtual void markDataAsDeprecated(
        QnUuid deviceId,
        std::chrono::milliseconds oldestDataToKeepTimestamp) = 0;
};

//-------------------------------------------------------------------------------------------------

namespace detail {

class TimePeriod
{
public:
    long long id = -1;
    long long deviceId = -1;
    std::chrono::milliseconds startTime = std::chrono::milliseconds::zero();
    std::chrono::milliseconds endTime = std::chrono::milliseconds::zero();
    std::chrono::milliseconds lastSavedEndTime = std::chrono::milliseconds::zero();

    std::chrono::milliseconds length() const;

    bool addPacketToPeriod(
        std::chrono::milliseconds timestamp,
        std::chrono::milliseconds duration);
};

} // namespace detai;

class EventsStorage:
    public AbstractEventsStorage
{
public:
    EventsStorage(const Settings& settings);

    virtual bool initialize() override;

    virtual void save(
        common::metadata::ConstDetectionMetadataPacketPtr packet,
        StoreCompletionHandler completionHandler) override;

    virtual void createLookupCursor(
        Filter filter,
        CreateCursorCompletionHandler completionHandler) override;

    virtual void lookup(
        Filter filter,
        LookupCompletionHandler completionHandler) override;

    virtual void lookupTimePeriods(
        Filter filter,
        TimePeriodsLookupOptions options,
        TimePeriodsLookupCompletionHandler completionHandler) override;

    virtual void markDataAsDeprecated(
        QnUuid deviceId,
        std::chrono::milliseconds oldestDataToKeepTimestamp) override;

private:
    using DeviceIdToCurrentTimePeriod = std::map<long long, detail::TimePeriod>;
    using IdToTimePeriod = std::map<long long, DeviceIdToCurrentTimePeriod::iterator>;

    const Settings& m_settings;
    DbController m_dbController;
    std::chrono::milliseconds m_maxRecordedTimestamp = std::chrono::milliseconds::zero();
    mutable QnMutex m_mutex;
    DeviceIdToCurrentTimePeriod m_deviceIdToCurrentTimePeriod;
    IdToTimePeriod m_idToTimePeriod;
    AttributesDao m_attributesDao;
    ObjectTypeDao m_objectTypeDao;
    DeviceDao m_deviceDao;

    ObjectCache m_objectCache;
    ObjectTrackAggregator m_trackAggregator;

    bool readMaximumEventTimestamp();

    bool loadDictionaries();

    nx::sql::DBResult savePacket(
        nx::sql::QueryContext*,
        common::metadata::ConstDetectionMetadataPacketPtr packet);

    /**
     * @return Inserted event id.
     * Throws on error.
     */
    void insertEvent(
        nx::sql::QueryContext* queryContext,
        const common::metadata::DetectionMetadataPacket& packet,
        const common::metadata::DetectedObject& detectedObject,
        long long attributesId,
        long long timePeriodId);

    void updateDictionariesIfNeeded(
        nx::sql::QueryContext* queryContext,
        const common::metadata::DetectionMetadataPacket& packet,
        const common::metadata::DetectedObject& detectedObject);

    /**
     * @return Id of the current time period.
     */
    long long insertOrUpdateTimePeriod(
        nx::sql::QueryContext* queryContext,
        const common::metadata::DetectionMetadataPacket& packet);

    detail::TimePeriod* insertOrFetchCurrentTimePeriod(
        nx::sql::QueryContext* queryContext,
        long long deviceId,
        std::chrono::milliseconds packetTimestamp,
        std::chrono::milliseconds packetDuration);

    void closeCurrentTimePeriod(
        nx::sql::QueryContext* queryContext,
        long long deviceId);

    void saveTimePeriodEnd(
        nx::sql::QueryContext* queryContext,
        long long id,
        std::chrono::milliseconds endTime);

    void fillCurrentTimePeriodsCache(nx::sql::QueryContext* queryContext);

    void prepareCursorQuery(const Filter& filter, nx::sql::SqlQuery* query);

    nx::sql::DBResult selectObjects(
        nx::sql::QueryContext* queryContext,
        const Filter& filter,
        std::vector<DetectedObject>* result);

    void prepareLookupQuery(const Filter& filter, nx::sql::SqlQuery* query);

    nx::sql::Filter prepareSqlFilterExpression(
        const Filter& filter,
        QString* eventsFilteredByFreeTextSubQuery);

    void addObjectTypeIdToFilter(
        const std::vector<QString>& objectTypeIds,
        nx::sql::Filter* sqlFilter);

    void addTimePeriodToFilter(
        const QnTimePeriod& timePeriod,
        nx::sql::Filter* sqlFilter,
        const char* leftBoundaryFieldName,
        const char* rightBoundaryFieldName);

    void addBoundingBoxToFilter(
        const QRectF& boundingBox,
        nx::sql::Filter* sqlFilter);

    void loadObjects(
        nx::sql::SqlQuery& selectEventsQuery,
        const Filter& filter,
        std::vector<DetectedObject>* result);

    void loadObject(
        nx::sql::SqlQuery* selectEventsQuery,
        DetectedObject* object);

    void mergeObjects(DetectedObject from, DetectedObject* to, bool loadTrack);

    void queryTrackInfo(
        nx::sql::QueryContext* queryContext,
        std::vector<DetectedObject>* result);

    nx::sql::DBResult selectTimePeriods(
        nx::sql::QueryContext* queryContext,
        const Filter& filter,
        const TimePeriodsLookupOptions& options,
        QnTimePeriodList* result);

    void prepareSelectTimePeriodsUnfilteredQuery(
        nx::sql::AbstractSqlQuery* query,
        const std::vector<QnUuid>& deviceIds,
        const QnTimePeriod& timePeriod,
        const TimePeriodsLookupOptions& options);

    void prepareSelectTimePeriodsFilteredQuery(
        nx::sql::AbstractSqlQuery* query,
        const Filter& filter,
        const TimePeriodsLookupOptions& options);

    void loadTimePeriods(
        nx::sql::AbstractSqlQuery* query,
        const QnTimePeriod& timePeriod,
        const TimePeriodsLookupOptions& options,
        QnTimePeriodList* result);

    nx::sql::DBResult cleanupData(
        nx::sql::QueryContext* queryContext,
        const QnUuid& deviceId,
        std::chrono::milliseconds oldestDataToKeepTimestamp);

    void cleanupEvents(
        nx::sql::QueryContext* queryContext,
        const QnUuid& deviceId,
        std::chrono::milliseconds oldestDataToKeepTimestamp);

    void cleanupEventProperties(nx::sql::QueryContext* queryContext);

    static int packCoordinate(double);
    static double unpackCoordinate(int);
};

//-------------------------------------------------------------------------------------------------

using EventsStorageFactoryFunction =
    std::unique_ptr<AbstractEventsStorage>(const Settings&);

class EventsStorageFactory:
    public nx::utils::BasicFactory<EventsStorageFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<EventsStorageFactoryFunction>;

public:
    EventsStorageFactory();

    static EventsStorageFactory& instance();

private:
    std::unique_ptr<AbstractEventsStorage> defaultFactoryFunction(const Settings&);
};

} // namespace nx::analytics::storage
