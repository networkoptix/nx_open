#pragma once

#include <chrono>
#include <deque>
#include <map>
#include <vector>

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
    struct AttributesCacheEntry
    {
        QByteArray md5;
        std::int64_t id = -1;
    };

    const Settings& m_settings;
    DbController m_dbController;
    std::chrono::microseconds m_maxRecordedTimestamp = std::chrono::microseconds::zero();
    mutable QnMutex m_mutex;
    std::map<QnUuid, std::int64_t> m_deviceGuidToId;
    std::map<std::int64_t, QnUuid> m_idToDeviceGuid;
    std::map<QString, std::int64_t> m_objectTypeToId;
    std::map<std::int64_t, QString> m_idToObjectType;
    std::deque<AttributesCacheEntry> m_attributesCache;

    bool readMaximumEventTimestamp();

    bool loadDictionaries();
    void loadDeviceDictionary(nx::sql::QueryContext* queryContext);
    void loadObjectTypeDictionary(nx::sql::QueryContext* queryContext);

    void addDeviceToDictionary(std::int64_t id, const QnUuid& deviceGuid);
    std::int64_t deviceIdFromGuid(const QnUuid& deviceGuid) const;
    QnUuid deviceGuidFromId(std::int64_t id) const;

    void addObjectTypeToDictionary(std::int64_t id, const QString& name);
    std::int64_t objectTypeIdFromName(const QString& name) const;
    QString objectTypeFromId(std::int64_t id) const;

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
        std::int64_t attributesId);

    void updateDictionariesIfNeeded(
        nx::sql::QueryContext* queryContext,
        const common::metadata::DetectionMetadataPacket& packet,
        const common::metadata::DetectedObject& detectedObject);

    void updateDeviceDictionaryIfNeeded(
        nx::sql::QueryContext* queryContext,
        const common::metadata::DetectionMetadataPacket& packet);

    void updateObjectTypeDictionaryIfNeeded(
        nx::sql::QueryContext* queryContext,
        const common::metadata::DetectionMetadataPacket& packet,
        const common::metadata::DetectedObject& detectedObject);

    /**
     * Inserts attributes or returns id or existing attributes.
     * @return attributesId
     */
    std::int64_t insertAttributes(
        nx::sql::QueryContext* queryContext,
        const std::vector<common::metadata::Attribute>& eventAttributes);

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
        nx::sql::Filter* sqlFilter);

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

    void loadTimePeriods(
        nx::sql::SqlQuery& query,
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

    void addToAttributesCache(std::int64_t id, const QByteArray& content);
    std::int64_t findAttributesIdInCache(const QByteArray& content);
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
