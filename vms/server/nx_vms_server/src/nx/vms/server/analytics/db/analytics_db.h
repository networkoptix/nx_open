#pragma once

#include <atomic>
#include <chrono>
#include <map>
#include <vector>

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
#include "object_track_data_saver.h"
#include "object_track_cache.h"
#include "object_track_group_dao.h"
#include "object_track_aggregator.h"
#include "object_type_dao.h"

class QnMediaServerModule;

namespace nx::analytics::db {

class EventsStorage:
    public AbstractEventsStorage
{
public:
    EventsStorage(QnMediaServerModule* mediaServerModule);
    virtual ~EventsStorage();

    virtual bool initialize(const Settings& settings) override;

    virtual void save(common::metadata::ConstObjectMetadataPacketPtr packet) override;

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

    virtual void flush(StoreCompletionHandler completionHandler) override;

    virtual bool readMinimumEventTimestamp(std::chrono::milliseconds* outResult) override;

private:
    QnMediaServerModule* m_mediaServerModule = nullptr;
    std::unique_ptr<DbController> m_dbController;
    std::list<AbstractCursor*> m_openedCursors;
    QnMutex m_dbControllerMutex;
    QnMutex m_cursorsMutex;
    std::atomic<bool> m_closingDbController {false};
    std::chrono::milliseconds m_maxRecordedTimestamp = std::chrono::milliseconds::zero();
    mutable QnMutex m_mutex;
    AttributesDao m_attributesDao;
    ObjectTypeDao m_objectTypeDao;
    DeviceDao m_deviceDao;
    std::unique_ptr<AnalyticsArchiveDirectory> m_analyticsArchiveDirectory;
    ObjectTrackCache m_objectTrackCache;
    ObjectTrackAggregator m_trackAggregator;
    ObjectTrackGroupDao m_trackGroupDao;

    bool ensureDbDirIsWritable(const QString& path);

    bool readMaximumEventTimestamp();

    bool loadDictionaries();

    /**
     * @return Inserted event id.
     * Throws on error.
     */
    void insertEvent(
        nx::sql::QueryContext* queryContext,
        const common::metadata::ObjectMetadataPacket& packet,
        const common::metadata::ObjectMetadata& objectMetadata,
        int64_t attributesId,
        int64_t timePeriodId);

    void updateDictionariesIfNeeded(
        nx::sql::QueryContext* queryContext,
        const common::metadata::ObjectMetadataPacket& packet,
        const common::metadata::ObjectMetadata& objectMetadata);

    void savePacketDataToCache(
        const QnMutexLockerBase& /*lock*/,
        const common::metadata::ConstObjectMetadataPacketPtr& packet);

    ObjectTrackDataSaver takeDataToSave(const QnMutexLockerBase& /*lock*/, bool flushData);

    void reportCreateCursorCompletion(
        sql::DBResult resultCode,
        QnUuid dbCursorId,
        CreateCursorCompletionHandler completionHandler);

    void scheduleDataCleanup(
        QnUuid deviceId,
        std::chrono::milliseconds oldestDataToKeepTimestamp);

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
