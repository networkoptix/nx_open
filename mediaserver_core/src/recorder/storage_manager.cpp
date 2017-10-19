#include "storage_manager.h"

#include <stdio.h>
#include <QtCore/QDir>

#include <utils/fs/file.h>
#include <utils/common/util.h>
#include <nx/utils/log/log.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_history.h>
#include "core/resource/resource_data.h"
#include "core/resource_management/resource_data_pool.h"
#include "api/common_message_processor.h"
#include "api/app_server_connection.h"
#include "nx_ec/dummy_handler.h"
#include "nx_ec/data/api_conversion_functions.h"

#include <recorder/server_stream_recorder.h>
#include <recorder/recording_manager.h>
#include <recorder/schedule_sync.h>
#include <appserver/processor.h>

#include <platform/monitoring/global_monitor.h>
#include <platform/platform_abstraction.h>
#include <plugins/resource/server_archive/dualquality_helper.h>
#include <plugins/resource/archive_camera/archive_camera.h>

#include "plugins/storage/file_storage/file_storage_resource.h"

#include <media_server/serverutil.h>
#include "file_deletor.h"
#include "utils/common/synctime.h"
#include "motion/motion_helper.h"
#include "common/common_module.h"
#include "media_server/settings.h"
#include "nx/fusion/serialization/lexical_enum.h"

#include <memory>
#include <thread>
#include <chrono>
#include <algorithm>
#include <array>
#include <sstream>
#include <map>
#include "database/server_db.h"
#include "common/common_globals.h"

//static const qint64 BALANCE_BY_FREE_SPACE_THRESHOLD = 1024*1024 * 500;
//static const int OFFLINE_STORAGES_TEST_INTERVAL = 1000 * 30;
//static const int DB_UPDATE_PER_RECORDS = 128;
namespace
{
static const qint64 MSECS_PER_DAY = 1000ll * 3600ll * 24ll;
static const qint64 MOTION_CLEANUP_INTERVAL = 1000ll * 3600;
static const qint64 BOOKMARK_CLEANUP_INTERVAL = 1000ll * 60;

const qint64 kMinStorageFreeSpace = 150 * 1024 * 1024LL;

static const QString SCAN_ARCHIVE_FROM(lit("SCAN_ARCHIVE_FROM"));

const QString SCAN_ARCHIVE_NORMAL_PREFIX = lit("NORMAL_");
const QString SCAN_ARCHIVE_BACKUP_PREFIX = lit("BACKUP_");

const QString kArchiveCameraUrlKey = lit("cameraUrl");
const QString kArchiveCameraNameKey = lit("cameraName");
const QString kArchiveCameraModelKey = lit("cameraModel");
const QString kArchiveCameraGroupIdKey = lit("groupId");
const QString kArchiveCameraGroupNameKey = lit("groupName");

const std::chrono::minutes kWriteInfoFilesInterval(5);

struct TasksQueueInfo {
    int tasksCount;
    int currentTask;

    TasksQueueInfo() : tasksCount(0), currentTask(0) {}
    void reset(int size = 0) {tasksCount = size; currentTask = 0;}
    bool isEmpty() const { return tasksCount == 0; }
};

const QString dbRefFileName( QLatin1String("%1_db_ref.guid") );

bool getSqlDbPath(const QnStorageResourcePtr &storage, QString &dbFolderPath)
{
    QString storageUrl = storage->getUrl();
    QString dbRefFilePath;

    dbRefFilePath = closeDirPath(storageUrl) + dbRefFileName.arg(QnStorageDbPool::getLocalGuid());
    QByteArray dbRefGuidStr;

    //checking for file db_ref.guid existence
    if (storage->isFileExists(dbRefFilePath))
    {
        //have to use db from data directory, not from storage
        //reading guid from file
        auto dbGuidFile = std::unique_ptr<QIODevice>(storage->open(dbRefFilePath, QIODevice::ReadOnly));

        if (!dbGuidFile)
            return false;
        dbRefGuidStr = dbGuidFile->readAll();
        //dbGuidFile->close();
    }

    if( !dbRefGuidStr.isEmpty() )
    {
        dbFolderPath = QDir(getDataDirectory() + "/storage_db/" + dbRefGuidStr).absolutePath();
        return true;
    }
    else if (storage->getCapabilities() & QnAbstractStorageResource::DBReady)
    {
        dbFolderPath = storage->getPath();
        return true;
    }
    return false;
}

} // namespace <anonymous>
class ArchiveScanPosition
{
public:
    ArchiveScanPosition(QnServer::StoragePool role)
        : m_role(role),
          m_catalog(QnServer::LowQualityCatalog)
    {}

    ArchiveScanPosition(QnServer::StoragePool       role,
                        const QnStorageResourcePtr  &storage,
                        QnServer::ChunksCatalog     catalog,
                        const QString               &cameraUniqueId)
        : m_role(role),
          m_storagePath(storage->getUrl()),
          m_catalog(catalog),
          m_cameraUniqueId(cameraUniqueId)
    {}

    void save() {
        QString serializedData(lit("%1;;%2;;%3"));
        serializedData = serializedData.arg(m_storagePath)
                                       .arg(QnLexical::serialized(m_catalog))
                                       .arg(m_cameraUniqueId);
        MSSettings::roSettings()->setValue(settingName(m_role), serializedData);
        MSSettings::roSettings()->sync();
    }

    void load() {
        QString serializedData = MSSettings::roSettings()->value(settingName(m_role)).toString();
        QStringList data = serializedData.split(";;");
        if (data.size() == 3) {
            m_storagePath = data[0];
            QnLexical::deserialize(data[1], &m_catalog);
            m_cameraUniqueId = data[2];
        }
    }

    static QString settingName(QnServer::StoragePool role)
    {
        return role == QnServer::StoragePool::Normal ?
                       SCAN_ARCHIVE_NORMAL_PREFIX + SCAN_ARCHIVE_FROM :
                       SCAN_ARCHIVE_BACKUP_PREFIX + SCAN_ARCHIVE_FROM;
    }

    static void reset(QnServer::StoragePool role) {
        MSSettings::roSettings()->setValue(settingName(role), QString());
        MSSettings::roSettings()->sync();
    }

    bool isEmpty() const { return m_cameraUniqueId.isEmpty(); }
    bool operator<(const ArchiveScanPosition& other) {
        if (m_storagePath != other.m_storagePath)
            return m_storagePath < other.m_storagePath;
        if (m_catalog != other.m_catalog)
            return m_catalog < other.m_catalog;
        return m_cameraUniqueId < other.m_cameraUniqueId;
    }
private:
    const QnServer::StoragePool m_role;
    QString                     m_storagePath;
    QnServer::ChunksCatalog     m_catalog;
    QString                     m_cameraUniqueId;
};


class ScanMediaFilesTask: public QnLongRunnable
{
private:
    typedef QnLongRunnable base_type;
    struct ScanData
    {
        ScanData(): partialScan(false) {}
        ScanData(const QnStorageResourcePtr& storage, bool partialScan): storage(storage), partialScan(partialScan) {}
        bool operator== (const ScanData& other) const { return storage == other.storage && partialScan == other.partialScan; }

        QnStorageResourcePtr storage;
        bool partialScan;
    };
    QnStorageManager* m_owner;
    QQueue<ScanData> m_scanTasks;
    mutable QnMutex m_mutex;
    QnWaitCondition m_waitCond;
    bool m_fullScanCanceled;

public:
    ScanMediaFilesTask(QnStorageManager* owner): QnLongRunnable(), m_owner(owner), m_fullScanCanceled(false)
    {
    }

    virtual ~ScanMediaFilesTask()
    {
        stop();
    }

    bool hasFullScanTasks() const
    {
        QnMutexLocker lock(&m_mutex);
        for (const auto& task: m_scanTasks) {
            if (!task.partialScan)
                return true;
        }
        return false;
    }

    void addStorageToScanUnsafe(const QnStorageResourcePtr& storage, bool partialScan)
    {
        ScanData scanData(storage, partialScan);
        if (m_scanTasks.contains(scanData))
            return;
        if (m_scanTasks.isEmpty())
            m_owner->setRebuildInfo(QnStorageScanData(partialScan ? Qn::RebuildState_PartialScan : Qn::RebuildState_FullScan, storage->getUrl(), 0.0, 0.0));
        if (partialScan)
            storage->addFlags(Qn::storage_fastscan);
        m_scanTasks.push_back(std::move(scanData));
    }

    void addStorageToScan(const QnStorageResourcePtr& storage, bool partialScan)
    {
        QnMutexLocker lock(&m_mutex);
        addStorageToScanUnsafe(storage, partialScan);
        m_waitCond.wakeAll();
    }

    void addStoragesToScan(const QVector<QnStorageResourcePtr>& storages, bool partialScan)
    {
        QnMutexLocker lock(&m_mutex);
        for (auto storage: storages)
            addStorageToScanUnsafe(storage, partialScan);
        m_waitCond.wakeAll();
    }

    void cancelFullScanTasks()
    {
        m_fullScanCanceled = true;
    }

    virtual void pleaseStop() override
    {
        base_type::pleaseStop();
        m_waitCond.wakeAll();
    }

    virtual void run() override
    {
        bool fullscanProcessed = false;
        bool partialScanProcessed = false;
        nx::caminfo::ArchiveCameraDataList archiveCameras;

        TasksQueueInfo currentQueueInfo;

        while (!needToStop())
        {
            if (m_fullScanCanceled)
            {
                {
                    QnMutexLocker lock(&m_mutex);
                    m_scanTasks.erase(std::remove_if(m_scanTasks.begin(), m_scanTasks.end(), [](const ScanData& data) { return !data.partialScan; }), m_scanTasks.end());
                    currentQueueInfo.reset();
                    ArchiveScanPosition::reset(m_owner->m_role);
                }
                m_fullScanCanceled = false;
                fullscanProcessed = false;
                m_owner->setRebuildInfo(QnStorageScanData(Qn::RebuildState_None, QString(), 0.0, 0.0));
                emit m_owner->rebuildFinished(QnSystemHealth::ArchiveRebuildCanceled);
            }

            ScanData scanData;
            {
                QnMutexLocker lock(&m_mutex);
                while (m_scanTasks.isEmpty() && !needToStop()) {
                    m_waitCond.wait(&m_mutex);
                }
                if (needToStop())
                    break;

                /* Updating tasks queue only when current queue became empty. */
                if (currentQueueInfo.isEmpty())
                    currentQueueInfo.reset(m_scanTasks.size());

                scanData = m_scanTasks.front();
            }

            const qreal totalProgressStep = 1.0 / (qreal) currentQueueInfo.tasksCount;
            const qreal totalProgressValue = currentQueueInfo.currentTask / (qreal) currentQueueInfo.tasksCount;
            const qreal nextTotalProgressValue = std::max(1.0, (currentQueueInfo.currentTask + 1) / (qreal) currentQueueInfo.tasksCount);

            if (scanData.partialScan)
            {
                partialScanProcessed = true;
                QMap<DeviceFileCatalogPtr, qint64> catalogToScan; // key - catalog, value - start scan time;
                {
                    int storageIndex = qnStorageDbPool->getStorageIndex(scanData.storage);
                    QnMutexLocker lock(&m_owner->m_mutexCatalog);
                    for(const DeviceFileCatalogPtr& catalog: m_owner->m_devFileCatalog[QnServer::LowQualityCatalog])
                        catalogToScan.insert(catalog, catalog->lastChunkStartTime(storageIndex));
                    for(const DeviceFileCatalogPtr& catalog: m_owner->m_devFileCatalog[QnServer::HiQualityCatalog])
                        catalogToScan.insert(catalog, catalog->lastChunkStartTime(storageIndex));
                }
                int totalStorageStep = catalogToScan.size();
                int currentStorageStep = 0;
                for(auto itr = catalogToScan.begin(); itr != catalogToScan.end(); ++itr)
                {
                    const qreal storageProgress = currentStorageStep / (qreal) totalStorageStep;
                    const qreal totalProgress = totalProgressValue + storageProgress * totalProgressStep;

                    m_owner->setRebuildInfo(QnStorageScanData(Qn::RebuildState_PartialScan, scanData.storage->getUrl(), storageProgress, totalProgress));
                    DeviceFileCatalog::ScanFilter filter;
                    filter.scanPeriod.startTimeMs = itr.value();
                    qint64 endScanTime = qnSyncTime->currentMSecsSinceEpoch();
                    qint64 scanPeriodDuration = qMax(1ll, endScanTime - filter.scanPeriod.startTimeMs);
                    NX_LOG(lit("[Scan]: Partial scan period duration for storage %1, catalog %2 = %3 ms (%4 hrs)")
                            .arg(scanData.storage->getUrl())
                            .arg(itr.key()->cameraUniqueId())
                            .arg(scanPeriodDuration)
                            .arg(scanPeriodDuration / (1000 * 60 * 60)), cl_logDEBUG2);
                    filter.scanPeriod.durationMs = scanPeriodDuration;
                    m_owner->partialMediaScan(itr.key(), scanData.storage, filter);
                    if (needToStop() || QnResource::isStopping())
                        return;
                    ++currentStorageStep;
                }
                m_owner->setRebuildInfo(QnStorageScanData(Qn::RebuildState_PartialScan, scanData.storage->getUrl(), 1.0, nextTotalProgressValue));
                scanData.storage->removeFlags(Qn::storage_fastscan);
                NX_LOG(lit("[Scan]: Partial scan for storage %1 has been finished").arg(scanData.storage->getUrl()), cl_logDEBUG2);
            }
            else
            {
                fullscanProcessed = true;

                auto genProgressCallback = [this, totalProgressValue, totalProgressStep, &archiveCameras](const QString &url, qreal offset)
                  -> std::function<void(int, int)>
                {
                    return [this, url, totalProgressValue, totalProgressStep, offset](int current, int total) {
                        if (total <= 0)
                        {   /* Lets think we have already done this =) */
                            total = 1;
                            current = 1;
                        }

                        /* Dividing storage progress by two as there are two catalogs rebuilding. */
                        const qreal storageProgress = offset + (0.5 * current / (qreal) total);
                        const qreal totalProgress = totalProgressValue + storageProgress * totalProgressStep;
                        m_owner->setRebuildInfo(QnStorageScanData(Qn::RebuildState_FullScan, url, storageProgress, totalProgress));
                    };
                };

                m_owner->setRebuildInfo(QnStorageScanData(Qn::RebuildState_FullScan, scanData.storage->getUrl(), 0.0, totalProgressValue));
                m_owner->loadFullFileCatalogFromMedia(scanData.storage, QnServer::LowQualityCatalog, archiveCameras, genProgressCallback(scanData.storage->getUrl(), 0.0));
                m_owner->loadFullFileCatalogFromMedia(scanData.storage, QnServer::HiQualityCatalog, archiveCameras, genProgressCallback(scanData.storage->getUrl(), 0.5));
                m_owner->setRebuildInfo(QnStorageScanData(Qn::RebuildState_FullScan, scanData.storage->getUrl(), 1.0, nextTotalProgressValue));
            }

            {
                QnMutexLocker lock(&m_mutex);
                m_scanTasks.pop_front();

                /* Check if queue info is finished and invalidate if needed. */
                currentQueueInfo.currentTask++;
                if (currentQueueInfo.currentTask >= currentQueueInfo.tasksCount)
                    currentQueueInfo.reset();

                if (m_scanTasks.isEmpty())
                {
                    // not data to process left
                    m_owner->updateCameraHistory();
                    m_owner->setRebuildInfo(QnStorageScanData(Qn::RebuildState_None, QString(), 0.0, 0.0));

                    if (fullscanProcessed)
                    {
                        if (!QnResource::isStopping())
                            ArchiveScanPosition::reset(m_owner->m_role); // do not reset position if server is going to restart

                        m_owner->createArchiveCameras(archiveCameras);
                        archiveCameras.clear();

                        if (!m_fullScanCanceled)
                            emit m_owner->rebuildFinished(QnSystemHealth::ArchiveRebuildFinished);
                    }
                    else if (partialScanProcessed)
                        emit m_owner->rebuildFinished(QnSystemHealth::ArchiveFastScanFinished);

                    fullscanProcessed = false;
                    partialScanProcessed = false;
                }
            }

            NX_ASSERT(qnBackupStorageMan->scheduleSync());
            qnBackupStorageMan->scheduleSync()->updateLastSyncChunk();
        }
    }
};

