#include "storage_manager.h"

#include <QtCore/QDir>

#include <api/app_server_connection.h>

#include <utils/fs/file.h>
#include <utils/common/util.h>
#include <utils/common/log.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/camera_bookmark.h>

#include <recorder/server_stream_recorder.h>
#include <recorder/recording_manager.h>

#include <platform/monitoring/global_monitor.h>
#include <platform/platform_abstraction.h>
#include <plugins/resource/server_archive/dualquality_helper.h>

#include "plugins/storage/file_storage/file_storage_resource.h"

#include <media_server/serverutil.h>
#include "file_deletor.h"
#include "utils/common/synctime.h"
#include "motion/motion_helper.h"
#include "common/common_module.h"


static const qint64 BALANCE_BY_FREE_SPACE_THRESHOLD = 1024*1024 * 500;
static const int OFFLINE_STORAGES_TEST_INTERVAL = 1000 * 30;
static const int DB_UPDATE_PER_RECORDS = 128;
static const qint64 MSECS_PER_DAY = 1000ll * 3600ll * 24ll;


class RebuildAsyncTask: public QnLongRunnable
{
public:
    RebuildAsyncTask(QnStorageManager* owner) : m_storageManager(owner){}
    virtual void run() override
    {
        m_storageManager->rebuildCatalogIndexInternal();
    }
private:
    QnStorageManager* m_storageManager;
};

class ScanMediaFilesTask: public QnLongRunnable
{
private:
    QMap<DeviceFileCatalogPtr, qint64> m_data; // key - catalog, value - start scan time;
    QnStorageManager* m_owner;
    QnStorageResourceList m_storagesToScan;
    qint64 m_endScanTime;
public:
    ScanMediaFilesTask(QnStorageManager* owner): QnLongRunnable(), m_owner(owner)
    {
        for(const DeviceFileCatalogPtr& catalog: owner->m_devFileCatalog[QnServer::LowQualityCatalog])
            m_data.insert(catalog, catalog->lastChunkStartTime());
        for(const DeviceFileCatalogPtr& catalog: owner->m_devFileCatalog[QnServer::HiQualityCatalog])
            m_data.insert(catalog, catalog->lastChunkStartTime());
        m_storagesToScan = m_owner->getStorages();
        m_endScanTime = qnSyncTime->currentMSecsSinceEpoch();

        m_owner->m_rebuildProgress = 0;
        m_owner->m_rebuildState = QnStorageManager::RebuildState_Initial;
    }
    virtual void run() override
    {
        int steps = m_data.size() * m_storagesToScan.size();
        for(const QnStorageResourcePtr& storage: m_storagesToScan)
        {
            for(auto itr = m_data.begin(); itr != m_data.end(); ++itr) 
            {
                DeviceFileCatalog::ScanFilter filter;
                filter.scanPeriod.startTimeMs = itr.value();
                filter.scanPeriod.durationMs = qMax(1ll, m_endScanTime - filter.scanPeriod.startTimeMs);
                m_owner->partialMediaScan(itr.key(), storage, filter);
                m_owner->m_rebuildProgress += 1.0 / steps;
                if (needToStop())
                    return;
            }
        }
        
        m_owner->m_rebuildProgress = 1.0;
        m_owner->m_rebuildState = QnStorageManager::RebuildState_None;
    }
};

class TestStorageThread: public QnLongRunnable
{
public:
    TestStorageThread(QnStorageManager* owner): m_owner(owner) {}
    virtual void run() override
    {
        QnStorageManager::StorageMap storageRoots = m_owner->getAllStorages();

        for (QnStorageManager::StorageMap::const_iterator itr = storageRoots.constBegin(); itr != storageRoots.constEnd(); ++itr)
        {
            if (needToStop())
                break;
            QnFileStorageResourcePtr fileStorage = qSharedPointerDynamicCast<QnFileStorageResource> (*itr);
            Qn::ResourceStatus status = fileStorage->isStorageAvailable() ? Qn::Online : Qn::Offline;
            if (fileStorage->getStatus() != status)
                m_owner->changeStorageStatus(fileStorage, status);
        }
    }
private:
    QnStorageManager* m_owner;
};

TestStorageThread* QnStorageManager::m_testStorageThread;

// -------------------- QnStorageManager --------------------


static QnStorageManager* QnStorageManager_instance = nullptr;

QnStorageManager::QnStorageManager():
    m_mutexStorages(QnMutex::Recursive),
    m_mutexCatalog(QnMutex::Recursive),
    m_storagesStatisticsReady(false),
    m_catalogLoaded(false),
    m_warnSended(false),
    m_isWritableStorageAvail(false),
    m_rebuildState(RebuildState_None),
    m_rebuildProgress(0),
    m_asyncRebuildTask(0),
    m_asyncPartialScan(0),
    m_initInProgress(true)
{
    m_lastTestTime.restart();
    m_storageWarnTimer.restart();
    m_testStorageThread = new TestStorageThread(this);

    assert( QnStorageManager_instance == nullptr );
    QnStorageManager_instance = this;
    connect(qnResPool, &QnResourcePool::resourceAdded, this, &QnStorageManager::onNewResource, Qt::DirectConnection);
    m_oldStorageIndexes = deserializeStorageFile();
}

//std::deque<DeviceFileCatalog::Chunk> QnStorageManager::correctChunksFromMediaData(const DeviceFileCatalogPtr &fileCatalog, const QnStorageResourcePtr &storage, const std::deque<DeviceFileCatalog::Chunk>& chunks)
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
        qnFileDeletor->deleteFile(emptyFile.fileName);

    // add to DB
    QnStorageDbPtr sdb = getSDB(storage);
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
    fileCatalog->addChunks(newChunks);
}


