#ifndef __STORAGE_DB_H_
#define __STORAGE_DB_H_

#include <memory>

#include <QElapsedTimer>


#include <server/server_globals.h>

#include "utils/db/db_helper.h"

#include "device_file_catalog.h"

class QnStorageDb: public QnDbHelper
{
public:

    struct DeleteRecordInfo
    {
        DeleteRecordInfo(): startTimeMs(-1) {}
        DeleteRecordInfo(const QString& cameraUniqueId, QnServer::ChunksCatalog catalog, qint64 startTimeMs): 
            cameraUniqueId(cameraUniqueId),
            catalog(catalog),
            startTimeMs(startTimeMs) 
        {}

        QString cameraUniqueId;
        QnServer::ChunksCatalog catalog;
        qint64 startTimeMs;
    };

    QnStorageDb(const QnStorageResourcePtr& storage, int storageIndex);
    virtual ~QnStorageDb();

    bool open(const QString& fileName);

    bool deleteRecords(const QString& cameraUniqueId, QnServer::ChunksCatalog catalog, qint64 startTimeMs = -1);
    /*!
        \return \a false if failed to save record to DB
    */
    bool addRecord(const QString& cameraUniqueId, QnServer::ChunksCatalog catalog, const DeviceFileCatalog::Chunk& chunk);
    /*!
        \return \a false if failed to save to DB
    */
    bool flushRecords();
    QVector<DeviceFileCatalogPtr> loadFullFileCatalog();

    void beforeDelete();
    void afterDelete();
    bool replaceChunks(const QString& cameraUniqueId, QnServer::ChunksCatalog catalog, const std::deque<DeviceFileCatalog::Chunk>& chunks);
private:
    bool createDatabase();

    virtual QnDbTransaction* getTransaction() override;
    bool flushRecordsNoLock();
    bool deleteRecordsInternal(const DeleteRecordInfo& delRecord);
    bool addRecordInternal(const QString& cameraUniqueId, QnServer::ChunksCatalog catalog, const DeviceFileCatalog::Chunk& chunk);

    QVector<DeviceFileCatalogPtr> loadChunksFileCatalog();

    void addCatalogFromMediaFolder(
        const QString&                  postfix, 
        QnServer::ChunksCatalog         catalog, 
        QVector<DeviceFileCatalogPtr>&  result
    );

private:
    QnStorageResourcePtr m_storage;
    int m_storageIndex;
    QElapsedTimer m_lastTranTime;

    struct DelayedData 
    {
        DelayedData (const QString& cameraUniqueId = QString(), 
                     QnServer::ChunksCatalog catalog = QnServer::ChunksCatalogCount, 
                     const DeviceFileCatalog::Chunk& chunk = DeviceFileCatalog::Chunk()): 
            cameraUniqueId(cameraUniqueId), 
            catalog(catalog),
            chunk(chunk) {}

        QString cameraUniqueId;
        QnServer::ChunksCatalog catalog;
        DeviceFileCatalog::Chunk chunk;
    };

    mutable QnMutex m_syncMutex;
    QVector<DelayedData> m_delayedData;
    QVector<DeleteRecordInfo> m_recordsToDelete;
    QnMutex m_delMutex;
    QnDbTransaction m_tran;
    bool m_needReopenDB;
};

typedef std::shared_ptr<QnStorageDb> QnStorageDbPtr;

#endif // __STORAGE_DB_H_
