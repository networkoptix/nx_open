#include "storage_manager.h"

#include <QtCore/QDir>

#include <api/app_server_connection.h>

#include <utils/fs/file.h>
#include <utils/common/util.h>
#include <utils/common/log.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
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
#include "media_server/settings.h"
#include "utils/serialization/lexical_enum.h"

#include <memory>
//static const qint64 BALANCE_BY_FREE_SPACE_THRESHOLD = 1024*1024 * 500;
//static const int OFFLINE_STORAGES_TEST_INTERVAL = 1000 * 30;
//static const int DB_UPDATE_PER_RECORDS = 128;
static const qint64 MSECS_PER_DAY = 1000ll * 3600ll * 24ll;
static const qint64 MOTION_CLEANUP_INTERVAL = 1000ll * 3600;
static const qint64 EMPTY_DIRS_CLEANUP_INTERVAL = 1000ll * 3600;
static const QString SCAN_ARCHIVE_FROM(lit("SCAN_ARCHIVE_FROM"));

class ArchiveScanPosition
{
public:
    ArchiveScanPosition(): m_catalog(QnServer::LowQualityCatalog) {}
    ArchiveScanPosition(const QnStorageResourcePtr& storage, QnServer::ChunksCatalog catalog, const QString& cameraUniqueId):
        m_storagePath(storage->getUrl()),
        m_catalog(catalog), 
        m_cameraUniqueId(cameraUniqueId) 
    {}

    void save() {
        QString serializedData(lit("%1;;%2;;%3"));
        serializedData = serializedData.arg(m_storagePath).arg(QnLexical::serialized(m_catalog)).arg(m_cameraUniqueId);
        MSSettings::roSettings()->setValue(SCAN_ARCHIVE_FROM, serializedData);
        MSSettings::roSettings()->sync();
    }
    void load() {
        QString serializedData = MSSettings::roSettings()->value(SCAN_ARCHIVE_FROM).toString();
        QStringList data = serializedData.split(";;");
        if (data.size() == 3) {
            m_storagePath = data[0];
            QnLexical::deserialize(data[1], &m_catalog);
            m_cameraUniqueId = data[2];
        }
    }
    static void reset() {
        MSSettings::roSettings()->setValue(SCAN_ARCHIVE_FROM, QString());
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
    QString m_storagePath;
    QnServer::ChunksCatalog m_catalog;
    QString m_cameraUniqueId;
};


class ScanMediaFilesTask: public QnLongRunnable
{
private:

    struct ScanData
    {
        ScanData(): partialScan(false) {}
        ScanData(const QnStorageResourcePtr& storage, bool partialScan): storage(storage), partialScan(partialScan) {}
        bool operator== (const ScanData& other) const { return storage == other.storage && partialScan == other.partialScan; }

        QnStorageResourcePtr storage;
        bool partialScan;
    };
    QnStorageManager* m_owner;
    CLThreadQueue<ScanData> m_scanTasks;
    QnMutex m_mutex;
    QnWaitCondition m_waitCond;
    bool m_fullScanCanceled;

public:
    ScanMediaFilesTask(QnStorageManager* owner): QnLongRunnable(), m_owner(owner), m_fullScanCanceled(false)
    {
    }

    void addStorageToScan(const QnStorageResourcePtr& storage, bool partialScan)
    {
        ScanData scanData(storage, partialScan);
        if (m_scanTasks.contains(scanData))
            return;
        if (m_scanTasks.isEmpty())
            m_owner->setRebuildInfo(QnStorageScanData(partialScan ? Qn::RebuildState_PartialScan : Qn::RebuildState_FullScan, QString(), 0.0));

        QnMutexLocker lock( &m_mutex );
        m_scanTasks.push(std::move(scanData));
        m_waitCond.wakeAll();
    }

    bool hasFullScanTasks()
    {
        m_scanTasks.lock();
        bool result = false;
        for (int i = m_scanTasks.size()-1; i >= 0; --i) {
            if (!m_scanTasks.atUnsafe(i).partialScan) {
                result = true;
                break;
            }

        }
        m_scanTasks.unlock();
        return result;
    }

