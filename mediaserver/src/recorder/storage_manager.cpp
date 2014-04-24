#include "storage_manager.h"

#include <QtCore/QDir>

#include "core/resource_management/resource_pool.h"
#include "core/resource/resource.h"
#include "core/resource/camera_resource.h"
#include <core/resource/media_server_resource.h>

#include <recorder/server_stream_recorder.h>
#include <recorder/recording_manager.h>

#include "plugins/storage/file_storage/file_storage_resource.h"

#include "utils/common/sleep.h"
#include <utils/fs/file.h>
#include "utils/common/util.h"

#include <media_server/serverutil.h>
#include "file_deletor.h"

static const qint64 BALANCE_BY_FREE_SPACE_THRESHOLD = 1024*1024 * 500;
static const int OFFLINE_STORAGES_TEST_INTERVAL = 1000 * 30;
static const int DB_UPDATE_PER_RECORDS = 128;

Q_GLOBAL_STATIC(QnStorageManager, QnStorageManager_inst)


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
}

QVector<DeviceFileCatalog::Chunk> QnStorageManager::correctChunksFromMediaData(DeviceFileCatalogPtr fileCatalog, QnStorageResourcePtr storage, const QVector<DeviceFileCatalog::Chunk>& chunks)
{
    const QByteArray& mac = fileCatalog->getMac();
    QnResource::ConnectionRole role = fileCatalog->getRole();

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
    QStringList emptyFileList;
    QString rootDir = closeDirPath(storage->getUrl()) + DeviceFileCatalog::prefixForRole(role) + QString('/') + mac;
    DeviceFileCatalog::ScanFilter filter;
    if (!chunks.isEmpty())
        filter.scanAfter = chunks.last();

    QMap<qint64, DeviceFileCatalog::Chunk> newChunksMap;
    fileCatalog->scanMediaFiles(rootDir, storage, newChunksMap, emptyFileList, filter);
    QVector<DeviceFileCatalog::Chunk> newChunks = newChunksMap.values().toVector();

    foreach(const QString& fileName, emptyFileList)
        qnFileDeletor->deleteFile(fileName);

    // add to DB
    QnStorageDbPtr sdb = m_chunksDB[storage->getUrl()];
    foreach(const DeviceFileCatalog::Chunk& chunk, newChunks)
        sdb->addRecord(mac, role, chunk);
    sdb->flushRecords();
    // merge chunks
    return DeviceFileCatalog::mergeChunks(chunks, newChunks);
}

bool QnStorageManager::loadFullFileCatalog(QnStorageResourcePtr storage, bool isRebuild)
{
    QnStorageDbPtr sdb = m_chunksDB[storage->getUrl()];
    if (!sdb)
        sdb = m_chunksDB[storage->getUrl()] = QnStorageDbPtr(new QnStorageDb(storage->getIndex()));
    QString fileName = closeDirPath(storage->getUrl()) + QString::fromLatin1("media.sqlite");

    if (!sdb->open(fileName))
    {
        qWarning() << "can't initialize sqlLite database! Actions log is not created!";
        return false;
    }
    foreach(DeviceFileCatalogPtr c, sdb->loadFullFileCatalog())
    {
        DeviceFileCatalogPtr fileCatalog = getFileCatalogInternal(c->getMac(), c->getRole());
        fileCatalog->addChunks(correctChunksFromMediaData(fileCatalog, storage, c->m_chunks));
    }

    return true;
}

double QnStorageManager::rebuildProgress() const
{
    return m_rebuildProgress;
}

void QnStorageManager::loadFullFileCatalog()
{
    /*
    foreach (QnStorageResourcePtr storage, m_storageRoots.values())
        loadFullFileCatalog(storage, true);
    */
    m_catalogLoaded = true;
    m_rebuildProgress = 1.0;
};

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
        loadFullFileCatalog(storage, true);

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
        cl_log.log("Catalog rebuild operation is canceled", cl_logINFO);
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

/*
void QnStorageManager::loadFullFileCatalogInternal(QnResource::ConnectionRole role, bool rebuildMode)
{
#ifdef _TEST_TWO_SERVERS
    QDir dir(closeDirPath(getDataDirectory()) + QString("test/record_catalog/media/") + DeviceFileCatalog::prefixForRole(role));
#else
    QDir dir(closeDirPath(getDataDirectory()) + QString("record_catalog/media/") + DeviceFileCatalog::prefixForRole(role));
#endif
    QFileInfoList list = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    foreach(QFileInfo fi, list)
    {
        if (rebuildMode && m_rebuildState != RebuildState_Started)
            return; // cancel rebuild
        if (rebuildMode)
        {
            DeviceFileCatalogPtr catalog(new DeviceFileCatalog(fi.fileName(), role));
            catalog->doRebuildArchive();
            addDataToCatalog(catalog, fi.fileName(), role);
            m_rebuildProgress += 0.5 / (double) list.size(); // we load catalog twice (HQ and LQ), so, use 0.5 instead of 1.0 for progress
        }
        else {
            getFileCatalogInternal(fi.fileName(), role);
        }
    }
}
*/


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

