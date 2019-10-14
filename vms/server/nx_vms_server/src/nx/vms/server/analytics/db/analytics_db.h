#pragma once

#include <atomic>
#include <chrono>
#include <map>
#include <vector>

#include <nx/sql/filter.h>
#include <nx/sql/query.h>
#include <nx/utils/async_operation_guard.h>
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

    /**
     * MUST be invoked before any usage.
     * If this method fails then the object cannot be used.
     */
    virtual bool initialize(const Settings& settings) override;

    virtual bool initialized() const override;

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
    enum class ChownMode
    {
        recursive,
        nonRecursive
    };

    QnMediaServerModule* m_mediaServerModule = nullptr;
    std::unique_ptr<DbController> m_dbController;
    std::list<AbstractCursor*> m_openedCursors;
    std::chrono::milliseconds m_maxRecordedTimestamp = std::chrono::milliseconds::zero();
    mutable QnMutex m_mutex;
    AttributesDao m_attributesDao;
    ObjectTypeDao m_objectTypeDao;
    DeviceDao m_deviceDao;
    std::unique_ptr<AnalyticsArchiveDirectory> m_analyticsArchiveDirectory;
    std::unique_ptr<ObjectTrackCache> m_objectTrackCache;
    ObjectTrackAggregator m_trackAggregator;
    ObjectTrackGroupDao m_trackGroupDao;
    bool m_stopped = false;
    nx::utils::AsyncOperationGuard m_asyncOperationGuard;

    bool readMaximumEventTimestamp();

    bool loadDictionaries();

    void closeAllCursors(const QnMutexLockerBase&);

    void savePacketDataToCache(
        const QnMutexLockerBase& /*lock*/,
        const common::metadata::ConstObjectMetadataPacketPtr& packet);

    ObjectTrackDataSaver takeDataToSave(const QnMutexLockerBase& /*lock*/, bool flushData);

    void reportCreateCursorCompletion(
        sql::DBResult resultCode,
        QnUuid dbCursorId,
        CreateCursorCompletionHandler completionHandler);

    void scheduleDataCleanup(
        const QnMutexLockerBase&,
        QnUuid deviceId,
        std::chrono::milliseconds oldestDataToKeepTimestamp);

    void logCleanupResult(
        sql::DBResult resultCode,
        const QnUuid& deviceId,
        std::chrono::milliseconds oldestDataToKeepTimestamp);

    void logDataSaveResult(sql::DBResult resultCode);
    bool makePath(const QString& path);
    bool changeOwner(const QString& path, ChownMode mode);

    static QRect packRect(const QRectF& rectf);
    static QRectF unpackRect(const QRect& rect);
};

//-------------------------------------------------------------------------------------------------

class MovableAnalyticsDb:
    public AbstractEventsStorage
{
public:
    MovableAnalyticsDb(QnMediaServerModule* mediaServerModule);
    virtual ~MovableAnalyticsDb();

    virtual bool initialize(const Settings& settings) override;

    virtual bool initialized() const override;

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
    QnMutex m_mutex;
    QnMediaServerModule* m_mediaServerModule = nullptr;
    std::shared_ptr<EventsStorage> m_db;

    std::shared_ptr<EventsStorage> getDb();
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
