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
public:
    QnChunkSequence(const QnResourcePtr res, qint64 startTime);
    QnChunkSequence(const QnResourceList& resList, qint64 startTime);
    DeviceFileCatalog::Chunk getNextChunk(QnResourcePtr res, qint64 time = -1);
    qint64 nextChunkStartTime(QnResourcePtr res);
    DeviceFileCatalog::Chunk getPrevChunk(QnResourcePtr res);
private slots:
    void onFirstDataRemoved(int n);
private:
    void addResource(QnResourcePtr res);
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


    QString getFileName(const qint64& fileDate, const QString& uniqId);
    bool addFileInfo(const qint64& startDate, const qint64& endDate, const QString& fileName);

    QList<ChunkMap::iterator> findFirstChunks(const QnResourceList& resList, qint64 startTime, qint64 endTime);

    static QString dateTimeStr(qint64 dateTimeMks);
    QMap<int, QnStoragePtr> storageRoots() const { return m_storageRoots; }
    DeviceFileCatalogPtr getFileCatalog(const QnResourcePtr resource);
private:
    QMap<int, QnStoragePtr> m_storageRoots;
    typedef QMap<QnResourcePtr, DeviceFileCatalogPtr> FileCatalogMap;
    FileCatalogMap m_devFileCatalog;
    QMutex m_mutex;
};

#define qnStorageMan QnStorageManager::instance()

#endif // __STORAGE_MANAGER_H__
