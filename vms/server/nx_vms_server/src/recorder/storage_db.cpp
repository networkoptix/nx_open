#include <cassert>
#include <algorithm>
#include <string>
#include <unordered_set>
#include <boost/scope_exit.hpp>

#include <plugins/storage/file_storage/file_storage_resource.h>

#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/utils/scope_guard.h>
#include "utils/common/util.h"
#include "storage_db.h"
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/std/future.h>
#include <media_server/media_server_module.h>
#include <recorder/storage_db_pool.h>

namespace {

template<typename F>
auto measureTime(F f, const QString& message) -> std::result_of_t<F()>
{
    NX_DEBUG(typeid(QnStorageDb), lm("%1 Starting").args(message));
    nx::utils::ElapsedTimer timer;
    timer.restart();

    auto onExit = nx::utils::makeScopeGuard([&message, &timer]()
    {
        NX_DEBUG(
            typeid(QnStorageDb),
            lm("%1. Finished. Elapsed: %2 ms").args(message, timer.elapsedMs()));
    });

    return f();
}

} // namespace <anonynous>

QnStorageDb::QnStorageDb(
    QnMediaServerModule* serverModule,
    const QnStorageResourcePtr& s,
    int storageIndex,
    std::chrono::seconds vacuumInterval)
    :
    nx::vms::server::ServerModuleAware(serverModule),
    m_storage(s),
    m_storageIndex(storageIndex),
    m_vacuumInterval(vacuumInterval)
{
    using namespace nx::media_db;
    m_vacuumTimer.addTimer(
        [this](nx::utils::TimerId)
        {
            startVacuum([this](bool success) { onVacuumFinished(success); });
        },
        m_vacuumInterval);
}

void QnStorageDb::onVacuumFinished(bool /*success*/)
{
    m_vacuumTimer.addTimer(
        [this](nx::utils::TimerId)
        {
            startVacuum([this](bool success) { onVacuumFinished(success); });
        },
        m_vacuumInterval);
}

void QnStorageDb::startVacuum(
    VacuumCompletionHandler completionHandler,
    QVector<DeviceFileCatalogPtr> *data)
{
    serverModule()->storageDbPool()->addTask(
        [this, completionHandler = std::move(completionHandler), data]()
        {
            m_dbWriter.reset(new nx::media_db::MediaDbWriter());
            const bool vacuumResult =
                measureTime([this, data]() { return vacuum(data); }, "Vacuum:");

            m_dbWriter->setDevice(m_ioDevice.get());
            completionHandler(vacuumResult);
        });
}

QnStorageDb::~QnStorageDb()
{
}

boost::optional<nx::media_db::CameraOperation> QnStorageDb::createCameraOperation(
    const QString& cameraUniqueId)
{
    std::unordered_set<int> usedIds;
    for (const auto uuidCamIdPair: m_uuidToHash.right)
        usedIds.insert(uuidCamIdPair.first);

    nx::media_db::CameraOperation cameraOperation;
    int cameraId = -1;
    for (int i = 0; i <= std::numeric_limits<uint16_t>::max(); ++i)
    {
        if (usedIds.find(i) == usedIds.cend())
        {
            cameraId = i;
            break;
        }
    }

    const QString warningMessage =
        lm("Failed to find an unused camera ID index for a unique id %1").args(cameraUniqueId);
    NX_ASSERT(cameraId != -1, warningMessage);
    if (cameraId == -1)
    {
        NX_WARNING(this, warningMessage);
        return boost::none;
    }

    m_uuidToHash.insert(UuidToHash::value_type(cameraUniqueId, cameraId));

    cameraOperation.setCameraId(cameraId);
    cameraOperation.setRecordType(nx::media_db::RecordType::CameraOperationAdd);
    cameraOperation.setCameraUniqueIdLen(cameraUniqueId.size());
    cameraOperation.setCameraUniqueId(cameraUniqueId.toUtf8());

    return cameraOperation;
}

int QnStorageDb::getOrGenerateCameraIdHash(const QString &cameraUniqueId)
{
    int cameraId;
    auto it = m_uuidToHash.left.find(cameraUniqueId);
    if (it != m_uuidToHash.left.end())
    {
        cameraId = it->second;
        return cameraId;
    }

    const auto cameraOperation = createCameraOperation(cameraUniqueId);
    if (!cameraOperation)
        return -1;

    m_dbWriter->writeRecord(*cameraOperation);
    return cameraOperation->getCameraId();
}

