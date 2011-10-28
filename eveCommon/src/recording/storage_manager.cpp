#include "storage_manager.h"
#include "utils/common/util.h"
#include "core/resourcemanagment/resource_pool.h"


Q_GLOBAL_STATIC(QnStorageManager, inst)

// -------------------- QnStorageManager --------------------

QnStorageManager::QnStorageManager()
{
}

void QnStorageManager::addRecordRoot(int key, const QString& path)
{
    m_storageRoots.insert(key, path);
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

QString QnStorageManager::getFileName(const qint64& dateTime, const QString& uniqId)
{
    QString base = m_storageRoots.begin().value();
    
    QString text = base + uniqId + QString("/") + dateTimeStr(dateTime);
    QDir dir(text);
    QList<QFileInfo> list = dir.entryInfoList(QDir::Files, QDir::Name);
    int fileNum = 0;
    if (!list.isEmpty()) {
        fileNum = list.last().baseName().toInt() + 1;
    }
    return text + strPadLeft(QString::number(fileNum), 3, '0');
}

DeviceFileCatalogPtr QnStorageManager::getFileCatalog(const QnResourcePtr resource)
{
    DeviceFileCatalogPtr fileCatalog = m_devFileCatalog[resource];
    if (fileCatalog == 0)
    {
        fileCatalog = DeviceFileCatalogPtr(new DeviceFileCatalog(resource));
        m_devFileCatalog[resource] = fileCatalog;
    }
    return fileCatalog;
}

bool QnStorageManager::addFileInfo(const qint64& startDate, const qint64& endDate, const QString& fileName)
{
    QMutexLocker lock(&m_mutex);
    for(QMap<int, QString>::iterator itr = m_storageRoots.begin(); itr != m_storageRoots.end(); ++itr)
    {
        QString root = itr.value();
        if (fileName.startsWith(root)) 
        {
            const QString url = fileName.mid(root.length(), fileName.indexOf('/', root.length()+1) - root.length());
            QnResourcePtr resource = qnResPool->getResourceByUrl(url);
            DeviceFileCatalogPtr catalog = getFileCatalog(resource);
            if (catalog == 0)
                return false;
            QFileInfo fi(fileName);
            catalog->addRecord(DeviceFileCatalog::Chunk(startDate, endDate - startDate, itr.key(), fi.baseName().toInt()));
            break;
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
    QString root = qnStorage->storageRoots()[0]; // index stored at primary storage
    info.m_catalog = qnStorage->getFileCatalog(res);
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
