#pragma once

#include <random>
#include <functional>

#include <QtCore/QElapsedTimer>
#include <QtCore/QString>
#include <QtCore/QMap>
#include <QtCore/QFile>
#include <nx/utils/thread/mutex.h>
#include <QtCore/QTimer>
#include <QtCore/QTime>

#include <server/server_globals.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/storage_resource.h>

#include "recording/time_period_list.h"
#include "device_file_catalog.h"
#include <nx/vms/event/event_fwd.h>
#include "utils/db/db_helper.h"
#include "storage_db.h"
#include <nx/utils/uuid.h>
#include <nx/utils/timer_manager.h>
#include <set>
#include <unordered_map>
#include "api/model/rebuild_archive_reply.h"
#include "api/model/recording_stats_reply.h"
#include <nx_ec/managers/abstract_camera_manager.h>
#include <recorder/camera_info.h>
#include <recorder/space_info.h>

#include <atomic>
#include <future>
#include <mutex>
#include <array>
#include <vector>
#include <functional>
#include "storage_db_pool.h"
#include "health/system_health.h"
#include "nx/vms/server/server_module_aware.h"
#include <media_server/media_server_module.h>
#include <api/helpers/chunks_request_data.h>

extern "C" {

#include <libavutil/avutil.h>

} // extern "C"

class QnAbstractMediaStreamDataProvider;
class TestStorageThread;
class RebuildAsyncTask;
class ScanMediaFilesTask;
class AuxiliaryTask;
class QnUuid;
class QnScheduleSync;

namespace nx { namespace analytics { namespace storage { class AbstractEventsStorage; }}}

class QnStorageManager: public QObject, public /*mixin*/ nx::vms::server::ServerModuleAware
{
    Q_OBJECT
    friend class TestHelper;
    friend class nx::caminfo::ServerWriterHandler;

public:
    typedef QMap<int, QnStorageResourcePtr> StorageMap;
    typedef QMap<QString, DeviceFileCatalogPtr> FileCatalogMap;   /* Map by camera unique id. */
    typedef QMap<QString, QSet<QDate>> UsedMonthsMap; /* Map by camera unique id. */

    static const qint64 BIG_STORAGE_THRESHOLD_COEFF = 10; // use if space >= 1/10 from max storage space

    QnStorageManager(
        QnMediaServerModule* serverModule,
        nx::analytics::db::AbstractEventsStorage* analyticsEventsStorage,
        QnServer::StoragePool kind,
        const char* threadName = nullptr);
    virtual ~QnStorageManager();
    void removeStorage(const QnStorageResourcePtr &storage);
    bool hasStorageUnsafe(const QnStorageResourcePtr &storage) const;
    bool hasStorage(const QnStorageResourcePtr &storage) const;

    /*
    * Remove storage if storage is absent in specified list
    */
    void removeAbsentStorages(const QnStorageResourceList &newStorages);
    void addStorage(const QnStorageResourcePtr &storage);

    QString getFileName(const qint64& fileDate, qint16 timeZone, const QnNetworkResourcePtr &netResource, const QString& prefix, const QnStorageResourcePtr& storage);
    bool fileStarted(
        const qint64& startDateMs,
        int timeZone,
        const QString& fileName,
        QnAbstractMediaStreamDataProvider* provider,
        bool sideRecorder = false);

    bool fileFinished(
        int durationMs,
        const QString& fileName,
        QnAbstractMediaStreamDataProvider* provider,
        qint64 fileSize,
        qint64 startTimeMs = AV_NOPTS_VALUE);

    /*
    * convert UTC time to folder name. Used for server archive catalog.
    * @param dateTimeMs UTC time in ms
    * timeZone server time zone offset in munutes. If value==-1 - current(system) time zone is used
    */
    static QString dateTimeStr(qint64 dateTimeMs, qint16 timeZone, const QString& separator);
    static QnStorageResourcePtr getStorageByUrl(
        QnMediaServerModule* serverModule,
        const QString &storageUrl,
        QnServer::StoragePool pool);

    static const std::array<QnServer::StoragePool, 2> getPools();