    void cancelFullScanTasks()
    {
        m_fullScanCanceled = true;
    }

    
    virtual void run() override
    {
        while (!needToStop())
        {
            if (m_fullScanCanceled) {
                m_scanTasks.detachDataByCondition([](const ScanData& data, const QVariant&) { return !data.partialScan; });
                m_fullScanCanceled = false;
            }

            {
                QnMutexLocker lock( &m_mutex );
                if (m_scanTasks.isEmpty()) {
                    m_owner->setRebuildInfo(QnStorageScanData(Qn::RebuildState_None, QString(), 1.0));
                    m_waitCond.wait(&m_mutex, 100);
                    continue;
                }
            }

            ScanData scanData = m_scanTasks.front();
            if (!scanData.storage) {
                m_scanTasks.removeFirst(1); // detached
                continue;
            }

            if (scanData.partialScan)
            {
                QMap<DeviceFileCatalogPtr, qint64> catalogToScan; // key - catalog, value - start scan time;
                {
                    QnMutexLocker lock(&m_owner->m_mutexCatalog);
                    for(const DeviceFileCatalogPtr& catalog: m_owner->m_devFileCatalog[QnServer::LowQualityCatalog])
                        catalogToScan.insert(catalog, catalog->lastChunkStartTime());
                    for(const DeviceFileCatalogPtr& catalog: m_owner->m_devFileCatalog[QnServer::HiQualityCatalog])
                        catalogToScan.insert(catalog, catalog->lastChunkStartTime());
                }
                int totalStep = catalogToScan.size();
                int currentStep = 0;
                for(auto itr = catalogToScan.begin(); itr != catalogToScan.end(); ++itr) 
                {
                    m_owner->setRebuildInfo(QnStorageScanData(Qn::RebuildState_PartialScan, scanData.storage->getUrl(), currentStep / (qreal) totalStep));
                    DeviceFileCatalog::ScanFilter filter;
                    filter.scanPeriod.startTimeMs = itr.value();
                    qint64 endScanTime = qnSyncTime->currentMSecsSinceEpoch();
                    filter.scanPeriod.durationMs = qMax(1ll, endScanTime - filter.scanPeriod.startTimeMs);
                    m_owner->partialMediaScan(itr.key(), scanData.storage, filter);
                    if (needToStop())
                        return;
                    ++currentStep;
                }
                m_owner->setRebuildInfo(QnStorageScanData(Qn::RebuildState_PartialScan, scanData.storage->getUrl(), 1.0));
            }
            else 
            {
                QElapsedTimer t;
                t.restart();
                m_owner->setRebuildInfo(QnStorageScanData(Qn::RebuildState_FullScan, scanData.storage->getUrl(), 0.0));
                m_owner->loadFullFileCatalogFromMedia(scanData.storage, QnServer::LowQualityCatalog, 0.5);
                m_owner->loadFullFileCatalogFromMedia(scanData.storage, QnServer::HiQualityCatalog, 0.5);
                m_owner->setRebuildInfo(QnStorageScanData(Qn::RebuildState_FullScan, scanData.storage->getUrl(), 1.0));
                qDebug() << "rebuild archive time for storage" << scanData.storage->getUrl() << "is:" << t.elapsed() << "msec";
            }
            m_scanTasks.removeFirst(1);
        }
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

            QnStorageResourcePtr fileStorage = qSharedPointerDynamicCast<QnStorageResource> (*itr);
            Qn::ResourceStatus status = fileStorage->isAvailable() ? Qn::Online : Qn::Offline;
            if (fileStorage->getStatus() != status)
                m_owner->changeStorageStatus(fileStorage, status);

            if (fileStorage->isAvailable())
            {
                const auto space = QString::number(fileStorage->getTotalSpace());
                if (fileStorage->setProperty(Qn::SPACE, space))
                    propertyDictionary->saveParams(fileStorage->getId());
            }
        }

