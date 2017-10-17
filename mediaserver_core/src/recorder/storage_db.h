#ifndef __STORAGE_DB_H_
#define __STORAGE_DB_H_

#include <memory>
#include <deque>
#include <vector>
#include <map>
#include <set>
#include <array>
#include <random>
#include <chrono>
#include <atomic>
#include <thread>
#include <boost/bimap.hpp>

#include <QElapsedTimer>

#include <nx/utils/std/thread.h>

//#include <server/server_globals.h>
#include "utils/media_db/media_db.h"

#include "device_file_catalog.h"


class QnStorageDb: public nx::media_db::DbHelperHandler 
{
public:
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
    // returns cameraId (hash for cameraUniqueId)
    int fillCameraOp(nx::media_db::CameraOperation &cameraOp, const QString &cameraUniqueId);
    QVector<DeviceFileCatalogPtr> buildReadResult() const;
    bool checkDataConsistency(const UuidToCatalogs &readDataCopy) const;
    bool startDbFile();
    int getCameraIdHash(const QString &cameraUniqueId);
    bool vacuumInternal();
    bool vacuum(QVector<DeviceFileCatalogPtr> *data = nullptr);

    template<typename Callback>
    void startVacuumAsync(Callback callback);

private:
    QnStorageResourcePtr m_storage;
    int m_storageIndex;
    mutable QnMutex m_syncMutex;
    mutable QnMutex m_readMutex;
    mutable QnMutex m_errorMutex;

    nx::media_db::DbHelper m_dbHelper;
    std::unique_ptr<QIODevice> m_ioDevice;
    QString m_dbFileName;
    uint8_t m_dbVersion;
    UuidToHash m_uuidToHash;
    UuidToHash m_readUuidToHash;
    UuidToCatalogs m_readData;

    nx::media_db::Error m_lastReadError;
    nx::media_db::Error m_lastWriteError;
	int m_readErrorCount;

    std::random_device m_rd;
    std::mt19937 m_gen;

    std::chrono::system_clock::time_point m_vacuumTimePoint;
    nx::utils::thread m_vacuumThread;
    std::atomic<bool> m_vacuumThreadRunning;
    mutable QnMutex m_vacuumMutex;
};

typedef std::shared_ptr<QnStorageDb> QnStorageDbPtr;

#endif // __STORAGE_DB_H_
