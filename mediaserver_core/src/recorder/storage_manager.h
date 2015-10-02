#ifndef __STORAGE_MANAGER_H__
#define __STORAGE_MANAGER_H__

#include <QtCore/QElapsedTimer>
#include <QtCore/QString>
#include <QtCore/QMap>
#include <QtCore/QFile>
#include <utils/thread/mutex.h>
#include <QtCore/QTimer>
#include <QtCore/QTime>

#include <server/server_globals.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>

#include "recording/time_period_list.h"
#include "device_file_catalog.h"
#include "business/business_fwd.h"
#include "utils/db/db_helper.h"
#include "storage_db.h"
#include "utils/common/uuid.h"
#include <set>
#include "api/model/rebuild_archive_reply.h"
#include "api/model/recording_stats_reply.h"

#include <atomic>
#include <future>
#include <mutex>

class QnAbstractMediaStreamDataProvider;
class TestStorageThread;
class RebuildAsyncTask;
class ScanMediaFilesTask;
class QnUuid;

class QnStorageManager: public QObject
{
    Q_OBJECT
public:
    typedef QMap<int, QnStorageResourcePtr> StorageMap;
    typedef QMap<QString, DeviceFileCatalogPtr> FileCatalogMap;   /* Map by camera unique id. */
    typedef QMap<QString, QSet<QDate>> UsedMonthsMap; /* Map by camera unique id. */

    static const qint64 BIG_STORAGE_THRESHOLD_COEFF = 10; // use if space >= 1/10 from max storage space
    
    QnStorageManager(QnServer::ArchiveKind kind);
    virtual ~QnStorageManager();
    static QnStorageManager* normalInstance();
    static QnStorageManager* backupInstance();
    void removeStorage(const QnStorageResourcePtr &storage);


    /*
    * Remove storage if storage is absent in specified list
    */
    void removeAbsentStorages(const QnStorageResourceList &newStorages);
    void addStorage(const QnStorageResourcePtr &storage);

    QString getFileName(const qint64& fileDate, qint16 timeZone, const QnNetworkResourcePtr &netResource, const QString& prefix, const QnStorageResourcePtr& storage);
    bool fileStarted(const qint64& startDateMs, int timeZone, const QString& fileName, QnAbstractMediaStreamDataProvider* provider);
    bool fileFinished(int durationMs, const QString& fileName, QnAbstractMediaStreamDataProvider* provider,  qint64 fileSize);

    /*
    * convert UTC time to folder name. Used for server archive catalog.
    * @param dateTimeMs UTC time in ms
    * timeZone server time zone offset in munutes. If value==-1 - current(system) time zone is used
    */
    static QString dateTimeStr(qint64 dateTimeMs, qint16 timeZone, const QString& separator);

    QnStorageResourcePtr getStorageByUrl(const QString& fileName);
    QnStorageResourcePtr getStorageByUrlExact(const QString& storageUrl);
    QnStorageResourcePtr storageRoot(int storage_index) const { QnMutexLocker lock( &m_mutexStorages ); return m_storageRoots.value(storage_index); }
    bool isStorageAvailable(int storage_index) const; 
    bool isStorageAvailable(const QnStorageResourcePtr& storage) const; 

    DeviceFileCatalogPtr getFileCatalog(const QString& cameraUniqueId, QnServer::ChunksCatalog catalog);
    DeviceFileCatalogPtr getFileCatalog(const QString& cameraUniqueId, const QString &catalogPrefix);

    QnTimePeriodList getRecordedPeriods(const QnVirtualCameraResourceList &cameras, qint64 startTime, qint64 endTime, qint64 detailLevel, const QList<QnServer::ChunksCatalog> &catalogs, int limit);
    QnRecordingStatsReply getChunkStatistics(qint64 bitrateAnalizePeriodMs);

    void doMigrateCSVCatalog(QnStorageResourcePtr extraAllowedStorage = QnStorageResourcePtr());
    void partialMediaScan(const DeviceFileCatalogPtr &fileCatalog, const QnStorageResourcePtr &storage, const DeviceFileCatalog::ScanFilter& filter);
    
