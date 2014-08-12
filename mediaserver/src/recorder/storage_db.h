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
        DeleteRecordInfo(const QUuid &cameraId, QnServer::ChunksCatalog catalog, qint64 startTimeMs): cameraId(cameraId), catalog(catalog), startTimeMs(startTimeMs) {}

        QUuid cameraId;
        QnServer::ChunksCatalog catalog;
        qint64 startTimeMs;
    };

    QnStorageDb(int storageIndex);
    virtual ~QnStorageDb();

    bool open(const QString& fileName);

    bool deleteRecords(const QUuid &cameraId, QnServer::ChunksCatalog catalog, qint64 startTimeMs = -1);
    void addRecord(const QUuid &cameraId, QnServer::ChunksCatalog catalog, const DeviceFileCatalog::Chunk& chunk);
    void flushRecords();
    bool createDatabase();
    QVector<DeviceFileCatalogPtr> loadFullFileCatalog();

    void beforeDelete();
    void afterDelete();
    bool replaceChunks(const QUuid &cameraId, QnServer::ChunksCatalog catalog, const std::deque<DeviceFileCatalog::Chunk>& chunks);

    bool removeCameraBookmarks(const QUuid &cameraId);
    bool addOrUpdateCameraBookmark(const QnCameraBookmark &bookmark, const QUuid &cameraId);
    bool deleteCameraBookmark(const QnCameraBookmark &bookmark);
    bool getBookmarks(const QUuid &cameraId, const QnCameraBookmarkSearchFilter &filter, QnCameraBookmarkList &result);
private:
    bool initializeBookmarksFtsTable();

    bool deleteRecordsInternal(const DeleteRecordInfo& delRecord);
    bool addRecordInternal(const QUuid &cameraId, QnServer::ChunksCatalog catalog, const DeviceFileCatalog::Chunk& chunk);

    QVector<DeviceFileCatalogPtr> loadChunksFileCatalog();
    QVector<DeviceFileCatalogPtr> loadBookmarksFileCatalog();
private:
    int m_storageIndex;
    QElapsedTimer m_lastTranTime;

    struct DelayedData 
    {
        DelayedData (const QUuid &cameraId = QUuid(), 
                     QnServer::ChunksCatalog catalog = QnServer::ChunksCatalogCount, 
                     const DeviceFileCatalog::Chunk& chunk = DeviceFileCatalog::Chunk()): 
        cameraId(cameraId), catalog(catalog), chunk(chunk) {}

        QUuid cameraId;
        QnServer::ChunksCatalog catalog;
        DeviceFileCatalog::Chunk chunk;
    };

    mutable QMutex m_mutex;
    QVector<DelayedData> m_delayedData;
    QVector<DeleteRecordInfo> m_recordsToDelete;
    QMutex m_delMutex;
};

typedef QSharedPointer<QnStorageDb> QnStorageDbPtr;

#endif // __STORAGE_DB_H_