    bool checkIfMyStorage(const QnStorageResourcePtr &storage) const;
    QnStorageResourcePtr getStorageByUrlExact(const QString& storageUrl);
    QnStorageResourcePtr getStorageByVolume(const QString& volumeRoot) const;
    QnStorageResourcePtr storageRoot(int storage_index) const { QnMutexLocker lock( &m_mutexStorages ); return m_storageRoots.value(storage_index); }
    bool isStorageAvailable(int storage_index) const;
    bool isStorageAvailable(const QnStorageResourcePtr& storage) const;

    DeviceFileCatalogPtr getFileCatalog(const QString& cameraUniqueId, QnServer::ChunksCatalog catalog);
    DeviceFileCatalogPtr getFileCatalog(const QString& cameraUniqueId, const QString &catalogPrefix);

    static QnTimePeriodList getRecordedPeriods(
        QnMediaServerModule* serverModule,
        const QnChunksRequestData& request,
        const QList<QnServer::ChunksCatalog> &catalogs);
    QnRecordingStatsReply getChunkStatistics(qint64 bitrateAnalyzePeriodMs);

    void doMigrateCSVCatalog(QnStorageResourcePtr extraAllowedStorage = QnStorageResourcePtr());
    void partialMediaScan(const DeviceFileCatalogPtr &fileCatalog, const QnStorageResourcePtr &storage, const DeviceFileCatalog::ScanFilter& filter);

    QnStorageResourcePtr getOptimalStorageRoot(
        std::function<bool(const QnStorageResourcePtr &)> pred =
            [](const QnStorageResourcePtr &storage) {
                return !storage->hasFlags(Qn::storage_fastscan) ||
                        storage->getFreeSpace() > storage->getSpaceLimit();
            });

    QnStorageResourceList getStorages() const;

    /*
     * Return writable storages with checkBox 'usedForWriting'
     */
    QSet<QnStorageResourcePtr> getUsedWritableStorages() const;
    QSet<QnStorageResourcePtr> getClearableStorages() const;

    /*
     * Return all storages which can be used for writing
     */
    QSet<QnStorageResourcePtr> getAllWritableStorages(
        const QnStorageResourceList* additionalStorages = nullptr) const;

    QnStorageResourceList getStoragesInLexicalOrder() const;
    bool hasRebuildingStorages() const;

    void clearSpace(bool forced=false);
    bool clearSpaceForFile(const QString& path, qint64 size);
    bool canAddChunk(qint64 timeMs, qint64 size);
    void checkSystemStorageSpace();
    void checkMetadataStorageSpace();

    bool clearOldestSpace(const QnStorageResourcePtr &storage, bool useMinArchiveDays, qint64 targetFreeSpace);
    void clearMaxDaysData();
    void clearMaxDaysData(QnServer::ChunksCatalog catalogIdx);

    void deleteRecordsToTime(DeviceFileCatalogPtr catalog, qint64 minTime);
    void clearDbByChunk(DeviceFileCatalogPtr catalog, const DeviceFileCatalog::Chunk& chunk);

    bool isWritableStoragesAvailable() const;

    static bool isArchiveTimeExists(QnMediaServerModule* serverModule, const QString& cameraUniqueId, qint64 timeMs);
    static bool isArchiveTimeExists(QnMediaServerModule* serverModule, const QString& cameraUniqueId, const QnTimePeriod period);
    void stopAsyncTasks();

    QnStorageScanData rebuildCatalogAsync();
    void cancelRebuildCatalogAsync();

    void setRebuildInfo(const QnStorageScanData& data);
    QnStorageScanData rebuildInfo() const;
    bool needToStopMediaScan() const;

    void initDone();
    QnStorageResourcePtr findStorageByOldIndex(int oldIndex);

    void migrateSqliteDatabase(const QnStorageResourcePtr & storage) const;
    /*
    * Return camera list with existing archive. Camera Unique ID is used as camera ID
    */
    std::vector<QnUuid> getCamerasWithArchiveHelper() const;

