#include <cassert>
#include <algorithm>
#include <string>
#include <unordered_set>
#include <boost/scope_exit.hpp>

#include <plugins/storage/file_storage/file_storage_resource.h>

#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/guarded_callback.h>
#include "utils/common/util.h"
#include "storage_db.h"
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/std/future.h>
#include <media_server/media_server_module.h>
#include <recorder/storage_db_pool.h>

namespace {

static const char* const kSeparator= "--";

template<typename F>
auto measureTime(F f, const QString& message) -> std::invoke_result_t<F>
{
    NX_DEBUG(typeid(QnStorageDb), lm("%1 Starting").args(message));
    nx::utils::ElapsedTimer timer;
    timer.restart();

    auto onExit = nx::utils::makeScopeGuard(
        [&message, &timer]()
        {
            NX_DEBUG(
                typeid(QnStorageDb),
                lm("%1. Finished. Elapsed: %2").args(message, timer.elapsed()));
        });

    return f();
}

static int64_t sequenceIdFromFilePath(const QnAbstractStorageResource::FileInfo& fileInfo)
{
    if (!fileInfo.baseName().contains(kSeparator))
        return 1;

    return fileInfo.baseName().split(kSeparator)[1].toLongLong();
}

} // namespace

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
    serverModule()->storageDbPool()->addTask(nx::utils::guarded(this,
        [this, completionHandler = std::move(completionHandler), data]()
        {
            m_dbWriter.reset(new nx::media_db::MediaDbWriter());
            const bool vacuumResult =
                measureTime([this, data]() { return vacuum(data); }, "Vacuum:");

            m_dbWriter->setDevice(m_ioDevice.get());
            completionHandler(vacuumResult);
        }));
}

QnStorageDb::~QnStorageDb()
{
}

boost::optional<nx::media_db::CameraOperation> QnStorageDb::createCameraOperation(
    const QString& cameraUniqueId)
{
    std::unordered_set<int> usedIds;
    for (const auto& uuidCamIdPair: m_uuidToHash.right)
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

    m_uuidToHash.insert(UuidToHash::value_type(cameraUniqueId, static_cast<uint16_t>(cameraId)));

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
    serverModule()->storageDbPool()->addTask(nx::utils::guarded(this,
        [this, cameraUniqueId, catalog, startTimeMs]()
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
        }));
}

void QnStorageDb::addRecord(
    const QString& cameraUniqueId,
    QnServer::ChunksCatalog catalog,
    const nx::vms::server::Chunk& chunk)
{
    serverModule()->storageDbPool()->addTask(nx::utils::guarded(this,
        [this, cameraUniqueId, catalog, chunk]()
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
        }));
}

void QnStorageDb::replaceChunks(
    const QString& cameraUniqueId, QnServer::ChunksCatalog catalog,
    const nx::vms::server::ChunksDeque& chunks)
{
    deleteRecords(cameraUniqueId, catalog, -1);
    for (const auto &chunk : chunks)
        addRecord(cameraUniqueId, catalog, chunk.chunk());
}

bool QnStorageDb::readDbHeader() const
{
    const QByteArray data = m_ioDevice->read(nx::media_db::FileHeader::kSerializedRecordSize);
    ByteStreamReader reader(data);
    nx::media_db::FileHeader fileHeader;

    if (!fileHeader.deserialize(&reader) || fileHeader.getDbVersion() != nx::media_db::kDbVersion)
    {
        NX_WARNING(
            this, "QnStorageDb::readDbHeader: Failed to read a DB header from (%1)", m_dbFileInfo);
        return false;
    }

    NX_DEBUG(this, "QnStorageDb::readDbHeader: DB header was has been successfully read");
    return true;
}

void QnStorageDb::removeFiles(
    const QnAbstractStorageResource::FileInfoList& toRemove,
    const QnAbstractStorageResource::FileInfo& except)
{
    NX_DEBUG(
        this, "QnStorageDb::removeFiles: To remove: %1, exception: %2", containerString(toRemove),
        except);
    for (const auto& fileInfo: toRemove)
    {
        if (fileInfo != except)
            m_storage->removeFile(fileInfo.absoluteFilePath());
    }
}

bool QnStorageDb::open(const QString& basePath)
{
    const auto dbFiles = allDbFiles(basePath);
    if (dbFiles.isEmpty())
    {
        NX_DEBUG(
            this,
            "QnStorageDB::open: Folder: %1. No previous DB files found. Starting with a new one",
            basePath);

        return startDbFile(basePath, /*incVersion*/ false);
    }

    NX_ASSERT(dbFiles.size() == 1 || dbFiles.size() == 2);
    m_dbFileInfo = dbFiles[0];
    removeFiles(dbFiles, m_dbFileInfo);

    if (!openDbFile() || !readDbHeader())
    {
        NX_DEBUG(
            this, "QnStorageDB::open: File: (%1). Failed to open a DB file or read a header",
            m_dbFileInfo);

        return startDbFile(basePath, /*incVersion*/ false);
    }

    NX_DEBUG(this, "QnStorageDB::open: File %1 was successfully opened", m_dbFileInfo);
    return true;
}

