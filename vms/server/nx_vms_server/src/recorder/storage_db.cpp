#include <cassert>
#include <algorithm>
#include <string>
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

const size_t kCatalogsCount = 2;
const size_t kMaxWriteQueueSize = 5000;

const std::chrono::seconds kVacuumInterval(3600 * 24);

inline void AV_WB64(char** dst, quint64 data)
{
    quint64* dst64 = (quint64*)(*dst);
    *dst64 = qToLittleEndian(data);
    *dst += sizeof(data);
}

inline void AV_WRITE_BUFFER(char** dst, const char* src, qint64 size)
{
    memcpy(*dst, src, size);
    *dst += size;
}

const uint8_t kDbVersion = 2;

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
    const QnStorageResourcePtr& s, int storageIndex)
    :
    nx::vms::server::ServerModuleAware(serverModule),
    m_storage(s),
    m_storageIndex(storageIndex),
    m_dbWriter(new nx::media_db::MediaDbWriter),
    m_ioDevice(nullptr),
    m_vacuumTimePoint(std::chrono::system_clock::now()),
    m_vacuumInProgress(false)
{
    using namespace nx::media_db;
    m_getTimeZoneFunc = &MediaFileOperation::getTimeZoneV2;
    m_getStartTimeFunc = &MediaFileOperation::getStartTimeV2;

    m_timer.start(
        kVacuumInterval,
        [this]() { startVacuum([this](bool success) { onVacuumFinished(success); }); });
}

void QnStorageDb::onVacuumFinished(bool success)
{
    m_timer.start(
        kVacuumInterval,
        [this]() { startVacuum([this](bool success) { onVacuumFinished(success); }); });
}

void QnStorageDb::startVacuum(
    VacuumCompletionHandler completionHandler,
    QVector<DeviceFileCatalogPtr> *data)
{
    if (m_vacuumInProgress)
        return;

    m_vacuumInProgress = true;
    m_dbWriter.reset(new nx::media_db::MediaDbWriter);
    auto sharedHandler = std::make_shared<VacuumCompletionHandler>(std::move(completionHandler));

    serverModule()->storageDbPool()->addVacuumTask(
        [this, sharedHandler, data]()
        {
            const bool vacuumResult =
                measureTime([this, data]() { return vacuum(data); }, "Vacuum:");

            m_timer.post(
                [this, sharedHandler, vacuumResult]()
                {
                    m_dbWriter->setDevice(m_ioDevice.get());
                    m_dbWriter->start();
                    while (!m_dbRecordQueue.empty())
                    {
                        const auto dbRecord = m_dbRecordQueue.front();
                        m_dbRecordQueue.pop();
                        m_dbWriter->writeRecord(dbRecord);
                    }

                    m_vacuumInProgress = false;
                    (*sharedHandler)(vacuumResult);
                });
        });
}

QnStorageDb::~QnStorageDb()
{
    m_timer.cancelSync();
}

int QnStorageDb::fillCameraOp(
    nx::media_db::CameraOperation &cameraOp,
    const QString &cameraUniqueId)
{
    int cameraId = qHash(cameraUniqueId);

    cameraOp.setRecordType(nx::media_db::RecordType::CameraOperationAdd);
    cameraOp.setCameraId(cameraId);
    cameraOp.setCameraUniqueIdLen(cameraUniqueId.size());
    cameraOp.setCameraUniqueId(
        QByteArray(cameraUniqueId.toLatin1().constData(), cameraUniqueId.size()));

    return cameraId;
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

    nx::media_db::CameraOperation cameraOp;
    cameraId = fillCameraOp(cameraOp, cameraUniqueId);
    auto existHashIdIt = m_uuidToHash.right.find(cameraId);

    const int kMaxTries = 20;
    int triesSoFar = 0;
    while (existHashIdIt != m_uuidToHash.right.end())
    {
        if (triesSoFar == kMaxTries)
        {
            NX_WARNING(
                this,
                lm("[media_db] Unable to generate unique hash for camera id %1")
                .arg(cameraUniqueId));
            return -1;
        }
        cameraId = nx::utils::random::number<uint16_t>();
        existHashIdIt = m_uuidToHash.right.find(cameraId);
        triesSoFar++;
    }

    NX_ASSERT(existHashIdIt == m_uuidToHash.right.end());
    if (existHashIdIt != m_uuidToHash.right.end())
        NX_WARNING(this, lit("%1 Bad camera hash").arg(Q_FUNC_INFO));

    m_uuidToHash.insert(UuidToHash::value_type(cameraUniqueId, cameraId));
    cameraOp.setCameraId(cameraId);
    m_dbWriter->writeRecord(cameraOp);
    return cameraId;
}

