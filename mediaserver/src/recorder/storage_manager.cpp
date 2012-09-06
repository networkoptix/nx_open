#include <QDir>
#include "storage_manager.h"
#include "utils/common/util.h"
#include "core/resourcemanagment/resource_pool.h"
#include "core/resource/resource.h"
#include "server_stream_recorder.h"
#include "recording_manager.h"
#include "serverutil.h"
#include "plugins/storage/file_storage/file_storage_resource.h"

static const qint64 BALANCE_BY_FREE_SPACE_THRESHOLD = 1024*1024 * 500;

Q_GLOBAL_STATIC(QnStorageManager, inst)

// -------------------- QnStorageManager --------------------


QnStorageManager::QnStorageManager():
    m_mutex(QMutex::Recursive),
    m_storageFileReaded(false),
    m_storagesStatisticsReady(false)
{
}

void QnStorageManager::loadFullFileCatalog()
{
    loadFullFileCatalogInternal(QnResource::Role_LiveVideo);
    loadFullFileCatalogInternal(QnResource::Role_SecondaryLiveVideo);
}

void QnStorageManager::loadFullFileCatalogInternal(QnResource::ConnectionRole role)
{
#ifdef _TEST_TWO_SERVERS
    QDir dir(closeDirPath(getDataDirectory()) + QString("test/record_catalog/media/") + DeviceFileCatalog::prefixForRole(role));
#else
    QDir dir(closeDirPath(getDataDirectory()) + QString("record_catalog/media/") + DeviceFileCatalog::prefixForRole(role));
#endif
    QFileInfoList list = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    foreach(QFileInfo fi, list)
    {
        getFileCatalog(fi.fileName(), role);
    }
}

bool QnStorageManager::deserializeStorageFile()
{
#ifdef _TEST_TWO_SERVERS
    QFile storageFile(closeDirPath(getDataDirectory()) + QString("test/record_catalog/media/storage_index.csv"));
#else
    QFile storageFile(closeDirPath(getDataDirectory()) + QString("record_catalog/media/storage_index.csv"));
#endif
    if (!storageFile.exists())
        return true;
    if (!storageFile.open(QFile::ReadOnly))
        return false;
    // deserialize storage file
    QString line = storageFile.readLine(); // skip csv header
    do {
        line = storageFile.readLine();
        QStringList params = line.split(';');
        if (params.size() >= 2)
            m_storageIndexes.insert(params[0], params[1].toInt());
    } while (!line.isEmpty());
    storageFile.close();
    return true;
}

bool QnStorageManager::serializeStorageFile()
{
#ifdef _TEST_TWO_SERVERS
    QString baseName = closeDirPath(getDataDirectory()) + QString("test/record_catalog/media/storage_index.csv");
#else
    QString baseName = closeDirPath(getDataDirectory()) + QString("record_catalog/media/storage_index.csv");
#endif
    QFile storageFile(baseName + ".new");
    if (!storageFile.open(QFile::WriteOnly | QFile::Truncate))
        return false;
    storageFile.write("path; index\n");
    for (QMap<QString, int>::const_iterator itr = m_storageIndexes.begin(); itr != m_storageIndexes.end(); ++itr)
    {
        storageFile.write(itr.key().toUtf8());
        storageFile.write(";");
        storageFile.write(QByteArray::number(itr.value()));
        storageFile.write("\n");
    }
    storageFile.close();
    if (QFile::exists(baseName))
        QFile::remove(baseName);
    return storageFile.rename(baseName);
}

// determine storage index (aka 16 bit hash)
int QnStorageManager::detectStorageIndex(const QString& path)
{
    if (!m_storageFileReaded)
        m_storageFileReaded = deserializeStorageFile();

    if (m_storageIndexes.contains(path))
    {
        return m_storageIndexes.value(path);
    }
    else {
        int index = -1;
        foreach (int value, m_storageIndexes.values())
            index = qMax(index, value);
        index++;
        m_storageIndexes.insert(path, index);
        serializeStorageFile();
        return index;
    }
}

void QnStorageManager::addStorage(QnStorageResourcePtr storage)
{
    storage->setIndex(detectStorageIndex(storage->getUrl()));
    QMutexLocker lock(&m_mutex);
    m_storagesStatisticsReady = false;
    
    cl_log.log(QString(QLatin1String("Adding storage. Path: %1. SpaceLimit: %2MiB. Currently avaiable: %3MiB")).arg(storage->getUrl()).arg(storage->getSpaceLimit() / 1024 / 1024).arg(storage->getFreeSpace() / 1024 / 1024), cl_logINFO);

    QnStorageResourcePtr oldStorage = removeStorage(storage); // remove existing storage record if exists
    if (oldStorage)
        storage->addWritedSpace(oldStorage->getWritedSpace());
    m_storageRoots.insert(storage->getIndex(), storage);
    if (storage->isStorageAvailable())
        storage->setStatus(QnResource::Online);
    connect(storage.data(), SIGNAL(archiveRangeChanged(qint64, qint64)), this, SLOT(at_archiveRangeChanged(qint64, qint64)), Qt::DirectConnection);
}