    QnScheduleSync* scheduleSync() const;
    static Qn::StorageStatuses storageStatus(
        QnMediaServerModule* serverModule,
        const QnStorageResourcePtr& storage);

signals:
    void storagesAvailable();
    void noStoragesAvailable();
    void storageFailure(const QnResourcePtr &storageRes, nx::vms::api::EventReason reason);
    void rebuildFinished(QnSystemHealth::MessageType msgType);
    void backupFinished(qint64 backedUpToMs, nx::vms::api::EventReason);
public slots:
    void at_archiveRangeChanged(const QnStorageResourcePtr &resource, qint64 newStartTimeMs, qint64 newEndTimeMs);
    void onNewResource(const QnResourcePtr &resource);
    void onDelResource(const QnResourcePtr &resource);
    void at_storageRoleChanged(const QnResourcePtr &storage);
    void testOfflineStorages();
private:
    friend class TestStorageThread;

    void createArchiveCameras(const nx::caminfo::ArchiveCameraDataList& archiveCameras);
    void getRecordedPeriodsInternal(
        std::vector<QnTimePeriodList>& periods,
        const QnChunksRequestData& request,
        const QList<QnServer::ChunksCatalog> &catalogs);
    bool isArchiveTimeExistsInternal(const QString& cameraUniqueId, qint64 timeMs);
    bool isArchiveTimeExistsInternal(const QString& cameraUniqueId, const QnTimePeriod period);
    //void loadFullFileCatalogInternal(QnServer::ChunksCatalog catalog, bool rebuildMode);
    QnStorageResourcePtr extractStorageFromFileName(int& storageIndex, const QString& fileName, QString& uniqueId, QString& quality);
    void getTimePeriodInternal(std::vector<QnTimePeriodList>& periods,
        const QnNetworkResourcePtr& camera,
        qint64 startTimeMs,
        qint64 endTimeMs,
        qint64 detailLevel,
        bool keepSmallChunks,
        int limit,
        Qt::SortOrder sortOrder,
        const DeviceFileCatalogPtr& catalog);
        bool existsStorageWithID(const QnStorageResourceList& storages,
        const QnUuid& id) const;
    QnStorageResourcePtr getStorageByUrlInternal(const QString& fileName);

    QString toCanonicalPath(const QString& path);
    StorageMap getAllStorages() const;
    QSet<QnStorageResourcePtr> getWritableStorages(
        std::function<bool (const QnStorageResourcePtr& storage)> filter) const;

    QnStorageResourcePtr getUsedWritableStorageByIndex(int storageIndex);

    void changeStorageStatus(const QnStorageResourcePtr &fileStorage, Qn::ResourceStatus status);
    DeviceFileCatalogPtr getFileCatalogInternal(const QString& cameraUniqueId, QnServer::ChunksCatalog catalog);

    void loadFullFileCatalogFromMedia(const QnStorageResourcePtr &storage, QnServer::ChunksCatalog catalog,
                                      nx::caminfo::ArchiveCameraDataList &archiveCamerasList, std::function<void(int current, int total)> progressCallback = nullptr);

    void replaceChunks(const QnTimePeriod& rebuildPeriod, const QnStorageResourcePtr &storage, const DeviceFileCatalogPtr &newCatalog, const QString& cameraUniqueId, QnServer::ChunksCatalog catalog);
    void doMigrateCSVCatalog(QnServer::ChunksCatalog catalog, QnStorageResourcePtr extraAllowedStorage);
    QMap<QString, QSet<int>> deserializeStorageFile();
    void clearUnusedMetadata();
    //void clearCameraHistory();
    //void minTimeByCamera(const FileCatalogMap &catalogMap, QMap<QString, qint64>& minTimes);
    void updateRecordedMonths(UsedMonthsMap& usedMonths);
    void updateRecordedMonths(const FileCatalogMap &catalogMap, UsedMonthsMap& usedMonths);
    void findTotalMinTime(const bool useMinArchiveDays, const FileCatalogMap& catalogMap, qint64& minTime, DeviceFileCatalogPtr& catalog);
    void addDataFromDatabase(const QnStorageResourcePtr &storage);
    bool writeCSVCatalog(const QString& fileName, const QVector<DeviceFileCatalog::Chunk> chunks);
    void backupFolderRecursive(const QString& src, const QString& dst);
    void getCamerasWithArchiveInternal(std::set<QString>& result,  const FileCatalogMap& catalog) const;
    void testStoragesDone();
    //QMap<QnUuid, QnRecordingStatsData> getChunkStatisticsInternal(qint64 startTime, qint64 endTime, QnServer::ChunksCatalog catalog);
    QnRecordingStatsData getChunkStatisticsByCamera(qint64 bitrateAnalyzePeriodMs, const QString& uniqueId);