bool QnStorageDb::deleteRecords(
    const QString& cameraUniqueId,
    QnServer::ChunksCatalog catalog,
    qint64 startTimeMs)
{
    nx::utils::promise<bool> readyPromise;
    nx::utils::future<bool> readyFuture = readyPromise.get_future();

    m_timer.post(
        [this,
        readyPromise = std::move(readyPromise),
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
                readyPromise.set_value(false);
                return;
            }

            nx::media_db::MediaFileOperation mediaFileOp;
            mediaFileOp.setCameraId(cameraId);
            mediaFileOp.setCatalog(catalog);
            mediaFileOp.setStartTime(startTimeMs);
            mediaFileOp.setRecordType(nx::media_db::RecordType::FileOperationDelete);

            writeOrCache(mediaFileOp);
            readyPromise.set_value(true);
        });

    return readyFuture.get();
}

void QnStorageDb::writeOrCache(const nx::media_db::DBRecord& dbRecord)
{
    if (m_vacuumInProgress)
    {
        if (m_dbRecordQueue.size() > kMaxWriteQueueSize)
            NX_WARNING(this, "Records queue overflown.");
        else
            m_dbRecordQueue.push(dbRecord);
    }
    else
    {
        m_dbWriter->writeRecord(dbRecord);
    }
}

bool QnStorageDb::addRecord(
    const QString& cameraUniqueId,
    QnServer::ChunksCatalog catalog,
    const DeviceFileCatalog::Chunk& chunk)
{
    nx::utils::promise<bool> readyPromise;
    nx::utils::future<bool> readyFuture = readyPromise.get_future();

    m_timer.post(
        [this, readyPromise = std::move(readyPromise), cameraUniqueId, catalog, chunk]() mutable
        {
            nx::media_db::MediaFileOperation mediaFileOp;
            int cameraId = getOrGenerateCameraIdHash(cameraUniqueId);
            if (cameraId == -1)
            {
                NX_WARNING(
                    this,
                    lit("[media_db, add] camera id hash is not generated. Unable to add record"));
                readyPromise.set_value(false);
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

            writeOrCache(mediaFileOp);
            readyPromise.set_value(true);
        });

    return readyFuture.get();
}

bool QnStorageDb::replaceChunks(
    const QString& cameraUniqueId,
    QnServer::ChunksCatalog catalog,
    const std::deque<DeviceFileCatalog::Chunk>& chunks)
{
    bool result = true;
    bool delResult = deleteRecords(cameraUniqueId, catalog, -1);

    for (const auto &chunk : chunks)
    {
        bool addResult = addRecord(cameraUniqueId, catalog, chunk);
        result = result && addResult;
    }
    return result && delResult;
}

bool QnStorageDb::open(const QString& fileName)
{
    return createDatabase(fileName);
}

bool QnStorageDb::resetIoDevice()
{
    m_ioDevice.reset(m_storage->open(m_dbFileName, QIODevice::ReadWrite));
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

    nx::media_db::MediaDbReader mediaDbReader(m_ioDevice.get());
    uint8_t dbVersion;
    if (!mediaDbReader.readFileHeader(&dbVersion))
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
            result << DeviceFileCatalogPtr(new DeviceFileCatalog(
                serverModule(),
                uniqueId, catalog,
                QnServer::StoragePool::None));
    }
}

bool QnStorageDb::startDbFile()
{
    m_ioDevice.reset();
    m_storage->removeFile(m_dbFileName);

    if (!resetIoDevice())
        return false;

    if (!nx::media_db::MediaDbWriter::writeFileHeader(m_ioDevice.get(), kDbVersion))
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

    startVacuum(
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
        },
        &result);

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