class TestStorageThread: public QnLongRunnable
{
public:
    TestStorageThread(QnStorageManager* owner): m_owner(owner) {}
    virtual void run() override
    {
        for (const auto& storage : m_owner->getStorages())
        {
            if (needToStop())
                return;

            Qn::ResourceStatus status = storage->initOrUpdate() == Qn::StorageInit_Ok ? Qn::Online : Qn::Offline;
            if (storage->getStatus() != status)
                m_owner->changeStorageStatus(storage, status);

            if (status == Qn::Online)
            {
                const auto space = QString::number(storage->getTotalSpace());
                if (storage->setProperty(Qn::SPACE, space))
                    propertyDictionary->saveParams(storage->getId());
            }
        }

        m_owner->testStoragesDone();
    }

private:
    QnStorageManager* m_owner;
};

// -------------------- QnStorageManager --------------------


static QnStorageManager* QnNormalStorageManager_instance = nullptr;
static QnStorageManager* QnBackupStorageManager_instance = nullptr;

QnStorageManager::QnStorageManager(QnServer::StoragePool role):
    m_role(role),
    m_mutexStorages(QnMutex::Recursive),
    m_mutexCatalog(QnMutex::Recursive),
    m_warnSended(false),
    m_isWritableStorageAvail(false),
    m_rebuildCancelled(false),
    m_rebuildArchiveThread(0),
    m_firstStoragesTestDone(false),
    m_isRenameDisabled(MSSettings::roSettings()->value("disableRename").toInt()),
    m_camInfoWriterHandler(this),
    m_camInfoWriter(&m_camInfoWriterHandler)
{
    m_storageDbPoolRef = qnStorageDbPool->create();

    NX_ASSERT(m_role == QnServer::StoragePool::Normal || m_role == QnServer::StoragePool::Backup);
    m_storageWarnTimer.restart();
    m_testStorageThread = new TestStorageThread(this);

    if (m_role == QnServer::StoragePool::Normal)
    {
        NX_ASSERT( QnNormalStorageManager_instance == nullptr );
        QnNormalStorageManager_instance = this;
    }
    else if (m_role == QnServer::StoragePool::Backup)
    {
        NX_ASSERT( QnBackupStorageManager_instance == nullptr );
        QnBackupStorageManager_instance = this;
    }

    m_oldStorageIndexes = deserializeStorageFile();

    connect(qnResPool, &QnResourcePool::resourceAdded, this, &QnStorageManager::onNewResource, Qt::QueuedConnection);
    connect(qnResPool, &QnResourcePool::resourceRemoved, this, &QnStorageManager::onDelResource, Qt::QueuedConnection);

	connect(this, &QnStorageManager::rebuildFinished,
        [this] (QnSystemHealth::MessageType message)
        {
    		if (message == QnSystemHealth::ArchiveFastScanFinished ||
                message == QnSystemHealth::ArchiveRebuildFinished)
            {
                for (const auto& storage: getUsedWritableStorages())
                {
                    if (!storage->hasFlags(Qn::storage_fastscan))
                    {
                        auto storageIndex = qnStorageDbPool->getStorageIndex(storage);
                        m_spaceInfo.storageRebuilded(storageIndex, storage->getFreeSpace(),
                            calculateNxOccupiedSpace(storageIndex), storage->getSpaceLimit());
                    }
                }
            }
    	});

    if (m_role == QnServer::StoragePool::Backup) {
        m_scheduleSync.reset(new QnScheduleSync());
        connect(m_scheduleSync.get(), &QnScheduleSync::backupFinished, this, &QnStorageManager::backupFinished, Qt::DirectConnection);
    }


    m_rebuildArchiveThread = new ScanMediaFilesTask(this);
    m_rebuildArchiveThread->start();

    m_clearMotionTimer.restart();
    m_clearBookmarksTimer.restart();
    m_removeEmtyDirTimer.invalidate();

    startAuxTimerTasks();
}

int64_t QnStorageManager::calculateNxOccupiedSpace(int storageIndex) const
{
    auto calculateNxOccupiedSpaceByQuality = [this](int storageIndex, QnServer::ChunksCatalog catalog)
    {
        int64_t result = 0;
        for (auto it = m_devFileCatalog[catalog].cbegin(); it != m_devFileCatalog[catalog].cend(); ++it)
            result += it.value()->getSpaceByStorageIndex(storageIndex);

        return result;
    };

    QnMutexLocker lock(&m_mutexCatalog);
    return calculateNxOccupiedSpaceByQuality(storageIndex, QnServer::HiQualityCatalog) +
        calculateNxOccupiedSpaceByQuality(storageIndex, QnServer::LowQualityCatalog);
}

void QnStorageManager::createArchiveCameras(const nx::caminfo::ArchiveCameraDataList& archiveCameras)
{
    nx::caminfo::ArchiveCameraDataList camerasToAdd;
    for (const auto &camera : archiveCameras)
    {
        auto cameraLowCatalog = getFileCatalog(camera.coreData.physicalId, QnServer::LowQualityCatalog);
        auto cameraHiCatalog = getFileCatalog(camera.coreData.physicalId, QnServer::HiQualityCatalog);

        bool doAdd = (cameraLowCatalog && !cameraLowCatalog->isEmpty()) || (cameraHiCatalog && !cameraHiCatalog->isEmpty());
        if (doAdd)
            camerasToAdd.push_back(camera);

        if (QnLog::logs() && QnLog::logs()->get()->logLevel() >= cl_logDEBUG2)
        {
            QString logMessage;
            QTextStream logStream(&logMessage);

            logStream << lit("camera info: Camera found %1").arg(camera.coreData.physicalId) << endl;
            for (const auto& prop: camera.properties)
                logStream << "\t" << prop.name << " : " << prop.value << endl << endl;

            NX_LOG(logMessage, cl_logDEBUG2);
        }
    }

    for (const auto &camera : camerasToAdd)
    {
        ec2::ErrorCode errCode =
            QnAppserverResourceProcessor::addAndPropagateCamResource(
                camera.coreData, camera.properties
            );
    }

    updateCameraHistory();
}

void QnStorageManager::partialMediaScan(const DeviceFileCatalogPtr &fileCatalog, const QnStorageResourcePtr &storage, const DeviceFileCatalog::ScanFilter& filter)
{
    QnServer::ChunksCatalog catalog = fileCatalog->getCatalog();

    /* Check new records, absent in the DB */
    QVector<DeviceFileCatalog::EmptyFileInfo> emptyFileList;
    QString rootDir = fileCatalog->rootFolder(storage, catalog);

    QMap<qint64, DeviceFileCatalog::Chunk> newChunksMap;
    fileCatalog->scanMediaFiles(rootDir, storage, newChunksMap, emptyFileList, filter);
    std::deque<DeviceFileCatalog::Chunk> newChunks; // = newChunksMap.values().toVector();
    for(auto itr = newChunksMap.begin(); itr != newChunksMap.end(); ++itr)
        newChunks.push_back(itr.value());

    for(const DeviceFileCatalog::EmptyFileInfo& emptyFile: emptyFileList)
        storage->removeFile(emptyFile.fileName);

    // add to DB
    QnStorageDbPtr sdb = qnStorageDbPool->getSDB(storage);
    QString cameraUniqueId = fileCatalog->cameraUniqueId();
    for(const DeviceFileCatalog::Chunk& chunk: newChunks) {
        if (QnResource::isStopping())
            break;
        if (sdb)
            sdb->addRecord(cameraUniqueId, catalog, chunk);
    }
    if (sdb)
        sdb->flushRecords();
    // merge chunks
    {
        QnMutexLocker lk(&m_mutexStorages);
        bool stillHaveThisStorage = hasStorageUnsafe(storage);
        if (!stillHaveThisStorage)
            return;
    }
    fileCatalog->addChunks(newChunks);
}

void QnStorageManager::initDone()
{
    QTimer::singleShot(0, this, SLOT(testOfflineStorages()));
}

QMap<QString, QSet<int>> QnStorageManager::deserializeStorageFile()
{
    QMap<QString, QSet<int>> storageIndexes;

    QString path = closeDirPath(getDataDirectory());
    QString separator = getPathSeparator(path);
    QFile storageFile(path + QString("record_catalog%1media%2storage_index.csv").arg(separator).arg(separator));
    if (!storageFile.exists())
        return storageIndexes;
    if (!storageFile.open(QFile::ReadOnly))
        return storageIndexes;
    // deserialize storage file
    QString line = storageFile.readLine(); // skip csv header
    do {
        line = storageFile.readLine();
        QStringList params = line.split(';');
        if (params.size() >= 2) {
            QString path = toCanonicalPath(params[0]);
            for (int i = 1; i < params.size(); ++i) {
                int index = params[i].toInt();
                storageIndexes[path].insert(index);
            }
        }
    } while (!line.isEmpty());
    storageFile.close();
    return storageIndexes;
}