bool QnStorageDb::openDbFile()
{
    m_ioDevice.reset(m_storage->open(
        m_dbFileInfo.absoluteFilePath(), QIODevice::ReadWrite | QIODevice::Unbuffered));
    if (!m_ioDevice)
    {
        NX_WARNING(this, "DB file (%1) open failed", m_dbFileInfo);
        return false;
    }

    return true;
}

QnAbstractStorageResource::FileInfoList QnStorageDb::allDbFiles(const QString& basePath) const
{
    const auto moduleGuid = moduleGUID().toSimpleString();
    QStringList filters = {moduleGuid + "_media\\.nxdb", moduleGuid + "--\\d+\\.nxdb"};

    auto candidates = m_storage->getFileList(basePath);
    candidates.erase(
        std::remove_if(
            candidates.begin(), candidates.end(),
            [&filters](const auto& fileInfo)
            {
                return fileInfo.isDir()
                    || std::none_of(
                           filters.cbegin(), filters.cend(),
                           [&fileInfo](const QString& filter)
                           {
                               return QRegularExpression(filter)
                                   .match(fileInfo.absoluteFilePath())
                                   .hasMatch();
                           });
            }),
        candidates.end());

    std::sort(
        candidates.begin(), candidates.end(),
        [](const auto& fileInfo1, const auto& fileInfo2)
        {
            const auto path1 = fileInfo1.absoluteFilePath();
            const auto path2 = fileInfo2.absoluteFilePath();
            NX_ASSERT(path1.contains(kSeparator) || path2.contains(kSeparator));

            if (!path1.contains(kSeparator))
                return true;

            if (!path2.contains(kSeparator))
                return false;

            return path1 < path2;
        });

    NX_DEBUG(this, "DB files found: %1", containerString(candidates));
    return candidates;
}

QVector<DeviceFileCatalogPtr> QnStorageDb::loadFullFileCatalog()
{
    QVector<DeviceFileCatalogPtr> result;
    result << loadChunksFileCatalog();

    addCatalogFromMediaFolder(lit("hi_quality"), QnServer::HiQualityCatalog, result);
    addCatalogFromMediaFolder(lit("low_quality"), QnServer::LowQualityCatalog, result);

    return result;
}

bool isCatalogExistInResult(
    const QVector<DeviceFileCatalogPtr>& result, QnServer::ChunksCatalog catalog,
    const QString& uniqueId)
{
    for(const DeviceFileCatalogPtr& c: result)
    {
        if (c->getRole() == catalog && c->cameraUniqueId() == uniqueId)
            return true;
    }
    return false;
}

void QnStorageDb::addCatalogFromMediaFolder(
    const QString& postfix,
    QnServer::ChunksCatalog catalog,
    QVector<DeviceFileCatalogPtr>& result)
{
    QString root = closeDirPath(m_dbFileInfo.absoluteDirPath()) + postfix;
    const auto fileInfos = m_storage->getFileList(root);
    NX_DEBUG(
        this, "QnStorageDb::addCatalogFromMediaFolder: root: %1, files: %2", root,
        containerString(fileInfos));
    for (const QnAbstractStorageResource::FileInfo& fi: fileInfos)
    {
        if (fi.isDir())
        {
            QString uniqueId = fi.baseName();
            if (!isCatalogExistInResult(result, catalog, uniqueId))
            {
                result << DeviceFileCatalogPtr(new DeviceFileCatalog(
                    serverModule(), uniqueId, catalog, QnServer::StoragePool::None));
            }
        }
    }
}

QString QnStorageDb::baseFileName(int64_t seqId)
{
    return moduleGUID().toSimpleString()
        + kSeparator
        + QString::number(static_cast<long long>(seqId))
        + ".nxdb";
}