    QnStorageResourcePtr getOptimalStorageRoot(QnAbstractMediaStreamDataProvider *provider);

    QnStorageResourceList getStorages() const;
    QnStorageResourceList getStoragesInLexicalOrder() const;

    void clearSpace();
    void removeEmptyDirs(const QnStorageResourcePtr &storage);
    
    void clearOldestSpace(const QnStorageResourcePtr &storage, bool useMinArchiveDays);
    void clearMaxDaysData();
    void clearMaxDaysData(QnServer::ChunksCatalog catalogIdx);

    void deleteRecordsToTime(DeviceFileCatalogPtr catalog, qint64 minTime);
    void clearDbByChunk(DeviceFileCatalogPtr catalog, const DeviceFileCatalog::Chunk& chunk);

    bool isWritableStoragesAvailable() const { return m_isWritableStorageAvail; }

    bool isArchiveTimeExists(const QString& cameraUniqueId, qint64 timeMs);
    bool isArchiveTimeExists(const QString& cameraUniqueId, const QnTimePeriod period);
    void stopAsyncTasks();

    QnStorageScanData rebuildCatalogAsync();
    void cancelRebuildCatalogAsync();

    void setRebuildInfo(const QnStorageScanData& data);
    QnStorageScanData rebuildInfo() const;
    bool needToStopMediaScan() const;
    
    /*
    * Return full path list from storage_index.csv (include absent in DB storages)
    */
    QStringList getAllStoragePathes() const;

    bool addBookmark(const QByteArray &cameraGuid, QnCameraBookmark &bookmark, bool forced = false);
    bool updateBookmark(const QByteArray &cameraGuid, QnCameraBookmark &bookmark);
    bool deleteBookmark(const QByteArray &cameraGuid, QnCameraBookmark &bookmark);
    bool getBookmarks(const QByteArray &cameraGuid, const QnCameraBookmarkSearchFilter &filter, QnCameraBookmarkList &result);
    void initDone();
    int getStorageIndex(const QnStorageResourcePtr& storage);
    QnStorageResourcePtr findStorageByOldIndex(int oldIndex);

    /*
    * Return camera list with existing archive. Camera Unique ID is used as camera ID
    */
    std::vector<QnUuid> getCamerasWithArchive() const;
signals:
    void noStoragesAvailable();
    void storageFailure(const QnResourcePtr &storageRes, QnBusiness::EventReason reason);
    void rebuildFinished();
public slots:
    void at_archiveRangeChanged(const QnStorageResourcePtr &resource, qint64 newStartTimeMs, qint64 newEndTimeMs);
    void onNewResource(const QnResourcePtr &resource);
    void onDelResource(const QnResourcePtr &resource);
    void at_storageChanged(const QnResourcePtr &storage);
    void testOfflineStorages();
private:
    friend class TestStorageThread;

    int detectStorageIndex(const QString& path);
    //void loadFullFileCatalogInternal(QnServer::ChunksCatalog catalog, bool rebuildMode);
    QnStorageResourcePtr extractStorageFromFileName(int& storageIndex, const QString& fileName, QString& uniqueId, QString& quality);
    void getTimePeriodInternal(std::vector<QnTimePeriodList> &cameras, const QnNetworkResourcePtr &camera, qint64 startTime, qint64 endTime, qint64 detailLevel, const DeviceFileCatalogPtr &catalog);
    bool existsStorageWithID(const QnStorageResourceList& storages, const QnUuid &id) const;
    void updateStorageStatistics();

