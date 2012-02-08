#include <QDir>
#include "storage_manager.h"
#include "utils/common/util.h"
#include "core/resourcemanagment/resource_pool.h"
#include "core/resource/resource.h"
#include "server_stream_recorder.h"
#include "recording_manager.h"


Q_GLOBAL_STATIC(QnStorageManager, inst)

// -------------------- QnStorageManager --------------------


QnStorageManager::QnStorageManager():
    m_mutex(QMutex::Recursive),
    m_storageFileReaded(false)
{
}

void QnStorageManager::loadFullFileCatalog()
{
    loadFullFileCatalogInternal(QnResource::Role_LiveVideo);
    loadFullFileCatalogInternal(QnResource::Role_SecondaryLiveVideo);
}

void QnStorageManager::loadFullFileCatalogInternal(QnResource::ConnectionRole role)
{
    QDir dir(closeDirPath(getDataDirectory()) + QString("record_catalog/media/") + DeviceFileCatalog::prefixForRole(role));
    QFileInfoList list = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    foreach(QFileInfo fi, list)
    {
        getFileCatalog(fi.fileName(), role);
    }
}

bool QnStorageManager::deserializeStorageFile()
{
    QFile storageFile(closeDirPath(getDataDirectory()) + QString("record_catalog/media/storage_index.csv"));
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
    QString baseName = closeDirPath(getDataDirectory()) + QString("record_catalog/media/storage_index.csv");
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
    m_storageRoots.insert(storage->getIndex(), storage);
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

QnTimePeriodList QnStorageManager::getRecordedPeriods(QnResourceList resList, qint64 startTime, qint64 endTime, qint64 detailLevel)
{
    QMutexLocker lock(&m_mutex);
    QVector<QnTimePeriodList> cameras;
    for (int i = 0; i < resList.size(); ++i)
    {
        QnNetworkResourcePtr camera = qSharedPointerDynamicCast<QnNetworkResource> (resList[i]);
        if (camera) 
        {
            DeviceFileCatalogPtr catalog = getFileCatalog(camera->getMAC().toString(), QnResource::Role_LiveVideo);
            if (catalog) {
                cameras << catalog->getTimePeriods(startTime, endTime, detailLevel);
                if (!cameras.last().isEmpty())
                {
                    QnTimePeriod& lastPeriod = cameras.last().last();
                    if (lastPeriod.durationMs == -1 && camera->getStatus() != QnResource::Online)
                    {
                        lastPeriod.durationMs = 0;
                        Recorders recorders = QnRecordingManager::instance()->findRecorders(camera);
                        if(recorders.recorderHiRes)
                            lastPeriod.durationMs = recorders.recorderHiRes->duration()/1000;
                    }
                }
            }
        }
    }

    return QnTimePeriod::mergeTimePeriods(cameras);
}

void QnStorageManager::clearSpace(QnStorageResourcePtr storage)
{
    if (storage->getSpaceLimit() == 0)
        return; // unlimited


    QString dir = storage->getUrl();
    qint64 freeSpace = getDiskFreeSpace(dir);
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
            if (catalogLowRes != 0) {
                int idx = catalogLowRes->findFileIndex(catalog->minTime(), DeviceFileCatalog::OnRecordHole_NextChunk);
                if (idx != -1)
                    catalogLowRes->deleteRecordsBefore(idx);
            }
        }
        else
            break; // nothing to delete
        freeSpace = getDiskFreeSpace(dir);
    }
}

QnStorageResourcePtr QnStorageManager::getOptimalStorageRoot()
{
    QMutexLocker lock(&m_mutex);
    QnStorageResourcePtr result;
    qint64 minFreeSpace = 0x7fffffffffffffffll;
    for (StorageMap::const_iterator itr = m_storageRoots.begin(); itr != m_storageRoots.end(); ++itr)
    {
        qint64 freeSpace = getDiskFreeSpace(itr.value()->getUrl());
        if (freeSpace < minFreeSpace)
        {
            minFreeSpace = freeSpace;
            result = itr.value();
        }
    }
    return result;
}

