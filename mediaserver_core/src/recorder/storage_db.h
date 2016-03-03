#ifndef __STORAGE_DB_H_
#define __STORAGE_DB_H_

#include <memory>
#include <deque>
#include <vector>
#include <map>
#include <set>
#include <array>
#include <boost/bimap.hpp>

#include <QElapsedTimer>

#include <server/server_globals.h>
#include "utils/media_db/media_db.h"

#include "device_file_catalog.h"

class QnStorageDb: public nx::media_db::DbHelperHandler 
{
    typedef boost::bimap<QString, uint16_t> UuidToHash;
    typedef std::set<DeviceFileCatalog::Chunk> ChunkSet;
    typedef std::array<ChunkSet, 2> LowHiChunksCatalogs;
    typedef std::map<QString, LowHiChunksCatalogs> UuidToCatalogs;

public:
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
    bool flushRecords() { return true; }
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
    QVector<DeviceFileCatalogPtr> loadChunksFileCatalog();

    void addCatalogFromMediaFolder(const QString& postfix, 
                                   QnServer::ChunksCatalog catalog,
                                   QVector<DeviceFileCatalogPtr>& result);

    bool resetIoDevice();
    // returns cameraId (hash from cameraUniqueId)
    int fillCameraOp(nx::media_db::CameraOperation &cameraOp, const QString &cameraUniqueId);
    QVector<DeviceFileCatalogPtr> buildReadResult() const;

private:
    QnStorageResourcePtr m_storage;
    int m_storageIndex;
    mutable QnMutex m_syncMutex;

    nx::media_db::DbHelper m_dbHelper;
    std::unique_ptr<QIODevice> m_ioDevice;
    QString m_dbFileName;
    uint8_t m_dbVersion;
    UuidToHash m_uuidToHash;
    UuidToCatalogs m_readData;

    nx::media_db::Error m_lastWriteError;
    nx::media_db::Error m_lastReadError;
};

typedef std::shared_ptr<QnStorageDb> QnStorageDbPtr;

#endif // __STORAGE_DB_H_