void QnStorageManager::migrateSqliteDatabase(const QnStorageResourcePtr & storage)
{
    QString dbPath;
    if (!getSqlDbPath(storage, dbPath))
        return;

    QString simplifiedGUID = QnStorageDbPool::getLocalGuid();
    QString oldFileName = closeDirPath(dbPath) + QString::fromLatin1("media.sqlite");
    QString fileName =
        closeDirPath(dbPath) +
        QString::fromLatin1("%1_media.sqlite").arg(simplifiedGUID);

    if (!QFile::exists(fileName))
    {
        if (QFile::exists(oldFileName))
        {
            if (!QFile::rename(oldFileName, fileName))
                return;
        }
        else
            return;
    }

    QSqlDatabase sqlDb = QSqlDatabase::addDatabase(
        lit("QSQLITE"),
        QString("QnStorageManager_%1").arg(fileName));

    sqlDb.setDatabaseName(fileName);
    if (!sqlDb.open())
    {
        NX_LOG(
            lit("%1 : Migration from sqlite DB failed. Can't open database file %2")
                .arg(Q_FUNC_INFO)
                .arg(fileName),
            cl_logWARNING);
        return;
    }
    int storageIndex = qnStorageDbPool->getStorageIndex(storage);
    QVector<DeviceFileCatalogPtr> oldCatalogs;

    QSqlQuery query(sqlDb);
    query.setForwardOnly(true);
    query.prepare("SELECT * FROM storage_data WHERE role <= :max_role ORDER BY unique_id, role, start_time");
    query.bindValue(":max_role", (int)QnServer::HiQualityCatalog);

    if (!query.exec())
    {
        NX_LOG(
            lit("%1 : Migration from sqlite DB failed. Select query exec failed")
                .arg(Q_FUNC_INFO),
            cl_logWARNING);
        return;
    }
    QSqlRecord queryInfo = query.record();
    int idFieldIdx = queryInfo.indexOf("unique_id");
    int roleFieldIdx = queryInfo.indexOf("role");
    int startTimeFieldIdx = queryInfo.indexOf("start_time");
    int fileNumFieldIdx = queryInfo.indexOf("file_index");
    int timezoneFieldIdx = queryInfo.indexOf("timezone");
    int durationFieldIdx = queryInfo.indexOf("duration");
    int filesizeFieldIdx = queryInfo.indexOf("filesize");

    DeviceFileCatalogPtr fileCatalog;
    std::deque<DeviceFileCatalog::Chunk> chunks;
    QnServer::ChunksCatalog prevCatalog = QnServer::ChunksCatalogCount; //should differ from all existing catalogs
    QByteArray prevId;

    while (query.next())
    {
        QByteArray id = query.value(idFieldIdx).toByteArray();
        QnServer::ChunksCatalog catalog = (QnServer::ChunksCatalog) query.value(roleFieldIdx).toInt();
        if (id != prevId || catalog != prevCatalog)
        {
            if (fileCatalog)
            {
                fileCatalog->addChunks(chunks);
                oldCatalogs << fileCatalog;
                chunks.clear();
            }

            prevCatalog = catalog;
            prevId = id;
            fileCatalog = DeviceFileCatalogPtr(
                new DeviceFileCatalog(
                    QString::fromUtf8(id),
                    catalog,
                    QnServer::StoragePool::None));
        }
        qint64 startTime = query.value(startTimeFieldIdx).toLongLong();
        qint64 filesize = query.value(filesizeFieldIdx).toLongLong();
        int timezone = query.value(timezoneFieldIdx).toInt();
        int fileNum = query.value(fileNumFieldIdx).toInt();
        int durationMs = query.value(durationFieldIdx).toInt();
        chunks.push_back(
            DeviceFileCatalog::Chunk(
                startTime,
                storageIndex,
                fileNum,
                durationMs,
                (qint16)timezone,
                (quint16)(filesize >> 32),
                (quint32)filesize));
    }
    if (fileCatalog)
    {
        fileCatalog->addChunks(chunks);
        oldCatalogs << fileCatalog;
    }

    auto connectionName = sqlDb.connectionName();
    sqlDb.close();
    sqlDb = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);

	QString depracatedFileName = fileName + lit("_deprecated");
	if (!QFile::remove(depracatedFileName))
		NX_LOG(lit("%1 Deprecated db file %2 found but remove failed. Remove it manually and restart server")
			.arg(Q_FUNC_INFO)
			.arg(depracatedFileName), cl_logWARNING);

    if (!QFile::rename(fileName, depracatedFileName))
		NX_LOG(lit("%1 Rename failed for deprecated db file %2. Rename (remove) it manually and restart server")
			.arg(Q_FUNC_INFO)
			.arg(fileName), cl_logWARNING);


    auto sdb = qnStorageDbPool->getSDB(storage);
    if (!sdb)
      return;

    auto newCatalogs = sdb->loadFullFileCatalog();
    QVector<DeviceFileCatalogPtr> catalogsToWrite;

    for (auto const &c : oldCatalogs)
    {
        auto newCatalogIt = std::find_if(
            newCatalogs.begin(),
            newCatalogs.end(),
            [&c](const DeviceFileCatalogPtr &catalog)
            {
                return c->cameraUniqueId() == catalog->cameraUniqueId() &&
                       c->getCatalog() == catalog->getCatalog();
            });

        if (newCatalogIt == newCatalogs.end())
            catalogsToWrite.push_back(c);
        else
        {
            DeviceFileCatalogPtr newCatalog = *newCatalogIt;
            DeviceFileCatalogPtr catalogToWrite = DeviceFileCatalogPtr(
                new DeviceFileCatalog(
                    c->cameraUniqueId(),
                    c->getCatalog(),
                    QnServer::StoragePool::None));

            // It is safe to use getChunksUnsafe method here
            // because there is no concurrent access to these
            // catalogs yet.
            NX_ASSERT(
                std::is_sorted(
                    c->getChunksUnsafe().cbegin(),
                    c->getChunksUnsafe().cend()));
            NX_ASSERT(
                std::is_sorted(
                    newCatalog->getChunksUnsafe().cbegin(),
                    newCatalog->getChunksUnsafe().cend()));

            std::set_difference(
                c->getChunksUnsafe().begin(),
                c->getChunksUnsafe().end(),
                newCatalog->getChunksUnsafe().begin(),
                newCatalog->getChunksUnsafe().end(),
                std::back_inserter(catalogToWrite->getChunksUnsafe()));

            catalogsToWrite.push_back(catalogToWrite);
        }
    }

    for (auto const &c : catalogsToWrite)
    {
        for (auto const &chunk : c->getChunksUnsafe())
            sdb->addRecord(c->cameraUniqueId(), c->getCatalog(), chunk);
    }
}

void QnStorageManager::addDataFromDatabase(const QnStorageResourcePtr &storage)
{
    QnStorageDbPtr sdb = qnStorageDbPool->getSDB(storage);
    if (!sdb)
        return;

    // load from database
    for(const DeviceFileCatalogPtr& c: sdb->loadFullFileCatalog())
    {
        DeviceFileCatalogPtr fileCatalog = getFileCatalogInternal(c->cameraUniqueId(), c->getCatalog());
        fileCatalog->addChunks(c->m_chunks);
        //fileCatalog->addChunks(correctChunksFromMediaData(fileCatalog, storage, c->m_chunks));
    }
}

QnStorageScanData QnStorageManager::rebuildInfo() const
{
    QnMutexLocker lock( &m_rebuildStateMtx );
    return m_archiveRebuildInfo;
}

QnStorageScanData QnStorageManager::rebuildCatalogAsync()
{
    QnStorageScanData result = rebuildInfo();
    if (!m_rebuildArchiveThread->hasFullScanTasks())
    {
        QnMutexLocker lock( &m_mutexRebuild );
        m_rebuildCancelled = false;
        QVector<QnStorageResourcePtr> storagesToScan;
        for(const QnStorageResourcePtr& storage: getStoragesInLexicalOrder()) {
            if (storage->getStatus() == Qn::Online)
                storagesToScan << storage;
        }

        if (QnLog::logs() && QnLog::logs()->get()->logLevel() >= cl_logDEBUG1)
        {
            QString logString;
            QTextStream logStream(&logString);

            logStream << Q_FUNC_INFO << " rebuildCatalogAsync triggered\n";
            if (storagesToScan.isEmpty())
                logStream << "\tNo online storages found";
            else
                logStream << "\tFollowing storages found:\n";

            for (const auto& s: storagesToScan)
                logStream << "\t" << s->getUrl() << "\n";

            NX_LOG(logString, cl_logDEBUG1);
        }

        if (storagesToScan.isEmpty())
            return result;
        if (result.state <= Qn::RebuildState_None)
            result = QnStorageScanData(Qn::RebuildState_FullScan, storagesToScan.first()->getUrl(), 0.0, 0.0);
        m_rebuildArchiveThread->addStoragesToScan(storagesToScan, false);
    }

    return result;
}

void QnStorageManager::cancelRebuildCatalogAsync()
{
    QnMutexLocker lock( &m_mutexRebuild );
    m_rebuildCancelled = true;
    m_rebuildArchiveThread->cancelFullScanTasks();
    NX_LOG("Catalog rebuild operation is canceled", cl_logINFO);
}

bool QnStorageManager::needToStopMediaScan() const
{
    QnMutexLocker lock( &m_mutexRebuild );
    return m_rebuildCancelled && m_archiveRebuildInfo.state == Qn::RebuildState_FullScan;
}

void QnStorageManager::setRebuildInfo(const QnStorageScanData& data)
{
    NX_ASSERT(data.totalProgress < 1.01, Q_FUNC_INFO, "invalid progress");
    QnMutexLocker lock( &m_rebuildStateMtx );
    m_archiveRebuildInfo = data;
}

QStringList QnStorageManager::getAllCameraIdsUnderLock(QnServer::ChunksCatalog catalog) const
{
    QStringList result;
    QnMutexLocker lock(&m_mutexCatalog);
    for (auto it = m_devFileCatalog[catalog].cbegin(); it != m_devFileCatalog[catalog].cend(); ++it)
        result.push_back(it.key());

    return result;
}

void QnStorageManager::loadFullFileCatalogFromMedia(const QnStorageResourcePtr &storage, QnServer::ChunksCatalog catalog,
                                                    nx::caminfo::ArchiveCameraDataList &archiveCameraList, std::function<void(int current, int total)> progressCallback)
{
    ArchiveScanPosition scanPos(m_role);
    scanPos.load(); // load from persistent storage

    QnAbstractStorageResource::FileInfoList list =
        storage->getFileList(
            closeDirPath(
                storage->getUrl()
            ) + DeviceFileCatalog::prefixByCatalog(catalog)
        );

    auto camerasIds = getAllCameraIdsUnderLock(catalog);

    for (const auto& cameraId: camerasIds)
    {
        QString cameraDir =
            closeDirPath(
                closeDirPath(storage->getUrl()) +
                DeviceFileCatalog::prefixByCatalog(catalog)
            ) + cameraId;

        if (!storage->isDirExists(cameraDir))
        {
            DeviceFileCatalogPtr newCatalog(
                new DeviceFileCatalog(
                    cameraId,
                    catalog,
                    m_role
                )
            );

            replaceChunks(
                QnTimePeriod(0, qnSyncTime->currentMSecsSinceEpoch()),
                storage,
                newCatalog,
                cameraId,
                catalog
            );
        }
    }

    int totalTasks = list.size();
    int currentTask = 0;
    if (progressCallback)
        progressCallback(currentTask, totalTasks);

    for(const QnAbstractStorageResource::FileInfo& fi: list)
    {
        if (m_rebuildCancelled)
            return; // cancel rebuild

        auto getFileDataFunc= [&storage](const QString& filePath)
            {
                auto file = std::unique_ptr<QIODevice>(storage->open(filePath, QIODevice::ReadOnly));
                if (!file)
                   return QByteArray();
                return file->readAll();
            };

        nx::caminfo::ServerReaderHandler readerHandler;
        nx::caminfo::Reader(&readerHandler, fi, getFileDataFunc)(&archiveCameraList);

        QString cameraUniqueId = fi.fileName();
        ArchiveScanPosition currentPos(m_role, storage, catalog, cameraUniqueId);
        if (currentPos < scanPos) {
            // already scanned
        }
        else {
            currentPos.save(); // save to persistent storage
            qint64 rebuildEndTime = qnSyncTime->currentMSecsSinceEpoch() - QnRecordingManager::RECORDING_CHUNK_LEN * 1250;
            DeviceFileCatalogPtr newCatalog(new DeviceFileCatalog(cameraUniqueId, catalog, m_role));
            QnTimePeriod rebuildPeriod = QnTimePeriod(0, rebuildEndTime);
            newCatalog->doRebuildArchive(storage, rebuildPeriod);

            bool stillHaveThisStorage;
            {
                QnMutexLocker lk(&m_mutexStorages);
                stillHaveThisStorage = hasStorageUnsafe(storage);
            }
                if (!m_rebuildCancelled && stillHaveThisStorage)
                    replaceChunks(rebuildPeriod, storage, newCatalog, cameraUniqueId, catalog);
        }
        currentTask++;
        if (progressCallback && !m_rebuildCancelled)
            progressCallback(currentTask, totalTasks);
    }
}

QString QnStorageManager::toCanonicalPath(const QString& path)
{
    QString result = QDir::toNativeSeparators(path);
    if (result.endsWith(QDir::separator()))
        result.chop(1);
    return result;
}

void QnStorageManager::addStorage(const QnStorageResourcePtr &storage)
{
    {
        int storageIndex = qnStorageDbPool->getStorageIndex(storage);
        NX_LOG(QString("Adding storage. Path: %1").arg(storage->getUrl()), cl_logINFO);

        removeStorage(storage); // remove existing storage record if exists
        storage->setStatus(Qn::Offline); // we will check status after
        {
            QnMutexLocker lk(&m_mutexStorages);
            m_storageRoots.insert(storageIndex, storage);
        }
        connect(storage.data(), SIGNAL(archiveRangeChanged(const QnStorageResourcePtr &, qint64, qint64)),
                this, SLOT(at_archiveRangeChanged(const QnStorageResourcePtr &, qint64, qint64)), Qt::DirectConnection);
    }
}

bool QnStorageManager::checkIfMyStorage(const QnStorageResourcePtr &storage) const
{
    if ((m_role == QnServer::StoragePool::Backup && !storage->isBackup()) ||
        (m_role == QnServer::StoragePool::Normal && storage->isBackup()))
    {
        return false;
    }
    return true;
}

