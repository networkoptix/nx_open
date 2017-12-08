#pragma once

#include <chrono>
#include <vector>

#include <nx/utils/basic_factory.h>
#include <nx/utils/db/filter.h>
#include <nx/utils/db/query.h>
#include <nx/utils/move_only_func.h>

#include <analytics/common/object_detection_metadata.h>
#include <recording/time_period_list.h>

#include "analytics_events_storage_cursor.h"
#include "analytics_events_storage_db_controller.h"
#include "analytics_events_storage_settings.h"
#include "analytics_events_storage_types.h"

namespace nx {
namespace analytics {
namespace storage {

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
     * Select periods of time which contain any data suitable for filter.
     */
    virtual void lookupTimePeriods(
        Filter filter,
        TimePeriodsLookupOptions options,
        TimePeriodsLookupCompletionHandler completionHandler) = 0;

    /**
     * In some undefined time after this call deprecated data will be removed from persistent storage.
     * @param deviceId Can be null.
     */
    virtual void markDataAsDeprecated(
        QnUuid deviceId,
        qint64 oldestNeededDataTimestamp) = 0;
};

//-------------------------------------------------------------------------------------------------

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
        qint64 oldestNeededDataTimestamp) override;

private:
    const Settings& m_settings;
    DbController m_dbController;

    nx::utils::db::DBResult savePacket(
        nx::utils::db::QueryContext*,
        common::metadata::ConstDetectionMetadataPacketPtr packet);
    /**
     * @return Inserted event id.
     * Throws on error.
     */
    std::int64_t insertEvent(
        nx::utils::db::QueryContext* queryContext,
        const common::metadata::DetectionMetadataPacket& packet,
        const common::metadata::DetectedObject& detectedObject);
    /**
     * Throws on error.
     */
    void insertEventAttributes(
        nx::utils::db::QueryContext* queryContext,
        std::int64_t eventId,
        const std::vector<common::metadata::Attribute>& eventAttributes);

    void prepareCursorQuery(const Filter& filter, nx::utils::db::SqlQuery* query);

    nx::utils::db::DBResult selectObjects(
        nx::utils::db::QueryContext* queryContext,
        const Filter& filter,
        std::vector<DetectedObject>* result);

    void prepareLookupQuery(const Filter& filter, nx::utils::db::SqlQuery* query);
    nx::utils::db::InnerJoinFilterFields prepareSqlFilterExpression(const Filter& filter);
    void addObjectTypeIdToFilter(
        const std::vector<QnUuid>& objectTypeIds,
        nx::utils::db::InnerJoinFilterFields* sqlFilter);
    void addTimePeriodToFilter(
        const QnTimePeriod& timePeriod,
        nx::utils::db::InnerJoinFilterFields* sqlFilter);
    void addBoundingBoxToFilter(
        const QRectF& boundingBox,
        nx::utils::db::InnerJoinFilterFields* sqlFilter);

    void loadObjects(
        nx::utils::db::SqlQuery& selectEventsQuery,
        const Filter& filter,
        std::vector<DetectedObject>* result);
    void loadObject(
        nx::utils::db::SqlQuery* selectEventsQuery,
        DetectedObject* object);
    void mergeObjects(DetectedObject from, DetectedObject* to);

    void queryTrackInfo(
        nx::utils::db::QueryContext* queryContext,
        std::vector<DetectedObject>* result);

    nx::utils::db::DBResult selectTimePeriods(
        nx::utils::db::QueryContext* queryContext,
        const Filter& filter,
        const TimePeriodsLookupOptions& options,
        QnTimePeriodList* result);

    void loadTimePeriods(
        nx::utils::db::SqlQuery& query,
        const TimePeriodsLookupOptions& options,
        QnTimePeriodList* result);
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

} // namespace storage
} // namespace analytics
} // namespace nx