QnStorageResourcePtr QnStorageManager::removeStorage(QnStorageResourcePtr storage)
{
    QMutexLocker lock(&m_mutex);
    m_storagesStatisticsReady = false;

    // remove existing storage record if exists
    for (StorageMap::iterator itr = m_storageRoots.begin(); itr != m_storageRoots.end(); ++itr)
    {
        if (itr.value()->getId() == storage->getId()) {
            QnStorageResourcePtr oldStorage = itr.value();
            m_storageRoots.erase(itr);
            return oldStorage;
        }
    }
    return QnStorageResourcePtr();
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
    QMutexLocker lock(&m_mutex);
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

}

QnStorageManager* QnStorageManager::instance()
{
    return inst();
}

QString QnStorageManager::dateTimeStr(qint64 dateTimeMs)
{
    QString text;
    QTextStream str(&text);
    QDateTime fileDate = QDateTime::fromMSecsSinceEpoch(dateTimeMs);
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
            bool isActive = !camera->isDisabled() && (camera->getStatus() == QnResource::Online || camera->getStatus() == QnResource::Recording);
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

QnTimePeriodList QnStorageManager::getRecordedPeriods(QnResourceList resList, qint64 startTime, qint64 endTime, qint64 detailLevel)
{
    QVector<QnTimePeriodList> cameras;
    for (int i = 0; i < resList.size(); ++i)
    {
        QnNetworkResourcePtr camera = qSharedPointerDynamicCast<QnNetworkResource> (resList[i]);
        if (camera) {
            QString physicalId = camera->getPhysicalId();
            getTimePeriodInternal(cameras, camera, startTime, endTime, detailLevel, getFileCatalog(physicalId, QnResource::Role_LiveVideo));
            getTimePeriodInternal(cameras, camera, startTime, endTime, detailLevel, getFileCatalog(physicalId, QnResource::Role_SecondaryLiveVideo));
        }
    }

    return QnTimePeriod::mergeTimePeriods(cameras);
}

void QnStorageManager::clearSpace(QnStorageResourcePtr storage)
{
    if (storage->getSpaceLimit() == 0)
        return; // unlimited


    QString dir = storage->getUrl();

    if (!storage->isNeedControlFreeSpace())
        return;

    qint64 freeSpace = storage->getFreeSpace();
    while (freeSpace != -1 && freeSpace < storage->getSpaceLimit())
    {
        qint64 minTime = 0x7fffffffffffffffll;
        QString mac;
        DeviceFileCatalogPtr catalog;
        {
            QMutexLocker lock(&m_mutex);
            for (FileCatalogMap::Iterator itr = m_devFileCatalogHi.begin(); itr != m_devFileCatalogHi.end(); ++itr)
            {
                qint64 firstTime = itr.value()->firstTime();
                if (firstTime != AV_NOPTS_VALUE && firstTime < minTime)
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
            catalog->deleteFirstRecord();
            DeviceFileCatalogPtr catalogLowRes = getFileCatalog(mac, QnResource::Role_SecondaryLiveVideo);
            if (catalogLowRes != 0) 
            {
                qint64 minTime = catalog->minTime();
                if (minTime != AV_NOPTS_VALUE) {
                    int idx = catalogLowRes->findFileIndex(minTime, DeviceFileCatalog::OnRecordHole_NextChunk);
                    if (idx != -1)
                        catalogLowRes->deleteRecordsBefore(idx);
                }
                else {
                    catalogLowRes->clear();
                }
            }
        }
        else
            break; // nothing to delete
        freeSpace = storage->getFreeSpace();
    }
}

void QnStorageManager::at_archiveRangeChanged(qint64 newStartTimeMs, qint64 newEndTimeMs)
{
    Q_UNUSED(newEndTimeMs)
    QnStorageResource* storage = qobject_cast<QnStorageResource*> (sender());
    if (!storage)
        return;

    int storageIndex = detectStorageIndex(storage->getUrl());

    foreach(DeviceFileCatalogPtr catalogHi, m_devFileCatalogHi)
        catalogHi->deleteRecordsByStorage(storageIndex, newStartTimeMs);
    
    foreach(DeviceFileCatalogPtr catalogLow, m_devFileCatalogLow)
        catalogLow->deleteRecordsByStorage(storageIndex, newStartTimeMs);
}

/*
QnStorageResourcePtr QnStorageManager::getOptimalStorageRoot(QnAbstractMediaStreamDataProvider* provider)
{
    Q_UNUSED(provider)
    QMutexLocker lock(&m_mutex);
    QnStorageResourcePtr result;
    qint64 maxFreeSpace = 0;
    float minBitrate = (float)INT_MAX;

    // balance storages evenly by bitrate
    bool balanceByBitrate = true;
    if (rand()%100 < 10)
    {
        // sometimes preffer drive with maximum free space
        qint64 maxSpace = 0;
        qint64 minSpace = INT64_MAX;
        //for (int i = 0; i < m_storageRoots.size(); ++i)
        for (StorageMap::const_iterator itr = m_storageRoots.begin(); itr != m_storageRoots.end(); ++itr)
        {
            if (itr.value()->getStatus() == QnResource::Offline)
                continue; // do not use offline storages for writting
            if (!itr.value()->isNeedControlFreeSpace()) {
                maxSpace = minSpace = 0; // do not count free space, balance by bitrate
                break;
            }
            qint64 freeSpace = itr.value()->getFreeSpace();
            maxSpace = qMax(maxSpace, freeSpace);
            minSpace = qMin(minSpace, freeSpace);
        }

        // If free space difference is small, keep balanceByBitrate strategy
        balanceByBitrate = maxSpace - minSpace <= BALANCE_BY_FREE_SPACE_THRESHOLD; 
    }
    
    for (StorageMap::const_iterator itr = m_storageRoots.begin(); itr != m_storageRoots.end(); ++itr)
    {
        if (itr.value()->getStatus() == QnResource::Offline)
            continue; // do not use offline storages for writting

        if (balanceByBitrate) 
        {   // select storage with mimimum bitrate
            float bitrate = itr.value()->bitrate();
            if (bitrate < minBitrate)
            {
                minBitrate = bitrate;
                result = itr.value();
            }
        }
        else 
        {   // select storage with maximum free space
            qint64 freeSpace = itr.value()->getFreeSpace();
            if (freeSpace > maxFreeSpace)
            {
                maxFreeSpace = freeSpace;
                result = itr.value();
            }
        }
    }

    if (result)
        qWarning() << "Selected storage for new file: " << result->getUrl() << ". strategy:" << (balanceByBitrate ? " bitrate balance" : "free space balance");


    return result;
}
*/

void QnStorageManager::updateStorageStatistics()
{
    double totalSpace = 0;
    for (StorageMap::const_iterator itr = m_storageRoots.begin(); itr != m_storageRoots.end(); ++itr)
    {
        QnFileStorageResourcePtr fileStorage = qSharedPointerDynamicCast<QnFileStorageResource> (itr.value());
        if (!fileStorage || fileStorage->getStatus() == QnResource::Offline)
            continue; // do not use offline storages for writting

        qint64 storageSpace = fileStorage->getFreeSpace() + fileStorage->getWritedSpace();
        totalSpace += storageSpace;
    }

    for (StorageMap::const_iterator itr = m_storageRoots.begin(); itr != m_storageRoots.end(); ++itr)
    {
        QnFileStorageResourcePtr fileStorage = qSharedPointerDynamicCast<QnFileStorageResource> (itr.value());
        if (!fileStorage || fileStorage->getStatus() == QnResource::Offline)
            continue; // do not use offline storages for writting

        qint64 storageSpace = fileStorage->getFreeSpace() + fileStorage->getWritedSpace();
        // write to large HDD more often then small HDD
        fileStorage->setStorageBitrateCoeff(1.0 - storageSpace / totalSpace);
    }
}

QnStorageResourcePtr QnStorageManager::getOptimalStorageRoot(QnAbstractMediaStreamDataProvider* provider)
{
    QMutexLocker lock(&m_mutex);
    QnStorageResourcePtr result;
    float minBitrate = INT_MAX;

    if (!m_storagesStatisticsReady) {
        updateStorageStatistics();
        m_storagesStatisticsReady = true;
    }

    QVector<QPair<float, QnStorageResourcePtr> > bitrateInfo;
    QVector<QnStorageResourcePtr> candidates;

    // Got storages with minimal bitrate value. Accept storages with minBitrate +10%

    for (StorageMap::const_iterator itr = m_storageRoots.begin(); itr != m_storageRoots.end(); ++itr)
    {
        if (itr.value()->getStatus() != QnResource::Offline) {
            float bitrate = itr.value()->bitrate();
            qDebug() << "QnFileStorageResource " << itr.value()->getUrl() << "current bitrate=" << itr.value()->bitrate();
            minBitrate = qMin(minBitrate, bitrate);
            bitrateInfo << QPair<float, QnStorageResourcePtr>(bitrate, itr.value());
        }
    }
    for (int i = 0; i < bitrateInfo.size(); ++i)
    {
        if (bitrateInfo[i].first <= minBitrate*1.1f)
            candidates << bitrateInfo[i].second;
    }

    // select storage with maximum free space
    qint64 maxFreeSpace = -INT_MAX;
    for (int i = 0; i < candidates.size(); ++i)
    {   
        qint64 freeSpace = candidates[i]->getFreeSpace();
        if (freeSpace > maxFreeSpace)
        {
            maxFreeSpace = freeSpace;
            result = candidates[i];
        }
    }

	if (result)
		qDebug() << "QnFileStorageResource. selectedStorage= " << result->getUrl() << "for provider" << provider->getResource()->getUrl();
	else
		qDebug() << "No storage available for recording";

    return result;
}

QString QnStorageManager::getFileName(const qint64& dateTime, const QnNetworkResourcePtr camera, const QString& prefix, QnStorageResourcePtr& storage)
{
    if (!storage) {
        qWarning() << "No disk storages";
        return QString();
    }
    Q_ASSERT(camera != 0);
    QString base = closeDirPath(storage->getUrl());

    if (!prefix.isEmpty())
        base += prefix + "/";

    QString text = base + camera->getPhysicalId();
    Q_ASSERT(!camera->getPhysicalId().isEmpty());
    text += QString("/") + dateTimeStr(dateTime);
    QList<QFileInfo> list = storage->getFileList(text);
    QList<QString> baseNameList;
    foreach(const QFileInfo& info, list)
        baseNameList << info.baseName();
    qSort(baseNameList.begin(), baseNameList.end());
    int fileNum = 0;
    if (!baseNameList.isEmpty()) 
        fileNum = baseNameList.last().toInt() + 1;
    clearSpace(storage);
    return text + strPadLeft(QString::number(fileNum), 3, '0');
}

DeviceFileCatalogPtr QnStorageManager::getFileCatalog(const QString& mac, const QString& qualityPrefix)
{
    return getFileCatalog(mac, DeviceFileCatalog::roleForPrefix(qualityPrefix));
}

DeviceFileCatalogPtr QnStorageManager::getFileCatalog(const QString& mac, QnResource::ConnectionRole role)
{
    QMutexLocker lock(&m_mutex);
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
    storageIndex = -1;
    for(StorageMap::const_iterator itr = m_storageRoots.begin(); itr != m_storageRoots.end(); ++itr)
    {
        QString root = closeDirPath(itr.value()->getUrl());
        if (fileName.startsWith(root))
        {
            int qualityLen = fileName.indexOf('/', root.length()+1) - root.length();
            quality = fileName.mid(root.length(), qualityLen);
            quality = QFileInfo(quality).baseName();
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
    for(StorageMap::const_iterator itr = m_storageRoots.begin(); itr != m_storageRoots.end(); ++itr)
    {
        QString root = itr.value()->getUrl();
        if (fileName.startsWith(root))
            return itr.value();
    }
    return QnStorageResourcePtr();
}

bool QnStorageManager::fileFinished(int durationMs, const QString& fileName, QnAbstractMediaStreamDataProvider* provider, qint64 fileSize)
{
    QMutexLocker lock(&m_mutex);
    int storageIndex;
    QString quality, mac;
    QnStorageResourcePtr storage = extractStorageFromFileName(storageIndex, fileName, mac, quality);
    if (storageIndex == -1)
        return false;
    storage->releaseBitrate(provider);
    storage->addWritedSpace(fileSize);
    DeviceFileCatalogPtr catalog = getFileCatalog(mac, quality);
    if (catalog == 0)
        return false;
    catalog->updateDuration(durationMs);
    return true;
}

bool QnStorageManager::fileStarted(const qint64& startDateMs, const QString& fileName, QnAbstractMediaStreamDataProvider* provider)
{
    QMutexLocker lock(&m_mutex);
    int storageIndex;
    QString quality, mac;

    QnStorageResourcePtr storage = extractStorageFromFileName(storageIndex, fileName, mac, quality);
    if (storageIndex == -1)
        return false;
    storage->addBitrate(provider);

    DeviceFileCatalogPtr catalog = getFileCatalog(mac, quality);
    if (catalog == 0)
        return false;
    catalog->addRecord(DeviceFileCatalog::Chunk(startDateMs, storageIndex, QFileInfo(fileName).baseName().toInt(), -1));
    return true;
}