void QnStorageDb::deleteRecords(
    const QString& cameraUniqueId,
    QnServer::ChunksCatalog catalog,
    qint64 startTimeMs)
{
    serverModule()->storageDbPool()->addTask(
        [this,
        cameraUniqueId,
        catalog,
        startTimeMs]() mutable
        {
            const int cameraId = getOrGenerateCameraIdHash(cameraUniqueId);
            if (cameraId == -1)
            {
                NX_WARNING(
                    this,
                    lit("[media_db, delete] camera id hash is not generated. Unable to delete"));
                return;
            }

            nx::media_db::MediaFileOperation mediaFileOp;
            mediaFileOp.setCameraId(cameraId);
            mediaFileOp.setCatalog(catalog);
            mediaFileOp.setStartTime(startTimeMs);
            mediaFileOp.setRecordType(nx::media_db::RecordType::FileOperationDelete);

            m_dbWriter->writeRecord(mediaFileOp);
        });
}

void QnStorageDb::addRecord(
    const QString& cameraUniqueId,
    QnServer::ChunksCatalog catalog,
    const DeviceFileCatalog::Chunk& chunk)
{
    serverModule()->storageDbPool()->addTask(
        [this, cameraUniqueId, catalog, chunk]() mutable
        {
            nx::media_db::MediaFileOperation mediaFileOp;
            int cameraId = getOrGenerateCameraIdHash(cameraUniqueId);
            if (cameraId == -1)
            {
                NX_WARNING(
                    this,
                    lit("[media_db, add] camera id hash is not generated. Unable to add record"));
                return;
            }

            mediaFileOp.setCameraId(cameraId);
            mediaFileOp.setCatalog(catalog);
            mediaFileOp.setDuration(chunk.durationMs);
            mediaFileOp.setFileTypeIndex(chunk.fileIndex);
            mediaFileOp.setFileSize(chunk.getFileSize());
            mediaFileOp.setRecordType(nx::media_db::RecordType::FileOperationAdd);
            mediaFileOp.setStartTime(chunk.startTimeMs);
            mediaFileOp.setTimeZone(chunk.timeZone);

            m_dbWriter->writeRecord(mediaFileOp);
        });
}

void QnStorageDb::replaceChunks(
    const QString& cameraUniqueId,
    QnServer::ChunksCatalog catalog,
    const std::deque<DeviceFileCatalog::Chunk>& chunks)
{
    deleteRecords(cameraUniqueId, catalog, -1);
    for (const auto &chunk : chunks)
        addRecord(cameraUniqueId, catalog, chunk);
}

bool QnStorageDb::open(const QString& fileName)
{
    return createDatabase(fileName);
}

bool QnStorageDb::resetIoDevice()
{
    m_ioDevice.reset(m_storage->open(m_dbFileName, QIODevice::ReadWrite | QIODevice::Unbuffered));
    if (!m_ioDevice)
    {
        NX_WARNING(this, lm("%1 DB file open failed").args(m_dbFileName));
        return false;
    }

    return true;
}

bool QnStorageDb::createDatabase(const QString &fileName)
{
    m_dbFileName = fileName;
    if (!resetIoDevice())
        return false;

    const QByteArray data = m_ioDevice->read(nx::media_db::FileHeader::kSerializedRecordSize);
    ByteStreamReader reader(data);
    nx::media_db::FileHeader fileHeader;

    if (!fileHeader.deserialize(&reader) || fileHeader.getDbVersion() != nx::media_db::kDbVersion)
        return startDbFile();

    return true;
}

QVector<DeviceFileCatalogPtr> QnStorageDb::loadFullFileCatalog()
{
    QVector<DeviceFileCatalogPtr> result;
    result << loadChunksFileCatalog();

    addCatalogFromMediaFolder(lit("hi_quality"), QnServer::HiQualityCatalog, result);
    addCatalogFromMediaFolder(lit("low_quality"), QnServer::LowQualityCatalog, result);

    return result;
}

bool isCatalogExistInResult(const QVector<DeviceFileCatalogPtr>& result,
                            QnServer::ChunksCatalog catalog,
                            const QString& uniqueId)
{
    for(const DeviceFileCatalogPtr& c: result)
    {
        if (c->getRole() == catalog && c->cameraUniqueId() == uniqueId)
            return true;
    }
    return false;
}