bool QnStorageDb::parseDb(QByteArray* fileContent)
{
    using namespace nx::media_db;

    QBuffer fileBuffer(fileContent);
    fileBuffer.open(QIODevice::ReadOnly);
    MediaDbReader mediaDbReader(&fileBuffer);

    uint8_t dbVersion;
    if (!mediaDbReader.readFileHeader(&dbVersion))
    {
        NX_WARNING(this, "Parse DB: read header failed");
        return false;
    }

    m_getTimeZoneFunc = dbVersion == 1
        ? &MediaFileOperation::getTimeZoneV1 : &MediaFileOperation::getTimeZoneV2;
    m_getStartTimeFunc = dbVersion == 1
        ? &MediaFileOperation::getStartTimeV1 : &MediaFileOperation::getStartTimeV2;

    struct ReadVisitor: public boost::static_visitor<>
    {
        ReadVisitor(QnStorageDb* storageDb): m_storageDb(storageDb)
        {
        }

        void operator()(const boost::blank& blank)
        {
            NX_DEBUG(this, "Unknown record type");
        }

        void operator()(const nx::media_db::CameraOperation& cameraOp)
        {
            QString cameraUniqueId = cameraOp.getCameraUniqueId();
            auto uuidIt = m_storageDb->m_readUuidToHash.left.find(cameraUniqueId);

            if (uuidIt == m_storageDb->m_readUuidToHash.left.end())
            {
                m_storageDb->m_readUuidToHash.insert(
                    UuidToHash::value_type(cameraUniqueId, cameraOp.getCameraId()));
            }
        }

        void operator()(const nx::media_db::MediaFileOperation& fileOp)
        {
            uint16_t cameraId = fileOp.getCameraId();
            auto cameraUuidIt = m_storageDb->m_readUuidToHash.right.find(cameraId);
            auto opType = fileOp.getRecordType();
            auto opCatalog = fileOp.getCatalog();

            // camera with this ID should have already been found
            NX_ASSERT(cameraUuidIt != m_storageDb->m_readUuidToHash.right.end());
            if (cameraUuidIt == m_storageDb->m_readUuidToHash.right.end())
            {
                NX_WARNING(
                    this,
                    lit("%1 Got media file with unknown camera ID. Skipping.")
                        .arg(Q_FUNC_INFO));
                return;
            }

            auto existCameraIt = m_storageDb->m_readData.find(cameraUuidIt->second);
            bool emplaceSuccess;
            if (existCameraIt == m_storageDb->m_readData.cend())
            {
                std::tie(existCameraIt, emplaceSuccess) = m_storageDb->m_readData.emplace(
                    cameraUuidIt->second,
                    LowHiChunksCatalogs());
            }

            int catalogIndex = opCatalog == QnServer::ChunksCatalog::LowQualityCatalog ? 0 : 1;
            ChunkSet *currentChunkSet = &existCameraIt->second[catalogIndex];

            DeviceFileCatalog::Chunk newChunk(
                DeviceFileCatalog::Chunk(
                    (fileOp.*(m_storageDb->m_getStartTimeFunc))(),
                    m_storageDb-> m_storageIndex,
                    fileOp.getFileTypeIndex(),
                    fileOp.getDuration(),
                    (fileOp.*(m_storageDb->m_getTimeZoneFunc))(),
                    (quint16)(fileOp.getFileSize() >> 32),
                    (quint32)fileOp.getFileSize()));

            switch (opType)
            {
                case nx::media_db::RecordType::FileOperationAdd:
                {
                    auto existChunk = currentChunkSet->find(newChunk);
                    if (existChunk == currentChunkSet->cend())
                        currentChunkSet->insert(newChunk);
                    else
                    {
                        currentChunkSet->erase(existChunk);
                        currentChunkSet->insert(newChunk);
                    }
                    break;
                }
                case nx::media_db::RecordType::FileOperationDelete:
                {
                    if (newChunk.startTimeMs == -1)
                        currentChunkSet->clear();
                    else
                        currentChunkSet->erase(newChunk);
                    break;
                }
                default:
                    NX_ASSERT(false);
                    NX_WARNING(this, lit("%1 Unknown record type.").arg(Q_FUNC_INFO));
                    break;
            }
        }

    private:
        QnStorageDb* m_storageDb = nullptr;
    };

    int64_t recordCount = 0;
    ReadVisitor visitor(this);

    while (true)
    {
        const auto dbRecord = mediaDbReader.readRecord();
        if (dbRecord.which() == 0) //< boost::blank
            break;

        boost::apply_visitor(visitor, dbRecord);
        if (++recordCount % 100000 == 0)
        {
            NX_VERBOSE(
                this,
                lm("[vacuum] %1 records read from %2").args(recordCount, m_dbFileName));
        }
    }

    return true;
}

bool QnStorageDb::vacuum(QVector<DeviceFileCatalogPtr> *data)
{
    m_ioDevice.reset();
    m_readData.clear();
    m_readUuidToHash.clear();

    auto currentFileContent = dbFileContent();
    if (!measureTime(
            [this, &currentFileContent]() { return parseDb(&currentFileContent); },
            QString("Vacuum: Parse DB:")))
    {
        NX_WARNING(this, lm("Failed to parse DB file %1").args(m_dbFileName));
        startDbFile();
        return false;
    }

    if (!measureTime(
            [this]() { return writeVacuumedData(); },
            QString("Vacuum: writeVacuumedData:")))
    {
        NX_WARNING(this, lm("Failed to write vacuumed data. DB file %1").args(m_dbFileName));
        startDbFile();
        return false;
    }

    if (data)
        *data = buildReadResult();

    return true;
}

