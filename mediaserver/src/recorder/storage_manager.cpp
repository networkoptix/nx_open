#include "storage_manager.h"

#include <QtCore/QDir>

#include <api/app_server_connection.h>

#include "core/resource_management/resource_pool.h"
#include "core/resource/resource.h"
#include "core/resource/camera_resource.h"
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/camera_bookmark.h>

#include <recorder/server_stream_recorder.h>
#include <recorder/recording_manager.h>

#include <device_plugins/server_archive/dualquality_helper.h>

#include "plugins/storage/file_storage/file_storage_resource.h"

#include "utils/common/sleep.h"
#include <utils/fs/file.h>
#include "utils/common/util.h"

#include <media_server/serverutil.h>
#include "file_deletor.h"
#include "utils/common/synctime.h"
#include "common/common_module.h"

static const qint64 BALANCE_BY_FREE_SPACE_THRESHOLD = 1024*1024 * 500;
static const int OFFLINE_STORAGES_TEST_INTERVAL = 1000 * 30;
static const int DB_UPDATE_PER_RECORDS = 128;


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
            QnResource::Status status = fileStorage->isStorageAvailable() ? QnResource::Online : QnResource::Offline;
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
    m_mutexStorages(QMutex::Recursive),
    m_mutexCatalog(QMutex::Recursive),
    m_storagesStatisticsReady(false),
    m_catalogLoaded(false),
    m_warnSended(false),
    m_isWritableStorageAvail(false),
    m_rebuildState(RebuildState_None),
    m_rebuildProgress(0),
    m_asyncRebuildTask(0)
{
    m_lastTestTime.restart();
    m_storageWarnTimer.restart();
    m_testStorageThread = new TestStorageThread(this);

    assert( QnStorageManager_instance == nullptr );
    QnStorageManager_instance = this;
}

std::deque<DeviceFileCatalog::Chunk> QnStorageManager::correctChunksFromMediaData(const DeviceFileCatalogPtr &fileCatalog, const QnStorageResourcePtr &storage, const std::deque<DeviceFileCatalog::Chunk>& chunks)
{
    const QByteArray& mac = fileCatalog->getMac();
    QnServer::ChunksCatalog catalog = fileCatalog->getCatalog();

#if 0
    /* Check real media data at the begin of chunks list readed from DB.
    *  Db is updated after deleting DB_UPDATE_PER_RECORDS records, so we should check no more then DB_UPDATE_PER_RECORDS records in the begin
    */
    int toCheck = qMin(chunks.size(), DB_UPDATE_PER_RECORDS);
    for (int i = 0; i < toCheck; ++i)
    {
        if (!chunkExist(chunks[i]))
            deleteDbByChunk(storage, mac, role, chunks[i]);
    }
#endif

    /* Check new records, absent in the DB
    */
    QVector<DeviceFileCatalog::EmptyFileInfo> emptyFileList;
    QString rootDir = closeDirPath(storage->getUrl()) + DeviceFileCatalog::prefixByCatalog(catalog) + QString('/') + mac;
    DeviceFileCatalog::ScanFilter filter;
    if (!chunks.empty())
        filter.scanAfter = chunks[chunks.size()-1];

    QMap<qint64, DeviceFileCatalog::Chunk> newChunksMap;
    fileCatalog->scanMediaFiles(rootDir, storage, newChunksMap, emptyFileList, filter);
    std::deque<DeviceFileCatalog::Chunk> newChunks; // = newChunksMap.values().toVector();
    for(auto itr = newChunksMap.begin(); itr != newChunksMap.end(); ++itr)
        newChunks.push_back(itr.value());

    foreach(const DeviceFileCatalog::EmptyFileInfo& emptyFile, emptyFileList)
        qnFileDeletor->deleteFile(emptyFile.fileName);

    // add to DB
    QnStorageDbPtr sdb = m_chunksDB[storage->getUrl()];
    foreach(const DeviceFileCatalog::Chunk& chunk, newChunks)
        sdb->addRecord(mac, catalog, chunk);
    sdb->flushRecords();
    // merge chunks
    return DeviceFileCatalog::mergeChunks(chunks, newChunks);
}

