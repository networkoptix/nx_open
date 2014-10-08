#ifndef __STORAGE_MANAGER_H__
#define __STORAGE_MANAGER_H__

#include <QtCore/QElapsedTimer>
#include <QtCore/QString>
#include <QtCore/QMap>
#include <QtCore/QFile>
#include <QtCore/QMutex>
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

class QnAbstractMediaStreamDataProvider;
class TestStorageThread;
class RebuildAsyncTask;
class QnUuid;

class QnStorageManager: public QObject
{
    Q_OBJECT
public:
    // TODO: #Elric #enum
    enum RebuildState {
        RebuildState_None,
        RebuildState_WaitForRecordersStopped,
        RebuildState_Started
    };

    typedef QMap<int, QnStorageResourcePtr> StorageMap;
    typedef QMap<QString, DeviceFileCatalogPtr> FileCatalogMap;   /* Map by camera unique id. */
    typedef QMap<QString, QSet<QDate>> UsedMonthsMap; /* Map by camera unique id. */

    static const qint64 BIG_STORAGE_THRESHOLD_COEFF = 10; // use if space >= 1/10 from max storage space


    QnStorageManager();
    virtual ~QnStorageManager();
    static QnStorageManager* instance();
    void removeStorage(const QnStorageResourcePtr &storage);

    /*
    * Remove storage if storage is absent in specified list
    */
    void removeAbsentStorages(const QnAbstractStorageResourceList &newStorages);
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
    QnStorageResourcePtr storageRoot(int storage_index) const { QMutexLocker lock(&m_mutexStorages); return m_storageRoots.value(storage_index); }
    bool isStorageAvailable(int storage_index) const; 

    DeviceFileCatalogPtr getFileCatalog(const QString& cameraUniqueId, QnServer::ChunksCatalog catalog);
    DeviceFileCatalogPtr getFileCatalog(const QString& cameraUniqueId, const QString &catalogPrefix);

    QnTimePeriodList getRecordedPeriods(const QnVirtualCameraResourceList &cameras, qint64 startTime, qint64 endTime, qint64 detailLevel, const QList<QnServer::ChunksCatalog> &catalogs);

    void doMigrateCSVCatalog();
    bool loadFullFileCatalog(const QnStorageResourcePtr &storage, bool isRebuild = false, qreal progressCoeff = 1.0);
    std::deque<DeviceFileCatalog::Chunk> correctChunksFromMediaData(const DeviceFileCatalogPtr &fileCatalog, const QnStorageResourcePtr &storage, const std::deque<DeviceFileCatalog::Chunk>& chunks);


    QnStorageResourcePtr getOptimalStorageRoot(QnAbstractMediaStreamDataProvider* provider);

    QnStorageResourceList getStorages() const;

    void clearSpace();
    
    void clearOldestSpace(const QnStorageResourcePtr &storage, bool useMinArchiveDays);
    void clearMaxDaysData();
    void clearMaxDaysData(const FileCatalogMap &catalogMap);

    void deleteRecordsToTime(DeviceFileCatalogPtr catalog, qint64 minTime);
    void clearDbByChunk(DeviceFileCatalogPtr catalog, const DeviceFileCatalog::Chunk& chunk);

    bool isWritableStoragesAvailable() const { return m_isWritableStorageAvail; }

    bool isArchiveTimeExists(const QString& cameraUniqueId, qint64 timeMs);
    void stopAsyncTasks();

    void rebuildCatalogAsync();
    void cancelRebuildCatalogAsync();
    double rebuildProgress() const;

    void setRebuildState(RebuildState state);
    RebuildState rebuildState() const;
    
    /*
    * Return full path list from storage_index.csv (include absent in DB storages)
    */
    QStringList getAllStoragePathes() const;

    bool addBookmark(const QByteArray &cameraGuid, QnCameraBookmark &bookmark);
    bool updateBookmark(const QByteArray &cameraGuid, QnCameraBookmark &bookmark);
    bool deleteBookmark(const QByteArray &cameraGuid, QnCameraBookmark &bookmark);
    bool getBookmarks(const QByteArray &cameraGuid, const QnCameraBookmarkSearchFilter &filter, QnCameraBookmarkList &result);
signals:
    void noStoragesAvailable();
    void storageFailure(const QnResourcePtr &storageRes, QnBusiness::EventReason reason);
    void rebuildFinished();
public slots:
    void at_archiveRangeChanged(const QnAbstractStorageResourcePtr &resource, qint64 newStartTimeMs, qint64 newEndTimeMs);
    void onNewResource(const QnResourcePtr &resource);
    void onDelResource(const QnResourcePtr &resource);
private:
    friend class TestStorageThread;