QByteArray QnStorageDb::serializeData() const
{
    QByteArray writeBuf;

    qint64 dataSize =
        m_readUuidToHash.size() * nx::media_db::CameraOperation::kSerializedRecordSize;

    for (auto it = m_readUuidToHash.right.begin(); it != m_readUuidToHash.right.end(); ++it)
        dataSize += it->second.size();

    for (auto it = m_readData.cbegin(); it != m_readData.cend(); ++it)
    {
        for (size_t i = 0; i < kCatalogsCount; ++i)
            dataSize += it->second[i].size() *  nx::media_db::MediaFileOperation::kSerializedRecordSize;
    }

    writeBuf.resize(dataSize);
    char* dst = writeBuf.data();

    for (auto it = m_readUuidToHash.right.begin(); it != m_readUuidToHash.right.end(); ++it)
    {
        nx::media_db::CameraOperation camOp;
        camOp.setCameraId(it->first);
        camOp.setCameraUniqueId(QByteArray(it->second.toLatin1().constData(),
            it->second.size()));
        camOp.setRecordType(nx::media_db::RecordType::CameraOperationAdd);
        camOp.setCameraUniqueIdLen(it->second.size());

        AV_WB64(&dst, camOp.part1);
        AV_WRITE_BUFFER(&dst, camOp.cameraUniqueId.data(), it->second.size());
    }

    for (auto it = m_readData.cbegin(); it != m_readData.cend(); ++it)
    {
        const auto cameraIdIt = m_readUuidToHash.left.find(it->first);
        NX_ASSERT(cameraIdIt != m_readUuidToHash.left.end());
        if (cameraIdIt == m_readUuidToHash.left.end())
        {
            NX_DEBUG(this, lit("[media_db] camera id %1 not found in UuidToHash map").arg(it->first));
            continue;
        }

        for (size_t i = 0; i < kCatalogsCount; ++i)
        {
            for (auto chunkIt = it->second[i].cbegin(); chunkIt != it->second[i].cend(); ++chunkIt)
            {
                nx::media_db::MediaFileOperation mediaFileOp;
                mediaFileOp.setRecordTypeUnsafe(nx::media_db::RecordType::FileOperationAdd);
                mediaFileOp.setCameraIdUnsafe(cameraIdIt->second);
                mediaFileOp.setCatalogUnsafe(i == 0 ? QnServer::ChunksCatalog::LowQualityCatalog :
                    QnServer::ChunksCatalog::HiQualityCatalog);

                mediaFileOp.setStartTimeUnsafe(chunkIt->startTimeMs);
                mediaFileOp.setDurationUnsafe(chunkIt->durationMs);
                mediaFileOp.setTimeZoneUnsafe(chunkIt->timeZone);
                mediaFileOp.setFileSizeUnsafe(chunkIt->getFileSize());
                mediaFileOp.setFileTypeIndexUnsafe(chunkIt->fileIndex);

                AV_WB64(&dst, mediaFileOp.part1);
                AV_WB64(&dst, mediaFileOp.part2);
            }
        }
    }
    writeBuf.truncate(dst - writeBuf.data());
    return writeBuf;
}

bool QnStorageDb::writeVacuumedData()
{
    using namespace std::chrono;

    nx::utils::ElapsedTimer timer;
    timer.restart();

    NX_DEBUG(this, "QnStorageDb::writeVacuumedData() begin");
    QByteArray writeBuf = serializeData();
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

    m_ioDevice->write(writeBuf.constData(), writeBuf.size());
    NX_DEBUG(
        this,
        "QnStorageDb::writeVacuumedData write to disk finished. time = %1 ms", timer.elapsedMs());
    return true;
}

bool QnStorageDb::checkDataConsistency(const UuidToCatalogs &readDataCopy) const
{
    for (auto it = readDataCopy.cbegin(); it != readDataCopy.cend(); ++it)
    {
        auto otherIt = m_readData.find(it->first);
        if (otherIt == m_readData.cend())
        {
            if (!it->second[0].empty() || !it->second[1].empty())
                return false;
            else
                continue;
        }
        for (size_t i = 0; i < 2; ++i)
        {
            if (otherIt->second[i] != it->second[i])
                return false;
        }
    }
    return true;
}

QVector<DeviceFileCatalogPtr> QnStorageDb::buildReadResult() const
{
    QVector<DeviceFileCatalogPtr> result;
    for (auto it = m_readData.cbegin(); it != m_readData.cend(); ++it)
    {
        DeviceFileCatalogPtr newFileCatalog(new DeviceFileCatalog(
            serverModule(),
            it->first,
            QnServer::ChunksCatalog::LowQualityCatalog,
            QnServer::StoragePool::None));

        newFileCatalog->assignChunksUnsafe(it->second[0].cbegin(), it->second[0].cend());
        result.push_back(newFileCatalog);

        newFileCatalog = DeviceFileCatalogPtr(new DeviceFileCatalog(
            serverModule(),
            it->first,
            QnServer::ChunksCatalog::HiQualityCatalog,
            QnServer::StoragePool::None));

        newFileCatalog->assignChunksUnsafe(it->second[1].cbegin(), it->second[1].cend());
        result.push_back(newFileCatalog);
    }
    return result;
}
