#include <QDir>
#include "storage_manager.h"
#include "utils/common/util.h"
#include "core/resourcemanagment/resource_pool.h"
#include "core/resource/resource.h"
#include "server_stream_recorder.h"
#include "recording_manager.h"

static const qint64 BALANCE_BY_FREE_SPACE_THRESHOLD = 1024*1024 * 500;

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
        freeSpace = getDiskFreeSpace(dir);
    }
}

QnStorageResourcePtr QnStorageManager::getOptimalStorageRoot(QnAbstractMediaStreamDataProvider* provider)
{
    QMutexLocker lock(&m_mutex);
    QnStorageResourcePtr result;
    qint64 maxFreeSpace = 0;
    float minBitrate = INT_MAX;

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
            qint64 freeSpace = getDiskFreeSpace(itr.value()->getUrl());
            maxSpace = qMax(maxSpace, freeSpace);
            minSpace = qMin(minSpace, freeSpace);
        }

        // If free space difference is small, keep balanceByBitrate strategy
        balanceByBitrate = maxSpace - minSpace <= BALANCE_BY_FREE_SPACE_THRESHOLD; 
    }
    
    if (balanceByBitrate)
    {
        // select storage with mimimum bitrate
        for (StorageMap::const_iterator itr = m_storageRoots.begin(); itr != m_storageRoots.end(); ++itr)
        {
            float bitrate = itr.value()->bitrate();
            if (bitrate < minBitrate)
            {
                minBitrate = bitrate;
                result = itr.value();
            }
        }
    }
    else 
    {
        // select storage with maximum free space
        for (StorageMap::const_iterator itr = m_storageRoots.begin(); itr != m_storageRoots.end(); ++itr)
        {
            qint64 freeSpace = getDiskFreeSpace(itr.value()->getUrl());
            if (freeSpace > maxFreeSpace)
            {
                maxFreeSpace = freeSpace;
                result = itr.value();
            }
        }
    }

    return result;
}

QString QnStorageManager::getFileName(const qint64& dateTime, const QnNetworkResourcePtr camera, const QString& prefix, QnAbstractMediaStreamDataProvider* provider)
{
    QnStorageResourcePtr storage = getOptimalStorageRoot(provider);
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

QnStorageResourcePtr QnStorageManager::extractStorageFromFileName(int& storageIndex, const QString& fileName, QString& mac, QString& quality)
{
    storageIndex = -1;
    for(StorageMap::iterator itr = m_storageRoots.begin(); itr != m_storageRoots.end(); ++itr)
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

bool QnStorageManager::fileFinished(int durationMs, const QString& fileName, QnAbstractMediaStreamDataProvider* provider)
{
    QMutexLocker lock(&m_mutex);
    int storageIndex;
    QString quality, mac;
    QnStorageResourcePtr storage = extractStorageFromFileName(storageIndex, fileName, mac, quality);
    if (storageIndex == -1)
        return false;
    storage->releaseBitrate(provider);
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