void QnStorageManager::onNewResource(const QnResourcePtr &resource)
{
    QnStorageResourcePtr storage = qSharedPointerDynamicCast<QnStorageResource>(resource);
    if (storage && storage->getParentId() == qnCommon->moduleGUID())
    {
		m_warnSended = false;
        connect(storage.data(), &QnStorageResource::isBackupChanged, this, &QnStorageManager::at_storageChanged);
        if (checkIfMyStorage(storage))
            addStorage(storage);
    }
}

void QnStorageManager::onDelResource(const QnResourcePtr &resource)
{
    QnStorageResourcePtr storage = qSharedPointerDynamicCast<QnStorageResource>(resource);
    if (storage && storage->getParentId() == qnCommon->moduleGUID() && checkIfMyStorage(storage)) {
		m_warnSended = false;
        removeStorage(storage);
        qnStorageDbPool->removeSDB(storage);
    }
}

bool QnStorageManager::hasStorageUnsafe(const QnStorageResourcePtr &storage) const
{
    for (auto itr = m_storageRoots.begin(); itr != m_storageRoots.end(); ++itr)
    {
        if (itr.value()->getId() == storage->getId())
            return true;
    }
    return false;
}

bool QnStorageManager::hasStorage(const QnStorageResourcePtr &storage) const
{
    QnMutexLocker lock(&m_mutexStorages);
    return hasStorageUnsafe(storage);
}

void QnStorageManager::removeStorage(const QnStorageResourcePtr &storage)
{
    int storageIndex = -1;
    {
        QnMutexLocker lock( &m_mutexStorages );
        // remove existing storage record if exists
        for (StorageMap::iterator itr = m_storageRoots.begin(); itr != m_storageRoots.end();)
        {
            if (itr.value()->getId() == storage->getId()) {
                storageIndex = itr.key();
                NX_LOG(lit("%1 Removing storage %2 from %3 StorageManager")
                        .arg(Q_FUNC_INFO)
                        .arg(storage->getUrl())
                        .arg(m_role == QnServer::StoragePool::Normal ? "Main" : "Backup"), cl_logDEBUG1);
                itr = m_storageRoots.erase(itr);
                break;
            }
            else {
                ++itr;
            }
        }
    }
    if (storageIndex != -1)
    {
        {
            QnMutexLocker lock(&m_mutexCatalog);
            for (int i = 0; i < QnServer::ChunksCatalogCount; ++i) {
                for (const auto catalog: m_devFileCatalog[i].values())
                    catalog->removeChunks(storageIndex);
            }
        }
    }
    m_spaceInfo.storageRemoved(storageIndex);
}

void QnStorageManager::at_storageChanged(const QnResourcePtr &resource)
{
    QnStorageResourcePtr storage = qSharedPointerDynamicCast<QnStorageResource>(resource);
    if (!storage)
        return;

    NX_LOG(lit("%1 role: %2, storage role: %3")
            .arg(Q_FUNC_INFO)
            .arg(m_role == QnServer::StoragePool::Normal ? "Main" : "Backup")
            .arg(storage->isBackup() ? "Backup" : "Main"), cl_logDEBUG1);

    if (checkIfMyStorage(storage)) {
        if (!hasStorage(storage))
            addStorage(storage);
    }
    else {
        if (hasStorage(storage))
            removeStorage(storage);
    }
}

bool QnStorageManager::existsStorageWithID(const QnStorageResourceList& storages, const QnUuid &id) const
{
    for(const QnStorageResourcePtr& storage: storages)
    {
        if (storage->getId() == id)
            return true;
    }
    return false;
}

void QnStorageManager::removeAbsentStorages(const QnStorageResourceList &newStorages)
{
    QnMutexLocker lock( &m_mutexStorages );
    for (StorageMap::iterator itr = m_storageRoots.begin(); itr != m_storageRoots.end();)
    {
        if (!existsStorageWithID(newStorages, itr.value()->getId()))
            itr = m_storageRoots.erase(itr);
        else
            ++itr;
    }
}

QnStorageManager::~QnStorageManager()
{
    if (m_role == QnServer::StoragePool::Normal)
        QnNormalStorageManager_instance = nullptr;
    else if (m_role == QnServer::StoragePool::Backup)
        QnBackupStorageManager_instance = nullptr;

    stopAsyncTasks();
}

QnStorageManager* QnStorageManager::normalInstance()
{
    return QnNormalStorageManager_instance;
}

QnStorageManager* QnStorageManager::backupInstance()
{
    return QnBackupStorageManager_instance;
}

QString QnStorageManager::dateTimeStr(qint64 dateTimeMs, qint16 timeZone, const QString& separator)
{
    QString text;
    QTextStream str(&text);
    QDateTime fileDate = QDateTime::fromMSecsSinceEpoch(dateTimeMs);
    if (timeZone != -1)
        fileDate = fileDate.toUTC().addSecs(timeZone*60);
    str << QString::number(fileDate.date().year()) << separator;
    str << strPadLeft(QString::number(fileDate.date().month()), 2, '0') << separator;
    str << strPadLeft(QString::number(fileDate.date().day()), 2, '0') << separator;
    str << strPadLeft(QString::number(fileDate.time().hour()), 2, '0') << separator;
    str.flush();
    return text;
}

void QnStorageManager::getTimePeriodInternal(std::vector<QnTimePeriodList> &periods, const QnNetworkResourcePtr &camera, qint64 startTime, qint64 endTime,
                                             qint64 detailLevel, bool keepSmallChunks, const DeviceFileCatalogPtr &catalog)
{
    if (catalog) {
        periods.push_back(catalog->getTimePeriods(startTime, endTime, detailLevel, keepSmallChunks, INT_MAX));
        if (!periods.rbegin()->empty())
        {
            QnTimePeriod& lastPeriod = periods.rbegin()->last();
            bool isActive = !camera->hasFlags(Qn::foreigner) && (camera->getStatus() == Qn::Online || camera->getStatus() == Qn::Recording);
            if (lastPeriod.durationMs == -1 && !isActive)
            {
                lastPeriod.durationMs = 0;
                Recorders recorders = QnRecordingManager::instance()->findRecorders(camera);
                if (catalog->getCatalog() == QnServer::HiQualityCatalog && recorders.recorderHiRes)
                    lastPeriod.durationMs = recorders.recorderHiRes->duration()/1000;
                else if (catalog->getCatalog() == QnServer::LowQualityCatalog && recorders.recorderLowRes)
                    lastPeriod.durationMs = recorders.recorderLowRes->duration()/1000;
            }
        }
    }
}

bool QnStorageManager::isArchiveTimeExists(const QString& cameraUniqueId, qint64 timeMs)
{
    return qnNormalStorageMan->isArchiveTimeExistsInternal(cameraUniqueId, timeMs) ||
           qnBackupStorageMan->isArchiveTimeExistsInternal(cameraUniqueId, timeMs);
}

bool QnStorageManager::isArchiveTimeExists(const QString& cameraUniqueId, const QnTimePeriod period)
{
    return qnNormalStorageMan->isArchiveTimeExistsInternal(cameraUniqueId, period) ||
           qnBackupStorageMan->isArchiveTimeExistsInternal(cameraUniqueId, period);
}

bool QnStorageManager::isArchiveTimeExistsInternal(const QString& cameraUniqueId, qint64 timeMs)
{
    DeviceFileCatalogPtr catalog = getFileCatalog(cameraUniqueId, QnServer::HiQualityCatalog);
    if (catalog && catalog->containTime(timeMs))
        return true;

    catalog = getFileCatalog(cameraUniqueId, QnServer::LowQualityCatalog);
    return catalog && catalog->containTime(timeMs);
}

bool QnStorageManager::isArchiveTimeExistsInternal(const QString& cameraUniqueId, const QnTimePeriod period)
{
    DeviceFileCatalogPtr catalog = getFileCatalog(cameraUniqueId, QnServer::HiQualityCatalog);
    if (catalog && catalog->containTime(period))
        return true;

    catalog = getFileCatalog(cameraUniqueId, QnServer::LowQualityCatalog);
    return catalog && catalog->containTime(period);
}

QnTimePeriodList QnStorageManager::getRecordedPeriods(const QnSecurityCamResourceList &cameras, qint64 startTime, qint64 endTime, qint64 detailLevel, bool keepSmallChunks,
                                                      const QList<QnServer::ChunksCatalog> &catalogs, int limit)
{
    std::vector<QnTimePeriodList> periods;
    qnNormalStorageMan->getRecordedPeriodsInternal(periods, cameras, startTime, endTime, detailLevel, keepSmallChunks, catalogs, limit);
    qnBackupStorageMan->getRecordedPeriodsInternal(periods, cameras, startTime, endTime, detailLevel, keepSmallChunks, catalogs, limit);
    return QnTimePeriodList::mergeTimePeriods(periods, limit);
}

void QnStorageManager::getRecordedPeriodsInternal(
    std::vector<QnTimePeriodList>& periods,
    const QnSecurityCamResourceList &cameras,
    qint64 startTime, qint64 endTime, qint64 detailLevel, bool keepSmallChunks,
    const QList<QnServer::ChunksCatalog> &catalogs,
    int /*limit*/)
{
    for (const QnSecurityCamResourcePtr &camera: cameras) {
        QString cameraUniqueId = camera->getUniqueId();
        for (int i = 0; i < QnServer::ChunksCatalogCount; ++i) {
            QnServer::ChunksCatalog catalog = static_cast<QnServer::ChunksCatalog> (i);
            if (!catalogs.contains(catalog))
                continue;

            if (camera->isDtsBased()) {
                if (catalog == QnServer::HiQualityCatalog) // both hi- and low-quality chunks are loaded with this method
                    periods.push_back(camera->getDtsTimePeriods(startTime, endTime, detailLevel));
            } else {
                getTimePeriodInternal(periods, camera, startTime, endTime, detailLevel, keepSmallChunks, getFileCatalog(cameraUniqueId, catalog));
            }
        }

    }
}

QnRecordingStatsReply QnStorageManager::getChunkStatistics(qint64 bitrateAnalizePeriodMs)
{
    QnRecordingStatsReply result;
    QSet<QString> cameras;
    {
        QnMutexLocker lock(&m_mutexCatalog);
        for(const auto& uniqueId: m_devFileCatalog[QnServer::HiQualityCatalog].keys())
            cameras << uniqueId;
        for(const auto& uniqueId: m_devFileCatalog[QnServer::LowQualityCatalog].keys())
            cameras << uniqueId;
    }

    for (const auto& uniqueId: cameras) {
        QnRecordingStatsData stats = getChunkStatisticsByCamera(bitrateAnalizePeriodMs, uniqueId);
        QnCamRecordingStatsData data(stats);
        data.uniqueId = uniqueId;
        if (data.recordedBytes > 0)
            result.push_back(data);
    }
    return result;
}

QnRecordingStatsData QnStorageManager::getChunkStatisticsByCamera(qint64 bitrateAnalizePeriodMs, const QString& uniqueId)
{
    QnMutexLocker lock(&m_mutexCatalog);
    auto catalogHi = m_devFileCatalog[QnServer::HiQualityCatalog].value(uniqueId);
    auto catalogLow = m_devFileCatalog[QnServer::LowQualityCatalog].value(uniqueId);

    if (catalogHi && !catalogHi->isEmpty() && catalogLow && !catalogLow->isEmpty())
        return mergeStatsFromCatalogs(bitrateAnalizePeriodMs, catalogHi, catalogLow);
    else if (catalogHi && !catalogHi->isEmpty())
        return catalogHi->getStatistics(bitrateAnalizePeriodMs);
    else if (catalogLow && !catalogLow->isEmpty())
        return catalogLow->getStatistics(bitrateAnalizePeriodMs);
    else
        return QnRecordingStatsData();
}