void QnStorageManager::addStorage(QnStorageResourcePtr storage)
{
    storage->setIndex(detectStorageIndex(storage->getUrl()));
    QMutexLocker lock(&m_mutexStorages);
    m_storagesStatisticsReady = false;
    
    cl_log.log(QString(QLatin1String("Adding storage. Path: %1. SpaceLimit: %2MiB. Currently avaiable: %3MiB")).arg(storage->getUrl()).arg(storage->getSpaceLimit() / 1024 / 1024).arg(storage->getFreeSpace() / 1024 / 1024), cl_logINFO);

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

void QnStorageManager::removeStorage(QnStorageResourcePtr storage)
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

bool QnStorageManager::existsStorageWithID(const QnAbstractStorageResourceList& storages, QnId id) const
{
    foreach(const QnAbstractStorageResourcePtr& storage, storages)
    {
        if (storage->getId() == id)
            return true;
    }
    return false;
}

void QnStorageManager::removeAbsentStorages(QnAbstractStorageResourceList newStorages)
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
    stopAsyncTasks();
}

QnStorageManager* QnStorageManager::instance()
{
    return QnStorageManager_inst();
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

void QnStorageManager::getTimePeriodInternal(QVector<QnTimePeriodList>& cameras, QnNetworkResourcePtr camera, qint64 startTime, qint64 endTime, qint64 detailLevel, DeviceFileCatalogPtr catalog)
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
                if (catalog->getRole() == QnResource::Role_LiveVideo && recorders.recorderHiRes)
                    lastPeriod.durationMs = recorders.recorderHiRes->duration()/1000;
                else if (catalog->getRole() == QnResource::Role_SecondaryLiveVideo && recorders.recorderLowRes)
                    lastPeriod.durationMs = recorders.recorderLowRes->duration()/1000;
            }
        }
    }
}

bool QnStorageManager::isArchiveTimeExists(const QString& physicalId, qint64 timeMs)
{
    DeviceFileCatalogPtr catalog = getFileCatalog(physicalId.toUtf8(), QnResource::Role_LiveVideo);
    if (catalog && catalog->containTime(timeMs))
        return true;

    catalog = getFileCatalog(physicalId.toUtf8(), QnResource::Role_SecondaryLiveVideo);
    return catalog && catalog->containTime(timeMs);
}


