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
    QnStorageDb(int storageIndex);
    virtual ~QnStorageDb();

    bool open(const QString& fileName);

    bool deleteRecords(const QByteArray& mac, QnServer::ChunksCatalog catalog, qint64 startTimeMs = -1);
    void addRecord(const QByteArray& mac, QnServer::ChunksCatalog catalog, const DeviceFileCatalog::Chunk& chunk);
    void flushRecords();
    bool createDatabase();
    QVector<DeviceFileCatalogPtr> loadFullFileCatalog();

    void beforeDelete();
    void afterDelete();
    bool replaceChunks(const QByteArray& mac, QnServer::ChunksCatalog catalog, const QVector<DeviceFileCatalog::Chunk>& chunks);

    bool removeCameraBookmarks(const QByteArray &mac);
    bool addOrUpdateCameraBookmark(const QnCameraBookmark &bookmark, const QByteArray &mac);
    QnCameraBookmarkList getBookmarks(const QByteArray &mac);
private:
    bool addRecordInternal(const QByteArray& mac, QnServer::ChunksCatalog catalog, const DeviceFileCatalog::Chunk& chunk);

    QVector<DeviceFileCatalogPtr> loadChunksFileCatalog();
    QVector<DeviceFileCatalogPtr> loadBookmarksFileCatalog();
private:
    int m_storageIndex;
    QElapsedTimer m_lastTranTime;

    struct DelayedData 
    {
        DelayedData (const QByteArray& mac = QByteArray(), 
                     QnServer::ChunksCatalog catalog = QnServer::ChunksCatalogCount, 
                     const DeviceFileCatalog::Chunk& chunk = DeviceFileCatalog::Chunk()): 
        mac(mac), catalog(catalog), chunk(chunk) {}

        QByteArray mac;
        QnServer::ChunksCatalog catalog;
        DeviceFileCatalog::Chunk chunk;
    };

    QVector<DelayedData> m_delayedData;
};

typedef QSharedPointer<QnStorageDb> QnStorageDbPtr;

#endif // __STORAGE_DB_H_