        m_owner->testStoragesDone();

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
    m_warnSended(false),
    m_isWritableStorageAvail(false),
    m_rebuildCancelled(false),
    m_rebuildArchiveThread(0),
    m_firstStorageTestDone(false)
{
    m_storageWarnTimer.restart();
    m_testStorageThread = new TestStorageThread(this);

    assert( QnStorageManager_instance == nullptr );
    QnStorageManager_instance = this;
    m_oldStorageIndexes = deserializeStorageFile();

    connect(qnResPool, &QnResourcePool::resourceAdded, this, &QnStorageManager::onNewResource, Qt::QueuedConnection);
    connect(qnResPool, &QnResourcePool::resourceRemoved, this, &QnStorageManager::onDelResource, Qt::QueuedConnection);

    m_rebuildArchiveThread = new ScanMediaFilesTask(this);
    m_rebuildArchiveThread->start();
    m_clearMotionTimer.restart();
    m_removeEmtyDirTimer.restart();
}


void QnStorageManager::getCurrentlyUsedLocalPathes(QList<QString> *pathList) const
{
    assert(pathList);
    QMutexLocker lock(&m_localPatches);
    for (const auto &entry: m_localPathsInUse)
        pathList->append(entry);
}


void QnStorageManager::addLocalPathInUse(const QString &path)
{
    QMutexLocker lock(&m_localPatches);
    m_localPathsInUse.append(path);
}

void QnStorageManager::removeFromLocalPathInUse(const QString &path)
{
    QMutexLocker lock(&m_localPatches);
    m_localPathsInUse.removeAll(path);
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
        storage->removeFile(emptyFile.fileName);

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

QnStorageScanData QnStorageManager::rebuildInfo() const
{
    QnMutexLocker lock( &m_rebuildStateMtx );
    return m_archiveRebuildInfo;
}

QnStorageScanData QnStorageManager::rebuildCatalogAsync()
{
    QnStorageScanData result(Qn::RebuildState_FullScan, QString(), 0.0);
    QnMutexLocker lock( &m_mutexRebuild );

    if (!m_rebuildArchiveThread->hasFullScanTasks()) 
    {
        m_rebuildCancelled = false;
        for(const QnStorageResourcePtr& storage: getStoragesInLexicalOrder()) {
            if (storage->getStatus() == Qn::Online)
                m_rebuildArchiveThread->addStorageToScan(storage, false);
        }
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
    bool isRebuildFinished = false;
    {
        QnMutexLocker lock( &m_rebuildStateMtx );
        isRebuildFinished = (data.state == Qn::RebuildState_None && m_archiveRebuildInfo.state == Qn::RebuildState_FullScan);
        m_archiveRebuildInfo = data;
    }
    if (isRebuildFinished) {
        if (!QnResource::isStopping())
            ArchiveScanPosition::reset(); // do not reset position if server is going to restart
        emit rebuildFinished();
    }
}

void QnStorageManager::loadFullFileCatalogFromMedia(const QnStorageResourcePtr &storage, QnServer::ChunksCatalog catalog, qreal progressCoeff)
{
    ArchiveScanPosition scanPos;
    scanPos.load(); // load from persistent storage

    QnAbstractStorageResource::FileInfoList list = 
        storage->getFileList(
            closeDirPath(
                storage->getUrl()
            ) + DeviceFileCatalog::prefixByCatalog(catalog)
        );

    FileCatalogMap& catalogMap = m_devFileCatalog[catalog];
    for (auto it = catalogMap.cbegin(); it != catalogMap.cend(); ++it)
    {
        QString cameraDir =
            closeDirPath(
                closeDirPath(storage->getUrl()) +
                DeviceFileCatalog::prefixByCatalog(catalog)
            ) + it.key();

        if (!storage->isDirExists(cameraDir))
        {
            DeviceFileCatalogPtr newCatalog(
                new DeviceFileCatalog(
                    it.key(), 
                    catalog
                )
            );
            
            replaceChunks(
                QnTimePeriod(0, qnSyncTime->currentMSecsSinceEpoch()), 
                storage, 
                newCatalog, 
                it.key(), 
                catalog
            );
        }
    }

    for(const QnAbstractStorageResource::FileInfo& fi: list)
    {
        if (m_rebuildCancelled)
            return; // cancel rebuild

        QString cameraUniqueId = fi.fileName();
        ArchiveScanPosition currentPos(storage, catalog, cameraUniqueId);
        if (currentPos < scanPos) {
            // already scanned
        }
        else {
            currentPos.save(); // save to persistent storage
            qint64 rebuildEndTime = qnSyncTime->currentMSecsSinceEpoch() - QnRecordingManager::RECORDING_CHUNK_LEN * 1250;
            DeviceFileCatalogPtr newCatalog(new DeviceFileCatalog(cameraUniqueId, catalog));
            QnTimePeriod rebuildPeriod = QnTimePeriod(0, rebuildEndTime);
            newCatalog->doRebuildArchive(storage, rebuildPeriod);
        
            if (!m_rebuildCancelled)
                replaceChunks(rebuildPeriod, storage, newCatalog, cameraUniqueId, catalog);
        }
        m_archiveRebuildInfo.progress += progressCoeff / (double) list.size();
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
    return detectStorageIndex(storage->getUrl());
}

// determine storage index (aka 16 bit hash)
int QnStorageManager::detectStorageIndex(const QString& p)
{
    QnMutexLocker lock( &m_mutexStorages );
    QString path = p;
    //QString path = toCanonicalPath(p);

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
    QString storageUrl = storage->getUrl();
    QString dbRefFilePath;
    
    //if (storagePath.indexOf(lit("://")) != -1)
    //    dbRefFilePath = dbRefFileName.arg(getLocalGuid());
    //else
    dbRefFilePath = closeDirPath(storageUrl) + dbRefFileName.arg(getLocalGuid());

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
        *dbDirectory = QDir(getDataDirectory() + "/storage_db/" + dbRefGuidStr).absolutePath();
        if (!QDir().exists(*dbDirectory))
        {
            if(!QDir().mkpath(*dbDirectory))
            {
                qWarning() << "DB path create failed (" << storageUrl << ")";
                return false;
            }
        }
        return true;
    }

    if (storage->getCapabilities() & QnAbstractStorageResource::DBReady)
    {
        *dbDirectory = storageUrl;
        return true;
    }
    else
    {   // we won't be able to create ref guid file if storage is not writable
        if (!(storage->getCapabilities() & QnAbstractStorageResource::WriteFile))
            return false;

        dbRefGuidStr = QUuid::createUuid().toString().toLatin1();
        if( dbRefGuidStr.size() < 2 )
            return false;   //bad guid, somehow
        //removing {}
        dbRefGuidStr.remove( dbRefGuidStr.size()-1, 1 );
        dbRefGuidStr.remove( 0, 1 );
        //saving db ref guid to file on storage
        QIODevice *dbGuidFile = storage->open(
            dbRefFilePath, 
            QIODevice::WriteOnly | QIODevice::Truncate 
        );
        if (dbGuidFile == nullptr)
            return false;
        if( dbGuidFile->write( dbRefGuidStr ) != dbRefGuidStr.size() )
            return false;
        dbGuidFile->close();

        storageUrl = QDir(getDataDirectory() + "/storage_db/" + dbRefGuidStr).absolutePath();
        if( !QDir().mkpath( storageUrl ) )
        {
            qWarning() << "DB path create failed (" << storageUrl << ")";
            return false;
        }
    }
    *dbDirectory = storageUrl;
    return true;
}

QnStorageDbPtr QnStorageManager::getSDB(const QnStorageResourcePtr &storage)
{
    QnMutexLocker lock( &m_sdbMutex );

    QnStorageDbPtr sdb = m_chunksDB[storage->getUrl()];
    if (!sdb) 
    {
        QString simplifiedGUID = getLocalGuid();
        QString dbPath;
        if( !getDBPath(storage, &dbPath) )
        {
            NX_LOG( lit("Failed to file path to storage DB file. Storage is not writable?"), cl_logWARNING );
            return QnStorageDbPtr();
        }
//        qWarning() << "DB Path: " << dbPath << "\n";
        QString fileName = closeDirPath(dbPath) + QString::fromLatin1("%1_media.sqlite").arg(simplifiedGUID);
        QString oldFileName = closeDirPath(dbPath) + QString::fromLatin1("media.sqlite");
        
        if (storage->getCapabilities() & QnAbstractStorageResource::DBReady)
        {
            if (storage->isFileExists(oldFileName) && !storage->isFileExists(fileName))
                storage->renameFile(oldFileName, fileName);

            sdb = m_chunksDB[storage->getUrl()] = 
                QnStorageDbPtr(new QnStorageDb(storage, getStorageIndex(storage)));
        }
        else
        {
            if (QFile::exists(oldFileName) && !QFile::exists(fileName))
                QFile::rename(oldFileName, fileName);
            sdb = m_chunksDB[storage->getUrl()] = 
                QnStorageDbPtr(new QnStorageDb(QnStorageResourcePtr(), getStorageIndex(storage)));
        }

        if (!sdb->open(fileName))
        {
            qWarning()
                << "can't initialize sqlLite database! Actions log is not created!"
                << " file open failed: " << fileName;
            return QnStorageDbPtr();
        }
    }
    return sdb;
}

void QnStorageManager::addStorage(const QnStorageResourcePtr &storage)
{
    {
        int storageIndex = detectStorageIndex(storage->getUrl());
        QnMutexLocker lock( &m_mutexStorages );
        m_storagesStatisticsReady = false;
    
        NX_LOG(QString("Adding storage. Path: %1").arg(storage->getUrl()), cl_logINFO);

        removeStorage(storage); // remove existing storage record if exists
        //QnStorageResourcePtr oldStorage = removeStorage(storage); // remove existing storage record if exists
        //if (oldStorage)
        //    storage->addWritedSpace(oldStorage->getWritedSpace());
        storage->setStatus(Qn::Offline); // we will check status after
        m_storageRoots.insert(storageIndex, storage);
        connect(storage.data(), SIGNAL(archiveRangeChanged(const QnStorageResourcePtr &, qint64, qint64)), 
                this, SLOT(at_archiveRangeChanged(const QnStorageResourcePtr &, qint64, qint64)), Qt::DirectConnection);
    }
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
        addStorage(storage);
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
    int storageIndex = -1;
    {
        QnMutexLocker lock( &m_mutexStorages );
        m_storagesStatisticsReady = false;

        // remove existing storage record if exists
        for (StorageMap::iterator itr = m_storageRoots.begin(); itr != m_storageRoots.end();)
        {
            if (itr.value()->getId() == storage->getId()) {
                storageIndex = itr.key();
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
        QnMutexLocker lock(&m_mutexCatalog);
        for (int i = 0; i < QnServer::ChunksCatalogCount; ++i) {
            for (const auto catalog: m_devFileCatalog[i].values())
                catalog->removeChunks(storageIndex);
        }
    }
}

void QnStorageManager::at_storageChanged(const QnResourcePtr &)
{
    {
        QnMutexLocker lock( &m_mutexStorages );
        m_storagesStatisticsReady = false;
    }
    updateStorageStatistics();
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

void QnStorageManager::getTimePeriodInternal(std::vector<QnTimePeriodList> &periods, const QnNetworkResourcePtr &camera, qint64 startTime, qint64 endTime, 
                                             qint64 detailLevel, const DeviceFileCatalogPtr &catalog)
{
    if (catalog) {
        periods.push_back(catalog->getTimePeriods(startTime, endTime, detailLevel));
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
    DeviceFileCatalogPtr catalog = getFileCatalog(cameraUniqueId, QnServer::HiQualityCatalog);
    if (catalog && catalog->containTime(timeMs))
        return true;

    catalog = getFileCatalog(cameraUniqueId, QnServer::LowQualityCatalog);
    return catalog && catalog->containTime(timeMs);
}

bool QnStorageManager::isArchiveTimeExists(const QString& cameraUniqueId, const QnTimePeriod period)
{
    DeviceFileCatalogPtr catalog = getFileCatalog(cameraUniqueId, QnServer::HiQualityCatalog);
    if (catalog && catalog->containTime(period))
        return true;

    catalog = getFileCatalog(cameraUniqueId, QnServer::LowQualityCatalog);
    return catalog && catalog->containTime(period);
}

QnTimePeriodList QnStorageManager::getRecordedPeriods(const QnVirtualCameraResourceList &cameras, qint64 startTime, qint64 endTime, qint64 detailLevel, 
                                                      const QList<QnServer::ChunksCatalog> &catalogs, int limit) 
{
    std::vector<QnTimePeriodList> periods;
    for (const QnVirtualCameraResourcePtr &camera: cameras) {
        QString cameraUniqueId = camera->getUniqueId();
        for (int i = 0; i < QnServer::ChunksCatalogCount; ++i) {
            QnServer::ChunksCatalog catalog = static_cast<QnServer::ChunksCatalog> (i);
            if (!catalogs.contains(catalog))
                continue;

            //TODO: #GDM #Bookmarks forbid bookmarks for the DTS cameras
            if (camera->isDtsBased()) {
                if (catalog == QnServer::HiQualityCatalog) // both hi- and low-quality chunks are loaded with this method
                    periods.push_back(camera->getDtsTimePeriods(startTime, endTime, detailLevel));
            } else {
                getTimePeriodInternal(periods, camera, startTime, endTime, detailLevel, getFileCatalog(cameraUniqueId, catalog));
            }
        }

    }

    return QnTimePeriodList::mergeTimePeriods(periods, limit);
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


    //auto itrHiLeft = qLowerBound(catalogHi->m_chunks.cbegin(), catalogHi->m_chunks.cend(), startTime);
    //auto itrHiRight = qUpperBound(itrHiLeft, catalogHi->m_chunks.cend(), endTime);
    auto itrHiLeft = catalogHi->m_chunks.cbegin();
    auto itrHiRight = catalogHi->m_chunks.cend();

    //auto itrLowLeft = qLowerBound(catalogLow->m_chunks.cbegin(), catalogLow->m_chunks.cend(), startTime);
    //auto itrLowRight = qUpperBound(itrLowLeft, catalogLow->m_chunks.cend(), endTime);
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
        Q_ASSERT(nextTime >= currentTime);
        qint64 blockDuration = nextTime - currentTime;
        
        if (hasHi) 
        {
            qreal persentUsage = blockDuration / (qreal) itrHiLeft->durationMs;
            Q_ASSERT(qBetween(0.0, persentUsage, 1.000001));
            result.recordedBytes += itrHiLeft->getFileSize() * persentUsage;
            result.recordedSecs += itrHiLeft->durationMs * persentUsage;
            if (itrHiLeft->startTimeMs >= bitrateThreshold) {
                bitrateStats.recordedBytes += itrHiLeft->getFileSize() * persentUsage;
                bitrateStats.recordedSecs += itrHiLeft->durationMs * persentUsage;
            }
        }

        if (hasLow) 
        {
            qreal persentUsage = blockDuration / (qreal) itrLowLeft->durationMs;
            Q_ASSERT(qBetween(0.0, persentUsage, 1.000001));
            result.recordedBytes += itrLowLeft->getFileSize() * persentUsage;

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
    Q_ASSERT(result.averageBitrate >= 0);
    return result;
}


void QnStorageManager::removeEmptyDirs(const QnStorageResourcePtr &storage)
{
    std::function<bool (const QnAbstractStorageResource::FileInfoList &)> recursiveRemover =
        [&](const QnAbstractStorageResource::FileInfoList &fl)
    {
        for (const auto& entry : fl)
        {
            if (entry.isDir())
            {
                QnAbstractStorageResource::FileInfoList dirFileList =
                    storage->getFileList(
                        entry.absoluteFilePath()
                    );
                if (!dirFileList.isEmpty() && !recursiveRemover(dirFileList))
                    return false;
                // ignore error here, trying to clean as much as we can
                storage->removeDir(entry.absoluteFilePath());
            }
            else // we've met file. solid reason to stop
                return false;
        }
        return true;
    }; 

    auto qualityFileList = storage->getFileList(storage->getUrl());
    for (const auto &qualityEntry : qualityFileList)
    {
        if (qualityEntry.isDir()) // quality 
        {
            auto cameraFileList = storage->getFileList(qualityEntry.absoluteFilePath());
            for (const auto &cameraEntry : cameraFileList)
            {   // for every year folder
                recursiveRemover(storage->getFileList(cameraEntry.absoluteFilePath()));
            }
        }
    }
}

void QnStorageManager::clearSpace()
{
    testOfflineStorages();
    {
        QnMutexLocker lock( &m_sdbMutex );
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

    // 3. Remove empty dirs
    if (m_removeEmtyDirTimer.elapsed() > EMPTY_DIRS_CLEANUP_INTERVAL)
    {
        m_removeEmtyDirTimer.restart();
        for (const QnStorageResourcePtr &storage : storages)
            removeEmptyDirs(storage);
    }

    // 4. DB cleanup
    {
        QnMutexLocker lock( &m_sdbMutex );
        for(const QnStorageDbPtr& sdb: m_chunksDB) {
            if (sdb)
                sdb->afterDelete();
        }
    }

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
}

QnStorageManager::StorageMap QnStorageManager::getAllStorages() const 
{ 
    QnMutexLocker lock( &m_mutexStorages ); 
    return m_storageRoots; 
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
    clearMaxDaysData(QnServer::HiQualityCatalog);
    clearMaxDaysData(QnServer::LowQualityCatalog);
}

void QnStorageManager::clearMaxDaysData(QnServer::ChunksCatalog catalogIdx)
{
    QnMutexLocker lock( &m_mutexCatalog );

    const FileCatalogMap &catalogMap = m_devFileCatalog[catalogIdx];

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
    QnMutexLocker lock( &m_mutexCatalog );

    UsedMonthsMap usedMonths;

    updateRecordedMonths(m_devFileCatalog[QnServer::HiQualityCatalog], usedMonths);
    updateRecordedMonths(m_devFileCatalog[QnServer::LowQualityCatalog], usedMonths);

    for( const QString& dir: QDir(QnMotionHelper::getBaseDir()).entryList(QDir::Dirs | QDir::NoDotAndDotDot))
        QnMotionHelper::instance()->deleteUnusedFiles(usedMonths.value(dir).toList(), dir);
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

void QnStorageManager::clearOldestSpace(const QnStorageResourcePtr &storage, bool useMinArchiveDays)
{
    if (storage->getSpaceLimit() == 0)
        return; // unlimited


    QString dir = storage->getUrl();

    if (!(storage->getCapabilities() & QnAbstractStorageResource::cap::RemoveFile))
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
            QnMutexLocker lock( &m_mutexCatalog );
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

void QnStorageManager::at_archiveRangeChanged(const QnStorageResourcePtr &resource, qint64 newStartTimeMs, qint64 newEndTimeMs)
{
    Q_UNUSED(newEndTimeMs)
    int storageIndex = detectStorageIndex(resource->getUrl());
    QnMutexLocker lock(&m_mutexCatalog);
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
        QnStorageResourcePtr fileStorage = qSharedPointerDynamicCast<QnStorageResource> (itr.value());
        if (fileStorage && fileStorage->getStatus() != Qn::Offline && fileStorage->isUsedForWriting()) 
        {
            qint64 available = fileStorage->getTotalSpace() - fileStorage->getSpaceLimit();
            bigStorageThreshold = qMax(bigStorageThreshold, available);
        }
    }
    bigStorageThreshold /= BIG_STORAGE_THRESHOLD_COEFF;

    for (StorageMap::const_iterator itr = storageRoots.constBegin(); itr != storageRoots.constEnd(); ++itr)
    {
        QnStorageResourcePtr fileStorage = qSharedPointerDynamicCast<QnStorageResource> (itr.value());
        if (fileStorage && fileStorage->getStatus() != Qn::Offline && fileStorage->isUsedForWriting()) 
        {
            qint64 available = fileStorage->getTotalSpace() - fileStorage->getSpaceLimit();
            if (available >= bigStorageThreshold)
                result << fileStorage;
        }
    }
    return result;
}

void QnStorageManager::testStoragesDone()
{
    m_firstStorageTestDone = true;
    
    ArchiveScanPosition rebuildPos;
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
        addDataFromDatabase(fileStorage);
        m_rebuildArchiveThread->addStorageToScan(fileStorage, true);
    }

    fileStorage->setStatus(status);
    if (status == Qn::Offline)
        emit storageFailure(fileStorage, QnBusiness::StorageIoErrorReason);
    m_storagesStatisticsReady = false;
}

void QnStorageManager::testOfflineStorages()
{
    QnMutexLocker lock( &m_mutexStorages );
    if (!m_testStorageThread->isRunning())
        m_testStorageThread->start();
}

void QnStorageManager::stopAsyncTasks()
{
    if (m_testStorageThread) {
        m_testStorageThread->stop();
        delete m_testStorageThread;
        m_testStorageThread = 0;
    }
    m_rebuildCancelled = true;
    if (m_rebuildArchiveThread) {
        m_rebuildArchiveThread->stop();
        delete m_rebuildArchiveThread;
        m_rebuildArchiveThread = 0;
    }
}

void QnStorageManager::updateStorageStatistics()
{
    QnMutexLocker lock( &m_mutexStorages );
    if (m_storagesStatisticsReady) 
        return;

    qint64 totalSpace = 0;
    qint64 minSpace = INT64_MAX;
    QSet<QnStorageResourcePtr> storages = getWritableStorages();
    m_isWritableStorageAvail = !storages.isEmpty();
    for (QSet<QnStorageResourcePtr>::const_iterator itr = storages.constBegin(); itr != storages.constEnd(); ++itr)
    {
        QnStorageResourcePtr fileStorage = qSharedPointerDynamicCast<QnStorageResource> (*itr);
        qint64 storageSpace = qMax(0ll, fileStorage->getTotalSpace() - fileStorage->getSpaceLimit());
        totalSpace += storageSpace;
        minSpace = qMin(minSpace, storageSpace);
    }

    for (QSet<QnStorageResourcePtr>::const_iterator itr = storages.constBegin(); itr != storages.constEnd(); ++itr)
    {
        QnStorageResourcePtr fileStorage = qSharedPointerDynamicCast<QnStorageResource> (*itr);
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

    updateStorageStatistics();

    QVector<QPair<float, QnStorageResourcePtr> > bitrateInfo;
    QVector<QnStorageResourcePtr> candidates;

    // Got storages with minimal bitrate value. Accept storages with minBitrate +10%
    const QSet<QnStorageResourcePtr> storages = getWritableStorages();
    for (QSet<QnStorageResourcePtr>::const_iterator itr = storages.constBegin(); itr != storages.constEnd(); ++itr)
    {
        QnStorageResourcePtr storage = *itr;
        qDebug() << "QnFileStorageResource " << storage->getUrl() << "current bitrate=" << storage->bitrate() << "coeff=" << storage->getStorageBitrateCoeff();
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
        if (!m_warnSended && m_firstStorageTestDone) {
            qWarning() << "No storage available for recording";
            emit noStoragesAvailable();
            m_warnSended = true;
        }
    }

    return result;
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
    QString base = closeDirPath(storage->getUrl());
    QString separator = getPathSeparator(base);

    if (!prefix.isEmpty())
        base += prefix + separator;
    base += camera->getPhysicalId();

    Q_ASSERT(!camera->getPhysicalId().isEmpty());
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
    QnMutexLocker lock( &m_mutexCatalog );
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
        storageIndex = getStorageIndex(ret);
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

QnStorageResourcePtr QnStorageManager::getStorageByUrl(const QString& fileName)
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
    DeviceFileCatalog::Chunk chunk(startDateMs, storageIndex, DeviceFileCatalog::Chunk::FILE_INDEX_NONE, -1, (qint16) timeZone);
    catalog->addRecord(chunk);
    return true;
}

// data migration from previous versions

void QnStorageManager::doMigrateCSVCatalog(QnStorageResourcePtr extraAllowedStorage) {
    for (int i = 0; i < QnServer::BookmarksCatalog; ++i)
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
    QnMutexLocker lock( &m_csvMigrationMutex );

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
                QnMutexLocker lock( &m_sdbMutex );
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
    QnMutexLocker lock( &m_sdbMutex );
    for (const QnStorageDbPtr &sdb: m_chunksDB) {
        if (!sdb)
            continue;
        if (!sdb->getBookmarks(cameraGuid, filter, result))
            return false;
    }
    return true;
}

std::vector<QnUuid> QnStorageManager::getCamerasWithArchive() const
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

void QnStorageManager::getCamerasWithArchiveInternal(std::set<QString>& result, const FileCatalogMap& catalogMap ) const
{
    for(auto itr = catalogMap.begin(); itr != catalogMap.end(); ++itr)
    {
        const DeviceFileCatalogPtr& catalog = itr.value();
        if (!catalog->isEmpty())
            result.insert(catalog->cameraUniqueId());
    }
}
