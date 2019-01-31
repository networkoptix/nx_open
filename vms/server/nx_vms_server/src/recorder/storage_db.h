#pragma once

#include <memory>
#include <deque>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <boost/bimap.hpp>

#include <QElapsedTimer>

#include <nx/network/aio/timer.h>
#include <nx/utils/move_only_func.h>

#include "utils/media_db/media_db.h"

#include "device_file_catalog.h"
#include <nx/vms/server/server_module_aware.h>

struct QStringHasher
{
    size_t operator()(QString const& s) const noexcept
    {
        return qHash(s);
    }
};
class QnStorageDb: public nx::vms::server::ServerModuleAware
{
public:
    typedef boost::bimap<QString, uint16_t> UuidToHash;
    typedef std::set<DeviceFileCatalog::Chunk> ChunkSet;
    typedef std::array<ChunkSet, 2> LowHiChunksCatalogs;
    typedef std::unordered_map<QString, LowHiChunksCatalogs, QStringHasher> UuidToCatalogs;

public:
    QnStorageDb(
        QnMediaServerModule* serverModule,
        const QnStorageResourcePtr& storage,
        int storageIndex);
    virtual ~QnStorageDb();

    bool open(const QString& fileName);

    bool deleteRecords(
        const QString& cameraUniqueId,
        QnServer::ChunksCatalog catalog,
        qint64 startTimeMs = -1);

    bool addRecord(const QString& cameraUniqueId,
                   QnServer::ChunksCatalog catalog,
                   const DeviceFileCatalog::Chunk& chunk);

    QVector<DeviceFileCatalogPtr> loadFullFileCatalog();

    bool replaceChunks(const QString& cameraUniqueId,
                       QnServer::ChunksCatalog catalog,
                       const std::deque<DeviceFileCatalog::Chunk>& chunks);

private:
    using VacuumCompletionHandler = nx::utils::MoveOnlyFunc<void(bool)>;

    QnStorageResourcePtr m_storage;
    int m_storageIndex;
    std::unique_ptr<nx::media_db::MediaDbWriter> m_dbWriter;
    std::unique_ptr<QIODevice> m_ioDevice;
    QString m_dbFileName;
    UuidToHash m_uuidToHash;
    std::chrono::system_clock::time_point m_vacuumTimePoint;
    bool m_vacuumInProgress = false;
    nx::media_db::DBRecordQueue m_dbRecordQueue;
    nx::network::aio::Timer m_timer;

    bool createDatabase(const QString &fileName);
    QVector<DeviceFileCatalogPtr> loadChunksFileCatalog();
    void addCatalogFromMediaFolder(const QString& postfix,
        QnServer::ChunksCatalog catalog,
        QVector<DeviceFileCatalogPtr>& result);
    bool resetIoDevice();
    // returns cameraId (hash for cameraUniqueId)
    int fillCameraOp(nx::media_db::CameraOperation &cameraOp, const QString &cameraUniqueId);
    bool startDbFile();
    int getOrGenerateCameraIdHash(const QString &cameraUniqueId);
    bool writeVacuumedData(
        std::unique_ptr<nx::media_db::DbReader::Data> readData,
        QVector<DeviceFileCatalogPtr> *outCatalog);
    void processDbContent(
        nx::media_db::DbReader::Data& parsedData,
        QVector<DeviceFileCatalogPtr> *deviceFileCatalog,
        ByteStreamWriter& writer);
    void putRecordsToCatalog(
        QVector<DeviceFileCatalogPtr>* deviceFileCatalog,
        int cameraId,
        int catalogIndex,
        std::deque <DeviceFileCatalog::Chunk> chunks,
        const UuidToHash& uuidToHash);
    DeviceFileCatalog::Chunk toChunk(const nx::media_db::MediaFileOperation& mediaData) const;
    bool vacuum(QVector<DeviceFileCatalogPtr> *data = nullptr);
    QByteArray dbFileContent();
    void startVacuum(
        VacuumCompletionHandler completionHandler,
        QVector<DeviceFileCatalogPtr> *data = nullptr);
    void writeOrCache(const nx::media_db::DBRecord& dbRecord);
    void onVacuumFinished(bool success);
};

typedef std::shared_ptr<QnStorageDb> QnStorageDbPtr;
