#ifndef __STORAGE_DB_H_
#define __STORAGE_DB_H_

#include <QElapsedTimer>

#include <core/resource/camera_bookmark_fwd.h>
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

    QnStorageDb(int storageIndex);
    virtual ~QnStorageDb();

    bool open(const QString& fileName);

    bool deleteRecords(const QString& cameraUniqueId, QnServer::ChunksCatalog catalog, qint64 startTimeMs = -1);
    void addRecord(const QString& cameraUniqueId, QnServer::ChunksCatalog catalog, const DeviceFileCatalog::Chunk& chunk);
    void flushRecords();
    bool createDatabase();
    QVector<DeviceFileCatalogPtr> loadFullFileCatalog();

    void beforeDelete();
    void afterDelete();
    bool replaceChunks(const QString& cameraUniqueId, QnServer::ChunksCatalog catalog, const std::deque<DeviceFileCatalog::Chunk>& chunks);

    bool removeCameraBookmarks(const QString& cameraUniqueId);
    bool addOrUpdateCameraBookmark(const QnCameraBookmark &bookmark, const QString& cameraUniqueId);
    bool deleteCameraBookmark(const QnCameraBookmark &bookmark);
    bool getBookmarks(const QString& cameraUniqueId, const QnCameraBookmarkSearchFilter &filter, QnCameraBookmarkList &result);
private:
    virtual QnDbTransaction* getTransaction() override;
    void flushRecordsNoLock();
    bool deleteRecordsInternal(const DeleteRecordInfo& delRecord);
    bool addRecordInternal(const QString& cameraUniqueId, QnServer::ChunksCatalog catalog, const DeviceFileCatalog::Chunk& chunk);

    QVector<DeviceFileCatalogPtr> loadChunksFileCatalog();
    QVector<DeviceFileCatalogPtr> loadBookmarksFileCatalog();

    void addCatalogFromMediaFolder(const QString& postfix, QnServer::ChunksCatalog catalog, QVector<DeviceFileCatalogPtr>& result);
private:
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

    mutable QMutex m_syncMutex;
    QVector<DelayedData> m_delayedData;
    QVector<DeleteRecordInfo> m_recordsToDelete;
    QMutex m_delMutex;
    QnDbTransaction m_tran;
};

typedef QSharedPointer<QnStorageDb> QnStorageDbPtr;

#endif // __STORAGE_DB_H_