    QString toCanonicalPath(const QString& path);
    StorageMap getAllStorages() const;
    QSet<QnStorageResourcePtr> getWritableStorages() const;
    void changeStorageStatus(const QnStorageResourcePtr &fileStorage, Qn::ResourceStatus status);
    DeviceFileCatalogPtr getFileCatalogInternal(const QString& cameraUniqueId, QnServer::ChunksCatalog catalog);
    void loadFullFileCatalogFromMedia(const QnStorageResourcePtr &storage, QnServer::ChunksCatalog catalog, qreal progressCoeff);
    void replaceChunks(const QnTimePeriod& rebuildPeriod, const QnStorageResourcePtr &storage, const DeviceFileCatalogPtr &newCatalog, const QString& cameraUniqueId, QnServer::ChunksCatalog catalog);
    void doMigrateCSVCatalog(QnServer::ChunksCatalog catalog, QnStorageResourcePtr extraAllowedStorage);
    QMap<QString, QSet<int>> deserializeStorageFile();
    void clearUnusedMotion();
    //void clearCameraHistory();
    //void minTimeByCamera(const FileCatalogMap &catalogMap, QMap<QString, qint64>& minTimes);
    void updateRecordedMonths(const FileCatalogMap &catalogMap, UsedMonthsMap& usedMonths);
    void findTotalMinTime(const bool useMinArchiveDays, const FileCatalogMap& catalogMap, qint64& minTime, DeviceFileCatalogPtr& catalog);
    void addDataFromDatabase(const QnStorageResourcePtr &storage);
    QnStorageDbPtr getSDB(const QnStorageResourcePtr &storage);
    bool writeCSVCatalog(const QString& fileName, const QVector<DeviceFileCatalog::Chunk> chunks);
    void backupFolderRecursive(const QString& src, const QString& dst);
    void getCamerasWithArchiveInternal(std::set<QString>& result,  const FileCatalogMap& catalog) const;
    void testStoragesDone();
    //QMap<QnUuid, QnRecordingStatsData> getChunkStatisticsInternal(qint64 startTime, qint64 endTime, QnServer::ChunksCatalog catalog);
    QnRecordingStatsData getChunkStatisticsByCamera(qint64 bitrateAnalizePeriodMs, const QString& uniqueId);

    // get statistics for the whole archive except of bitrate. It's analyzed for the last records of archive only in range <= bitrateAnalizePeriodMs
    QnRecordingStatsData mergeStatsFromCatalogs(qint64 bitrateAnalizePeriodMs, const DeviceFileCatalogPtr& catalogHi, const DeviceFileCatalogPtr& catalogLow);

    // redundant storages
    void startRedundantSyncWatchdog();
    void stopRedundantSyncWatchdog();

    template<typename NeedStopCB>
    void synchronizeStorages(
        QnStorageResourcePtr   storage,
        NeedStopCB             needStop
    );
private:
    QnServer::ArchiveKind   m_kind;
    StorageMap              m_storageRoots;
    FileCatalogMap          m_devFileCatalog[QnServer::ChunksCatalogCount];

    mutable QnMutex m_mutexStorages;
    mutable QnMutex m_mutexCatalog;
    mutable QnMutex m_mutexRebuild;
    mutable QnMutex m_rebuildStateMtx;
    mutable QnMutex m_localPatches;

    QMap<QString, QSet<int> > m_storageIndexes;
    bool m_storagesStatisticsReady;
    QTimer m_timer;

    bool m_warnSended;
    bool m_isWritableStorageAvail;
    QElapsedTimer m_storageWarnTimer;
    static TestStorageThread* m_testStorageThread;
    QMap<QnUuid, bool> m_diskFullWarned;
    
    //RebuildState m_rebuildState;
    //QnStorageResourcePtr m_rebuildStorage;
    //double m_rebuildProgress;
    QnStorageScanData m_archiveRebuildInfo;
    bool m_rebuildCancelled;

    friend class RebuildAsyncTask;
    friend class ScanMediaFilesTask;

    ScanMediaFilesTask* m_rebuildArchiveThread;

    QMap<QString, QnStorageDbPtr> m_chunksDB;
    bool m_initInProgress;
    mutable QnMutex m_sdbMutex;
    QMap<QString, QSet<int>> m_oldStorageIndexes;
    mutable QnMutex m_csvMigrationMutex;
    bool m_firstStorageTestDone;
    QElapsedTimer m_clearMotionTimer;
    QElapsedTimer m_removeEmtyDirTimer;

    std::atomic<bool>   m_redundantSyncOn;
    std::future<void>   m_redundantFuture;
};

#define qnNormalStorageMan QnStorageManager::normalInstance()
#define qnBackupStorageMan QnStorageManager::backupInstance()

#endif // __STORAGE_MANAGER_H__
