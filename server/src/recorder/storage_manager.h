#ifndef __STORAGE_MANAGER_H__
#define __STORAGE_MANAGER_H__

#include <QString>
#include <QMap>
#include <QFile>
#include <QMutex>
#include "device_file_catalog.h"
#include "core/resource/storage.h"

// This class used for extract chunk sequence from storage
class QnChunkSequence: public QObject
{
    Q_OBJECT
public:
    QnChunkSequence(const QnNetworkResourcePtr res, qint64 startTime);
    QnChunkSequence(const QnNetworkResourceList& resList, qint64 startTime);
    DeviceFileCatalog::Chunk findChunk(QnResourcePtr res, qint64 time, DeviceFileCatalog::FindMethod findMethod);
    DeviceFileCatalog::Chunk getNextChunk(QnResourcePtr res);
    //qint64 nextChunkStartTime(QnResourcePtr res);
    //DeviceFileCatalog::Chunk getPrevChunk(QnResourcePtr res);
private slots:
    void onFirstDataRemoved(int n);
private:
    void addResource(QnNetworkResourcePtr res);
private:
    struct CacheInfo
    {
        DeviceFileCatalogPtr m_catalog;
        int m_index; // last find index
        qint64 m_startTimeMs;
    };
    QMap<QnResourcePtr, CacheInfo> m_cache;
    qint64 m_startTime;
};

class QnStorageManager
{
public:
    QnStorageManager();
    virtual ~QnStorageManager();
    static QnStorageManager* instance();
    void addStorage(QnStoragePtr storage);


    QString getFileName(const qint64& fileDate, const QnNetworkResourcePtr netResource, const QString& prefix);
    bool fileStarted(const qint64& startDateMs, const QString& fileName);
    bool fileFinished(int durationMs, const QString& fileName);

    static QString dateTimeStr(qint64 dateTimeMs);
    QnStoragePtr storageRoot(int storage_index) const { return m_storageRoots.value(storage_index); }
    bool isStorageAvailable(int storage_index) const 
    {
        QnStoragePtr storage = storageRoot(storage_index);
        return storage && storage->getStatus() == QnResource::Online; 
    }
    DeviceFileCatalogPtr getFileCatalog(const QString& mac);
    QnTimePeriodList getRecordedPeriods(QnResourceList resList, qint64 startTime, qint64 endTime, qint64 detailLevel);
    void loadFullFileCatalog();
private:
    QnStoragePtr getOptimalStorageRoot();
    void clearSpace(QnStoragePtr storage);
    int detectStorageIndex(const QString& path);
    bool deserializeStorageFile();
    bool serializeStorageFile();
private:
    typedef QMap<int, QnStoragePtr> StorageMap;
    StorageMap m_storageRoots;
    typedef QMap<QString, DeviceFileCatalogPtr> FileCatalogMap;
    FileCatalogMap m_devFileCatalog;
    mutable QMutex m_mutex;

    QMap<QString, int> m_storageIndexes;
    bool m_storageFileReaded;
};

#define qnStorageMan QnStorageManager::instance()

#endif // __STORAGE_MANAGER_H__
