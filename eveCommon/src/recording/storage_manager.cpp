#include "storage_manager.h"
#include "utils/common/util.h"
#include "core/resourcemanagment/resource_pool.h"


Q_GLOBAL_STATIC(QnStorageManager, inst)

// -------------------- QnStorageManager --------------------

QnStorageManager::QnStorageManager():
    m_mutex(QMutex::Recursive)
{
}

void QnStorageManager::addStorage(QnStoragePtr storage)
{
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

void QnStorageManager::clearSpace(QnStoragePtr storage)
{
    if (storage->getSpaceLimit() == 0)
        return; // unlimited


    QString dir = storage->getUrl();

    while (getDiskFreeSpace(dir) < storage->getSpaceLimit())
    {
        qint64 minTime = 0x7fffffffffffffffll;
        QnResourcePtr camera;
        DeviceFileCatalogPtr catalog;
        {
            QMutexLocker lock(&m_mutex);
            for (FileCatalogMap::Iterator itr = m_devFileCatalog.begin(); itr != m_devFileCatalog.end(); ++itr)
            {
                qint64 firstTime = itr.value()->firstTime();
                if (firstTime != AV_NOPTS_VALUE && firstTime < minTime)
                {
                    minTime = itr.value()->firstTime();
                    camera = itr.key();
                }
            }
            if (camera != 0)
                catalog = getFileCatalog(camera);
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

QString QnStorageManager::getFileName(const qint64& dateTime, const QString& uniqId)
{
    QnStoragePtr storage = getOptimalStorageRoot();
    QnResourcePtr camera = qnResPool->getResourceByUrl(uniqId);
    Q_ASSERT(camera != 0);
    QString base = closeDirPath(storage->getUrl());
    
    QString text = base + uniqId + QString("/") + dateTimeStr(dateTime);
    QDir dir(text);
    QList<QFileInfo> list = dir.entryInfoList(QDir::Files, QDir::Name);
    int fileNum = 0;
    if (!list.isEmpty()) {
        fileNum = list.last().baseName().toInt() + 1;
    }
    clearSpace(storage);
    return text + strPadLeft(QString::number(fileNum), 3, '0');
}

DeviceFileCatalogPtr QnStorageManager::getFileCatalog(const QnResourcePtr camera)
{
    QMutexLocker lock(&m_mutex);
    DeviceFileCatalogPtr fileCatalog = m_devFileCatalog[camera];
    if (fileCatalog == 0)
    {
        fileCatalog = DeviceFileCatalogPtr(new DeviceFileCatalog(camera));
        m_devFileCatalog[camera] = fileCatalog;
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
            QString url = fileName.mid(root.length(), fileName.indexOf('/', root.length()+1) - root.length());
            url = QFileInfo(url).baseName();
            QnResourcePtr camera = qnResPool->getResourceByUniqId(url);
            DeviceFileCatalogPtr catalog = getFileCatalog(camera);
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
            QString url = fileName.mid(root.length(), fileName.indexOf('/', root.length()+1) - root.length());
            url = QFileInfo(url).baseName();
            QnResourcePtr camera = qnResPool->getResourceByUniqId(url);
            DeviceFileCatalogPtr catalog = getFileCatalog(camera);
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

QnChunkSequence::QnChunkSequence(const QnResourcePtr res, qint64 startTime)
{
    m_startTime = startTime;
    addResource(res);
}

QnChunkSequence::QnChunkSequence(const QnResourceList& resList, qint64 startTime)
{
    // find all media roots and all cameras 
    m_startTime = startTime;
    foreach(QnResourcePtr res, resList)
    {
        addResource(res);
    }
}

void QnChunkSequence::addResource(QnResourcePtr res)
{
    CacheInfo info;
    QString root = qnStorageMan->storageRoots().begin().value()->getUrl(); // index stored at primary storage
    info.m_catalog = qnStorageMan->getFileCatalog(res);
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

DeviceFileCatalog::Chunk QnChunkSequence::getNextChunk(QnResourcePtr res, qint64 time)
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
        info.m_index = info.m_catalog->findFileIndex(info.m_startTime);
    }

    if (info.m_index != -1) 
    {
        DeviceFileCatalog::Chunk chunk = info.m_catalog->chunkAt(info.m_index);
        info.m_startTime = chunk.startTime;
        info.m_index++;
        return chunk;
    }
    return DeviceFileCatalog::Chunk();
}

qint64 QnChunkSequence::nextChunkStartTime(QnResourcePtr res)
{
    QMap<QnResourcePtr, CacheInfo>::iterator resourceItr = m_cache.find(res);
    if (resourceItr == m_cache.end())
        return -1;

    CacheInfo& info = resourceItr.value();
    return info.m_catalog->chunkAt(info.m_index+1).startTime;
}