void QnStorageManager::initDone()
{
    m_catalogLoaded = true;
    m_rebuildProgress = 1.0;

    m_initInProgress = false;
    for(const QnStorageResourcePtr& storage: getStorages()) {
        if (storage->getStatus() == Qn::Online)
            addDataFromDatabase(storage);
    }
    disconnect(qnResPool);
    connect(qnResPool, &QnResourcePool::resourceAdded, this, &QnStorageManager::onNewResource, Qt::QueuedConnection);
    connect(qnResPool, &QnResourcePool::resourceRemoved, this, &QnStorageManager::onDelResource, Qt::QueuedConnection);

    m_asyncPartialScan = new ScanMediaFilesTask(this);
    m_asyncPartialScan->start();
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

bool QnStorageManager::loadFullFileCatalog(const QnStorageResourcePtr &storage, bool isRebuild, qreal progressCoeff)
{
    QnStorageDbPtr sdb = getSDB(storage);
    if (!sdb)
        return false;
    if (!isRebuild)
    {
        if (!m_initInProgress)
            addDataFromDatabase(storage);
    }
    else {
        // load from media folder
        for (int i = 0; i < QnServer::ChunksCatalogCount; ++i) {
            loadFullFileCatalogFromMedia(storage, static_cast<QnServer::ChunksCatalog> (i), progressCoeff / QnServer::ChunksCatalogCount);
        }
    }

    return true;
}

void QnStorageManager::addDataFromDatabase(const QnStorageResourcePtr &storage)
{
    QnStorageDbPtr sdb = getSDB(storage);
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

double QnStorageManager::rebuildProgress() const
{
    return m_rebuildProgress;
}

void QnStorageManager::rebuildCatalogIndexInternal()
{
    {
        SCOPED_MUTEX_LOCK( lock, &m_mutexCatalog);
        m_rebuildProgress = 0;
        //m_catalogLoaded = false;
        m_rebuildCancelled = false;
        /*
        for(const DeviceFileCatalogPtr& catalog:  m_devFileCatalogHi)
            catalog->beforeRebuildArchive();
        for(const DeviceFileCatalogPtr& catalog:  m_devFileCatalogLow)
            catalog->beforeRebuildArchive();
        m_devFileCatalogHi.clear();
        m_devFileCatalogLow.clear();
        */
        DeviceFileCatalog::setRebuildArchive(DeviceFileCatalog::Rebuild_All);
    }

    for (const QnStorageResourcePtr& storage: m_storageRoots.values()) {
        if (storage->getStatus() == Qn::Online)
            loadFullFileCatalog(storage, true, 1.0 / m_storageRoots.size());
    }

    m_rebuildState = RebuildState_None;
    m_catalogLoaded = true;
    m_rebuildProgress = 1.0;

    if(!m_rebuildCancelled)
        emit rebuildFinished();
}

void QnStorageManager::rebuildCatalogAsync()
{
    if (m_rebuildState == RebuildState_None) {
        m_rebuildProgress = 0.0;
        //setRebuildState(QnStorageManager::RebuildState_WaitForRecordersStopped);
        setRebuildState(QnStorageManager::RebuildState_Started);
    }
}

void QnStorageManager::cancelRebuildCatalogAsync()
{
    if (m_rebuildState != RebuildState_None) 
    {
        NX_LOG("Catalog rebuild operation is canceled", cl_logINFO);
        m_rebuildCancelled = true;
        DeviceFileCatalog::setRebuildArchive(DeviceFileCatalog::Rebuild_None);
        setRebuildState(RebuildState_None);
    }
}

void QnStorageManager::setRebuildState(RebuildState state)
{
    m_rebuildState = state;
    if(m_rebuildState == RebuildState_Started) 
    {
        if (m_asyncRebuildTask == 0)
            m_asyncRebuildTask = new RebuildAsyncTask(this);
        if (!m_asyncRebuildTask->isRunning())
            m_asyncRebuildTask->start();
    }
}

QnStorageManager::RebuildState QnStorageManager::rebuildState() const
{
    return m_rebuildState;        
}


bool QnStorageManager::isCatalogLoaded() const
{
    return m_catalogLoaded;
}

void QnStorageManager::loadFullFileCatalogFromMedia(const QnStorageResourcePtr &storage, QnServer::ChunksCatalog catalog, qreal progressCoeff)
{
    QDir dir(closeDirPath(storage->getPath()) + DeviceFileCatalog::prefixByCatalog(catalog));
    QFileInfoList list = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for(const QFileInfo& fi: list)
    {
        if (m_rebuildState != RebuildState_Started)
            return; // cancel rebuild

        QString cameraUniqueId = fi.fileName();

        qint64 rebuildEndTime = qnSyncTime->currentMSecsSinceEpoch() - QnRecordingManager::RECORDING_CHUNK_LEN * 1250;
        DeviceFileCatalogPtr newCatalog(new DeviceFileCatalog(cameraUniqueId, catalog));
        QnTimePeriod rebuildPeriod = QnTimePeriod(0, rebuildEndTime);
        newCatalog->doRebuildArchive(storage, rebuildPeriod);
        
        DeviceFileCatalogPtr fileCatalog = getFileCatalogInternal(cameraUniqueId, catalog);
        replaceChunks(rebuildPeriod, storage, newCatalog, cameraUniqueId, catalog);

        m_rebuildProgress += progressCoeff / (double) list.size();
    }
}

QString QnStorageManager::toCanonicalPath(const QString& path)
{
    QString result = QDir::toNativeSeparators(path);
    if (result.endsWith(QDir::separator()))
        result.chop(1);
    return result;
}

int QnStorageManager::getStorageIndex(const QnStorageResourcePtr& storage)
{
    return detectStorageIndex(storage->getPath());
}

// determine storage index (aka 16 bit hash)
int QnStorageManager::detectStorageIndex(const QString& p)
{
    SCOPED_MUTEX_LOCK( lock, &m_mutexStorages);
    QString path = toCanonicalPath(p);

    if (m_storageIndexes.contains(path))
    {
        return *m_storageIndexes.value(path).begin();
    }
    else {
        int index = -1;
        for (const QSet<int>& indexes: m_storageIndexes.values()) 
        {
            for (const int& value: indexes) 
                index = qMax(index, value);
        }
        index++;
        m_storageIndexes.insert(path, QSet<int>() << index);
        return index;
    }
}

static QString getLocalGuid()
{
    QString simplifiedGUID = qnCommon->moduleGUID().toString();
    simplifiedGUID = simplifiedGUID.replace("{", "");
    simplifiedGUID = simplifiedGUID.replace("}", "");
    return simplifiedGUID;
}

static const QString dbRefFileName( QLatin1String("%1_db_ref.guid") );

static bool getDBPath( const QnStorageResourcePtr& storage, QString* const dbDirectory )
{
    QString storagePath = storage->getPath();
    const QString dbRefFilePath = closeDirPath(storagePath) + dbRefFileName.arg(getLocalGuid());

    QByteArray dbRefGuidStr;
    //checking for file db_ref.guid existence
    if( QFile::exists(dbRefFilePath) )
    {
        //have to use db from data directory, not from storage
        //reading guid from file
        QFile dbGuidFile( dbRefFilePath );
        if( !dbGuidFile.open( QIODevice::ReadOnly ) )
            return false;
        dbRefGuidStr = dbGuidFile.readAll();
    }

    if( !dbRefGuidStr.isEmpty() )
    {
        *dbDirectory = QDir(getDataDirectory() + "/storage_db/" + dbRefGuidStr).absolutePath();
        return true;
    }

#ifdef _WIN32
    //on windows always placing db to a storage directory
    *dbDirectory = storagePath;
    return true;
#else
    //On linux, sqlite db cannot be safely placed on a network partition due to lock problem
        //So, for such storages creating DB in data dir

    QList<QnPlatformMonitor::PartitionSpace> partitions = 
        qnPlatform->monitor()->QnPlatformMonitor::totalPartitionSpaceInfo(
            QnPlatformMonitor::NetworkPartition );

    for( const QnPlatformMonitor::PartitionSpace& partition: partitions )
    {
        if( !storagePath.startsWith( partition.path ) )
            continue;

        dbRefGuidStr = QUuid::createUuid().toString().toLatin1();
        if( dbRefGuidStr.size() < 2 )
            return false;   //bad guid, somehow
        //removing {}
        dbRefGuidStr.remove( dbRefGuidStr.size()-1, 1 );
        dbRefGuidStr.remove( 0, 1 );
        //saving db ref guid to file on storage
        QFile dbGuidFile( dbRefFilePath );
        if( !dbGuidFile.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
            return false;
        if( dbGuidFile.write( dbRefGuidStr ) != dbRefGuidStr.size() )
            return false;
        dbGuidFile.close();

        storagePath = QDir(getDataDirectory() + "/storage_db/" + dbRefGuidStr).absolutePath();
        if( !QDir().mkpath( storagePath ) )
            return false;
        break;
    }

    *dbDirectory = storagePath;
    return true;
#endif
}

QnStorageDbPtr QnStorageManager::getSDB(const QnStorageResourcePtr &storage)
{
    SCOPED_MUTEX_LOCK( lock, &m_sdbMutex);
    QnStorageDbPtr sdb = m_chunksDB[storage->getPath()];
    if (!sdb) 
    {
        QString simplifiedGUID = getLocalGuid();
        QString dbPath;
        if( !getDBPath(storage, &dbPath) )
        {
            NX_LOG( lit("Failed to file path to storage DB file. Storage is not writable?"), cl_logWARNING );
            return QnStorageDbPtr();
        }
        QString fileName = closeDirPath(dbPath) + QString::fromLatin1("%1_media.sqlite").arg(simplifiedGUID);
        QString oldFileName = closeDirPath(dbPath) + QString::fromLatin1("media.sqlite");
        if (QFile::exists(oldFileName) && !QFile::exists(fileName))
            QFile::rename(oldFileName, fileName);

        sdb = m_chunksDB[storage->getPath()] = QnStorageDbPtr(new QnStorageDb(getStorageIndex(storage)));
        if (!sdb->open(fileName))
        {
            qWarning() << "can't initialize sqlLite database! Actions log is not created!";
            return QnStorageDbPtr();
        }
    }
    return sdb;
}

void QnStorageManager::addStorage(const QnStorageResourcePtr &storage)
{
    {
        int storageIndex = detectStorageIndex(storage->getPath());
        SCOPED_MUTEX_LOCK( lock, &m_mutexStorages);
        m_storagesStatisticsReady = false;
    
        NX_LOG(QString("Adding storage. Path: %1. SpaceLimit: %2MiB. Currently available: %3MiB").arg(storage->getPath()).arg(storage->getSpaceLimit() / 1024 / 1024).arg(storage->getFreeSpace() / 1024 / 1024), cl_logINFO);

        removeStorage(storage); // remove existing storage record if exists
        //QnStorageResourcePtr oldStorage = removeStorage(storage); // remove existing storage record if exists
        //if (oldStorage)
        //    storage->addWritedSpace(oldStorage->getWritedSpace());
        m_storageRoots.insert(storageIndex, storage);
        if (storage->isStorageAvailable())
            storage->setStatus(Qn::Online);
        else
            storage->setStatus(Qn::Offline);

        connect(storage.data(), SIGNAL(archiveRangeChanged(const QnAbstractStorageResourcePtr &, qint64, qint64)), 
                this, SLOT(at_archiveRangeChanged(const QnAbstractStorageResourcePtr &, qint64, qint64)), Qt::DirectConnection);
    }
#if 1
    if (m_catalogLoaded && storage->getStatus() == Qn::Online) {
        doMigrateCSVCatalog();
        addDataFromDatabase(storage);
    }
#else
    if (storage->getStatus() == Qn::Online)
        loadFullFileCatalog(storage);
#endif
    updateStorageStatistics();
}

class AddStorageTask: public QRunnable
{
public:
    AddStorageTask(QnStorageResourcePtr storage): m_storage(storage) {}
    void run()
    {
        qnStorageMan->addStorage(m_storage);
    }
private:
    QnStorageResourcePtr m_storage;
};

void QnStorageManager::onNewResource(const QnResourcePtr &resource)
{
    connect(resource.data(), &QnResource::resourceChanged, this, &QnStorageManager::at_storageChanged);
    QnStorageResourcePtr storage = qSharedPointerDynamicCast<QnStorageResource>(resource);
    if (storage && storage->getParentId() == qnCommon->moduleGUID()) 
    {
#if 0
        if (m_initInProgress) {
            addStorage(storage);
        }
        else {
            AddStorageTask *task = new AddStorageTask(storage);
            task->setAutoDelete(true);
            QThreadPool::globalInstance()->start(task);
        }
#else
        addStorage(storage);
#endif
    }
}

void QnStorageManager::onDelResource(const QnResourcePtr &resource)
{
    QnStorageResourcePtr storage = qSharedPointerDynamicCast<QnStorageResource>(resource);
    if (storage && storage->getParentId() == qnCommon->moduleGUID())  {
        removeStorage(storage);
        updateStorageStatistics();
    }
}

QStringList QnStorageManager::getAllStoragePathes() const
{
    return m_storageIndexes.keys();
}

void QnStorageManager::removeStorage(const QnStorageResourcePtr &storage)
{
    SCOPED_MUTEX_LOCK( lock, &m_mutexStorages);
    m_storagesStatisticsReady = false;

    // remove existing storage record if exists
    for (StorageMap::iterator itr = m_storageRoots.begin(); itr != m_storageRoots.end();)
    {
        if (itr.value()->getId() == storage->getId()) {
            QnStorageResourcePtr oldStorage = itr.value();
            itr = m_storageRoots.erase(itr);
        }
        else {
            ++itr;
        }
    }
}

void QnStorageManager::at_storageChanged(const QnResourcePtr &)
{
    {
        SCOPED_MUTEX_LOCK( lock, &m_mutexStorages);
        m_storagesStatisticsReady = false;
    }
    updateStorageStatistics();
}

bool QnStorageManager::existsStorageWithID(const QnAbstractStorageResourceList& storages, const QnUuid &id) const
{
    for(const QnAbstractStorageResourcePtr& storage: storages)
    {
        if (storage->getId() == id)
            return true;
    }
    return false;
}

void QnStorageManager::removeAbsentStorages(const QnAbstractStorageResourceList &newStorages)
{
    SCOPED_MUTEX_LOCK( lock, &m_mutexStorages);
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
    QnStorageManager_instance = nullptr;

    stopAsyncTasks();
}

QnStorageManager* QnStorageManager::instance()
{
    return QnStorageManager_instance;
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

void QnStorageManager::getTimePeriodInternal(QVector<QnTimePeriodList> &periods, const QnNetworkResourcePtr &camera, qint64 startTime, qint64 endTime, qint64 detailLevel, const DeviceFileCatalogPtr &catalog)
{
    if (catalog) {
        periods << catalog->getTimePeriods(startTime, endTime, detailLevel);
        if (!periods.last().isEmpty())
        {
            QnTimePeriod& lastPeriod = periods.last().last();
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
    DeviceFileCatalogPtr catalog = getFileCatalog(cameraUniqueId, QnServer::HiQualityCatalog);
    if (catalog && catalog->containTime(timeMs))
        return true;

    catalog = getFileCatalog(cameraUniqueId, QnServer::LowQualityCatalog);
    return catalog && catalog->containTime(timeMs);
}


QnTimePeriodList QnStorageManager::getRecordedPeriods(const QnVirtualCameraResourceList &cameras, qint64 startTime, qint64 endTime, qint64 detailLevel, const QList<QnServer::ChunksCatalog> &catalogs) {
    QVector<QnTimePeriodList> periods;
    for (const QnVirtualCameraResourcePtr &camera: cameras) {
        QString cameraUniqueId = camera->getUniqueId();
        for (int i = 0; i < QnServer::ChunksCatalogCount; ++i) {
            QnServer::ChunksCatalog catalog = static_cast<QnServer::ChunksCatalog> (i);
            if (!catalogs.contains(catalog))
                continue;

            //TODO: #GDM #Bookmarks forbid bookmarks for the DTS cameras
            if (camera->isDtsBased()) {
                if (catalog == QnServer::HiQualityCatalog) // both hi- and low-quality chunks are loaded with this method
                    periods << camera->getDtsTimePeriods(startTime, endTime, detailLevel);
            } else {
                getTimePeriodInternal(periods, camera, startTime, endTime, detailLevel, getFileCatalog(cameraUniqueId, catalog));
            }
        }

    }

    return QnTimePeriodList::mergeTimePeriods(periods);
}

void QnStorageManager::clearSpace()
{
    if (!m_catalogLoaded)
        return;

    testOfflineStorages();
    {
        SCOPED_MUTEX_LOCK( lock, &m_sdbMutex);
        for(const QnStorageDbPtr& sdb: m_chunksDB) {
            if (sdb)
                sdb->beforeDelete();
        }
    }

    // 1. delete old data if cameras have max duration limit
    clearMaxDaysData();

    // 2. free storage space
    const QSet<QnStorageResourcePtr> storages = getWritableStorages();
    for(const QnStorageResourcePtr& storage: storages)
        clearOldestSpace(storage, true);
    for(const QnStorageResourcePtr& storage: storages)
        clearOldestSpace(storage, false);

    {
        SCOPED_MUTEX_LOCK( lock, &m_sdbMutex);
        for(const QnStorageDbPtr& sdb: m_chunksDB) {
            if (sdb)
                sdb->afterDelete();
        }
    }

    clearUnusedMotion();
    //clearCameraHistory();
}

QnStorageManager::StorageMap QnStorageManager::getAllStorages() const 
{ 
    SCOPED_MUTEX_LOCK( lock, &m_mutexStorages); 
    return m_storageRoots; 
} 

QnStorageResourceList QnStorageManager::getStorages() const 
{
    SCOPED_MUTEX_LOCK( lock, &m_mutexStorages);
    return m_storageRoots.values().toSet().toList(); // remove storage duplicates. Duplicates are allowed in sake for v1.4 compatibility
}

void QnStorageManager::deleteRecordsToTime(DeviceFileCatalogPtr catalog, qint64 minTime)
{
    int idx = catalog->findFileIndex(minTime, DeviceFileCatalog::OnRecordHole_NextChunk);
    if (idx != -1) {
        QVector<DeviceFileCatalog::Chunk> deletedChunks = catalog->deleteRecordsBefore(idx);
        for(const DeviceFileCatalog::Chunk& chunk: deletedChunks) 
            clearDbByChunk(catalog, chunk);
    }
}

void QnStorageManager::clearDbByChunk(DeviceFileCatalogPtr catalog, const DeviceFileCatalog::Chunk& chunk)
{
    {
        QnStorageResourcePtr storage = storageRoot(chunk.storageIndex);
        if (storage) {
            QnStorageDbPtr sdb = getSDB(storage);
            if (sdb)
                sdb->deleteRecords(catalog->cameraUniqueId(), catalog->getRole(), chunk.startTimeMs);
        }
    }
}

void QnStorageManager::clearMaxDaysData()
{
    clearMaxDaysData(m_devFileCatalog[QnServer::HiQualityCatalog]);
    clearMaxDaysData(m_devFileCatalog[QnServer::LowQualityCatalog]);
}

void QnStorageManager::clearMaxDaysData(const FileCatalogMap &catalogMap)
{
    SCOPED_MUTEX_LOCK( lock, &m_mutexCatalog);
    for(const DeviceFileCatalogPtr& catalog: catalogMap.values()) {
        QnSecurityCamResourcePtr camera = qnResPool->getResourceByUniqId(catalog->cameraUniqueId()).dynamicCast<QnSecurityCamResource>();
        if (camera && camera->maxDays() > 0) {
            qint64 timeToDelete = qnSyncTime->currentMSecsSinceEpoch() - MSECS_PER_DAY * camera->maxDays();
            deleteRecordsToTime(catalog, timeToDelete);
        }
    }
}

void QnStorageManager::clearUnusedMotion()
{
    SCOPED_MUTEX_LOCK( lock, &m_mutexCatalog);

    UsedMonthsMap usedMonths;

    updateRecordedMonths(m_devFileCatalog[QnServer::HiQualityCatalog], usedMonths);
    updateRecordedMonths(m_devFileCatalog[QnServer::LowQualityCatalog], usedMonths);

    for(const DeviceFileCatalogPtr& catalog: m_devFileCatalog[QnServer::HiQualityCatalog].values())
        QnMotionHelper::instance()->deleteUnusedFiles(usedMonths[catalog->cameraUniqueId()].toList(), catalog->cameraUniqueId());
}

/*
void QnStorageManager::clearCameraHistory()
{
    SCOPED_MUTEX_LOCK( lock, &m_mutexCatalog);
    QMap<QString, qint64> minTimes; // min archive time by camera unique ID
    minTimeByCamera(m_devFileCatalog[QnServer::HiQualityCatalog], minTimes);
    minTimeByCamera(m_devFileCatalog[QnServer::LowQualityCatalog], minTimes);

    for(auto itr = minTimes.begin(); itr != minTimes.end(); ++itr) {
        if (itr.value() == AV_NOPTS_VALUE)
            itr.value() == DATETIME_NOW; // delete all history if catalog is empty
    }

    QList<QnCameraHistoryItem> itemsToRemove = QnCameraHistoryPool::instance()->getUnusedItems(minTimes, qnCommon->moduleGUID());
    ec2::AbstractECConnectionPtr ec2Connection = QnAppServerConnectionFactory::getConnection2();
    for(const QnCameraHistoryItem& item: itemsToRemove) {
        ec2::ErrorCode errCode = ec2Connection->getCameraManager()->removeCameraHistoryItemSync(item);
        if (errCode == ec2::ErrorCode::ok)
            QnCameraHistoryPool::instance()->removeCameraHistoryItem(item);
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
                QnSecurityCamResourcePtr camera = qnResPool->getResourceByUniqId(itr.key()).dynamicCast<QnSecurityCamResource>();
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

void QnStorageManager::clearOldestSpace(const QnStorageResourcePtr &storage, bool useMinArchiveDays)
{
    if (storage->getSpaceLimit() == 0)
        return; // unlimited


    QString dir = storage->getPath();

    if (!storage->isNeedControlFreeSpace())
        return;

    qint64 freeSpace = storage->getFreeSpace();
    if (freeSpace == -1)
        return;
    qint64 toDelete = storage->getSpaceLimit() - freeSpace;

    while (toDelete > 0)
    {
        qint64 minTime = 0x7fffffffffffffffll;
        DeviceFileCatalogPtr catalog;
        {
            SCOPED_MUTEX_LOCK( lock, &m_mutexCatalog);
            findTotalMinTime(useMinArchiveDays, m_devFileCatalog[QnServer::HiQualityCatalog], minTime, catalog);
            findTotalMinTime(useMinArchiveDays, m_devFileCatalog[QnServer::LowQualityCatalog], minTime, catalog);
        }
        if (catalog != 0) 
        {
            DeviceFileCatalog::Chunk deletedChunk = catalog->deleteFirstRecord();
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
                if (catalog->isEmpty() && altCatalog->isEmpty())
                    break; // nothing to delete
            }
            else {
                if (catalog->isEmpty())
                    break; // nothing to delete
            }
        }
        else
            break; // nothing to delete

        qint64 freeSpace = storage->getFreeSpace();
        if (freeSpace == -1)
            return;
        toDelete = storage->getSpaceLimit() - freeSpace;
    }

    if (toDelete > 0 && !useMinArchiveDays) {
        if (!m_diskFullWarned[storage->getId()]) {
            QnMediaServerResourcePtr mediaServer = qSharedPointerDynamicCast<QnMediaServerResource> (qnResPool->getResourceById(serverGuid()));
            emit storageFailure(storage, QnBusiness::StorageFullReason);
            m_diskFullWarned[storage->getId()] = true;
        }
    }
    else {
        m_diskFullWarned[storage->getId()] = false;
    }
}

void QnStorageManager::at_archiveRangeChanged(const QnAbstractStorageResourcePtr &resource, qint64 newStartTimeMs, qint64 newEndTimeMs)
{
    Q_UNUSED(newEndTimeMs)
    int storageIndex = detectStorageIndex(resource->getUrl());

    for(const DeviceFileCatalogPtr& catalogHi: m_devFileCatalog[QnServer::HiQualityCatalog])
        catalogHi->deleteRecordsByStorage(storageIndex, newStartTimeMs);
    
    for(const DeviceFileCatalogPtr& catalogLow: m_devFileCatalog[QnServer::LowQualityCatalog])
        catalogLow->deleteRecordsByStorage(storageIndex, newStartTimeMs);

    //TODO: #vasilenko should we delete bookmarks here too?
}

QSet<QnStorageResourcePtr> QnStorageManager::getWritableStorages() const
{
    QSet<QnStorageResourcePtr> result;

    QnStorageManager::StorageMap storageRoots = getAllStorages();
    qint64 bigStorageThreshold = 0;
    for (StorageMap::const_iterator itr = storageRoots.constBegin(); itr != storageRoots.constEnd(); ++itr)
    {
        QnFileStorageResourcePtr fileStorage = qSharedPointerDynamicCast<QnFileStorageResource> (itr.value());
        if (fileStorage && fileStorage->getStatus() != Qn::Offline && fileStorage->isUsedForWriting()) 
        {
            qint64 available = fileStorage->getTotalSpace() - fileStorage->getSpaceLimit();
            bigStorageThreshold = qMax(bigStorageThreshold, available);
        }
    }
    bigStorageThreshold /= BIG_STORAGE_THRESHOLD_COEFF;

    for (StorageMap::const_iterator itr = storageRoots.constBegin(); itr != storageRoots.constEnd(); ++itr)
    {
        QnFileStorageResourcePtr fileStorage = qSharedPointerDynamicCast<QnFileStorageResource> (itr.value());
        if (fileStorage && fileStorage->getStatus() != Qn::Offline && fileStorage->isUsedForWriting()) 
        {
            qint64 available = fileStorage->getTotalSpace() - fileStorage->getSpaceLimit();
            if (available >= bigStorageThreshold)
                result << fileStorage;
        }
    }
    return result;
}

void QnStorageManager::changeStorageStatus(const QnStorageResourcePtr &fileStorage, Qn::ResourceStatus status)
{
    //SCOPED_MUTEX_LOCK( lock, &m_mutexStorages);
    if (status == Qn::Online && fileStorage->getStatus() == Qn::Offline) {
        // add data before storage goes to the writable state
        doMigrateCSVCatalog(fileStorage);
        addDataFromDatabase(fileStorage);
    }

    fileStorage->setStatus(status);
    if (status == Qn::Offline)
        emit storageFailure(fileStorage, QnBusiness::StorageIoErrorReason);
    m_storagesStatisticsReady = false;
}

void QnStorageManager::testOfflineStorages()
{
    SCOPED_MUTEX_LOCK( lock, &m_mutexStorages);

    if (m_lastTestTime.elapsed() < OFFLINE_STORAGES_TEST_INTERVAL || m_testStorageThread->isRunning())
        return;

    m_testStorageThread->start();
    m_lastTestTime.restart();
}

void QnStorageManager::stopAsyncTasks()
{
    if (m_testStorageThread) {
        m_testStorageThread->stop();
        delete m_testStorageThread;
        m_testStorageThread = 0;
    }
    m_rebuildState = RebuildState_None;
    if (m_asyncRebuildTask) {
        m_asyncRebuildTask->stop();
        delete m_asyncRebuildTask;
        m_asyncRebuildTask = 0;
    }

    if (m_asyncPartialScan) {
        m_asyncPartialScan->stop();
        delete m_asyncPartialScan;
        m_asyncPartialScan = 0;
    }
}

void QnStorageManager::updateStorageStatistics()
{
    SCOPED_MUTEX_LOCK( lock, &m_mutexStorages);
    if (m_storagesStatisticsReady) 
        return;

    qint64 totalSpace = 0;
    qint64 minSpace = INT64_MAX;
    QSet<QnStorageResourcePtr> storages = getWritableStorages();
    m_isWritableStorageAvail = !storages.isEmpty();
    for (QSet<QnStorageResourcePtr>::const_iterator itr = storages.constBegin(); itr != storages.constEnd(); ++itr)
    {
        QnFileStorageResourcePtr fileStorage = qSharedPointerDynamicCast<QnFileStorageResource> (*itr);
        qint64 storageSpace = qMax(0ll, fileStorage->getTotalSpace() - fileStorage->getSpaceLimit());
        totalSpace += storageSpace;
        minSpace = qMin(minSpace, storageSpace);
    }

    for (QSet<QnStorageResourcePtr>::const_iterator itr = storages.constBegin(); itr != storages.constEnd(); ++itr)
    {
        QnFileStorageResourcePtr fileStorage = qSharedPointerDynamicCast<QnFileStorageResource> (*itr);
        qint64 storageSpace = qMax(0ll, fileStorage->getTotalSpace() - fileStorage->getSpaceLimit());
        // write to large HDD more often then small HDD
        fileStorage->setStorageBitrateCoeff(storageSpace / (double) minSpace);
    }
    m_storagesStatisticsReady = true;
    m_warnSended = false;
}

QnStorageResourcePtr QnStorageManager::getOptimalStorageRoot(QnAbstractMediaStreamDataProvider* provider)
{
    QnStorageResourcePtr result;
    float minBitrate = (float) INT_MAX;

    testOfflineStorages();
    updateStorageStatistics();

    QVector<QPair<float, QnStorageResourcePtr> > bitrateInfo;
    QVector<QnStorageResourcePtr> candidates;

    // Got storages with minimal bitrate value. Accept storages with minBitrate +10%
    const QSet<QnStorageResourcePtr> storages = getWritableStorages();
    for (QSet<QnStorageResourcePtr>::const_iterator itr = storages.constBegin(); itr != storages.constEnd(); ++itr)
    {
        QnStorageResourcePtr storage = *itr;
        qDebug() << "QnFileStorageResource " << storage->getPath() << "current bitrate=" << storage->bitrate() << "coeff=" << storage->getStorageBitrateCoeff();
        float bitrate = storage->bitrate() / storage->getStorageBitrateCoeff();
        minBitrate = qMin(minBitrate, bitrate);
        bitrateInfo << QPair<float, QnStorageResourcePtr>(bitrate, storage);
    }
    for (int i = 0; i < bitrateInfo.size(); ++i)
    {
        if (bitrateInfo[i].first <= minBitrate*1.1f)
            candidates << bitrateInfo[i].second;
    }

    // select storage with maximum free space and do not use storages without free space at all
    qint64 maxFreeSpace = 0;
    for (int i = 0; i < candidates.size(); ++i)
    {   
        qint64 freeSpace = candidates[i]->getFreeSpace();
        if (freeSpace > maxFreeSpace)
        {
            maxFreeSpace = freeSpace;
            result = candidates[i];
        }
    }

    if (result) {
        qDebug() << "QnFileStorageResource. selectedStorage= " << result->getUrl() << "for provider" << provider->getResource()->getUrl();
    }
    else {
        qDebug() << "No storage available for recording";
        if (!m_warnSended) {
            emit noStoragesAvailable();
            m_warnSended = true;
        }
    }

    return result;
}

int QnStorageManager::getFileNumFromCache(const QString& base, const QString& folder)
{
    SCOPED_MUTEX_LOCK( lock, &m_cacheMutex);
    FileNumCache::iterator itr = m_fileNumCache.find(base);
    if (itr == m_fileNumCache.end())
        itr = m_fileNumCache.insert(base, QPair<QString, int >());
    if (itr.value().first != folder) {
        itr.value().first = folder;
        itr.value().second = -1;
    }
    return itr.value().second;
}

void QnStorageManager::putFileNumToCache(const QString& base, int fileNum)
{
    SCOPED_MUTEX_LOCK( lock, &m_cacheMutex);
    m_fileNumCache[base].second = fileNum;
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
    Q_ASSERT(camera != 0);
    QString base = closeDirPath(storage->getPath());
    QString separator = getPathSeparator(base);

    if (!prefix.isEmpty())
        base += prefix + separator;
    base += camera->getPhysicalId();

    Q_ASSERT(!camera->getPhysicalId().isEmpty());
    QString text = base + separator + dateTimeStr(dateTime, timeZone, separator);

    int fileNum = getFileNumFromCache(base, text);
    if (fileNum == -1)
    {
        fileNum = 0;
        QList<QFileInfo> list = storage->getFileList(text);
        QList<QString> baseNameList;
        for(const QFileInfo& info: list)
            baseNameList << info.completeBaseName();
        qSort(baseNameList.begin(), baseNameList.end());
        if (!baseNameList.isEmpty()) 
            fileNum = baseNameList.last().toInt() + 1;
    }
    else {
        fileNum++; // using cached value
    }
    putFileNumToCache(base, fileNum);
    return text + strPadLeft(QString::number(fileNum), 3, '0');
}

DeviceFileCatalogPtr QnStorageManager::getFileCatalog(const QString& cameraUniqueId, const QString &catalogPrefix)
{
    if (!m_catalogLoaded)
        return DeviceFileCatalogPtr();
    return getFileCatalogInternal(cameraUniqueId, DeviceFileCatalog::catalogByPrefix(catalogPrefix));
}

DeviceFileCatalogPtr QnStorageManager::getFileCatalog(const QString& cameraUniqueId, QnServer::ChunksCatalog catalog)
{
    if (!m_catalogLoaded)
        return DeviceFileCatalogPtr();
    return getFileCatalogInternal(cameraUniqueId, catalog);
}

void QnStorageManager::replaceChunks(const QnTimePeriod& rebuildPeriod, const QnStorageResourcePtr &storage, const DeviceFileCatalogPtr &newCatalog, const QString& cameraUniqueId, QnServer::ChunksCatalog catalog)
{
    SCOPED_MUTEX_LOCK( lock, &m_mutexCatalog);
    int storageIndex = getStorageIndex(storage);
    
    // add new recorded chunks to scan data
    qint64 scannedDataLastTime = newCatalog->m_chunks.empty() ? 0 : newCatalog->m_chunks[newCatalog->m_chunks.size()-1].startTimeMs;
    qint64 rebuildLastTime = qMax(rebuildPeriod.endTimeMs(), scannedDataLastTime);
    
    DeviceFileCatalogPtr ownCatalog = getFileCatalogInternal(cameraUniqueId, catalog);
    auto itr = qLowerBound(ownCatalog->m_chunks.begin(), ownCatalog->m_chunks.end(), rebuildLastTime);
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

    QnStorageDbPtr sdb = getSDB(storage);
    if (sdb)
        sdb->replaceChunks(cameraUniqueId, catalog, newCatalog->m_chunks);
}

DeviceFileCatalogPtr QnStorageManager::getFileCatalogInternal(const QString& cameraUniqueId, QnServer::ChunksCatalog catalog)
{
    SCOPED_MUTEX_LOCK( lock, &m_mutexCatalog);
    FileCatalogMap& catalogMap = m_devFileCatalog[catalog];
    DeviceFileCatalogPtr fileCatalog = catalogMap[cameraUniqueId];
    if (fileCatalog == 0)
    {
        fileCatalog = DeviceFileCatalogPtr(new DeviceFileCatalog(cameraUniqueId, catalog));
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
    for(StorageMap::const_iterator itr = storages.constBegin(); itr != storages.constEnd(); ++itr)
    {
        QString root = closeDirPath(itr.value()->getPath());
        QString separator = getPathSeparator(root);
        if (fileName.startsWith(root))
        {
            int qualityLen = fileName.indexOf(separator, root.length()+1) - root.length();
            quality = fileName.mid(root.length(), qualityLen);
            int idPos = root.length() + qualityLen;
            uniqueId = fileName.mid(idPos+1, fileName.indexOf(separator, idPos+1) - idPos-1);

            storageIndex = getStorageIndex(itr.value());
            return *itr;
        }
    }
    return QnStorageResourcePtr();
}

QnStorageResourcePtr QnStorageManager::getStorageByUrl(const QString& fileName)
{
    SCOPED_MUTEX_LOCK( lock, &m_mutexStorages);
    for(StorageMap::const_iterator itr = m_storageRoots.constBegin(); itr != m_storageRoots.constEnd(); ++itr)
    {
        QString root = itr.value()->getUrl();
        if (fileName.startsWith(root))
            return itr.value();
    }
    return QnStorageResourcePtr();
}

bool QnStorageManager::fileFinished(int durationMs, const QString& fileName, QnAbstractMediaStreamDataProvider* provider, qint64 fileSize)
{
    int storageIndex;
    QString quality;
    QString cameraUniqueId;
    QnStorageResourcePtr storage = extractStorageFromFileName(storageIndex, fileName, cameraUniqueId, quality);
    if (!storage)
        return false;
    if (storageIndex >= 0)
        storage->releaseBitrate(provider);
        //storage->addWritedSpace(fileSize);

    DeviceFileCatalogPtr catalog = getFileCatalog(cameraUniqueId, quality);
    if (catalog == 0)
        return false;
    QnStorageDbPtr sdb = getSDB(storage);
    if (sdb)
        sdb->addRecord(cameraUniqueId, DeviceFileCatalog::catalogByPrefix(quality), catalog->updateDuration(durationMs, fileSize));
    return true;
}

bool QnStorageManager::fileStarted(const qint64& startDateMs, int timeZone, const QString& fileName, QnAbstractMediaStreamDataProvider* provider)
{
    int storageIndex;
    QString quality, mac;

    QnStorageResourcePtr storage = extractStorageFromFileName(storageIndex, fileName, mac, quality);
    if (storageIndex == -1)
        return false;
    storage->addBitrate(provider);

    DeviceFileCatalogPtr catalog = getFileCatalog(mac.toUtf8(), quality);
    if (catalog == 0)
        return false;
    DeviceFileCatalog::Chunk chunk(startDateMs, storageIndex, QnFile::baseName(fileName).toInt(), -1, (qint16) timeZone);
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
                return getStorageByUrl(itr.key());
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
    QMutexLocker lock(&m_csvMigrationMutex);

    QString base = closeDirPath(getDataDirectory());
    QString separator = getPathSeparator(base);
    backupFolderRecursive(base + lit("record_catalog"), base + lit("record_catalog_backup"));
    QDir dir(base + QString("record_catalog") + separator + QString("media") + separator + DeviceFileCatalog::prefixByCatalog(catalogType));
    QFileInfoList list = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for(QFileInfo fi: list)
    {
        QByteArray mac = fi.fileName().toUtf8();
        DeviceFileCatalogPtr catalogFile(new DeviceFileCatalog(mac, catalogType));
        QString catalogName = closeDirPath(fi.absoluteFilePath()) + lit("title.csv");
        QVector<DeviceFileCatalog::Chunk> notMigratedChunks;
        if (catalogFile->fromCSVFile(catalogName))
        {
            for(const DeviceFileCatalog::Chunk& chunk: catalogFile->m_chunks) 
            {
                QnStorageResourcePtr storage = findStorageByOldIndex(chunk.storageIndex);
                if (storage && storage != extraAllowedStorage && storage->getStatus() != Qn::Online)
                    storage.clear();

                QnStorageDbPtr sdb = storage ? getSDB(storage) : QnStorageDbPtr();
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
            {
                SCOPED_MUTEX_LOCK( lock, &m_sdbMutex );
                for(const QnStorageDbPtr& sdb: m_chunksDB.values()) {
                    if (sdb)
                        sdb->flushRecords();
                }
            }
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

bool QnStorageManager::addBookmark(const QByteArray &cameraGuid, QnCameraBookmark &bookmark, bool forced) {
    //TODO: #GDM #Bookmarks #API #High make sure guid is absent in the database, fill if not exists
    QnDualQualityHelper helper;
    helper.openCamera(cameraGuid);

    DeviceFileCatalog::Chunk chunkBegin, chunkEnd;
    DeviceFileCatalogPtr catalog;
    helper.findDataForTime(bookmark.startTimeMs, chunkBegin, catalog, DeviceFileCatalog::OnRecordHole_NextChunk, false);
    if (chunkBegin.startTimeMs < 0) {
        if (!forced)
            return false; //recorded chunk was not found

        /* If we are forced, try to find any chunk. */
        helper.findDataForTime(bookmark.startTimeMs, chunkBegin, catalog, DeviceFileCatalog::OnRecordHole_PrevChunk, false);
        if (chunkBegin.startTimeMs < 0)
            return false; // no recorded chunk were found at all
    }

    helper.findDataForTime(bookmark.endTimeMs(), chunkEnd, catalog, DeviceFileCatalog::OnRecordHole_PrevChunk, false);
    if (chunkEnd.startTimeMs < 0 && !forced)
        return false; //recorded chunk was not found

    qint64 endTimeMs = bookmark.endTimeMs();

    /* For usual case move bookmark borders to the chunk borders. */
    if (!forced) {
        bookmark.startTimeMs = qMax(bookmark.startTimeMs, chunkBegin.startTimeMs);  // move bookmark start to the start of the chunk in case of hole
        endTimeMs = qMin(endTimeMs, chunkEnd.endTimeMs());
        bookmark.durationMs = endTimeMs - bookmark.startTimeMs;                     // move bookmark end to the end of the closest chunk in case of hole

        if (bookmark.durationMs <= 0)
            return false;   // bookmark ends before the chunk starts
    }

    // this chunk will be added to the bookmark catalog
    chunkBegin.startTimeMs = bookmark.startTimeMs;
    chunkBegin.durationMs = bookmark.durationMs;

    DeviceFileCatalogPtr bookmarksCatalog = getFileCatalog(cameraGuid, QnServer::BookmarksCatalog);
    bookmarksCatalog->addChunk(chunkBegin);

    QnStorageResourcePtr storage = storageRoot(chunkBegin.storageIndex);
    if (!storage)
        return false;

    QnStorageDbPtr sdb = getSDB(storage);
    if (!sdb)
        return false;

    if (!sdb->addOrUpdateCameraBookmark(bookmark, cameraGuid))
        return false;

    if (!bookmark.tags.isEmpty())
        QnAppServerConnectionFactory::getConnection2()->getCameraManager()->addBookmarkTags(bookmark.tags, this, [](int /*reqID*/, ec2::ErrorCode /*errorCode*/) {});
    

    return true;
}

bool QnStorageManager::updateBookmark(const QByteArray &cameraGuid, QnCameraBookmark &bookmark) {
    //TODO: #GDM #Bookmarks #API #High make sure guid is present and exists in the database
    DeviceFileCatalogPtr catalog = qnStorageMan->getFileCatalog(cameraGuid, QnServer::BookmarksCatalog);
    int idx = catalog->findFileIndex(bookmark.startTimeMs, DeviceFileCatalog::OnRecordHole_NextChunk);
    if (idx < 0)
        return false;
    QnStorageResourcePtr storage = storageRoot(catalog->chunkAt(idx).storageIndex);
    if (!storage)
        return false;

    QnStorageDbPtr sdb = getSDB(storage);
    if (!sdb)
        return false;

    if (!sdb->addOrUpdateCameraBookmark(bookmark, cameraGuid))
        return false;

    if (!bookmark.tags.isEmpty())
        QnAppServerConnectionFactory::getConnection2()->getCameraManager()->addBookmarkTags(bookmark.tags, this, [](int /*reqID*/, ec2::ErrorCode /*errorCode*/) {});

    return true;
}


bool QnStorageManager::deleteBookmark(const QByteArray &cameraGuid, QnCameraBookmark &bookmark) {
    //TODO: #GDM #Bookmarks #API #High make sure guid is present and exists in the database
    DeviceFileCatalogPtr catalog = qnStorageMan->getFileCatalog(cameraGuid, QnServer::BookmarksCatalog);
    if (!catalog)
        return false;

    DeviceFileCatalog::Chunk chunk = catalog->takeChunk(bookmark.startTimeMs, bookmark.durationMs);
    if (chunk.durationMs == 0)
        return false;

    QnStorageResourcePtr storage = storageRoot(chunk.storageIndex);
    if (!storage)
        return false;

    QnStorageDbPtr sdb = getSDB(storage);
    if (!sdb)
        return false;

    if (!sdb->deleteCameraBookmark(bookmark))
        return false;

    return true;
}


bool QnStorageManager::getBookmarks(const QByteArray &cameraGuid, const QnCameraBookmarkSearchFilter &filter, QnCameraBookmarkList &result) 
{
    SCOPED_MUTEX_LOCK( lock, &m_sdbMutex);
    for (const QnStorageDbPtr &sdb: m_chunksDB) {
        if (!sdb)
            continue;
        if (!sdb->getBookmarks(cameraGuid, filter, result))
            return false;
    }
    return true;
}