QnRecordingStatsData QnStorageManager::mergeStatsFromCatalogs(qint64 bitrateAnalizePeriodMs, const DeviceFileCatalogPtr& catalogHi, const DeviceFileCatalogPtr& catalogLow)
{
    QnRecordingStatsData result;
    QnRecordingStatsData bitrateStats; // temp stats for virtual bitrate calculation
    qint64 archiveStartTimeMs = -1;
    qint64 bitrateThreshold = DATETIME_NOW;
    QnMutexLocker lock1(&catalogHi->m_mutex);
    QnMutexLocker lock2(&catalogLow->m_mutex);

    if (catalogHi && !catalogHi->m_chunks.empty()) {
        archiveStartTimeMs = catalogHi->m_chunks[0].startTimeMs;
        bitrateThreshold = catalogHi->m_chunks[catalogHi->m_chunks.size()-1].startTimeMs;
    }
    if (catalogLow && !catalogLow->m_chunks.empty()) {
        if (archiveStartTimeMs == -1) {
            archiveStartTimeMs = catalogLow->m_chunks[0].startTimeMs;
            bitrateThreshold = catalogLow->m_chunks[catalogLow->m_chunks.size()-1].startTimeMs;
        }
        else {
            archiveStartTimeMs = qMin(archiveStartTimeMs, catalogLow->m_chunks[0].startTimeMs);
            // not need to merge bitrateThreshold. getting from Hi archive is OK
        }
    }
    bitrateThreshold = bitrateAnalizePeriodMs ? bitrateThreshold - bitrateAnalizePeriodMs : 0;
    result.archiveDurationSecs = qMax(0ll, (qnSyncTime->currentMSecsSinceEpoch() - archiveStartTimeMs) / 1000);


    //auto itrHiLeft = std::lower_bound(catalogHi->m_chunks.cbegin(), catalogHi->m_chunks.cend(), startTime);
    //auto itrHiRight = std::upper_bound(itrHiLeft, catalogHi->m_chunks.cend(), endTime);
    auto itrHiLeft = catalogHi->m_chunks.cbegin();
    auto itrHiRight = catalogHi->m_chunks.cend();

    //auto itrLowLeft = std::lower_bound(catalogLow->m_chunks.cbegin(), catalogLow->m_chunks.cend(), startTime);
    //auto itrLowRight = std::upper_bound(itrLowLeft, catalogLow->m_chunks.cend(), endTime);
    auto itrLowLeft = catalogLow->m_chunks.cbegin();
    auto itrLowRight = catalogLow->m_chunks.cend();

    qint64 hiTime = itrHiLeft < itrHiRight ? itrHiLeft->startTimeMs : DATETIME_NOW;
    qint64 lowTime = itrLowLeft < itrLowRight ? itrLowLeft->startTimeMs : DATETIME_NOW;
    qint64 currentTime = qMin(hiTime, lowTime);

    while (itrHiLeft < itrHiRight || itrLowLeft < itrLowLeft)
    {
        qint64 nextHiTime = DATETIME_NOW;
        qint64 nextLowTime = DATETIME_NOW;
        bool hasHi = false;
        bool hasLow = false;
        if (itrHiLeft != itrHiRight) {
            nextHiTime = itrHiLeft->containsTime(currentTime) ? itrHiLeft->endTimeMs() : itrHiLeft->startTimeMs;
            hasHi = itrHiLeft->durationMs > 0 && itrHiLeft->containsTime(currentTime);
        }
        if (itrLowLeft != itrLowRight) {
            nextLowTime = itrLowLeft->containsTime(currentTime) ? itrLowLeft->endTimeMs() : itrLowLeft->startTimeMs;
            hasLow = itrLowLeft->durationMs > 0 && itrLowLeft->containsTime(currentTime);
        }

        qint64 nextTime = qMin(nextHiTime, nextLowTime);
        NX_ASSERT(nextTime >= currentTime);
        qint64 blockDuration = nextTime - currentTime;

        if (hasHi)
        {
            qreal persentUsage = blockDuration / (qreal) itrHiLeft->durationMs;
            NX_ASSERT(qBetween(0.0, persentUsage, 1.000001));
            auto storage = storageRoot(itrHiLeft->storageIndex);
            result.recordedBytes += itrHiLeft->getFileSize() * persentUsage;
            if (storage)
                result.recordedBytesPerStorage[storage->getId()] += itrHiLeft->getFileSize() * persentUsage;

            result.recordedSecs += itrHiLeft->durationMs * persentUsage;
            if (itrHiLeft->startTimeMs >= bitrateThreshold) {
                bitrateStats.recordedBytes += itrHiLeft->getFileSize() * persentUsage;
                bitrateStats.recordedSecs += itrHiLeft->durationMs * persentUsage;
            }
        }

        if (hasLow)
        {
            qreal persentUsage = blockDuration / (qreal) itrLowLeft->durationMs;
            NX_ASSERT(qBetween(0.0, persentUsage, 1.000001));
            auto storage = storageRoot(itrLowLeft->storageIndex);
            result.recordedBytes += itrLowLeft->getFileSize() * persentUsage;
            if (storage)
                result.recordedBytesPerStorage[storage->getId()] += itrLowLeft->getFileSize() * persentUsage;
            if (hasHi)
            {
                // do not include bitrate calculation if only LQ quality
                if (itrLowLeft->startTimeMs >= bitrateThreshold)
                    bitrateStats.recordedBytes += itrLowLeft->getFileSize() * persentUsage;
            }
            else {
                // inc time if no HQ
                result.recordedSecs += itrLowLeft->durationMs * persentUsage;
            }
        }

        while (itrHiLeft < itrHiRight && nextTime >= itrHiLeft->endTimeMs())
            ++itrHiLeft;
        while (itrLowLeft < itrLowRight && nextTime >= itrLowLeft->endTimeMs())
            ++itrLowLeft;
        currentTime = nextTime;
    }

    result.recordedSecs /= 1000;   // msec to sec
    bitrateStats.recordedSecs /= 1000; // msec to sec
    if (bitrateStats.recordedBytes > 0 && bitrateStats.recordedSecs > 0)
        result.averageBitrate = bitrateStats.recordedBytes / (qreal) bitrateStats.recordedSecs;
    NX_ASSERT(result.averageBitrate >= 0);
    return result;
}


void QnStorageManager::removeEmptyDirs(const QnStorageResourcePtr &storage)
{
    const std::function<bool(const QnAbstractStorageResource::FileInfoList&, size_t)> removeEmptyDir =
        [&](const QnAbstractStorageResource::FileInfoList& fl, size_t depthLimit)
        {
            for (const auto& entry: fl)
            {
                if (QnResource::isStopping())
                    return false;

                if (entry.isDir())
                {
                    if (depthLimit == 0)
                    {
                        NX_LOGX(lm("Directory depth is above the limit, corrupted file system? %1")
                            .str(entry.absoluteFilePath()), cl_logERROR);

                        return false;
                    }

                    const auto dirFileList = storage->getFileList(entry.absoluteFilePath());
                    if (!dirFileList.isEmpty() && !removeEmptyDir(dirFileList, depthLimit - 1))
                        return false;

                    // Ignore error here, trying to clean as much as we can.
                    storage->removeDir(entry.absoluteFilePath());
                }
                else
                {
                    // We've met file. Solid reason to stop.
                    return false;
                }
            }

            return true;
        };

    auto qualityFileList = storage->getFileList(storage->getUrl());
    for (const auto &qualityEntry : qualityFileList)
    {
        if (qualityEntry.isDir()) //< Quality.
        {
            auto cameraFileList = storage->getFileList(qualityEntry.absoluteFilePath());
            for (const auto &cameraEntry : cameraFileList) //< For every year folder.
            {
                static const size_t kDepthLimit = 10; //< Little more depth, than required.
                removeEmptyDir(storage->getFileList(cameraEntry.absoluteFilePath()), kDepthLimit);
            }
        }
    }
}

void QnStorageManager::updateCameraHistory()
{
    auto archivedListNew = getCamerasWithArchive();

    std::vector<QnUuid> archivedListOld =
        qnCameraHistoryPool->getServerFootageData(qnCommon->moduleGUID());
    std::sort(archivedListOld.begin(), archivedListOld.end());

    if (archivedListOld == archivedListNew)
        return;

    const ec2::AbstractECConnectionPtr& appServerConnection = QnAppServerConnectionFactory::getConnection2();

    ec2::ErrorCode errCode = appServerConnection->getCameraManager(Qn::kSystemAccess)->setServerFootageDataSync(qnCommon->moduleGUID(), archivedListNew);

    if (errCode != ec2::ErrorCode::ok) {
        qCritical() << "ECS server error during execute method addCameraHistoryItem: "
                    << ec2::toString(errCode);
        return;
    }
    qnCameraHistoryPool->setServerFootageData(qnCommon->moduleGUID(),
                                              archivedListNew);
    return;
}

void QnStorageManager::clearSpace(bool forced)
{
    QnMutexLocker lk(&m_clearSpaceMutex);
    // if Backup on Schedule (or on demand) synchronization routine is
    // running at the moment, dont run clearSpace() if it's been triggered by
    // the timer.
    bool backupOnAndTimerTriggered = m_role == QnServer::StoragePool::Backup && !forced &&
                                     scheduleSync()->getStatus().state ==
                                     Qn::BackupState::BackupState_InProgress;
    if (backupOnAndTimerTriggered)
        return;

    if (m_firstStoragesTestDone)
        testOfflineStorages();

    // 1. delete old data if cameras have max duration limit
    clearMaxDaysData();

    // 2. free storage space
    bool allStoragesReady = true;
    QSet<QnStorageResourcePtr> storages;

    for (const auto& storage: getUsedWritableStorages()) {
        if (!storage->hasFlags(Qn::storage_fastscan)) {
            storages << storage;
        } else {
            NX_LOG(lit("[Cleanup]: Storage %1 is being fast scanned. Skipping")
                    .arg(storage->getUrl()), cl_logDEBUG2);
            allStoragesReady = false;
        }
    }

    if (allStoragesReady && m_role == QnServer::StoragePool::Normal) {
        updateCameraHistory();
    }

    qint64 toDeleteTotal = 0;
    std::chrono::time_point<std::chrono::steady_clock> cleanupStartTime = std::chrono::steady_clock::now();

    if (QnLog::logs() && QnLog::logs()->get()->logLevel() >= cl_logDEBUG2)
    {
        NX_LOG(lit("[Cleanup, measure]: %1 storages are ready for a cleanup").arg(storages.size()), cl_logDEBUG2);
        NX_LOG(lit("[Cleanup, measure]: Starting cleanup routine for %1 storage manager")
                .arg((m_role == QnServer::StoragePool::Normal ? lit("main") : lit("backup"))), cl_logDEBUG2);

        for (const auto& storage: storages)
        {
            if (storage->getSpaceLimit() == 0 ||
                (storage->getCapabilities() & QnAbstractStorageResource::cap::RemoveFile)
                    != QnAbstractStorageResource::cap::RemoveFile)
            {
                NX_LOG(lit("[Cleanup, measure]: storage: %1 spaceLimit: %2, RemoveFileCap: %3, skipping")
                        .arg(storage->getUrl())
                        .arg(storage->getSpaceLimit())
                        .arg((storage->getCapabilities() & QnAbstractStorageResource::cap::RemoveFile) == QnAbstractStorageResource::cap::RemoveFile), cl_logDEBUG2);
                continue;
            }

            qint64 toDeleteForStorage = storage->getSpaceLimit() - storage->getFreeSpace();
            NX_LOG(lit("[Cleanup, measure]: storage: %1, spaceLimit: %2, freeSpace: %3, toDelete: %4")
                    .arg(storage->getUrl())
                    .arg(storage->getSpaceLimit())
                    .arg(storage->getFreeSpace())
                    .arg(toDeleteForStorage), cl_logDEBUG2);

            if (toDeleteForStorage > 0)
                toDeleteTotal += toDeleteForStorage;
        }

        NX_LOG(lit("[Cleanup, measure]: Total bytes to cleanup: %1 (%2 Mb) (%3 Gb)")
                .arg(toDeleteTotal)
                .arg(toDeleteTotal / (1024 * 1024))
                .arg(toDeleteTotal / (1024 * 1024 * 1024)), cl_logDEBUG2);
    }

    QnStorageResourceList delAgainList;
    for(const QnStorageResourcePtr& storage: storages) {
        if (!clearOldestSpace(storage, true))
            delAgainList << storage;
    }
    for(const QnStorageResourcePtr& storage: delAgainList)
        clearOldestSpace(storage, false);

    if (QnLog::logs() && QnLog::logs()->get()->logLevel() >= cl_logDEBUG2 && toDeleteTotal > 0)
    {
        QString clearSpaceLogMessage;
        QTextStream clearSpaceLogStream(&clearSpaceLogMessage);
        auto cleanupEndTime = std::chrono::steady_clock::now();
        qint64 elapsedMs =
            std::chrono::duration_cast<std::chrono::milliseconds>
                (cleanupEndTime - cleanupStartTime).count();
        qint64 elapsedSecs = qMax((elapsedMs / 1000), 1ll);

        clearSpaceLogStream << "[Cleanup, measure]: Cleanup routine for "
                            << (m_role == QnServer::StoragePool::Normal ? "main " : "backup")
                            << " storage manager has finished" << endl;
        clearSpaceLogStream << "[Cleanup, measure]: time elapsed: " << elapsedMs << " ms "
                            << "(" << elapsedSecs << " secs) "
                            << "(" << (elapsedSecs / (60 * 60)) << " hrs)" << endl;
        clearSpaceLogStream << "[Cleanup, measure]: cleanup speed was "
                            << (toDeleteTotal / (1024 * 1024 * elapsedSecs)) << " Mb/s"
                            << endl;
        NX_LOG(clearSpaceLogMessage, cl_logDEBUG2);
    }

    // 4. DB cleanup
    qnStorageDbPool->flush();

    if (m_role != QnServer::StoragePool::Normal)
        return;

    // 5. Cleanup motion

    bool readyToDeleteMotion = (m_archiveRebuildInfo.state == Qn::RebuildState_None); // do not delete motion while rebuilding in progress (just in case, unnecessary)
    for(const QnStorageResourcePtr& storage: getAllStorages()) {
        if (storage->getStatus() == Qn::Offline) {
            readyToDeleteMotion = false; // offline storage may contain archive. do not delete motion so far
            break;
        }

    }
    if (readyToDeleteMotion)
    {
        if (m_clearMotionTimer.elapsed() > MOTION_CLEANUP_INTERVAL) {
            m_clearMotionTimer.restart();
            clearUnusedMotion();
        }
    }
    else {
        m_clearMotionTimer.restart();
    }

    // 6. Cleanup bookmarks
    if (m_clearBookmarksTimer.elapsed() > BOOKMARK_CLEANUP_INTERVAL) {
        m_clearBookmarksTimer.restart();
        clearBookmarks();
    }
}