QMap<QString, QSet<int>> QnStorageManager::deserializeStorageFile()
{
    QMap<QString, QSet<int>> storageIndexes;

    QFile storageFile(closeDirPath(getDataDirectory()) + QString("record_catalog/media/storage_index.csv"));
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
    QnStorageDbPtr sdb = m_chunksDB[storage->getUrl()];
    if (!sdb)
        sdb = m_chunksDB[storage->getUrl()] = QnStorageDbPtr(new QnStorageDb(storage->getIndex()));
    QString simplifiedGUID = qnCommon->moduleGUID().toString();
    simplifiedGUID = simplifiedGUID.replace("{", "");
    simplifiedGUID = simplifiedGUID.replace("}", "");
    QString fileName = closeDirPath(storage->getUrl()) + QString::fromLatin1("%1_media.sqlite").arg(simplifiedGUID);
    QString oldFileName = closeDirPath(storage->getUrl()) + QString::fromLatin1("media.sqlite");
    if (QFile::exists(oldFileName) && !QFile::exists(fileName))
        QFile::rename(oldFileName, fileName);

    if (!sdb->open(fileName))
    {
        qWarning() << "can't initialize sqlLite database! Actions log is not created!";
        return false;
    }

    if (!isRebuild)
    {
        // load from database
        foreach(DeviceFileCatalogPtr c, sdb->loadFullFileCatalog())
        {
            DeviceFileCatalogPtr fileCatalog = getFileCatalogInternal(c->getMac(), c->getCatalog());
            fileCatalog->addChunks(correctChunksFromMediaData(fileCatalog, storage, c->m_chunks));
        }
    }
    else {
        // load from media folder
        for (int i = 0; i < QnServer::ChunksCatalogCount; ++i) {
            loadFullFileCatalogFromMedia(storage, static_cast<QnServer::ChunksCatalog> (i), progressCoeff / QnServer::ChunksCatalogCount);
        }
        m_catalogLoaded = true;
        m_rebuildProgress = 1.0;
    }

    return true;
}

double QnStorageManager::rebuildProgress() const
{
    return m_rebuildProgress;
}

void QnStorageManager::rebuildCatalogIndexInternal()
{
    {
        QMutexLocker lock(&m_mutexCatalog);
        m_rebuildProgress = 0;
        //m_catalogLoaded = false;
        m_rebuildCancelled = false;
        /*
        foreach(DeviceFileCatalogPtr catalog,  m_devFileCatalogHi)
            catalog->beforeRebuildArchive();
        foreach(DeviceFileCatalogPtr catalog,  m_devFileCatalogLow)
            catalog->beforeRebuildArchive();
        m_devFileCatalogHi.clear();
        m_devFileCatalogLow.clear();
        */
        DeviceFileCatalog::setRebuildArchive(DeviceFileCatalog::Rebuild_All);
    }

    foreach (QnStorageResourcePtr storage, m_storageRoots.values())
        loadFullFileCatalog(storage, true, 1.0 / m_storageRoots.size());

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
    QDir dir(closeDirPath(storage->getUrl()) + DeviceFileCatalog::prefixByCatalog(catalog));
    QFileInfoList list = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    foreach(QFileInfo fi, list)
    {
        if (m_rebuildState != RebuildState_Started)
            return; // cancel rebuild

        qint64 rebuildEndTime = qnSyncTime->currentMSecsSinceEpoch() - 100 * 1000;
        DeviceFileCatalogPtr newCatalog(new DeviceFileCatalog(fi.fileName(), catalog));
        QnTimePeriod rebuildPeriod = QnTimePeriod(0, rebuildEndTime);
        newCatalog->doRebuildArchive(storage, rebuildPeriod);

        QByteArray mac = fi.fileName().toUtf8();
        DeviceFileCatalogPtr fileCatalog = getFileCatalogInternal(mac, catalog);
        replaceChunks(rebuildPeriod, storage, newCatalog, mac, catalog);

        m_rebuildProgress += progressCoeff / (double) list.size();
    }
}

QString QnStorageManager::toCanonicalPath(const QString& path)
{
    QString result = path;
    result.replace(L'\\', L'/');
    if (!result.endsWith(L'/'))
        result += QLatin1String("/");
    return result;
}