QString QnStorageManager::getFileName(const qint64& dateTime, const QnNetworkResourcePtr camera, const QString& prefix)
{
    QnStorageResourcePtr storage = getOptimalStorageRoot();
    Q_ASSERT(camera != 0);
    QString base = closeDirPath(storage->getUrl());

    if (!prefix.isEmpty())
        base += prefix + "/";

    QString text = base + camera->getMAC().toString();
    Q_ASSERT(!camera->getMAC().toString().isEmpty());
    text += QString("/") + dateTimeStr(dateTime);
    QDir dir(text);
    QList<QFileInfo> list = dir.entryInfoList(QDir::Files, QDir::Name);
    int fileNum = 0;
    if (!list.isEmpty()) {
        fileNum = list.last().baseName().toInt() + 1;
    }
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

bool QnStorageManager::fileFinished(int durationMs, const QString& fileName)
{
    QMutexLocker lock(&m_mutex);
    int storageIndex;
    QString quality, mac;
    extractFromFileName(storageIndex, fileName, mac, quality);
    if (storageIndex == -1)
        return false;
    DeviceFileCatalogPtr catalog = getFileCatalog(mac, quality);
    if (catalog == 0)
        return false;
    catalog->updateDuration(durationMs);
    return true;
}

void QnStorageManager::extractFromFileName(int& storageIndex, const QString& fileName, QString& mac, QString& quality)
{
    storageIndex = -1;
    for(QMap<int, QnStorageResourcePtr>::iterator itr = m_storageRoots.begin(); itr != m_storageRoots.end(); ++itr)
    {
        QString root = closeDirPath(itr.value()->getUrl());
        if (fileName.startsWith(root))
        {
            int qualityLen = fileName.indexOf('/', root.length()+1) - root.length();
            quality = fileName.mid(root.length(), qualityLen);
            quality = QFileInfo(quality).baseName();
            int macPos = root.length() + qualityLen;
            mac = fileName.mid(macPos+1, fileName.indexOf('/', macPos+1) - macPos-1);
            storageIndex = itr.key();
            break;
        }
    }
}

bool QnStorageManager::fileStarted(const qint64& startDateMs, const QString& fileName)
{
    QMutexLocker lock(&m_mutex);
    int storageIndex;
    QString quality, mac;
    extractFromFileName(storageIndex, fileName, mac, quality);
    if (storageIndex == -1)
        return false;
    DeviceFileCatalogPtr catalog = getFileCatalog(mac, quality);
    if (catalog == 0)
        return false;
    catalog->addRecord(DeviceFileCatalog::Chunk(startDateMs, storageIndex, QFileInfo(fileName).baseName().toInt(), -1));
    return true;
}

// -------------------------------- QnChunkSequence ------------------------

QnChunkSequence::QnChunkSequence(const QnNetworkResourcePtr res,  QnResource::ConnectionRole role, qint64 startTime)
{
    m_startTime = startTime;
    m_role = role;
    addResource(res);
}

QnChunkSequence::QnChunkSequence(const QnNetworkResourceList& resList,  QnResource::ConnectionRole role, qint64 startTime)
{
    // find all media roots and all cameras 
    m_startTime = startTime;
    m_role = role;
    foreach(QnNetworkResourcePtr res, resList)
    {
        addResource(res);
    }
}

void QnChunkSequence::addResource(QnNetworkResourcePtr res)
{
    CacheInfo info;
    //QString root = qnStorageMan->storageRoots().begin().value()->getUrl(); // index stored at primary storage
    info.m_catalog = qnStorageMan->getFileCatalog(res->getMAC().toString(), m_role);
    connect(info.m_catalog.data(), SIGNAL(firstDataRemoved(int)), this, SLOT(onFirstDataRemoved(int)));
    info.m_startTimeMs = m_startTime;
    info.m_index = -1;
    m_cache.insert(res, info);
}


void QnChunkSequence::onFirstDataRemoved(int n)
{
    foreach(CacheInfo info, m_cache.values())
    {
        if (info.m_catalog == sender())
        {
            info.m_index -= n;
            info.m_index = qMax(0, info.m_index);
            break;
        }
    }
}

DeviceFileCatalog::Chunk QnChunkSequence::findChunk(QnResourcePtr res, qint64 timeMs, DeviceFileCatalog::FindMethod findMethod)
{
    QMap<QnResourcePtr, CacheInfo>::iterator resourceItr = m_cache.find(res);
    if (resourceItr == m_cache.end())
        return DeviceFileCatalog::Chunk();

    CacheInfo& info = resourceItr.value();

    if (timeMs != -1) {
        info.m_index = -1;
        info.m_startTimeMs = timeMs;
    }
    // we can reset index in any time if we are going to search from begin again
    if (info.m_index == -1) {
        info.m_index = info.m_catalog->findFileIndex(info.m_startTimeMs, findMethod);
    }

    if (info.m_index != -1) 
    {
        DeviceFileCatalog::Chunk chunk = info.m_catalog->chunkAt(info.m_index);
        if (chunk.startTimeMs != -1) 
        {
            info.m_startTimeMs = chunk.startTimeMs;
            info.m_index++;
        }
        else {
            // todo: switch to live here
            chunk = chunk;
        }
        return chunk;
    }
    return DeviceFileCatalog::Chunk();
}

DeviceFileCatalog::Chunk QnChunkSequence::getNextChunk(QnResourcePtr res)
{
    return findChunk(res, -1, DeviceFileCatalog::OnRecordHole_NextChunk);
}
