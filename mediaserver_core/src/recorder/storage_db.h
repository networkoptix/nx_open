#ifndef __STORAGE_DB_H_
#define __STORAGE_DB_H_

#include <memory>
#include <deque>
#include <vector>
#include <map>
#include <boost/bimap.hpp>

#include <QElapsedTimer>

#include <server/server_globals.h>
#include "utils/media_db/media_db.h"

#include "device_file_catalog.h"

class QnStorageDb: public nx::media_db::DbHelperHandler 
{
    typedef boost::bimap<QString, uint16_t> UuidToHash;

public:
    struct DeleteRecordInfo
    {
        DeleteRecordInfo(): startTimeMs(-1) {}
        DeleteRecordInfo(const QString& cameraUniqueId,
                         QnServer::ChunksCatalog catalog, 
                         qint64 startTimeMs): 
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

    bool deleteRecords(const QString& cameraUniqueId,
                       QnServer::ChunksCatalog catalog, 
                       qint64 startTimeMs = -1);
    /*!
        \return \a false if failed to save record to DB
    */
    bool addRecord(const QString& cameraUniqueId,
                   QnServer::ChunksCatalog catalog,
                   const DeviceFileCatalog::Chunk& chunk);
    /*!
        \return \a false if failed to save to DB
    */
    bool flushRecords();
    QVector<DeviceFileCatalogPtr> loadFullFileCatalog();

    bool replaceChunks(const QString& cameraUniqueId,
                       QnServer::ChunksCatalog catalog,
                       const std::deque<DeviceFileCatalog::Chunk>& chunks);

private: // nx::media_db::DbHelperHandler implementation
    void handleCameraOp(const nx::media_db::CameraOperation &cameraOp, 
                        nx::media_db::Error error) override;

    void handleMediaFileOp(const nx::media_db::MediaFileOperation &mediaFileOp,
                           nx::media_db::Error error) override;

    void handleError(nx::media_db::Error error) override;
    void handleRecordWrite(nx::media_db::Error error) override;

private:
    bool createDatabase(const QString &fileName);

    bool flushRecordsNoLock();
    bool deleteRecordsInternal(const DeleteRecordInfo& delRecord);
    bool addRecordInternal(const QString& cameraUniqueId,
                           QnServer::ChunksCatalog catalog,
                           const DeviceFileCatalog::Chunk& chunk);

    QVector<DeviceFileCatalogPtr> loadChunksFileCatalog();

    void addCatalogFromMediaFolder(const QString& postfix, 
                                   QnServer::ChunksCatalog catalog,
                                   QVector<DeviceFileCatalogPtr>& result);

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
    bool m_needReopenDB;

    nx::media_db::DbHelper m_dbHelper;
    std::unique_ptr<QIODevice> m_ioDevice;
    QString m_dbFileName;
    uint8_t m_dbVersion;
    QVector<DeviceFileCatalogPtr> m_readResult;
    UuidToHash m_uuidToHash;
    nx::media_db::Error m_lastError;
};

typedef std::shared_ptr<QnStorageDb> QnStorageDbPtr;

#endif // __STORAGE_DB_H_