void QnStorageManager::clearBookmarks()
{
    QMap<QString, qint64> minTimes; // key - unique id, value - timestamp to delete
    if (!qnNormalStorageMan->getMinTimes(minTimes))
        return;
    if (!qnBackupStorageMan->getMinTimes(minTimes))
        return;

    QMap<QnUuid, qint64> dataToDelete;
    for (auto itr = minTimes.begin(); itr != minTimes.end(); ++itr)
    {
        const QString& uniqueId = itr.key();
        const qint64 timestampMs = itr.value();

        auto itrPrev = m_lastCatalogTimes.find(uniqueId);
        if (itrPrev != m_lastCatalogTimes.end() && itrPrev.value() != timestampMs)
        {
            dataToDelete.insert(
                QnSecurityCamResource::makeCameraIdFromUniqueId(uniqueId), timestampMs);
        }
        m_lastCatalogTimes[uniqueId] = timestampMs;
    }

    if (!dataToDelete.isEmpty())
        qnServerDb->deleteBookmarksToTime(dataToDelete);

    m_lastCatalogTimes = minTimes;
}

bool QnStorageManager::getMinTimes(QMap<QString, qint64>& lastTime)
{
    QnStorageManager::StorageMap storageRoots = getAllStorages();
    qint64 bigStorageThreshold = 0;
    for (StorageMap::const_iterator itr = storageRoots.constBegin(); itr != storageRoots.constEnd(); ++itr)
    {
        QnStorageResourcePtr fileStorage = qSharedPointerDynamicCast<QnStorageResource> (itr.value());
        if (!fileStorage)
            continue;
        if (fileStorage->getStatus() == Qn::Offline)
            return false; // do not cleanup bookmarks until all storages are online
        if (fileStorage->hasFlags(Qn::storage_fastscan))
            return false; // do not cleanup bookmarks until all data loaded
    }

    QnMutexLocker lock(&m_mutexCatalog);
    processCatalogForMinTime(lastTime, m_devFileCatalog[QnServer::LowQualityCatalog]);
    processCatalogForMinTime(lastTime, m_devFileCatalog[QnServer::HiQualityCatalog]);

    return true;
}

void QnStorageManager::processCatalogForMinTime(QMap<QString, qint64>& lastTime, const FileCatalogMap& catalogMap)
{
    for (FileCatalogMap::const_iterator itr = catalogMap.constBegin(); itr != catalogMap.constEnd(); ++itr)
    {
        const QString& uniqueId = itr.key();
        DeviceFileCatalogPtr curCatalog = itr.value();
        qint64 firstTime = curCatalog->firstTime();
        if (firstTime == AV_NOPTS_VALUE)
            firstTime = DATETIME_NOW;
        auto itrLastTime = lastTime.find(uniqueId);
        if (itrLastTime == lastTime.end())
            lastTime.insert(uniqueId, firstTime);
        else
            itrLastTime.value() = qMin(itrLastTime.value(), firstTime);
    }
}

QnStorageManager::StorageMap QnStorageManager::getAllStorages() const
{
    QnMutexLocker lock( &m_mutexStorages );
    return m_storageRoots;
}

bool QnStorageManager::hasRebuildingStorages() const
{
    bool result = false;
    for (const auto &storage : getUsedWritableStorages())
    {
        if (storage->hasFlags(Qn::storage_fastscan))
        {
            result = true;
            break;
        }
    }
    return result || m_archiveRebuildInfo.state == Qn::RebuildState_FullScan;
}

QnStorageResourceList QnStorageManager::getStorages() const
{
    QnMutexLocker lock( &m_mutexStorages );
    return m_storageRoots.values().toSet().toList(); // remove storage duplicates. Duplicates are allowed in sake for v1.4 compatibility
}

QnStorageResourceList QnStorageManager::getStoragesInLexicalOrder() const
{
    // duplicate storage path's aren't used any more
    QnMutexLocker lock(&m_mutexStorages);
    QnStorageResourceList result = m_storageRoots.values();
    std::sort(result.begin(), result.end(),
              [](const QnStorageResourcePtr& storage1, const QnStorageResourcePtr& storage2)
    {
        return storage1->getUrl() < storage2->getUrl();
    });
    return result;
}

void QnStorageManager::deleteRecordsToTime(DeviceFileCatalogPtr catalog, qint64 minTime)
{
    int idx = catalog->findFileIndex(minTime, DeviceFileCatalog::OnRecordHole_NextChunk);
    if (idx == -1)
        idx = std::numeric_limits<int>::max();
    QVector<DeviceFileCatalog::Chunk> deletedChunks = catalog->deleteRecordsBefore(idx);
    for(const DeviceFileCatalog::Chunk& chunk: deletedChunks)
        clearDbByChunk(catalog, chunk);
}

void QnStorageManager::clearDbByChunk(DeviceFileCatalogPtr catalog, const DeviceFileCatalog::Chunk& chunk)
{
    {
        QnStorageResourcePtr storage = storageRoot(chunk.storageIndex);
        if (storage) {
            QnStorageDbPtr sdb = qnStorageDbPool->getSDB(storage);
            if (sdb)
                sdb->deleteRecords(catalog->cameraUniqueId(), catalog->getRole(), chunk.startTimeMs);
        }
    }
}

void QnStorageManager::clearMaxDaysData()
{
    clearMaxDaysData(QnServer::HiQualityCatalog);
    clearMaxDaysData(QnServer::LowQualityCatalog);
}

void QnStorageManager::clearMaxDaysData(QnServer::ChunksCatalog catalogIdx)
{
    FileCatalogMap catalogMap;
    {
        QnMutexLocker lock(&m_mutexCatalog);
        catalogMap = m_devFileCatalog[catalogIdx];
    }

    for(const DeviceFileCatalogPtr& catalog: catalogMap.values()) {
        QnSecurityCamResourcePtr camera = qnResPool->getResourceByUniqueId<QnSecurityCamResource>(catalog->cameraUniqueId());
        if (camera && camera->maxDays() > 0) {
            qint64 timeToDelete = qnSyncTime->currentMSecsSinceEpoch() - MSECS_PER_DAY * camera->maxDays();
            deleteRecordsToTime(catalog, timeToDelete);
        }
    }
}

void QnStorageManager::clearUnusedMotion()
{
    UsedMonthsMap usedMonths;

    qnNormalStorageMan->updateRecordedMonths(usedMonths);
    qnBackupStorageMan->updateRecordedMonths(usedMonths);

    for( const QString& dir: QDir(QnMotionHelper::getBaseDir()).entryList(QDir::Dirs | QDir::NoDotAndDotDot))
        QnMotionHelper::instance()->deleteUnusedFiles(usedMonths.value(dir).toList(), dir);
}

void QnStorageManager::updateRecordedMonths(UsedMonthsMap& usedMonths)
{
    QnMutexLocker lock( &m_mutexCatalog );
    updateRecordedMonths(m_devFileCatalog[QnServer::HiQualityCatalog], usedMonths);
    updateRecordedMonths(m_devFileCatalog[QnServer::LowQualityCatalog], usedMonths);
}

/*
void QnStorageManager::clearCameraHistory()
{
    QnMutexLocker lock( &m_mutexCatalog );
    QMap<QString, qint64> minTimes; // min archive time by camera unique ID
    minTimeByCamera(m_devFileCatalog[QnServer::HiQualityCatalog], minTimes);
    minTimeByCamera(m_devFileCatalog[QnServer::LowQualityCatalog], minTimes);

    for(auto itr = minTimes.begin(); itr != minTimes.end(); ++itr) {
        if (itr.value() == AV_NOPTS_VALUE)
            itr.value() == DATETIME_NOW; // delete all history if catalog is empty
    }

    QList<QnCameraHistoryItem> itemsToRemove = qnCameraHistoryPool->getUnusedItems(minTimes, qnCommon->moduleGUID());
    ec2::AbstractECConnectionPtr ec2Connection = QnAppServerConnectionFactory::getConnection2();
    for(const QnCameraHistoryItem& item: itemsToRemove) {
        ec2::ErrorCode errCode = ec2Connection->getCameraManager()->removeCameraHistoryItemSync(item);
        if (errCode == ec2::ErrorCode::ok)
            qnCameraHistoryPool->removeCameraHistoryItem(item);
    }
}

void QnStorageManager::minTimeByCamera(const FileCatalogMap &catalogMap, QMap<QString, qint64>& minTimes)
{
    for (FileCatalogMap::const_iterator itr = catalogMap.constBegin(); itr != catalogMap.constEnd(); ++itr)
    {
        DeviceFileCatalogPtr curCatalog = itr.value();

        auto resultItr = minTimes.find(curCatalog->cameraUniqueId());
        if (resultItr == minTimes.end())
            resultItr = minTimes.insert(curCatalog->cameraUniqueId(), AV_NOPTS_VALUE);

        qint64 archiveTime = curCatalog->firstTime();
        if (archiveTime != AV_NOPTS_VALUE) {
            if (resultItr.value() == AV_NOPTS_VALUE)
                resultItr.value() = archiveTime;
            else if (archiveTime < resultItr.value())
                resultItr.value() = archiveTime;
        }
    }
}
*/

void QnStorageManager::updateRecordedMonths(const FileCatalogMap &catalogMap, UsedMonthsMap& usedMonths)
{
    for(const DeviceFileCatalogPtr& catalog: catalogMap.values())
        usedMonths[catalog->cameraUniqueId()] += catalog->recordedMonthList();
}

void QnStorageManager::findTotalMinTime(const bool useMinArchiveDays, const FileCatalogMap& catalogMap, qint64& minTime, DeviceFileCatalogPtr& catalog)
{
    for (FileCatalogMap::const_iterator itr = catalogMap.constBegin(); itr != catalogMap.constEnd(); ++itr)
    {
        DeviceFileCatalogPtr curCatalog = itr.value();
        qint64 curMinTime = curCatalog->firstTime();
        if (curMinTime != (qint64)AV_NOPTS_VALUE && curMinTime < minTime)
        {
            if (useMinArchiveDays) {
                QnSecurityCamResourcePtr camera = qnResPool->getResourceByUniqueId<QnSecurityCamResource>(itr.key());
                if (camera && camera->minDays() > 0) {
                    qint64 threshold = qnSyncTime->currentMSecsSinceEpoch() - MSECS_PER_DAY * camera->minDays();
                    if (threshold < curMinTime)
                        continue; // do not delete archive for this camera
                }
            }
            minTime = curMinTime;
            catalog = curCatalog;
        }
    }
}

bool QnStorageManager::clearOldestSpace(const QnStorageResourcePtr &storage, bool useMinArchiveDays)
{
    if (storage->getSpaceLimit() == 0)
        return true; // unlimited. nothing to delete


    QString dir = storage->getUrl();

    if (!(storage->getCapabilities() & QnAbstractStorageResource::cap::RemoveFile))
        return true; // nothing to delete

    qint64 freeSpace = storage->getFreeSpace();
    if (freeSpace == -1)
        return true; // nothing to delete

    qint64 toDelete = storage->getSpaceLimit() - freeSpace;

    if (toDelete > 0)
      NX_LOG(lit("Cleanup. Starting for storage %1. %2 Mb to clean")
              .arg(storage->getUrl())
              .arg(toDelete / (1024 * 1024)), cl_logINFO);

    DeviceFileCatalog::Chunk deletedChunk;

    while (toDelete > 0)
    {
        if (QnResource::isStopping())
        {   // Return true to mark this storage as succesfully cleaned since
            // we don't want this storage to be added in clear-again list when
            // server is going to stop.
            return true;
        }

        qint64 minTime = 0x7fffffffffffffffll;
        DeviceFileCatalogPtr catalog;
        {
            QnMutexLocker lock( &m_mutexCatalog );
            findTotalMinTime(useMinArchiveDays, m_devFileCatalog[QnServer::HiQualityCatalog], minTime, catalog);
            findTotalMinTime(useMinArchiveDays, m_devFileCatalog[QnServer::LowQualityCatalog], minTime, catalog);
        }
        if (catalog != 0)
        {
            deletedChunk = catalog->deleteFirstRecord();
            clearDbByChunk(catalog, deletedChunk);
            QnServer::ChunksCatalog altQuality = catalog->getRole() == QnServer::HiQualityCatalog ? QnServer::LowQualityCatalog : QnServer::HiQualityCatalog;
            DeviceFileCatalogPtr altCatalog = getFileCatalog(catalog->cameraUniqueId(), altQuality);
            if (altCatalog != 0)
            {
                qint64 minTime = catalog->minTime();
                if (minTime != (qint64)AV_NOPTS_VALUE)
                    deleteRecordsToTime(altCatalog, minTime);
                else
                    deleteRecordsToTime(altCatalog, DATETIME_NOW);
            }
        }
        else
            break; // nothing to delete

        qint64 freeSpace = storage->getFreeSpace();
        if (freeSpace == -1)
            return true; // nothing to delete

        qint64 oldToDelete = toDelete;
        toDelete = storage->getSpaceLimit() - freeSpace;
        if (oldToDelete == toDelete && deletedChunk.startTimeMs != -1)
        {	// Non-empty chunk's been found and delete attempt's been made.
            // But ToDelete is still the same. This might mean that storage went offline.
            // Let's check it.
            if (storage->getStatus() == Qn::ResourceStatus::Offline)
                return false;
        }
        // reset Chunk
        deletedChunk = DeviceFileCatalog::Chunk();
    }

    if (toDelete > 0 && !useMinArchiveDays) {
        if (!m_diskFullWarned[storage->getId()]) {
            emit storageFailure(storage, QnBusiness::StorageFullReason);
            m_diskFullWarned[storage->getId()] = true;
        }
    }
    else {
        m_diskFullWarned[storage->getId()] = false;
    }

    return toDelete <= 0;
}

