#include "storage_manager.h"
#include "utils/common/util.h"
#include "core/resourcemanagment/resource_pool.h"
#include "core/resource/resource.h"


Q_GLOBAL_STATIC(QnStorageManager, inst)

// -------------------- QnStorageManager --------------------

QnStorageManager::QnStorageManager():
    m_mutex(QMutex::Recursive),
    m_storageFileReaded(false)
{
}

void QnStorageManager::loadFullFileCatalog()
{
    QDir dir(closeDirPath(getDataDirectory()) + QString("record_catalog"));
    QFileInfoList list = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    foreach(QFileInfo fi, list)
    {
        getFileCatalog(fi.fileName());
    }
}

bool QnStorageManager::deserializeStorageFile()
{
    QFile storageFile(closeDirPath(getDataDirectory()) + QString("record_catalog/storage_index.csv"));
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
    QString baseName = closeDirPath(getDataDirectory()) + QString("record_catalog/storage_index.csv");
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

void QnStorageManager::addStorage(QnStoragePtr storage)
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

QString QnStorageManager::dateTimeStr(qint64 dateTimeMks)
{
    QString text;
    QTextStream str(&text);
    QDateTime fileDate = QDateTime::fromMSecsSinceEpoch(dateTimeMks/1000);
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
            DeviceFileCatalogPtr catalog = getFileCatalog(camera->getMAC().toString());
            if (catalog) 
                cameras << catalog->getTimePeriods(startTime, endTime, detailLevel);
        }
    }

    return QnTimePeriod::mergeTimePeriods(cameras);
}

void QnStorageManager::clearSpace(QnStoragePtr storage)
{
    if (storage->getSpaceLimit() == 0)
        return; // unlimited


    QString dir = storage->getUrl();
    while (getDiskFreeSpace(dir) < storage->getSpaceLimit())
    {
        qint64 minTime = 0x7fffffffffffffffll;
        QString mac;
        DeviceFileCatalogPtr catalog;
        {
            QMutexLocker lock(&m_mutex);
            for (FileCatalogMap::Iterator itr = m_devFileCatalog.begin(); itr != m_devFileCatalog.end(); ++itr)
            {
                qint64 firstTime = itr.value()->firstTime();
                if (firstTime != AV_NOPTS_VALUE && firstTime < minTime)
                {
                    minTime = itr.value()->firstTime();
                    mac = itr.key();
                }
            }
            if (!mac.isEmpty())
                catalog = getFileCatalog(mac);
        }
        if (catalog != 0)
            catalog->deleteFirstRecord();
        else
            break; // nothing to delete
    }
}

