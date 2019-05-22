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

#include <analytics/db/abstract_storage.h>

#include "analytics_db_controller.h"
#include "analytics_archive_directory.h"
#include "attributes_dao.h"
#include "device_dao.h"
#include "detection_data_saver.h"
#include "object_cache.h"
#include "object_track_aggregator.h"
#include "object_type_dao.h"
#include "time_period_dao.h"

class QnMediaServerModule;

namespace nx::analytics::db {

class EventsStorage:
    public AbstractEventsStorage
{
public:
    EventsStorage(QnMediaServerModule* mediaServerModule);
    virtual ~EventsStorage();

    virtual bool initialize(const Settings& settings) override;

    virtual void save(common::metadata::ConstDetectionMetadataPacketPtr packet) override;

    virtual void createLookupCursor(
        Filter filter,
        CreateCursorCompletionHandler completionHandler) override;

    virtual void closeCursor(const std::shared_ptr<AbstractCursor>& cursor) override;

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

    virtual void flush(StoreCompletionHandler completionHandler) override;

private:
    QnMediaServerModule* m_mediaServerModule = nullptr;
    std::unique_ptr<DbController> m_dbController;
    std::list<std::shared_ptr<AbstractCursor>> m_openedCursors;
    QnMutex m_dbControllerMutex;
    QnMutex m_cursorsMutex;
    std::atomic<bool> m_closingDbController {false};
    std::chrono::milliseconds m_maxRecordedTimestamp = std::chrono::milliseconds::zero();
    mutable QnMutex m_mutex;
    AttributesDao m_attributesDao;
    ObjectTypeDao m_objectTypeDao;
    DeviceDao m_deviceDao;
    TimePeriodDao m_timePeriodDao;
    std::unique_ptr<AnalyticsArchiveDirectory> m_analyticsArchiveDirectory;
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
        int64_t attributesId,
        int64_t timePeriodId);

    void updateDictionariesIfNeeded(
        nx::sql::QueryContext* queryContext,
        const common::metadata::DetectionMetadataPacket& packet,
        const common::metadata::DetectedObject& detectedObject);

    void savePacketDataToCache(
        const QnMutexLockerBase& /*lock*/,
        const common::metadata::ConstDetectionMetadataPacketPtr& packet);

    DetectionDataSaver takeDataToSave(const QnMutexLockerBase& /*lock*/, bool flushData);

    void prepareCursorQuery(const Filter& filter, nx::sql::SqlQuery* query);

    nx::sql::DBResult selectObjects(
        nx::sql::QueryContext* queryContext,
        const Filter& filter,
        std::vector<DetectedObject>* result);

    void prepareLookupQuery(const Filter& filter, nx::sql::SqlQuery* query);

    nx::sql::Filter prepareSqlFilterExpression(
        const Filter& filter,
        QString* eventsFilteredByFreeTextSubQuery);

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

    void reportCreateCursorCompletion(
        sql::DBResult resultCode,
        QnUuid dbCursorId,
        CreateCursorCompletionHandler completionHandler);

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

    void scheduleDataCleanup(
        QnUuid deviceId,
        std::chrono::milliseconds oldestDataToKeepTimestamp);

    nx::sql::DBResult cleanupData(
        nx::sql::QueryContext* queryContext,
        const QnUuid& deviceId,
        std::chrono::milliseconds oldestDataToKeepTimestamp);

    void cleanupEvents(
        nx::sql::QueryContext* queryContext,
        const QnUuid& deviceId,
        std::chrono::milliseconds oldestDataToKeepTimestamp);

    void cleanupEventProperties(nx::sql::QueryContext* queryContext);

    void logCleanupResult(
        sql::DBResult resultCode,
        const QnUuid& deviceId,
        std::chrono::milliseconds oldestDataToKeepTimestamp);

    void logDataSaveResult(sql::DBResult resultCode);

    static QRect packRect(const QRectF& rectf);
    static QRectF unpackRect(const QRect& rect);
};

//-------------------------------------------------------------------------------------------------

using EventsStorageFactoryFunction =
    std::unique_ptr<AbstractEventsStorage>(QnMediaServerModule*);

class EventsStorageFactory:
    public nx::utils::BasicFactory<EventsStorageFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<EventsStorageFactoryFunction>;

public:
    EventsStorageFactory();

    static EventsStorageFactory& instance();

private:
    std::unique_ptr<AbstractEventsStorage> defaultFactoryFunction(QnMediaServerModule*);
};

} // namespace nx::analytics::db