// determine storage index (aka 16 bit hash)
int QnStorageManager::detectStorageIndex(const QString& p)
{
    QString path = toCanonicalPath(p);

    if (m_storageIndexes.contains(path))
    {
        return *m_storageIndexes.value(path).begin();
    }
    else {
        int index = -1;
        foreach (const QSet<int>& indexes, m_storageIndexes.values()) 
        {
            foreach (const int& value, indexes) 
                index = qMax(index, value);
        }
        index++;
        m_storageIndexes.insert(path, QSet<int>() << index);
        return index;
    }
}

QSet<int> QnStorageManager::getDeprecateIndexList(const QString& p)
{
    QString path = toCanonicalPath(p);
    QSet<int> result = m_storageIndexes.value(path);

    if (!result.isEmpty())
        result.erase(result.begin());

    return result;
}

void QnStorageManager::addStorage(const QnStorageResourcePtr &storage)
{
    storage->setIndex(detectStorageIndex(storage->getUrl()));
    QMutexLocker lock(&m_mutexStorages);
    m_storagesStatisticsReady = false;
    
    NX_LOG(QString("Adding storage. Path: %1. SpaceLimit: %2MiB. Currently available: %3MiB").arg(storage->getUrl()).arg(storage->getSpaceLimit() / 1024 / 1024).arg(storage->getFreeSpace() / 1024 / 1024), cl_logINFO);

    removeStorage(storage); // remove existing storage record if exists
    //QnStorageResourcePtr oldStorage = removeStorage(storage); // remove existing storage record if exists
    //if (oldStorage)
    //    storage->addWritedSpace(oldStorage->getWritedSpace());
    m_storageRoots.insert(storage->getIndex(), storage);
    if (storage->isStorageAvailable())
        storage->setStatus(QnResource::Online);


    QSet<int> depracateStorageIndexes = getDeprecateIndexList(storage->getUrl());
    foreach(const int& value, depracateStorageIndexes)
        m_storageRoots.insert(value, storage);

    connect(storage.data(), SIGNAL(archiveRangeChanged(const QnAbstractStorageResourcePtr &, qint64, qint64)), this, SLOT(at_archiveRangeChanged(const QnAbstractStorageResourcePtr &, qint64, qint64)), Qt::DirectConnection);
    loadFullFileCatalog(storage);
}

QStringList QnStorageManager::getAllStoragePathes() const
{
    return m_storageIndexes.keys();
}

