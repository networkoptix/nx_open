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

#include "abstract_storage.h"
#include "analytics_db_controller.h"
#include "analytics_archive_directory.h"
#include "attributes_dao.h"
#include "device_dao.h"
#include "object_track_data_saver.h"
#include "object_track_cache.h"
#include "object_track_group_dao.h"
#include "object_track_aggregator.h"
#include "object_type_dao.h"

class QnCommonModule;

namespace nx::analytics::db {

class AbstractObjectTypeDictionary;

enum class ChownMode
{
    recursive,
    nonRecursive,
    mountPoint,
};

QString NX_ANALYTICS_DB_API toString(ChownMode mode);

class NX_ANALYTICS_DB_API EventsStorage:
    public AbstractEventsStorage
{
public:
    EventsStorage(
        QnCommonModule* commonModule,
        AbstractObjectTrackBestShotCache* imageCache,
        AbstractObjectTypeDictionary* objectTypeDictionary);
    virtual ~EventsStorage();

    /**
     * MUST be invoked before any usage.
     * If this method fails then the object cannot be used.
     */
    virtual InitResult initialize(const Settings& settings) override;

    virtual bool initialized() const override;

    virtual void save(common::metadata::ConstObjectMetadataPacketPtr packet) override;

    /**
     * Returns track details taken from the cache only.
     */
    virtual std::vector<ObjectPosition> lookupTrackDetailsSync(const ObjectTrack& track) override;

    virtual void lookup(
        Filter filter,
        LookupCompletionHandler completionHandler) override;

    virtual void lookupBestShot(
        const QnUuid& trackId,
        BestShotLookupCompletionHandler completionHandler) override;

    virtual void lookupTimePeriods(
        Filter filter,
        TimePeriodsLookupOptions options,
        TimePeriodsLookupCompletionHandler completionHandler) override;

    virtual void markDataAsDeprecated(
        QnUuid deviceId,
        std::chrono::milliseconds oldestDataToKeepTimestamp) override;

    virtual void flush(StoreCompletionHandler completionHandler) override;

    virtual bool readMinimumEventTimestamp(std::chrono::milliseconds* outResult) override;

    virtual std::optional<nx::sql::QueryStatistics> statistics() const override;

protected:
    using PathAndMode = std::tuple<QString, ChownMode>;

    /**
     * Creates directory including all parents folders.
     * @return true if path exists after return and it is a directory. false otherwise.
     */
    virtual bool makePath(const QString& path);

    /**
     * Makes all files and directories from the pathAndModeList writable.
     * @return true if succeeded for all entries.
     */
    virtual bool makeWritable(const std::vector<PathAndMode>& pathAndModeList);

    QnGlobalSettings* globalSettings() const;

private:
    QnCommonModule* m_commonModule = nullptr;
    AbstractObjectTrackBestShotCache* m_imageCache = nullptr;
    const AbstractObjectTypeDictionary& m_objectTypeDictionary;
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

    void scheduleDataCleanup(
        const QnMutexLockerBase&,
        QnUuid deviceId,
        std::chrono::milliseconds oldestDataToKeepTimestamp);

    void logCleanupResult(
        sql::DBResult resultCode,
        const QnUuid& deviceId,
        std::chrono::milliseconds oldestDataToKeepTimestamp);

    void logDataSaveResult(sql::DBResult resultCode);

    static std::vector<PathAndMode> enumerateSqlFiles(const QString& dbFileName);
};

} // namespace nx::analytics::db