void QnStorageDb::addCatalogFromMediaFolder(const QString& postfix,
                                            QnServer::ChunksCatalog catalog,
                                            QVector<DeviceFileCatalogPtr>& result)
{
    QString root = closeDirPath(QFileInfo(m_dbFileName).absoluteDir().path()) + postfix;

    QnAbstractStorageResource::FileInfoList files;
    if (m_storage)
        files = m_storage->getFileList(root);
    else
        files = QnAbstractStorageResource::FIListFromQFIList(
            QDir(root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot,
                                     QDir::Name));

    for (const QnAbstractStorageResource::FileInfo& fi: files)
    {
        QString uniqueId = fi.baseName();
        if (!isCatalogExistInResult(result, catalog, uniqueId))
        {
            result << DeviceFileCatalogPtr(new DeviceFileCatalog(
                serverModule(),
                uniqueId, catalog,
                QnServer::StoragePool::None));
        }
    }
}

bool QnStorageDb::startDbFile()
{
    m_ioDevice.reset();
    m_storage->removeFile(m_dbFileName);

    if (!resetIoDevice())
        return false;

    if (!nx::media_db::MediaDbWriter::writeFileHeader(m_ioDevice.get(), nx::media_db::kDbVersion))
    {
        NX_WARNING(this, lit("%1 write DB header failed").arg(Q_FUNC_INFO));
        return false;
    }

    return true;
}

QVector<DeviceFileCatalogPtr> QnStorageDb::loadChunksFileCatalog()
{
    QVector<DeviceFileCatalogPtr> result;
    NX_INFO(this, lit("Loading chunks from DB. storage: %1, file: %2")
            .arg(m_storage->getUrl())
            .arg(m_dbFileName));

    nx::utils::promise<void> readyPromise;
    auto readyFuture = readyPromise.get_future();

    auto completionHandler =
        [readyPromise = std::move(readyPromise), this](bool success) mutable
        {
            if (success)
            {
                NX_INFO(
                    this,
                    lit("finished loading chunks from DB. storage: %1, file: %2")
                    .arg(m_storage->getUrl())
                    .arg(m_dbFileName));
            }
            else
            {
                NX_WARNING(
                    this,
                    lit("loading chunks from DB failed. storage: %1, file: %2")
                    .arg(m_storage->getUrl())
                    .arg(m_dbFileName));
            }

            readyPromise.set_value();
        };

    startVacuum(std::move(completionHandler), &result);

    readyFuture.wait();
    return result;
}

QByteArray QnStorageDb::dbFileContent()
{
    std::unique_ptr<QIODevice> file(m_storage->open(m_dbFileName, QIODevice::ReadOnly));
    if (!file)
    {
        NX_DEBUG(this, lm("Failed to open DB file %1").args(m_dbFileName));
        return QByteArray();
    }

    return file->readAll();
}

bool QnStorageDb::vacuum(QVector<DeviceFileCatalogPtr> *data)
{
    m_ioDevice.reset();

    auto parsedData = std::make_unique<nx::media_db::DbReader::Data>();
    auto fileContent = dbFileContent();

    if (!measureTime(
            [this, &fileContent, &parsedData]()
            {
                return nx::media_db::DbReader::parse(fileContent, parsedData.get());
            },
            QString("Vacuum: Parse DB:")))
    {
        NX_WARNING(this, lm("Failed to parse DB file %1").args(m_dbFileName));
        startDbFile();
        return false;
    }

    if (!measureTime(
            [this, &parsedData, data]() { return writeVacuumedData(std::move(parsedData), data); },
            QString("Vacuum: writeVacuumedData:")))
    {
        NX_WARNING(this, lm("Failed to write vacuumed data. DB file %1").args(m_dbFileName));
        startDbFile();
        return false;
    }

    return true;
}