void QnStorageManager::removeStorage(const QnStorageResourcePtr &storage)
{
    QMutexLocker lock(&m_mutexStorages);
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

bool QnStorageManager::existsStorageWithID(const QnAbstractStorageResourceList& storages, const QnId &id) const
{
    foreach(const QnAbstractStorageResourcePtr& storage, storages)
    {
        if (storage->getId() == id)
            return true;
    }
    return false;
}

void QnStorageManager::removeAbsentStorages(const QnAbstractStorageResourceList &newStorages)
{
    QMutexLocker lock(&m_mutexStorages);
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

QString QnStorageManager::dateTimeStr(qint64 dateTimeMs, qint16 timeZone)
{
    QString text;
    QTextStream str(&text);
    QDateTime fileDate = QDateTime::fromMSecsSinceEpoch(dateTimeMs);
    if (timeZone != -1)
        fileDate = fileDate.toUTC().addSecs(timeZone*60);
    str << QString::number(fileDate.date().year()) << '/';
    str << strPadLeft(QString::number(fileDate.date().month()), 2, '0') << '/';
    str << strPadLeft(QString::number(fileDate.date().day()), 2, '0') << '/';
    str << strPadLeft(QString::number(fileDate.time().hour()), 2, '0') << '/';
    str.flush();
    return text;
}

void QnStorageManager::getTimePeriodInternal(QVector<QnTimePeriodList> &cameras, const QnNetworkResourcePtr &camera, qint64 startTime, qint64 endTime, qint64 detailLevel, const DeviceFileCatalogPtr &catalog)
{
    if (catalog) {
        cameras << catalog->getTimePeriods(startTime, endTime, detailLevel);
        if (!cameras.last().isEmpty())
        {
            QnTimePeriod& lastPeriod = cameras.last().last();
            bool isActive = !camera->hasFlags(QnResource::foreigner) && (camera->getStatus() == QnResource::Online || camera->getStatus() == QnResource::Recording);
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

bool QnStorageManager::isArchiveTimeExists(const QString& physicalId, qint64 timeMs)
{
    DeviceFileCatalogPtr catalog = getFileCatalog(physicalId.toUtf8(), QnServer::HiQualityCatalog);
    if (catalog && catalog->containTime(timeMs))
        return true;

    catalog = getFileCatalog(physicalId.toUtf8(), QnServer::LowQualityCatalog);
    return catalog && catalog->containTime(timeMs);
}


QnTimePeriodList QnStorageManager::getRecordedPeriods(const QnResourceList &resList, qint64 startTime, qint64 endTime, qint64 detailLevel, const QList<QnServer::ChunksCatalog> &catalogs) {
    QVector<QnTimePeriodList> periods;
    foreach (const QnResourcePtr &resource, resList) {
        QnVirtualCameraResourcePtr camera = qSharedPointerDynamicCast<QnVirtualCameraResource> (resource);
        if (!camera)
            continue;

        QString physicalId = camera->getPhysicalId();
        for (int i = 0; i < QnServer::ChunksCatalogCount; ++i) {
            QnServer::ChunksCatalog catalog = static_cast<QnServer::ChunksCatalog> (i);
            if (!catalogs.contains(catalog))
                continue;

            //TODO: #GDM #Bookmarks forbid bookmarks for the DTS cameras
            if (camera->isDtsBased()) {
                if (catalog == QnServer::HiQualityCatalog) // both hi- and low-quality chunks are loaded with this method
                    periods << camera->getDtsTimePeriods(startTime, endTime, detailLevel);
            } else {
                getTimePeriodInternal(periods, camera, startTime, endTime, detailLevel, getFileCatalog(physicalId.toUtf8(), catalog));
            }
        }

    }

    return QnTimePeriodList::mergeTimePeriods(periods);
}

void QnStorageManager::clearSpace()
{
    if (!m_catalogLoaded)
        return;
    const QSet<QnStorageResourcePtr> storages = getWritableStorages();
    foreach(QnStorageResourcePtr storage, storages)
        clearSpace(storage);
}

QnStorageManager::StorageMap QnStorageManager::getAllStorages() const 
{ 
    QMutexLocker lock(&m_mutexStorages); 
    return m_storageRoots; 
} 

QnStorageResourceList QnStorageManager::getStorages() const 
{
    QMutexLocker lock(&m_mutexStorages);
    return m_storageRoots.values().toSet().toList(); // remove storage duplicates. Duplicates are allowed in sake for v1.4 compatibility
}

void QnStorageManager::clearSpace(const QnStorageResourcePtr &storage)
{
    if (storage->getSpaceLimit() == 0)
        return; // unlimited


    QString dir = storage->getUrl();

    if (!storage->isNeedControlFreeSpace())
        return;

    qint64 freeSpace = storage->getFreeSpace();
    if (freeSpace == -1)
        return;
    qint64 toDelete = storage->getSpaceLimit() - freeSpace;

    QnStorageDbPtr sdb = m_chunksDB[storage->getUrl()];
    sdb->beforeDelete();

    while (toDelete > 0)
    {
        qint64 minTime = 0x7fffffffffffffffll;
        QByteArray mac;
        DeviceFileCatalogPtr catalog;
        {
            QMutexLocker lock(&m_mutexCatalog);
            for (FileCatalogMap::const_iterator itr = m_devFileCatalog[QnServer::HiQualityCatalog].constBegin(); itr != m_devFileCatalog[QnServer::HiQualityCatalog].constEnd(); ++itr)
            {
                qint64 firstTime = itr.value()->firstTime();
                if (firstTime != (qint64)AV_NOPTS_VALUE && firstTime < minTime)
                {
                    minTime = itr.value()->firstTime();
                    mac = itr.key();
                }
            }
            if (!mac.isEmpty())
                catalog = getFileCatalog(mac, QnServer::HiQualityCatalog);
        }
        if (catalog != 0) 
        {
            qint64 deletedTime = catalog->deleteFirstRecord();
            sdb->deleteRecords(mac, QnServer::HiQualityCatalog, deletedTime);
            
            DeviceFileCatalogPtr catalogLowRes = getFileCatalog(mac, QnServer::LowQualityCatalog);
            if (catalogLowRes != 0) 
            {
                qint64 minTime = catalog->minTime();
                if (minTime != (qint64)AV_NOPTS_VALUE) {
                    int idx = catalogLowRes->findFileIndex(minTime, DeviceFileCatalog::OnRecordHole_NextChunk);
                    if (idx != -1) {
                        QVector<qint64> deletedTimeList = catalogLowRes->deleteRecordsBefore(idx);
                        foreach(const qint64& deletedTime, deletedTimeList)
                            sdb->deleteRecords(mac, QnServer::LowQualityCatalog, deletedTime);
                    }
                }
                else {
                    catalogLowRes->clear();
                    sdb->deleteRecords(mac, QnServer::LowQualityCatalog);
                }

                if (catalog->isEmpty() && catalogLowRes->isEmpty())
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

    if (toDelete > 0) {
        if (!m_diskFullWarned[storage->getId()]) {
            QnMediaServerResourcePtr mediaServer = qSharedPointerDynamicCast<QnMediaServerResource> (qnResPool->getResourceById(serverGuid()));
            emit storageFailure(storage, QnBusiness::StorageFullReason);
            m_diskFullWarned[storage->getId()] = true;
        }
    }
    else {
        m_diskFullWarned[storage->getId()] = false;
    }

    sdb->afterDelete();
}

void QnStorageManager::at_archiveRangeChanged(const QnAbstractStorageResourcePtr &resource, qint64 newStartTimeMs, qint64 newEndTimeMs)
{
    Q_UNUSED(newEndTimeMs)
    int storageIndex = detectStorageIndex(resource->getUrl());

    foreach(DeviceFileCatalogPtr catalogHi, m_devFileCatalog[QnServer::HiQualityCatalog])
        catalogHi->deleteRecordsByStorage(storageIndex, newStartTimeMs);
    
    foreach(DeviceFileCatalogPtr catalogLow, m_devFileCatalog[QnServer::LowQualityCatalog])
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
        if (fileStorage && fileStorage->getStatus() != QnResource::Offline && fileStorage->isUsedForWriting()) 
        {
            qint64 available = fileStorage->getTotalSpace() - fileStorage->getSpaceLimit();
            bigStorageThreshold = qMax(bigStorageThreshold, available);
        }
    }
    bigStorageThreshold /= BIG_STORAGE_THRESHOLD_COEFF;

    for (StorageMap::const_iterator itr = storageRoots.constBegin(); itr != storageRoots.constEnd(); ++itr)
    {
        QnFileStorageResourcePtr fileStorage = qSharedPointerDynamicCast<QnFileStorageResource> (itr.value());
        if (fileStorage && fileStorage->getStatus() != QnResource::Offline && fileStorage->isUsedForWriting()) 
        {
            qint64 available = fileStorage->getTotalSpace() - fileStorage->getSpaceLimit();
            if (available >= bigStorageThreshold)
                result << fileStorage;
        }
    }
    return result;
}

void QnStorageManager::changeStorageStatus(const QnStorageResourcePtr &fileStorage, QnResource::Status status)
{
    QMutexLocker lock(&m_mutexStorages);
    fileStorage->setStatus(status);
    m_storagesStatisticsReady = false;
    if (status == QnResource::Offline)
        emit storageFailure(fileStorage, QnBusiness::StorageIoErrorReason);
}

void QnStorageManager::testOfflineStorages()
{
    QMutexLocker lock(&m_mutexStorages);

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
}

void QnStorageManager::updateStorageStatistics()
{
    QMutexLocker lock(&m_mutexStorages);
    if (m_storagesStatisticsReady) 
        return;

    double totalSpace = 0;
    QSet<QnStorageResourcePtr> storages = getWritableStorages();
    m_isWritableStorageAvail = !storages.isEmpty();
    for (QSet<QnStorageResourcePtr>::const_iterator itr = storages.constBegin(); itr != storages.constEnd(); ++itr)
    {
        QnFileStorageResourcePtr fileStorage = qSharedPointerDynamicCast<QnFileStorageResource> (*itr);
        qint64 storageSpace = qMax(0ll, fileStorage->getTotalSpace() - fileStorage->getSpaceLimit());
        totalSpace += storageSpace;
    }

    for (QSet<QnStorageResourcePtr>::const_iterator itr = storages.constBegin(); itr != storages.constEnd(); ++itr)
    {
        QnFileStorageResourcePtr fileStorage = qSharedPointerDynamicCast<QnFileStorageResource> (*itr);

        qint64 storageSpace = qMax(0ll, fileStorage->getTotalSpace() - fileStorage->getSpaceLimit());
        // write to large HDD more often then small HDD
        fileStorage->setStorageBitrateCoeff(1.0 - storageSpace / totalSpace);
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
        qDebug() << "QnFileStorageResource " << storage->getUrl() << "current bitrate=" << storage->bitrate();
        float bitrate = storage->bitrate() * storage->getStorageBitrateCoeff();
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
    QMutexLocker lock(&m_cacheMutex);
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
    QMutexLocker lock(&m_cacheMutex);
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
    QString base = closeDirPath(storage->getUrl());

    if (!prefix.isEmpty())
        base += prefix + "/";
    base += camera->getPhysicalId();

    Q_ASSERT(!camera->getPhysicalId().isEmpty());
    QString text = base + QString("/") + dateTimeStr(dateTime, timeZone);

    int fileNum = getFileNumFromCache(base, text);
    if (fileNum == -1)
    {
        fileNum = 0;
        QList<QFileInfo> list = storage->getFileList(text);
        QList<QString> baseNameList;
        foreach(const QFileInfo& info, list)
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

DeviceFileCatalogPtr QnStorageManager::getFileCatalog(const QByteArray& mac, const QString &catalogPrefix)
{
    if (!m_catalogLoaded)
        return DeviceFileCatalogPtr();
    return getFileCatalogInternal(mac, DeviceFileCatalog::catalogByPrefix(catalogPrefix));
}

DeviceFileCatalogPtr QnStorageManager::getFileCatalog(const QByteArray& mac, QnServer::ChunksCatalog catalog)
{
    if (!m_catalogLoaded)
        return DeviceFileCatalogPtr();
    return getFileCatalogInternal(mac, catalog);
}

void QnStorageManager::replaceChunks(const QnTimePeriod& rebuildPeriod, const QnStorageResourcePtr &storage, const DeviceFileCatalogPtr &newCatalog, const QByteArray& mac, QnServer::ChunksCatalog catalog)
{
    QMutexLocker lock(&m_mutexCatalog);
    int storageIndex = storage->getIndex();
    
    // add new recorded chunks to scan data
    qint64 scannedDataLastTime = newCatalog->m_chunks.empty() ? 0 : newCatalog->m_chunks[newCatalog->m_chunks.size()-1].startTimeMs;
    qint64 rebuildLastTime = qMax(rebuildPeriod.endTimeMs(), scannedDataLastTime);
    
    DeviceFileCatalogPtr ownCatalog = getFileCatalogInternal(mac, catalog);
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

    qint64 recordingTime = ownCatalog->getLatRecordingTime();
    ownCatalog->replaceChunks(storageIndex, newCatalog->m_chunks);
    if (recordingTime > 0)
        ownCatalog->setLatRecordingTime(recordingTime);

    QnStorageDbPtr sdb = m_chunksDB[storage->getUrl()];
    sdb->replaceChunks(mac, catalog, newCatalog->m_chunks);
}

DeviceFileCatalogPtr QnStorageManager::getFileCatalogInternal(const QByteArray& mac, QnServer::ChunksCatalog catalog)
{
    QMutexLocker lock(&m_mutexCatalog);
    FileCatalogMap& catalogMap = m_devFileCatalog[catalog];
    DeviceFileCatalogPtr fileCatalog = catalogMap[mac];
    if (fileCatalog == 0)
    {
        fileCatalog = DeviceFileCatalogPtr(new DeviceFileCatalog(mac, catalog));
        catalogMap[mac] = fileCatalog;
    }
    return fileCatalog;
}

QnStorageResourcePtr QnStorageManager::extractStorageFromFileName(int& storageIndex, const QString& fileName, QString& mac, QString& quality)
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
        QString root = closeDirPath(itr.value()->getUrl());
        if (fileName.startsWith(root))
        {
            int qualityLen = fileName.indexOf('/', root.length()+1) - root.length();
            quality = fileName.mid(root.length(), qualityLen);
            int macPos = root.length() + qualityLen;
            mac = fileName.mid(macPos+1, fileName.indexOf('/', macPos+1) - macPos-1);
            storageIndex = itr.value()->getIndex();
            return *itr;
        }
    }
    return QnStorageResourcePtr();
}

QnStorageResourcePtr QnStorageManager::getStorageByUrl(const QString& fileName)
{
    QMutexLocker lock(&m_mutexStorages);
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
    QString quality, mac;
    QnStorageResourcePtr storage = extractStorageFromFileName(storageIndex, fileName, mac, quality);
    if (storageIndex >= 0)
        storage->releaseBitrate(provider);
        //storage->addWritedSpace(fileSize);

    DeviceFileCatalogPtr catalog = getFileCatalog(mac.toUtf8(), quality);
    if (catalog == 0)
        return false;
    QnStorageDbPtr sdb = m_chunksDB[storage->getUrl()];
    sdb->addRecord(mac.toUtf8(), DeviceFileCatalog::catalogByPrefix(quality), catalog->updateDuration(durationMs, fileSize));
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

void QnStorageManager::doMigrateCSVCatalog() {
    for (int i = 0; i < QnServer::ChunksCatalogCount; ++i)
        doMigrateCSVCatalog(static_cast<QnServer::ChunksCatalog>(i));
    m_catalogLoaded = true;
    m_rebuildProgress = 1.0;
}

QnStorageResourcePtr QnStorageManager::findStorageByOldIndex(int oldIndex, QMap<QString, QSet<int>> oldIndexes)
{
    for(QMap<QString, QSet<int>>::const_iterator itr = oldIndexes.begin(); itr != oldIndexes.end(); ++itr)
    {
        foreach(int idx, itr.value())
        {
            if (oldIndex == idx)
                return getStorageByUrl(itr.key());
        }
    }
    return QnStorageResourcePtr();
}

void QnStorageManager::doMigrateCSVCatalog(QnServer::ChunksCatalog catalog)
{
    QMap<QString, QSet<int>> storageIndexes = deserializeStorageFile();
    QDir dir(closeDirPath(getDataDirectory()) + QString("record_catalog/media/") + DeviceFileCatalog::prefixByCatalog(catalog));
    QFileInfoList list = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    foreach(QFileInfo fi, list) 
    {
        QByteArray mac = fi.fileName().toUtf8();
        DeviceFileCatalogPtr catalogFile = getFileCatalogInternal(mac, catalog);
        QString catalogName = closeDirPath(fi.absoluteFilePath()) + lit("title.csv");
        if (catalogFile->fromCSVFile(catalogName)) 
        {
            foreach(const DeviceFileCatalog::Chunk& chunk, catalogFile->m_chunks) 
            {
                QnStorageResourcePtr storage = findStorageByOldIndex(chunk.storageIndex, storageIndexes);
                if (storage) {
                    QnStorageDbPtr sdb = m_chunksDB[storage->getUrl()];
                    sdb->addRecord(mac, catalog, chunk);
                }
            }
            foreach(QnStorageDbPtr sdb, m_chunksDB.values())
                sdb->flushRecords();
            QFile::remove(catalogName);
            QDir dir;
        }
        dir.rmdir(fi.absoluteFilePath());
    }
}

bool QnStorageManager::isStorageAvailable(int storage_index) const {
    QnStorageResourcePtr storage = storageRoot(storage_index);
    return storage && storage->getStatus() == QnResource::Online; 
}

bool QnStorageManager::addBookmark(const QByteArray &cameraGuid, QnCameraBookmark &bookmark) {

    QnDualQualityHelper helper;
    helper.openCamera(cameraGuid);

    DeviceFileCatalog::Chunk chunkBegin, chunkEnd;
    DeviceFileCatalogPtr catalog;
    helper.findDataForTime(bookmark.startTimeMs, chunkBegin, catalog, DeviceFileCatalog::OnRecordHole_NextChunk, false);
    if (chunkBegin.startTimeMs < 0)
        return false; //recorded chunk was not found

    helper.findDataForTime(bookmark.endTimeMs(), chunkEnd, catalog, DeviceFileCatalog::OnRecordHole_PrevChunk, false);
    if (chunkEnd.startTimeMs < 0)
        return false; //recorded chunk was not found

    qint64 endTimeMs = bookmark.endTimeMs();

    bookmark.startTimeMs = qMax(bookmark.startTimeMs, chunkBegin.startTimeMs);  // move bookmark start to the start of the chunk in case of hole
    endTimeMs = qMin(endTimeMs, chunkEnd.endTimeMs());
    bookmark.durationMs = endTimeMs - bookmark.startTimeMs;                     // move bookmark end to the end of the closest chunk in case of hole

    if (bookmark.durationMs <= 0)
        return false;   // bookmark ends before the chunk starts

    // this chunk will be added to the bookmark catalog
    chunkBegin.startTimeMs = bookmark.startTimeMs;
    chunkBegin.durationMs = bookmark.durationMs;

    DeviceFileCatalogPtr bookmarksCatalog = getFileCatalog(cameraGuid, QnServer::BookmarksCatalog);
    bookmarksCatalog->addChunk(chunkBegin);

    QnStorageResourcePtr storage = storageRoot(chunkBegin.storageIndex);
    if (!storage)
        return false;

    QnStorageDbPtr sdb = m_chunksDB[storage->getUrl()];
    if (!sdb)
        return false;

    if (!sdb->addOrUpdateCameraBookmark(bookmark, cameraGuid))
        return false;

    if (!bookmark.tags.isEmpty())
        QnAppServerConnectionFactory::getConnection2()->getCameraManager()->addBookmarkTags(bookmark.tags, this, [](int /*reqID*/, ec2::ErrorCode /*errorCode*/) {});
    

    return true;
}

bool QnStorageManager::updateBookmark(const QByteArray &cameraGuid, QnCameraBookmark &bookmark) {
    DeviceFileCatalogPtr catalog = qnStorageMan->getFileCatalog(cameraGuid, QnServer::BookmarksCatalog);
    int idx = catalog->findFileIndex(bookmark.startTimeMs, DeviceFileCatalog::OnRecordHole_NextChunk);
    if (idx < 0)
        return false;
    QnStorageResourcePtr storage = storageRoot(catalog->chunkAt(idx).storageIndex);
    if (!storage)
        return false;

    QnStorageDbPtr sdb = m_chunksDB[storage->getUrl()];
    if (!sdb)
        return false;

    if (!sdb->addOrUpdateCameraBookmark(bookmark, cameraGuid))
        return false;

    if (!bookmark.tags.isEmpty())
        QnAppServerConnectionFactory::getConnection2()->getCameraManager()->addBookmarkTags(bookmark.tags, this, [](int /*reqID*/, ec2::ErrorCode /*errorCode*/) {});

    return true;
}


bool QnStorageManager::deleteBookmark(const QByteArray &cameraGuid, QnCameraBookmark &bookmark) {
    DeviceFileCatalogPtr catalog = qnStorageMan->getFileCatalog(cameraGuid, QnServer::BookmarksCatalog);
    if (!catalog)
        return false;

    DeviceFileCatalog::Chunk chunk = catalog->takeChunk(bookmark.startTimeMs, bookmark.durationMs);
    if (chunk.durationMs == 0)
        return false;

    QnStorageResourcePtr storage = storageRoot(chunk.storageIndex);
    if (!storage)
        return false;

    QnStorageDbPtr sdb = m_chunksDB[storage->getUrl()];
    if (!sdb)
        return false;

    if (!sdb->deleteCameraBookmark(bookmark, cameraGuid))
        return false;

    return true;
}


bool QnStorageManager::getBookmarks(const QByteArray &cameraGuid, const QnCameraBookmarkSearchFilter &filter, QnCameraBookmarkList &result) {
    foreach (const QnStorageDbPtr &sdb, m_chunksDB) {
        if (!sdb)
            continue;
        if (!sdb->getBookmarks(cameraGuid, filter, result))
            return false;
    }
    return true;
}