QnStoragePtr QnStorageManager::getOptimalStorageRoot()
{
    QMutexLocker lock(&m_mutex);
    QnStoragePtr result;
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

QString QnStorageManager::getFileName(const qint64& dateTime, const QnNetworkResourcePtr camera)
{
    QnStoragePtr storage = getOptimalStorageRoot();
    Q_ASSERT(camera != 0);
    QString base = closeDirPath(storage->getUrl());
    
    QString text = base + camera->getMAC().toString() + QString("/") + dateTimeStr(dateTime);
    QDir dir(text);
    QList<QFileInfo> list = dir.entryInfoList(QDir::Files, QDir::Name);
    int fileNum = 0;
    if (!list.isEmpty()) {
        fileNum = list.last().baseName().toInt() + 1;
    }
    clearSpace(storage);
    return text + strPadLeft(QString::number(fileNum), 3, '0');
}

DeviceFileCatalogPtr QnStorageManager::getFileCatalog(const QString& mac)
{
    QMutexLocker lock(&m_mutex);
    DeviceFileCatalogPtr fileCatalog = m_devFileCatalog[mac];
    if (fileCatalog == 0)
    {
        fileCatalog = DeviceFileCatalogPtr(new DeviceFileCatalog(mac));
        m_devFileCatalog[mac] = fileCatalog;
    }
    return fileCatalog;
}

bool QnStorageManager::fileFinished(int duration, const QString& fileName)
{
    QMutexLocker lock(&m_mutex);
    for(QMap<int, QnStoragePtr>::iterator itr = m_storageRoots.begin(); itr != m_storageRoots.end(); ++itr)
    {
        QString root = closeDirPath(itr.value()->getUrl());
        if (fileName.startsWith(root))
        {
            QString mac = fileName.mid(root.length(), fileName.indexOf('/', root.length()+1) - root.length());
            mac = QFileInfo(mac).baseName();
            //QnNetworkResourcePtr camera = qnResPool->getNetResourceByMac(mac);
            //if (!camera) {
            //    return false;
            //}
            DeviceFileCatalogPtr catalog = getFileCatalog(mac);
            if (catalog == 0)
                return false;
            QFileInfo fi(fileName);
            catalog->updateDuration(duration);
            return true;
        }
    }
    return false;
}

bool QnStorageManager::fileStarted(const qint64& startDate, const QString& fileName)
{
    QMutexLocker lock(&m_mutex);
    for(QMap<int, QnStoragePtr>::iterator itr = m_storageRoots.begin(); itr != m_storageRoots.end(); ++itr)
    {
        QString root = closeDirPath(itr.value()->getUrl());
        if (fileName.startsWith(root))
        {
            QString mac = fileName.mid(root.length(), fileName.indexOf('/', root.length()+1) - root.length());
            mac = QFileInfo(mac).baseName();
            //QnNetworkResourcePtr camera = qnResPool->getNetResourceByMac(mac);
            //if (!camera)
            //    return false;
            DeviceFileCatalogPtr catalog = getFileCatalog(mac);
            if (catalog == 0)
                return false;
            QFileInfo fi(fileName);
            catalog->addRecord(DeviceFileCatalog::Chunk(startDate, itr.key(), fi.baseName().toInt(), -1));
            return true;
        }
    }
    return false;
}

// -------------------------------- QnChunkSequence ------------------------

QnChunkSequence::QnChunkSequence(const QnNetworkResourcePtr res, qint64 startTime)
{
    m_startTime = startTime;
    addResource(res);
}

QnChunkSequence::QnChunkSequence(const QnNetworkResourceList& resList, qint64 startTime)
{
    // find all media roots and all cameras 
    m_startTime = startTime;
    foreach(QnNetworkResourcePtr res, resList)
    {
        addResource(res);
    }
}

void QnChunkSequence::addResource(QnNetworkResourcePtr res)
{
    CacheInfo info;
    //QString root = qnStorageMan->storageRoots().begin().value()->getUrl(); // index stored at primary storage
    info.m_catalog = qnStorageMan->getFileCatalog(res->getMAC().toString());
    connect(info.m_catalog.data(), SIGNAL(firstDataRemoved(int)), this, SLOT(onFirstDataRemoved(int)));
    info.m_startTime = m_startTime;
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

/*
DeviceFileCatalog::Chunk QnChunkSequence::getPrevChunk(QnResourcePtr res)
{
    QMap<QnResourcePtr, CacheInfo>::iterator resourceItr = m_cache.find(res);
    if (resourceItr == m_cache.end())
        return DeviceFileCatalog::Chunk();

    CacheInfo& info = resourceItr.value();

    // we can reset index in any time if we are going to search from begin again
    if (info.m_index == -1) {
        info.m_index = info.m_catalog->findFileIndex(0);
    }

    if (info.m_index != -1 && info.m_index > 0) 
    {
        DeviceFileCatalog::Chunk chunk = info.m_catalog->chunkAt(info.m_index-1);
        info.m_startTime = chunk.startTime;
        info.m_index--;
        return chunk;
    }
    return DeviceFileCatalog::Chunk();
}
*/

DeviceFileCatalog::Chunk QnChunkSequence::findChunk(QnResourcePtr res, qint64 time, DeviceFileCatalog::FindMethod findMethod)
{
    QMap<QnResourcePtr, CacheInfo>::iterator resourceItr = m_cache.find(res);
    if (resourceItr == m_cache.end())
        return DeviceFileCatalog::Chunk();

    CacheInfo& info = resourceItr.value();

    if (time != -1) {
        info.m_index = -1;
        info.m_startTime = time;
    }
    // we can reset index in any time if we are going to search from begin again
    if (info.m_index == -1) {
        info.m_index = info.m_catalog->findFileIndex(info.m_startTime, findMethod);
    }

    if (info.m_index != -1) 
    {
        DeviceFileCatalog::Chunk chunk = info.m_catalog->chunkAt(info.m_index);
        if (chunk.startTime != -1) 
        {
            info.m_startTime = chunk.startTime;
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

qint64 QnChunkSequence::nextChunkStartTime(QnResourcePtr res)
{
    QMap<QnResourcePtr, CacheInfo>::iterator resourceItr = m_cache.find(res);
    if (resourceItr == m_cache.end())
        return -1;

    CacheInfo& info = resourceItr.value();
    return info.m_catalog->chunkAt(info.m_index+1).startTime;
}