bool QnStorageDb::writeVacuumedData(
    std::unique_ptr<nx::media_db::DbReader::Data> parsedData,
    QVector<DeviceFileCatalogPtr> *outCatalog)
{
    using namespace std::chrono;

    nx::utils::ElapsedTimer timer;
    timer.restart();

    NX_DEBUG(this, "QnStorageDb::writeVacuumedData() begin");

    int expectedBufferSize = 0;
    for (const auto& cameraData: parsedData->cameras)
        expectedBufferSize += cameraData.serializedRecordSize();
    for (const auto& catalog: parsedData->addRecords)
    {
        expectedBufferSize +=
            (int) catalog.second.size() * nx::media_db::MediaFileOperation::kSerializedRecordSize;
    }

    ByteStreamWriter writer(expectedBufferSize);
    processDbContent(*(parsedData.get()), outCatalog, writer);
    writer.flush();

    NX_DEBUG(
        this,
        "QnStorageDb::serializedData() completed successfully. time = %1 ms", timer.elapsedMs());

    bool res = m_storage->removeFile(m_dbFileName);
    NX_ASSERT(res);
    if (!res)
    {
        NX_WARNING(this, lm("Failed to remove DB file %1").args(m_dbFileName));
        return false;
    }

    if (!startDbFile())
        return false;

    m_ioDevice->write(writer.data());
    NX_DEBUG(
        this,
        "QnStorageDb::writeVacuumedData write to disk finished. time = %1 ms", timer.elapsedMs());

    return true;
}

void QnStorageDb::putRecordsToCatalog(
    QVector<DeviceFileCatalogPtr>* deviceFileCatalog,
    int cameraId,
    int catalogIndex,
    std::deque <DeviceFileCatalog::Chunk> chunks,
    const UuidToHash& uuidToHash)
{
    auto cameraUuidIt = uuidToHash.right.find(cameraId);
    if (cameraUuidIt == uuidToHash.right.end())
    {
        NX_WARNING(this, "Skip catalog %1 because there is no cameraUnique registerd", cameraId);
        return;
    }

    DeviceFileCatalogPtr newFileCatalog(new DeviceFileCatalog(
        serverModule(),
        cameraUuidIt->get_left(),
        (QnServer::ChunksCatalog) catalogIndex,
        QnServer::StoragePool::None));
    std::sort(chunks.begin(), chunks.end());

    newFileCatalog->assignChunksUnsafe(chunks.begin(), chunks.end());
    deviceFileCatalog->push_back(newFileCatalog);
}

DeviceFileCatalog::Chunk QnStorageDb::toChunk(
    const nx::media_db::MediaFileOperation& mediaData) const
{
    return DeviceFileCatalog::Chunk(
        mediaData.getStartTime(),
        m_storageIndex,
        mediaData.getFileTypeIndex(),
        mediaData.getDuration(),
        mediaData.getTimeZone(),
        (quint16)(mediaData.getFileSize() >> 32),
        (quint32)mediaData.getFileSize());
}

void QnStorageDb::processDbContent(
    nx::media_db::DbReader::Data& parsedData,
    QVector<DeviceFileCatalogPtr>* deviceFileCatalog,
    ByteStreamWriter& writer)
{
    UuidToHash uuidToHash;
    for (const auto& cameraData : parsedData.cameras)
    {
        int index = cameraData.getCameraId() * 2;
        if (parsedData.addRecords[index].empty() && parsedData.addRecords[index + 1].empty())
            continue;

        uuidToHash.insert(UuidToHash::value_type(
            cameraData.getCameraUniqueId(), cameraData.getCameraId()));
        cameraData.serialize(writer);
    }

    m_uuidToHash = uuidToHash;
    for (auto itr = parsedData.addRecords.begin(); itr != parsedData.addRecords.end(); ++itr)
    {
        int index = itr->first;
        auto& catalog = itr->second;

        std::deque <DeviceFileCatalog::Chunk> chunks;
        auto& removeCatalog = parsedData.removeRecords[index];
        for (size_t i = 0; i < catalog.size(); ++i)
        {
            const auto& mediaData = catalog[i];
            const auto hash = mediaData.getHashInCatalog();
            auto removeItr = std::upper_bound(
                removeCatalog.begin(),
                removeCatalog.end(),
                nx::media_db::DbReader::RemoveData { hash, i });

            if (removeItr != removeCatalog.end() && removeItr->hash == hash)
                continue; //< Value removed

            mediaData.serialize(writer);
            if (deviceFileCatalog)
                chunks.push_back(toChunk(mediaData));
        }

        if (deviceFileCatalog && !chunks.empty())
        {
            putRecordsToCatalog(
                deviceFileCatalog, index / 2,
                index & 1,
                std::move(chunks),
                uuidToHash);
        }
    }
}