void QnStorageManager::at_archiveRangeChanged(const QnStorageResourcePtr &resource, qint64 newStartTimeMs, qint64 newEndTimeMs)
{
    Q_UNUSED(newEndTimeMs)
    int storageIndex = qnStorageDbPool->getStorageIndex(resource);
    QnMutexLocker lock(&m_mutexCatalog);
    for(const DeviceFileCatalogPtr& catalogHi: m_devFileCatalog[QnServer::HiQualityCatalog])
        catalogHi->deleteRecordsByStorage(storageIndex, newStartTimeMs);

    for(const DeviceFileCatalogPtr& catalogLow: m_devFileCatalog[QnServer::LowQualityCatalog])
        catalogLow->deleteRecordsByStorage(storageIndex, newStartTimeMs);

    //TODO: #vasilenko should we delete bookmarks here too?
}

bool QnStorageManager::isWritableStoragesAvailable() const
{
    QnMutexLocker lk(&m_mutexStorages);
    return m_isWritableStorageAvail;
}

QSet<QnStorageResourcePtr> QnStorageManager::getUsedWritableStorages() const
{
    auto allWritableStorages = getAllWritableStorages();
    QSet<QnStorageResourcePtr> result;
    for (const auto& storage: allWritableStorages)
        if (storage->isUsedForWriting())
            result.insert(storage);

    if (!result.empty())
        m_isWritableStorageAvail = true;
    else
        m_isWritableStorageAvail = false;

    return result;
}

QSet<QnStorageResourcePtr> QnStorageManager::getAllWritableStorages() const
{
    QSet<QnStorageResourcePtr> result;

    QnStorageManager::StorageMap storageRoots = getAllStorages();
    qint64 bigStorageThreshold = 0;
    for (StorageMap::const_iterator itr = storageRoots.constBegin(); itr != storageRoots.constEnd(); ++itr)
    {
        QnStorageResourcePtr fileStorage = itr.value();
        if (fileStorage->getStatus() != Qn::Offline)
        {
            qint64 available = fileStorage->getTotalSpace() - fileStorage->getSpaceLimit();
            bigStorageThreshold = qMax(bigStorageThreshold, available);
        }
    }
    bigStorageThreshold /= BIG_STORAGE_THRESHOLD_COEFF;

    for (StorageMap::const_iterator itr = storageRoots.constBegin(); itr != storageRoots.constEnd(); ++itr)
    {
        QnStorageResourcePtr fileStorage = itr.value();
        if (fileStorage->getStatus() != Qn::Offline)
        {
            qint64 available = fileStorage->getTotalSpace() - fileStorage->getSpaceLimit();
            if (available >= bigStorageThreshold)
                result << fileStorage;
        }
    }

    const qint64 kSystemStorageTreshold = 5;

    qint64 totalNonSystemStoragesSpace = 0;
    qint64 systemStorageSpace = 0;
    std::vector<QSet<QnStorageResourcePtr>::iterator> systemStorageItVec;

    for (auto it = result.begin(); it != result.end(); ++it)
    {
        if (!(*it)->isSystem())
            totalNonSystemStoragesSpace += (*it)->getTotalSpace();
        else
        {
            systemStorageItVec.push_back(it);
            systemStorageSpace += (*it)->getTotalSpace();
        }
    }

    if (totalNonSystemStoragesSpace > systemStorageSpace * kSystemStorageTreshold && !systemStorageItVec.empty())
    {
        for (auto it : systemStorageItVec)
            result.remove(*it);
    }

    return result;
}

void QnStorageManager::testStoragesDone()
{
    m_firstStoragesTestDone = true;
    ArchiveScanPosition rebuildPos(m_role);
    rebuildPos.load();
    if (!rebuildPos.isEmpty())
        rebuildCatalogAsync(); // continue to rebuild
}

void QnStorageManager::changeStorageStatus(const QnStorageResourcePtr &fileStorage, Qn::ResourceStatus status)
{
    //QnMutexLocker lock( &m_mutexStorages );
    if (status == Qn::Online && fileStorage->getStatus() == Qn::Offline) {
        NX_LOG(QString("Storage. Path: %1. Goes to the online state. SpaceLimit: %2MiB. Currently available: %3MiB").
            arg(fileStorage->getUrl()).arg(fileStorage->getSpaceLimit() / 1024 / 1024).arg(fileStorage->getFreeSpace() / 1024 / 1024), cl_logINFO);

        // add data before storage goes to the writable state
        doMigrateCSVCatalog(fileStorage);
        migrateSqliteDatabase(fileStorage);
        addDataFromDatabase(fileStorage);
        NX_LOG(lit("[Storage, scan]: storage %1 - finished loading data from DB. Ready for scan").arg(fileStorage->getUrl()), cl_logDEBUG2);
        m_rebuildArchiveThread->addStorageToScan(fileStorage, true);
        m_spaceInfo.storageAdded(qnStorageDbPool->getStorageIndex(fileStorage), fileStorage->getTotalSpace());
    }

    fileStorage->setStatus(status);
    if (status == Qn::Offline)
        emit storageFailure(fileStorage, QnBusiness::StorageIoErrorReason);
}

void QnStorageManager::startAuxTimerTasks()
{
    static const std::chrono::minutes kWriteInfoFilesInterval(5);

    m_auxTasksTimerManager.addNonStopTimer(
        [this](nx::utils::TimerId) { m_camInfoWriter.write(); },
        kWriteInfoFilesInterval,
        kWriteInfoFilesInterval);

    static const std::chrono::minutes kRemoveEmptyDirsInterval(60);

    m_auxTasksTimerManager.addNonStopTimer(
        [this](nx::utils::TimerId)
        {
            for (const auto& storage : getUsedWritableStorages())
            {
                if (storage->hasFlags(Qn::storage_fastscan))
                    continue;
                removeEmptyDirs(storage);
            }
        },
        kRemoveEmptyDirsInterval,
        kRemoveEmptyDirsInterval);
}

void QnStorageManager::testOfflineStorages()
{
    QnMutexLocker lock( &m_testStorageThreadMutex );
    if (m_testStorageThread && !m_testStorageThread->isRunning())
        m_testStorageThread->start();
}

void QnStorageManager::stopAsyncTasks()
{
    {
        QnMutexLocker lock( &m_testStorageThreadMutex );
        if (m_testStorageThread) {
            m_testStorageThread->stop();
            delete m_testStorageThread;
            m_testStorageThread = 0;
        }
    }

    m_rebuildCancelled = true;
    if (m_rebuildArchiveThread) {
        m_rebuildArchiveThread->stop();
        delete m_rebuildArchiveThread;
        m_rebuildArchiveThread = 0;
    }

    m_auxTasksTimerManager.stop();
}

QnStorageResourcePtr QnStorageManager::getStorageByIndex(int index) const
{
    QnMutexLocker lock(&m_mutexStorages);
    return m_storageRoots.value(index);
}

QnStorageResourcePtr QnStorageManager::getOptimalStorageRoot(
    std::function<bool(const QnStorageResourcePtr &)> pred)
{
    QnStorageResourcePtr result;
    std::vector<int> allowedIndexes;

    auto hasFastScanned = [this]
    {
        auto allStorages = getAllStorages();
        for (auto it = allStorages.cbegin(); it != allStorages.cend(); ++it)
        {
            if (it.value()->hasFlags(Qn::storage_fastscan))
                return true;
        }
        return false;
    };

    auto emitFailureAndReturnNullStorage = [this, hasFastScanned](int optimalStorageIndex)
    {
        NX_LOG(lit("[Storage, Selection] Failed to find storage for index %1").arg(optimalStorageIndex),
               cl_logDEBUG2);
        if (!m_warnSended && !hasFastScanned() && m_firstStoragesTestDone)
        {
            if (m_role == QnServer::StoragePool::Normal)
            {   // 'noStorageAvailbale' signal results in client notification.
                // For backup storages No Available Storage error is translated to
                // specific backup error by the calling code and this error
                // is reported to the client (and also logged).
                // Hence these below seem redundant for Backup storage manager.
                emit noStoragesAvailable();
                qWarning() << "No storage available for recording";
            }
            m_warnSended = true;
        }

        return QnStorageResourcePtr();
    };

    for (const auto& storage: getUsedWritableStorages())
    {
        if (pred(storage) &&
            storage->getFreeSpace() > kMinStorageFreeSpace)
        {
            allowedIndexes.push_back(qnStorageDbPool->getStorageIndex(storage));
        }
    }

    auto optimalStorageIndex = m_spaceInfo.getOptimalStorageIndex(allowedIndexes);
    if (optimalStorageIndex == -1)
        return emitFailureAndReturnNullStorage(optimalStorageIndex);

    result = getStorageByIndex(optimalStorageIndex);
    if (result)
    {
        NX_LOG(lit("[Storage, Selection] Selected storage %1").arg(result->getUrl()), cl_logDEBUG2);
        return result;
    }

    return emitFailureAndReturnNullStorage(optimalStorageIndex);
}

QString QnStorageManager::getFileName(const qint64& dateTime, qint16 timeZone, const QnNetworkResourcePtr &camera, const QString& prefix, const QnStorageResourcePtr& storage)
{
    if (!storage) {
        if (m_storageWarnTimer.elapsed() > 5000) {
            qWarning() << "No disk storages";
            m_storageWarnTimer.restart();
        }
        return QString();
    }
    NX_ASSERT(camera != 0);
    QString base = closeDirPath(storage->getUrl());
    QString separator = getPathSeparator(base);

    if (!prefix.isEmpty())
        base += prefix + separator;
    base += camera->getPhysicalId();

    NX_ASSERT(!camera->getPhysicalId().isEmpty());
    QString text = base + separator + dateTimeStr(dateTime, timeZone, separator);

    return text + QString::number(dateTime);
}

DeviceFileCatalogPtr QnStorageManager::getFileCatalog(const QString& cameraUniqueId, const QString &catalogPrefix)
{
    return getFileCatalogInternal(cameraUniqueId, DeviceFileCatalog::catalogByPrefix(catalogPrefix));
}

DeviceFileCatalogPtr QnStorageManager::getFileCatalog(const QString& cameraUniqueId, QnServer::ChunksCatalog catalog)
{
    return getFileCatalogInternal(cameraUniqueId, catalog);
}

void QnStorageManager::replaceChunks(const QnTimePeriod& rebuildPeriod, const QnStorageResourcePtr &storage, const DeviceFileCatalogPtr &newCatalog, const QString& cameraUniqueId, QnServer::ChunksCatalog catalog)
{
    QnMutexLocker lock( &m_mutexCatalog );
    int storageIndex = qnStorageDbPool->getStorageIndex(storage);

    // add new recorded chunks to scan data
    qint64 scannedDataLastTime = newCatalog->m_chunks.empty() ? 0 : newCatalog->m_chunks[newCatalog->m_chunks.size()-1].startTimeMs;
    qint64 rebuildLastTime = qMax(rebuildPeriod.endTimeMs(), scannedDataLastTime);

    DeviceFileCatalogPtr ownCatalog = getFileCatalogInternal(cameraUniqueId, catalog);
    auto itr = std::lower_bound(ownCatalog->m_chunks.begin(), ownCatalog->m_chunks.end(), rebuildLastTime);
    for (; itr != ownCatalog->m_chunks.end(); ++itr)
    {
        if (itr->storageIndex == storageIndex) {

            if (!newCatalog->isEmpty())
            {
                DeviceFileCatalog::Chunk& lastChunk = newCatalog->m_chunks[newCatalog->m_chunks.size()-1];
                if (lastChunk.startTimeMs == itr->startTimeMs) {
                    lastChunk.durationMs = qMax(lastChunk.durationMs, itr->durationMs);
                        continue;
                }
            }

            newCatalog->addChunk(*itr);
        }
    }

    ownCatalog->replaceChunks(storageIndex, newCatalog->m_chunks);

    QnStorageDbPtr sdb = qnStorageDbPool->getSDB(storage);
    if (sdb)
        sdb->replaceChunks(cameraUniqueId, catalog, newCatalog->m_chunks);
}

DeviceFileCatalogPtr QnStorageManager::getFileCatalogInternal(const QString& cameraUniqueId, QnServer::ChunksCatalog catalog)
{
    QnMutexLocker lock( &m_mutexCatalog );
    FileCatalogMap& catalogMap = m_devFileCatalog[catalog];
    DeviceFileCatalogPtr fileCatalog = catalogMap[cameraUniqueId];
    if (fileCatalog == 0)
    {
        fileCatalog = DeviceFileCatalogPtr(
            new DeviceFileCatalog(
                cameraUniqueId,
                catalog,
                m_role
            )
       );
        catalogMap[cameraUniqueId] = fileCatalog;
    }
    return fileCatalog;
}