    int detectStorageIndex(const QString& path);
    QSet<int> getDeprecateIndexList(const QString& p);
    //void loadFullFileCatalogInternal(QnServer::ChunksCatalog catalog, bool rebuildMode);
    QnStorageResourcePtr extractStorageFromFileName(int& storageIndex, const QString& fileName, QString& uniqueId, QString& quality);
    void getTimePeriodInternal(QVector<QnTimePeriodList> &cameras, const QnNetworkResourcePtr &camera, qint64 startTime, qint64 endTime, qint64 detailLevel, const DeviceFileCatalogPtr &catalog);
    bool existsStorageWithID(const QnAbstractStorageResourceList& storages, const QnUuid &id) const;
    void updateStorageStatistics();
    void testOfflineStorages();
    void rebuildCatalogIndexInternal();
    bool isCatalogLoaded() const;

    int getFileNumFromCache(const QString& base, const QString& folder);
    void putFileNumToCache(const QString& base, int fileNum);
    QString toCanonicalPath(const QString& path);
    StorageMap getAllStorages() const;
    QSet<QnStorageResourcePtr> getWritableStorages() const;
    void changeStorageStatus(const QnStorageResourcePtr &fileStorage, Qn::ResourceStatus status);
    DeviceFileCatalogPtr getFileCatalogInternal(const QString& cameraUniqueId, QnServer::ChunksCatalog catalog);
    void loadFullFileCatalogFromMedia(const QnStorageResourcePtr &storage, QnServer::ChunksCatalog catalog, qreal progressCoeff);
    void replaceChunks(const QnTimePeriod& rebuildPeriod, const QnStorageResourcePtr &storage, const DeviceFileCatalogPtr &newCatalog, const QString& cameraUniqueId, QnServer::ChunksCatalog catalog);
    void doMigrateCSVCatalog(QnServer::ChunksCatalog catalog);
    QMap<QString, QSet<int>> deserializeStorageFile();
    QnStorageResourcePtr findStorageByOldIndex(int oldIndex, QMap<QString, QSet<int>> oldIndexes);
    void clearUnusedMotion();
    //void clearCameraHistory();
    //void minTimeByCamera(const FileCatalogMap &catalogMap, QMap<QString, qint64>& minTimes);
    void updateRecordedMonths(const FileCatalogMap &catalogMap, UsedMonthsMap& usedMonths);
    void findTotalMinTime(const bool useMinArchiveDays, const FileCatalogMap& catalogMap, qint64& minTime, DeviceFileCatalogPtr& catalog);
private:
    StorageMap m_storageRoots;
    FileCatalogMap m_devFileCatalog[QnServer::ChunksCatalogCount];

    mutable QMutex m_mutexStorages;
    mutable QMutex m_mutexCatalog;

    QMap<QString, QSet<int> > m_storageIndexes;
    bool m_storagesStatisticsReady;
    QTimer m_timer;

    typedef QMap<QString, QPair<QString, int > > FileNumCache;
    FileNumCache m_fileNumCache;
    QMutex m_cacheMutex;
    bool m_catalogLoaded;
    bool m_warnSended;
    bool m_isWritableStorageAvail;
    QTime m_lastTestTime;
    QElapsedTimer m_storageWarnTimer;
    static TestStorageThread* m_testStorageThread;
    QMap<QnUuid, bool> m_diskFullWarned;
    RebuildState m_rebuildState;
    double m_rebuildProgress;
    bool m_rebuildCancelled;

    friend class RebuildAsyncTask;
    RebuildAsyncTask* m_asyncRebuildTask;

    QMap<QString, QnStorageDbPtr> m_chunksDB;
};

#define qnStorageMan QnStorageManager::instance()

#endif // __STORAGE_MANAGER_H__
