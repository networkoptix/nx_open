#ifndef __STORAGE_MANAGER_H__
#define __STORAGE_MANAGER_H__

#include <QString>
#include <QMap>
#include <QFile>
#include <QMutex>
#include "device_file_catalog.h"
#include "core/resource/qnstorage.h"

// This class used for extract chunk sequence from storage
class QnChunkSequence: public QObject
{
    Q_OBJECT
public:
    QnChunkSequence(const QnNetworkResourcePtr res, qint64 startTime);
    QnChunkSequence(const QnNetworkResourceList& resList, qint64 startTime);
    DeviceFileCatalog::Chunk findChunk(QnResourcePtr res, qint64 time, DeviceFileCatalog::FindMethod findMethod);
    DeviceFileCatalog::Chunk getNextChunk(QnResourcePtr res);
    qint64 nextChunkStartTime(QnResourcePtr res);
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
        qint64 m_startTime;
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


    QString getFileName(const qint64& fileDate, const QnNetworkResourcePtr netResource);
    bool fileStarted(const qint64& startDate, const QString& fileName);
    bool fileFinished(int duration, const QString& fileName);

    static QString dateTimeStr(qint64 dateTimeMks);
    QnStoragePtr storageRoot(int storage_index) const { return m_storageRoots.value(storage_index); }
    DeviceFileCatalogPtr getFileCatalog(const QString& mac);
    QnTimePeriodList getRecordedPeriods(QnResourceList resList, qint64 startTime, qint64 endTime, qint64 detailLevel);
    void loadFullFileCatalog();
private:
    QnStoragePtr getOptimalStorageRoot();
    void clearSpace(QnStoragePtr storage);
private:
    typedef QMap<int, QnStoragePtr> StorageMap;
    StorageMap m_storageRoots;
    typedef QMap<QString, DeviceFileCatalogPtr> FileCatalogMap;
    FileCatalogMap m_devFileCatalog;
    mutable QMutex m_mutex;
};

#define qnStorageMan QnStorageManager::instance()

#endif // __STORAGE_MANAGER_H__