QnTimePeriodList QnStorageManager::getRecordedPeriods(QnResourceList resList, qint64 startTime, qint64 endTime, qint64 detailLevel)
{
    QVector<QnTimePeriodList> periods;
    for (int i = 0; i < resList.size(); ++i)
    {
        QnVirtualCameraResourcePtr camera = qSharedPointerDynamicCast<QnVirtualCameraResource> (resList[i]);
        if (camera) {
            if (camera->isDtsBased())
            {
                periods << camera->getDtsTimePeriods(startTime, endTime, detailLevel);
            }
            else {
                QString physicalId = camera->getPhysicalId();
                getTimePeriodInternal(periods, camera, startTime, endTime, detailLevel, getFileCatalog(physicalId.toUtf8(), QnResource::Role_LiveVideo));
                getTimePeriodInternal(periods, camera, startTime, endTime, detailLevel, getFileCatalog(physicalId.toUtf8(), QnResource::Role_SecondaryLiveVideo));
            }
        }
    }

    return QnTimePeriod::mergeTimePeriods(periods);
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

void QnStorageManager::clearSpace(QnStorageResourcePtr storage)
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
            for (FileCatalogMap::const_iterator itr = m_devFileCatalogHi.constBegin(); itr != m_devFileCatalogHi.constEnd(); ++itr)
            {
                qint64 firstTime = itr.value()->firstTime();
                if (firstTime != (qint64)AV_NOPTS_VALUE && firstTime < minTime)
                {
                    minTime = itr.value()->firstTime();
                    mac = itr.key();
                }
            }
            if (!mac.isEmpty())
                catalog = getFileCatalog(mac, QnResource::Role_LiveVideo);
        }
        if (catalog != 0) 
        {
            qint64 deletedTime = catalog->deleteFirstRecord();
            sdb->deleteRecords(mac, QnResource::Role_LiveVideo, deletedTime);
            
            DeviceFileCatalogPtr catalogLowRes = getFileCatalog(mac, QnResource::Role_SecondaryLiveVideo);
            if (catalogLowRes != 0) 
            {
                qint64 minTime = catalog->minTime();
                if (minTime != (qint64)AV_NOPTS_VALUE) {
                    int idx = catalogLowRes->findFileIndex(minTime, DeviceFileCatalog::OnRecordHole_NextChunk);
                    if (idx != -1) {
                        QVector<qint64> deletedTimeList = catalogLowRes->deleteRecordsBefore(idx);
                        foreach(const qint64& deletedTime, deletedTimeList)
                            sdb->deleteRecords(mac, QnResource::Role_SecondaryLiveVideo, deletedTime);
                    }
                }
                else {
                    catalogLowRes->clear();
                    sdb->deleteRecords(mac, QnResource::Role_SecondaryLiveVideo);
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
            emit storageFailure(storage, QnBusiness::StorageNotEnoughSpaceReason);
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

    foreach(DeviceFileCatalogPtr catalogHi, m_devFileCatalogHi)
        catalogHi->deleteRecordsByStorage(storageIndex, newStartTimeMs);
    
    foreach(DeviceFileCatalogPtr catalogLow, m_devFileCatalogLow)
        catalogLow->deleteRecordsByStorage(storageIndex, newStartTimeMs);
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

void QnStorageManager::changeStorageStatus(QnStorageResourcePtr fileStorage, QnResource::Status status)
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

QString QnStorageManager::getFileName(const qint64& dateTime, qint16 timeZone, const QnNetworkResourcePtr camera, const QString& prefix, QnStorageResourcePtr& storage)
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

DeviceFileCatalogPtr QnStorageManager::getFileCatalog(const QByteArray& mac, const QString& qualityPrefix)
{
    if (!m_catalogLoaded)
        return DeviceFileCatalogPtr();
    return getFileCatalogInternal(mac, DeviceFileCatalog::roleForPrefix(qualityPrefix));
}

DeviceFileCatalogPtr QnStorageManager::getFileCatalog(const QByteArray& mac, QnResource::ConnectionRole role)
{
    if (!m_catalogLoaded)
        return DeviceFileCatalogPtr();
    return getFileCatalogInternal(mac, role);
}

/*
void QnStorageManager::addDataToCatalog(DeviceFileCatalogPtr newCatalog, const QByteArray& mac, QnResource::ConnectionRole role)
{
    QMutexLocker lock(&m_mutexCatalog);
    bool hiQuality = role == QnResource::Role_LiveVideo;
    FileCatalogMap& catalog = hiQuality ? m_devFileCatalogHi : m_devFileCatalogLow;
    DeviceFileCatalogPtr existingCatalog = catalog[mac];
    bool isLastRecordRecording = false;
    if (existingCatalog == 0)
    {
        existingCatalog = catalog[mac] = newCatalog;
    }
    else 
    {
        existingCatalog->close();

        isLastRecordRecording = existingCatalog->isLastRecordRecording();
        if (!newCatalog->isEmpty() && !existingCatalog->isEmpty()) {
            // merge data
            DeviceFileCatalog::Chunk& newChunk = newCatalog->m_chunks.last();
            int idx = existingCatalog->m_chunks.size()-1;
            for (; idx >= 0; --idx)
            {
                DeviceFileCatalog::Chunk oldChunk = existingCatalog->chunkAt(idx);
                if (oldChunk.startTimeMs < newChunk.startTimeMs)
                    break;
            }
            for (int i = idx+1; i < existingCatalog->m_chunks.size(); ++i)
            {
                if (existingCatalog->m_chunks[i].startTimeMs == newChunk.startTimeMs)
                    newChunk.durationMs = existingCatalog->m_chunks[i].durationMs;
                else
                    newCatalog->addChunk(existingCatalog->m_chunks[i]);
            }

            qint64 recordingTime = existingCatalog->getLatRecordingTime();
            existingCatalog = catalog[mac] = newCatalog;
            if (recordingTime > 0)
                existingCatalog->setLatRecordingTime(recordingTime);
        }
        else if (existingCatalog->isEmpty())
        {
            existingCatalog->m_chunks = newCatalog->m_chunks;
        }
    }
    existingCatalog->rewriteCatalog(isLastRecordRecording);
}
*/

DeviceFileCatalogPtr QnStorageManager::getFileCatalogInternal(const QByteArray& mac, QnResource::ConnectionRole role)
{
    QMutexLocker lock(&m_mutexCatalog);
    bool hiQuality = role == QnResource::Role_LiveVideo;
    FileCatalogMap& catalog = hiQuality ? m_devFileCatalogHi : m_devFileCatalogLow;
    DeviceFileCatalogPtr fileCatalog = catalog[mac];
    if (fileCatalog == 0)
    {
        fileCatalog = DeviceFileCatalogPtr(new DeviceFileCatalog(mac, role));
        catalog[mac] = fileCatalog;
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
    sdb->addRecord(mac.toUtf8(), DeviceFileCatalog::roleForPrefix(quality), catalog->updateDuration(durationMs, fileSize));
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