QnStorageResourcePtr QnStorageManager::extractStorageFromFileName(int& storageIndex, const QString& fileName, QString& uniqueId, QString& quality)
{
    // 1.4 to 1.5 compatibility notes:
    // 1.5 prevent duplicates path to same physical storage (aka c:/test and c:/test/)
    // for compatibility with 1.4 I keep all such patches as difference keys to same storage
    // In other case we are going to lose archive from 1.4 because of storage_index is different for same physical folder
    // If several storage keys are exists, function return minimal storage index

    storageIndex = -1;
    const StorageMap storages = getAllStorages();
    QnStorageResourcePtr ret;
    int matchLen = 0;
    QString storageUrl;
    for(StorageMap::const_iterator itr = storages.constBegin(); itr != storages.constEnd(); ++itr)
    {
        QString root = closeDirPath(itr.value()->getUrl());
        if (fileName.startsWith(root))
        {
            if (matchLen < root.size())
            {
                matchLen = root.size();
                ret = *itr;
                storageUrl = root;
            }
        }
    }

    if (ret)
    {
        QString separator = getPathSeparator(storageUrl);
        int qualityLen = fileName.indexOf(separator, storageUrl.length()+1) - storageUrl.length();
        quality = fileName.mid(storageUrl.length(), qualityLen);
        int idPos = storageUrl.length() + qualityLen;
        uniqueId = fileName.mid(idPos+1, fileName.indexOf(separator, idPos+1) - idPos-1);
        storageIndex = qnStorageDbPool->getStorageIndex(ret);
    }
    return ret;
}

QnStorageResourcePtr QnStorageManager::getStorageByUrlExact(const QString& storageUrl)
{
    QnMutexLocker lock(&m_mutexStorages);
    for(StorageMap::const_iterator itr = m_storageRoots.constBegin(); itr != m_storageRoots.constEnd(); ++itr)
    {
        QString root = (*itr)->getUrl();
        if (storageUrl == root)
            return *itr;
    }
    return QnStorageResourcePtr();
}

QnStorageResourcePtr QnStorageManager::getStorageByUrlInternal(const QString& fileName)
{
    QnMutexLocker lock( &m_mutexStorages );
    QnStorageResourcePtr ret;
    int matchLen = 0;
    for(StorageMap::const_iterator itr = m_storageRoots.constBegin(); itr != m_storageRoots.constEnd(); ++itr)
    {
        QString root = (*itr)->getUrl();
        if (fileName.startsWith(root))
        {
            if (matchLen < root.size())
            {
                matchLen = root.size();
                ret = *itr;
            }
        }
    }
    return ret;
}

bool QnStorageManager::renameFileWithDuration(
    const QString               &oldName,
    QString                     &newName,
    int64_t                     duration,
    const QnStorageResourcePtr  &storage
)
{
    auto separator    = getPathSeparator(oldName);
    auto lastSepIndex = oldName.lastIndexOf(separator);

    auto fname = oldName.mid(lastSepIndex + 1);
    auto fpath = oldName.mid(0, lastSepIndex+1);

    if (fname.indexOf(lit("_")) != -1)
        return true; // file's already been renamed

    auto nameParts = fname.split(lit("."));
    newName = nameParts[0] + lit("_") + QString::number(duration);

    for (int i = 1; i < nameParts.size(); ++i)
        newName += lit(".") + nameParts[i];

    newName = fpath + newName;
    return storage->renameFile(oldName, newName);
}

bool QnStorageManager::fileFinished(int durationMs, const QString& fileName, QnAbstractMediaStreamDataProvider* /*provider*/, qint64 fileSize)
{
    int storageIndex;
    QString quality;
    QString cameraUniqueId;
    QnStorageResourcePtr storage = extractStorageFromFileName(storageIndex, fileName, cameraUniqueId, quality);
    if (!storage)
        return false;

    QString newName;
    bool renameOK = false;
    if (!m_isRenameDisabled)
    {
        renameOK = renameFileWithDuration(fileName, newName, durationMs, storage);
        if (!renameOK)
            qDebug() << lit("File %1 rename failed").arg(fileName);
    }

    DeviceFileCatalogPtr catalog = getFileCatalog(cameraUniqueId, quality);
    if (catalog == 0)
        return false;
    auto chunk = catalog->updateDuration(durationMs, fileSize, renameOK);
    if (chunk.startTimeMs != -1)
    {
        QnStorageDbPtr sdb = qnStorageDbPool->getSDB(storage);
        if (sdb)
            sdb->addRecord(cameraUniqueId, DeviceFileCatalog::catalogByPrefix(quality), chunk);
        return true;
    }
    else if (renameOK)
        qnFileDeletor->deleteFile(newName, storage->getId());
    return false;
}

bool QnStorageManager::fileStarted(const qint64& startDateMs, int timeZone, const QString& fileName, QnAbstractMediaStreamDataProvider* /*provider*/)
{
    int storageIndex;
    QString quality, mac;

    QnStorageResourcePtr storage = extractStorageFromFileName(
        storageIndex,
        fileName,
        mac,
        quality
    );
    if (!storage)
        return false;

    if (storageIndex == -1)
        return false;
    //if (provider)
    //    storage->addBitrate(provider);

    DeviceFileCatalogPtr catalog = getFileCatalog(mac.toUtf8(), quality);
    if (catalog == 0)
        return false;
    DeviceFileCatalog::Chunk chunk(
        startDateMs,
        storageIndex,
        DeviceFileCatalog::Chunk::FILE_INDEX_NONE,
        -1,
        (qint16) timeZone
    );
    catalog->addRecord(chunk);
    return true;
}

// data migration from previous versions

void QnStorageManager::doMigrateCSVCatalog(QnStorageResourcePtr extraAllowedStorage) {
    for (int i = 0; i < QnServer::ChunksCatalogCount; ++i)
        doMigrateCSVCatalog(static_cast<QnServer::ChunksCatalog>(i), extraAllowedStorage);
}

QnStorageResourcePtr QnStorageManager::findStorageByOldIndex(int oldIndex)
{
    for(auto itr = m_oldStorageIndexes.begin(); itr != m_oldStorageIndexes.end(); ++itr)
    {
        for(int idx: itr.value())
        {
            if (oldIndex == idx)
                return getStorageByUrl(itr.key(), QnServer::StoragePool::Both);
        }
    }
    return QnStorageResourcePtr();
}

bool QnStorageManager::writeCSVCatalog(const QString& fileName, const QVector<DeviceFileCatalog::Chunk> chunks)
{
    QFile file(fileName);
    if (!file.open(QFile::WriteOnly))
        return false;

    QTextStream str(&file);
    str << "timezone; start; storage; index; duration\n"; // write CSV header

    for (const auto& chunk: chunks)
        str << chunk.timeZone << ';' << chunk.startTimeMs  << ';' << chunk.storageIndex << ';' << chunk.fileIndex << ';' << chunk.durationMs << '\n';
    str.flush();

    file.close();
    return true;
}

void QnStorageManager::backupFolderRecursive(const QString& srcDir, const QString& dstDir)
{
    for (const auto& dirEntry: QDir(srcDir).entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot))
    {
        QString srcFileName = closeDirPath(srcDir) + dirEntry.fileName();
        QString dstFileName = closeDirPath(dstDir) + dirEntry.fileName();
        if (dirEntry.isDir())
            backupFolderRecursive(srcFileName, dstFileName);
        else if (dirEntry.isFile()) {
            QDir().mkpath(dstDir);
            if (!QFile::exists(dstFileName))
                QFile::copy(srcFileName, dstFileName);
        }
    }
}

void QnStorageManager::doMigrateCSVCatalog(QnServer::ChunksCatalog catalogType, QnStorageResourcePtr extraAllowedStorage)
{
    QnMutexLocker lock( &m_csvMigrationMutex );

    QString base = closeDirPath(getDataDirectory());
    QString separator = getPathSeparator(base);
    //backupFolderRecursive(base + lit("record_catalog"), base + lit("record_catalog_backup"));
    QDir dir(base + QString("record_catalog") + separator + QString("media") + separator + DeviceFileCatalog::prefixByCatalog(catalogType));
    QFileInfoList list = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for(QFileInfo fi: list)
    {
        QByteArray mac = fi.fileName().toUtf8();
        DeviceFileCatalogPtr catalogFile(new DeviceFileCatalog(mac, catalogType, m_role));
        QString catalogName = closeDirPath(fi.absoluteFilePath()) + lit("title.csv");
        QVector<DeviceFileCatalog::Chunk> notMigratedChunks;
        if (catalogFile->fromCSVFile(catalogName))
        {
            for(const DeviceFileCatalog::Chunk& chunk: catalogFile->m_chunks)
            {
                QnStorageResourcePtr storage = findStorageByOldIndex(chunk.storageIndex);
                if (storage && storage != extraAllowedStorage && storage->getStatus() != Qn::Online)
                    storage.clear();

                QnStorageDbPtr sdb = storage ? qnStorageDbPool->getSDB(storage) : QnStorageDbPtr();
                if (sdb)
                {
                    if (catalogFile->csvMigrationCheckFile(chunk, storage))
                    {
                        if (chunk.durationMs > QnRecordingManager::RECORDING_CHUNK_LEN*1000 * 2 || chunk.durationMs < 1)
                        {
                            const QString fileName = catalogFile->fullFileName(chunk);
                            qWarning() << "File " << fileName << "has invalid duration " << chunk.durationMs/1000.0 << "s and corrupted. Delete file from catalog";
                            storage->removeFile(fileName);
                        }
                        else {
                            sdb->addRecord(mac, catalogType, chunk);
                        }
                    }
                }
                else {
                    notMigratedChunks << chunk;
                }
            }
            qnStorageDbPool->flush();
            QFile::remove(catalogName);
            if (!notMigratedChunks.isEmpty())
                writeCSVCatalog(catalogName, notMigratedChunks);
        }
        dir.rmdir(fi.absoluteFilePath());
    }
}

bool QnStorageManager::isStorageAvailable(int storage_index) const {
    QnStorageResourcePtr storage = storageRoot(storage_index);
    return storage && storage->getStatus() == Qn::Online;
}

bool QnStorageManager::isStorageAvailable(const QnStorageResourcePtr& storage) const {
    return storage && storage->getStatus() == Qn::Online;
}


std::vector<QnUuid> QnStorageManager::getCamerasWithArchiveHelper() const
{
    QnMutexLocker locker(&m_mutexCatalog);
    std::set<QString> internalData;
    std::vector<QnUuid> result;
    getCamerasWithArchiveInternal(internalData, m_devFileCatalog[QnServer::LowQualityCatalog]);
    getCamerasWithArchiveInternal(internalData, m_devFileCatalog[QnServer::HiQualityCatalog]);
    for(const QString& uniqueId: internalData) {
        const QnResourcePtr cam = qnResPool->getResourceByUniqueId(uniqueId);
        if (cam)
            result.push_back(cam->getId());
    }
    return result;
}

std::vector<QnUuid> QnStorageManager::getCamerasWithArchive()
{
    std::vector<QnUuid> archivedListNormal =
        qnNormalStorageMan->getCamerasWithArchiveHelper();

    std::vector<QnUuid> archivedListBackup =
        qnBackupStorageMan->getCamerasWithArchiveHelper();

    std::sort(archivedListNormal.begin(), archivedListNormal.end());
    std::sort(archivedListBackup.begin(), archivedListBackup.end());

    std::vector<QnUuid> resultList;
    std::set_union(archivedListNormal.cbegin(),
                   archivedListNormal.cend(),
                   archivedListBackup.cbegin(),
                   archivedListBackup.cend(),
                   std::back_inserter(resultList));

    return resultList;
}

void QnStorageManager::getCamerasWithArchiveInternal(std::set<QString>& result, const FileCatalogMap& catalogMap ) const
{
    for(auto itr = catalogMap.begin(); itr != catalogMap.end(); ++itr)
    {
        const DeviceFileCatalogPtr& catalog = itr.value();
        if (!catalog->isEmpty())
            result.insert(catalog->cameraUniqueId());
    }
}

QnStorageResourcePtr QnStorageManager::getStorageByUrl(const QString &storageUrl,
                                                       QnServer::StoragePool pool)
{
    QnStorageResourcePtr result;
    if ((pool & QnServer::StoragePool::Normal) == QnServer::StoragePool::Normal) {
        result = qnNormalStorageMan->getStorageByUrlInternal(storageUrl);
        if (result)
            return result;
    }
    if ((pool & QnServer::StoragePool::Backup) == QnServer::StoragePool::Backup) {
        result = qnBackupStorageMan->getStorageByUrlInternal(storageUrl);
    }
    return result;
}

const std::array<QnServer::StoragePool, 2> QnStorageManager::getPools() {
    static const
    std::array<QnServer::StoragePool, 2> pools = {QnServer::StoragePool::Normal,
                                                  QnServer::StoragePool::Backup};
    return pools;
}

QnScheduleSync* QnStorageManager::scheduleSync() const
{
    return m_scheduleSync.get();
}