    // get statistics for the whole archive except of bitrate. It's analyzed for the last records of archive only in range <= bitrateAnalyzePeriodMs
    QnRecordingStatsData mergeStatsFromCatalogs(qint64 bitrateAnalyzePeriodMs, const DeviceFileCatalogPtr& catalogHi, const DeviceFileCatalogPtr& catalogLow);
    void clearAnalyticsEvents(const QMap<QnUuid, qint64>& dataToDelete);
    void forciblyClearAnalyticsEvents();
    QMap<QnUuid, qint64> calculateOldestDataTimestampByCamera();
    bool getMinTimes(QMap<QString, qint64>& lastTime);
    void processCatalogForMinTime(QMap<QString, qint64>& lastTime, const FileCatalogMap& catalogMap);

    QStringList getAllCameraIdsUnderLock(QnServer::ChunksCatalog catalog) const;
    void writeCameraInfoFiles();
    static bool renameFileWithDuration(
        const QString               &oldName,
        QString                     &newName,
        int64_t                     duration,
        const QnStorageResourcePtr  &storage
    );
    void updateCameraHistory() const;
    static std::vector<QnUuid> getCamerasWithArchive(QnMediaServerModule* serverModule);
    int64_t calculateNxOccupiedSpace(int storageIndex) const;
    bool hasArchive(int storageIndex) const;
    QnStorageResourcePtr getStorageByIndex(int index) const;
    bool getSqlDbPath(const QnStorageResourcePtr &storage, QString &dbFolderPath) const;
    void startAuxTimerTasks();
    void checkWritableStoragesExist();
    Qn::StorageStatuses storageStatusInternal(const QnStorageResourcePtr& storage);

private:
    nx::analytics::db::AbstractEventsStorage* m_analyticsEventsStorage;
    const QnServer::StoragePool m_role;
    StorageMap                  m_storageRoots;
    FileCatalogMap              m_devFileCatalog[QnServer::ChunksCatalogCount];

    mutable QnMutex m_mutexStorages;
    mutable QnMutex m_mutexCatalog;
    mutable QnMutex m_mutexRebuild;
    mutable QnMutex m_rebuildStateMtx;
    mutable QnMutex m_localPatches;
    mutable QnMutex m_testStorageThreadMutex;
    QnMutex m_clearSpaceMutex;

    QTimer m_timer;

    mutable bool m_isWritableStorageAvail;
    QElapsedTimer m_storageWarnTimer;
    TestStorageThread* m_testStorageThread;
    /**
     * Storage id --> 'corresponding storage has too much data not managed by VMS' flag map.
     */
    QSet<QnUuid> m_fullDisksIds;

    QnStorageScanData m_archiveRebuildInfo;
    std::atomic<bool> m_rebuildCancelled;

    friend class RebuildAsyncTask;
    friend class ScanMediaFilesTask;
    friend class AuxiliaryTask;

    ScanMediaFilesTask* m_rebuildArchiveThread;

    bool m_initInProgress;
    QMap<QString, QSet<int>> m_oldStorageIndexes;
    mutable QnMutex m_csvMigrationMutex;
    QElapsedTimer m_clearMotionTimer;
    QElapsedTimer m_clearBookmarksTimer;
    QElapsedTimer m_removeEmtyDirTimer;
    QMap<QString, qint64> m_lastCatalogTimes;
    std::unique_ptr<QnScheduleSync> m_scheduleSync;
    std::atomic<bool> m_firstStoragesTestDone;

    bool m_isRenameDisabled;
    nx::recorder::SpaceInfo m_spaceInfo;
    nx::caminfo::ServerWriterHandler m_camInfoWriterHandler;
    nx::caminfo::Writer m_camInfoWriter;

    nx::utils::StandaloneTimerManager m_auxTasksTimerManager;
    std::optional<bool> m_hasWritableStorages;
};
