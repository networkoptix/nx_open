#include <QDir>
#include "storage_manager.h"
#include "utils/common/util.h"
#include "core/resource_managment/resource_pool.h"
#include "core/resource/resource.h"
#include "server_stream_recorder.h"
#include "recording_manager.h"
#include "serverutil.h"
#include "plugins/storage/file_storage/file_storage_resource.h"

static const qint64 BALANCE_BY_FREE_SPACE_THRESHOLD = 1024*1024 * 500;

Q_GLOBAL_STATIC(QnStorageManager, inst)

// -------------------- QnStorageManager --------------------


QnStorageManager::QnStorageManager():
    m_mutexStorages(QMutex::Recursive),
    m_mutexCatalog(QMutex::Recursive),
    m_storageFileReaded(false),
    m_storagesStatisticsReady(false),
    m_catalogLoaded(false)
{
}

void QnStorageManager::loadFullFileCatalog()
{
    loadFullFileCatalogInternal(QnResource::Role_LiveVideo);
    loadFullFileCatalogInternal(QnResource::Role_SecondaryLiveVideo);
    m_catalogLoaded = true;
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

QString QnStorageManager::toCanonicalPath(const QString& path)
{
    QString result = path;
    result.replace(L'\\', L'/');
    if (!result.endsWith(L'/'))
        result += QLatin1String("/");
    return result;
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
        if (params.size() >= 2) {
            QString path = toCanonicalPath(params[0]);
            for (int i = 1; i < params.size(); ++i) {
                int index = params[i].toInt();
                m_storageIndexes[path].insert(index);
            }
        }
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
    for (QMap<QString, QSet<int> >::const_iterator itr = m_storageIndexes.constBegin(); itr != m_storageIndexes.constEnd(); ++itr)
    {
        storageFile.write(itr.key().toUtf8());
        const QSet<int>& values = itr.value();
        foreach(const int& value, values) {
            storageFile.write(";");
            storageFile.write(QByteArray::number(value));
        }
        storageFile.write("\n");
    }
    storageFile.close();
    if (QFile::exists(baseName))
        QFile::remove(baseName);
    return storageFile.rename(baseName);
}

// determine storage index (aka 16 bit hash)
int QnStorageManager::detectStorageIndex(const QString& p)
{
    QString path = toCanonicalPath(p);
    if (!m_storageFileReaded)
        m_storageFileReaded = deserializeStorageFile();

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
        serializeStorageFile();
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

}

QnStorageManager* QnStorageManager::instance()
{
    return inst();
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

void QnStorageManager::clearSpace()
{
    if (!m_catalogLoaded)
        return;
    const StorageMap storages = getAllStorages();
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
    return m_storageRoots.values().toSet().toList(); // TODO: #Elric totally evil. Store storage list as a member.
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

    while (toDelete > 0)
    {
        qint64 minTime = 0x7fffffffffffffffll;
        QString mac;
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
            qint64 fileSize = catalog->deleteFirstRecord();
            if (fileSize > 0)
                toDelete -= fileSize;
            DeviceFileCatalogPtr catalogLowRes = getFileCatalog(mac, QnResource::Role_SecondaryLiveVideo);
            if (catalogLowRes != 0) 
            {
                qint64 minTime = catalog->minTime();
                if (minTime != (qint64)AV_NOPTS_VALUE) {
                    int idx = catalogLowRes->findFileIndex(minTime, DeviceFileCatalog::OnRecordHole_NextChunk);
                    if (idx != -1)
                        toDelete -= catalogLowRes->deleteRecordsBefore(idx);
                }
                else {
                    catalogLowRes->clear();
                }
            }
            if (fileSize == -1)
                break; // nothing to delete
        }
        else
            break; // nothing to delete
    }
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

void QnStorageManager::updateStorageStatistics()
{
    double totalSpace = 0;
    for (StorageMap::const_iterator itr = m_storageRoots.constBegin(); itr != m_storageRoots.constEnd(); ++itr)
    {
        QnFileStorageResourcePtr fileStorage = qSharedPointerDynamicCast<QnFileStorageResource> (itr.value());
        if (!fileStorage || fileStorage->getStatus() == QnResource::Offline)
            continue; // do not use offline storages for writting

        //qint64 storageSpace = fileStorage->getFreeSpace() - fileStorage->getSpaceLimit() + fileStorage->getWritedSpace();
        qint64 storageSpace = fileStorage->getSpaceLimit();
        totalSpace += storageSpace;
    }

    for (StorageMap::const_iterator itr = m_storageRoots.constBegin(); itr != m_storageRoots.constEnd(); ++itr)
    {
        QnFileStorageResourcePtr fileStorage = qSharedPointerDynamicCast<QnFileStorageResource> (itr.value());
        if (!fileStorage || fileStorage->getStatus() == QnResource::Offline)
            continue; // do not use offline storages for writting

        //qint64 storageSpace = fileStorage->getFreeSpace() - fileStorage->getSpaceLimit() + fileStorage->getWritedSpace();
        qint64 storageSpace = fileStorage->getSpaceLimit();
        // write to large HDD more often then small HDD
        fileStorage->setStorageBitrateCoeff(1.0 - storageSpace / totalSpace);
    }
}

QnStorageResourcePtr QnStorageManager::getOptimalStorageRoot(QnAbstractMediaStreamDataProvider* provider)
{
    QnStorageResourcePtr result;
    float minBitrate = (float) INT_MAX;

    {
        QMutexLocker lock(&m_mutexStorages);
        if (!m_storagesStatisticsReady) {
            updateStorageStatistics();
            m_storagesStatisticsReady = true;
        }
    }

    QVector<QPair<float, QnStorageResourcePtr> > bitrateInfo;
    QVector<QnStorageResourcePtr> candidates;

    // Got storages with minimal bitrate value. Accept storages with minBitrate +10%
    const StorageMap storages = getAllStorages();
    for (StorageMap::const_iterator itr = storages.constBegin(); itr != storages.constEnd(); ++itr)
    {
		QnStorageResourcePtr storage = itr.value();
        if (storage->getStatus() != QnResource::Offline) {
            qDebug() << "QnFileStorageResource " << storage->getUrl() << "current bitrate=" << storage->bitrate();
            float bitrate = storage->bitrate() * storage->getStorageBitrateCoeff();
            minBitrate = qMin(minBitrate, bitrate);
            bitrateInfo << QPair<float, QnStorageResourcePtr>(bitrate, storage);
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
        qWarning() << "No disk storages";
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
            baseNameList << info.baseName();
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

DeviceFileCatalogPtr QnStorageManager::getFileCatalog(const QString& mac, const QString& qualityPrefix)
{
    return getFileCatalog(mac, DeviceFileCatalog::roleForPrefix(qualityPrefix));
}

DeviceFileCatalogPtr QnStorageManager::getFileCatalog(const QString& mac, QnResource::ConnectionRole role)
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

    DeviceFileCatalogPtr catalog = getFileCatalog(mac, quality);
    if (catalog == 0)
        return false;
    catalog->updateDuration(durationMs, fileSize);

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

    DeviceFileCatalogPtr catalog = getFileCatalog(mac, quality);
    if (catalog == 0)
        return false;
    catalog->addRecord(DeviceFileCatalog::Chunk(startDateMs, storageIndex, QFileInfo(fileName).baseName().toInt(), -1, (qint16) timeZone));
    return true;
}