bool QnStorageDb::startDbFile(const QString& basePath, bool incVersion)
{
    m_ioDevice.reset();
    const auto dbFiles = allDbFiles(basePath);
    int64_t newSeqId = 1;

    if (!incVersion)
    {
        removeFiles(dbFiles, QnAbstractStorageResource::FileInfo());
    }
    else if (!dbFiles.isEmpty())
    {
        removeFiles(dbFiles, dbFiles[dbFiles.size() - 1]);
        const int64_t prevSeqId = sequenceIdFromFilePath(dbFiles[dbFiles.size() - 1]);
        newSeqId = prevSeqId + 1;
        if (prevSeqId == std::numeric_limits<int64_t>::max())
        {
            const auto newPrevFileName = closeDirPath(basePath) + baseFileName(/*seqId*/ 1);
            if (!m_storage->renameFile(
                    dbFiles[dbFiles.size() - 1].absoluteFilePath(), newPrevFileName))
            {
                NX_WARNING(
                    this, "Failed to rename the DB file with max sequence id (%1)",
                    dbFiles[dbFiles.size() - 1]);
                return false;
            }

            newSeqId = 2;
        }
    }

    m_dbFileInfo = QnAbstractStorageResource::FileInfo(
        closeDirPath(basePath) + baseFileName(newSeqId), /*size*/ 0, /*isDir*/ false);

    NX_DEBUG(this, "Creating a new DB file (%1)", m_dbFileInfo);
    if (!openDbFile())
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
    NX_INFO(
        this, "Loading chunks from DB. storage: %1, file: %2", m_storage->getUrl(), m_dbFileInfo);

    nx::utils::promise<void> readyPromise;
    auto readyFuture = readyPromise.get_future();

    auto completionHandler =
        [readyPromise = std::move(readyPromise), this](bool success) mutable
        {
            if (success)
            {
                NX_INFO(
                    this, "finished loading chunks from DB. storage: %1, file: %2",
                    m_storage->getUrl(), m_dbFileInfo);
            }
            else
            {
                NX_WARNING(
                    this, "loading chunks from DB failed. storage: %1, file: %2",
                    m_storage->getUrl(), m_dbFileInfo);
            }

            readyPromise.set_value();
        };

    startVacuum(std::move(completionHandler), &result);

    readyFuture.wait();
    return result;
}

QByteArray QnStorageDb::dbFileContent()
{
    std::unique_ptr<QIODevice> file(
        m_storage->open(m_dbFileInfo.absoluteFilePath(), QIODevice::ReadOnly));
    if (!file)
    {
        NX_DEBUG(this, "Failed to open DB file %1", m_dbFileInfo);
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
            [&fileContent, &parsedData]()
            {
                return nx::media_db::DbReader::parse(fileContent, parsedData.get());
            },
            QString("Vacuum: Parse DB:")))
    {
        NX_WARNING(this, "Failed to parse DB file %1", m_dbFileInfo);
        startDbFile(m_dbFileInfo.absoluteDirPath(), /*incVersion*/ false);
        return false;
    }

    if (!measureTime(
            [this, &parsedData, data]() { return writeVacuumedData(std::move(parsedData), data); },
            QString("Vacuum: writeVacuumedData:")))
    {
        NX_WARNING(this, "Failed to write vacuumed data. DB file %1", m_dbFileInfo);
        startDbFile(m_dbFileInfo.absoluteDirPath(), /*incVersion*/ false);
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
            static_cast<int>(catalog.second.size())
            * nx::media_db::MediaFileOperation::kSerializedRecordSize;
    }

    ByteStreamWriter writer(expectedBufferSize);
    processDbContent(*(parsedData.get()), outCatalog, writer);
    writer.flush();

    NX_DEBUG(
        this,
        "QnStorageDb::serializedData() completed successfully. time = %1", timer.elapsed());

    if (!startDbFile(m_dbFileInfo.absoluteDirPath(), /*incVersion*/ true))
        return false;

    const auto dbFiles = allDbFiles(m_dbFileInfo.absoluteDirPath());
    NX_ASSERT(dbFiles.size() == 2);

    m_ioDevice->write(writer.data());
    NX_DEBUG(
        this, "QnStorageDb::writeVacuumedData write to disk finished. time = %1",
        timer.elapsed());

    removeFiles(dbFiles, m_dbFileInfo);
    return true;
}

void QnStorageDb::putRecordsToCatalog(
    QVector<DeviceFileCatalogPtr>* deviceFileCatalog,
    int cameraId,
    int catalogIndex,
    std::deque <nx::vms::server::Chunk> chunks,
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
        static_cast<QnServer::ChunksCatalog>(catalogIndex),
        QnServer::StoragePool::None));
    std::sort(chunks.begin(), chunks.end());

    newFileCatalog->assignChunksUnsafe(chunks.begin(), chunks.end());
    deviceFileCatalog->push_back(newFileCatalog);
}

nx::vms::server::Chunk QnStorageDb::toChunk(
    const nx::media_db::MediaFileOperation& mediaData) const
{
    return nx::vms::server::Chunk(
        mediaData.getStartTime(),
        m_storageIndex,
        mediaData.getFileTypeIndex(),
        mediaData.getDuration(),
        static_cast<int16_t>(mediaData.getTimeZone()),
        static_cast<quint16>(mediaData.getFileSize() >> 32),
        static_cast<quint32>(mediaData.getFileSize()));
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
            cameraData.getCameraUniqueId(), static_cast<uint16_t>(cameraData.getCameraId())));
        cameraData.serialize(writer);
    }

    m_uuidToHash = uuidToHash;
    for (auto itr = parsedData.addRecords.begin(); itr != parsedData.addRecords.end(); ++itr)
    {
        int index = itr->first;
        auto& catalog = itr->second;

        std::deque<nx::vms::server::Chunk> chunks;
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
