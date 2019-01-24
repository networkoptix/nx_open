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

inline void AV_WB64(char** dst, quint64 data)
{
    quint64* dst64 = (quint64*)(*dst);
    *dst64 = qToLittleEndian(data);
    *dst += 8;
}

inline void AV_WRITE_BUFFER(char** dst, const char* src, qint64 size)
{
    memcpy(*dst, src, size);
    *dst += size;
}

const uint8_t kDbVersion = 1;

/*    It is ok to have at most 2 read errors per storage at mediaserver start.
*    In most cases this means that we've read all data and hit the eof.
*/
const int kMaxReadErrorCount = 2;

namespace {

const size_t kCatalogsCount = 2;

const std::chrono::seconds kVacuumInterval(3600 * 24);

class VacuumHandler : public nx::media_db::DbHelperHandler
{
public:
    VacuumHandler(QnStorageDb::UuidToCatalogs &readData):
        m_readData(readData)
    {}

public:
    void handleCameraOp(const nx::media_db::CameraOperation &/*cameraOp*/,
                        nx::media_db::Error /*error*/) override
    {

    }

    void handleMediaFileOp(const nx::media_db::MediaFileOperation &/*mediaFileOp*/,
                           nx::media_db::Error /*error*/) override
    {

    }

    void handleError(nx::media_db::Error error) override
    {
        if (error != nx::media_db::Error::NoError)
            NX_WARNING(this, lit("%1 temporary DB file error: %2").arg(Q_FUNC_INFO).arg((int)error));
        m_error = error;
    }

    void handleRecordWrite(nx::media_db::Error error) override
    {
        if (error != nx::media_db::Error::NoError && error != nx::media_db::Error::Eof)
            NX_WARNING(this, lit("%1 temporary DB file write error: %2").arg(Q_FUNC_INFO).arg((int)error));

        if (++m_recordCount % 1000 == 0)
            NX_VERBOSE(this, lm("[vacuum] %1 records written").args(m_recordCount));

        m_error = error;
    }

    nx::media_db::Error getError() const { return m_error; }

private:
    QnStorageDb::UuidToCatalogs &m_readData;
    nx::media_db::Error m_error = nx::media_db::Error::NoError;
    int64_t m_recordCount = 0;
};

} // namespace <anonynous>

QnStorageDb::QnStorageDb(
    QnMediaServerModule* serverModule,
    const QnStorageResourcePtr& s, int storageIndex)
    :
    nx::vms::server::ServerModuleAware(serverModule),
    m_storage(s),
    m_storageIndex(storageIndex),
    m_dbHelper(this),
    m_ioDevice(nullptr),
    m_lastReadError(nx::media_db::Error::NoError),
    m_lastWriteError(nx::media_db::Error::NoError),
    m_readErrorCount(0),
    m_gen(m_rd()),
    m_vacuumTimePoint(std::chrono::system_clock::now()),
    m_vacuumThreadRunning(false)
{
}

QnStorageDb::~QnStorageDb()
{
    if (m_vacuumThread.joinable())
        m_vacuumThread.join();
}

int QnStorageDb::fillCameraOp(nx::media_db::CameraOperation &cameraOp,
                              const QString &cameraUniqueId)
{
    int cameraId = qHash(cameraUniqueId);

    cameraOp.setRecordType(nx::media_db::RecordType::CameraOperationAdd);
    cameraOp.setCameraId(cameraId);
    cameraOp.setCameraUniqueIdLen(cameraUniqueId.size());
    cameraOp.setCameraUniqueId(QByteArray(cameraUniqueId.toLatin1().constData(),
                                          cameraUniqueId.size()));
    return cameraId;
}

int QnStorageDb::getCameraIdHash(const QString &cameraUniqueId)
{
    int cameraId;
    {
        QnMutexLocker lk(&m_syncMutex);
        auto it = m_uuidToHash.left.find(cameraUniqueId);
        if (it == m_uuidToHash.left.end())
        {
            nx::media_db::CameraOperation cameraOp;
            cameraId = fillCameraOp(cameraOp, cameraUniqueId);
            auto existHashIdIt = m_uuidToHash.right.find(cameraId);

            const int kMaxTries = 20;
            int triesSoFar = 0;
            while (existHashIdIt != m_uuidToHash.right.end())
            {
                if (triesSoFar == kMaxTries)
                {
                    NX_WARNING(this, lit("[media_db] Unable to generate unique hash for camera id %1").arg(cameraUniqueId));
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
            m_dbHelper.writeRecord(cameraOp);
        }
        else
            cameraId = it->second;
    }
    return cameraId;
}

bool QnStorageDb::deleteRecords(const QString& cameraUniqueId,
                                QnServer::ChunksCatalog catalog,
                                qint64 startTimeMs)
{
    nx::media_db::MediaFileOperation mediaFileOp;
    int cameraId = getCameraIdHash(cameraUniqueId);
    if (cameraId == -1)
    {
        NX_WARNING(this, lit("[media_db, delete] camera id hash is not generated. Unable to delete"));
        return false;
    }
    mediaFileOp.setCameraId(cameraId);
    mediaFileOp.setCatalog(catalog);
    mediaFileOp.setStartTime(startTimeMs);
    mediaFileOp.setRecordType(nx::media_db::RecordType::FileOperationDelete);

    m_dbHelper.writeRecord(mediaFileOp);

    {
        QnMutexLocker lk(&m_errorMutex);
        if (m_lastWriteError != nx::media_db::Error::NoError &&
            m_lastWriteError != nx::media_db::Error::Eof)
        {
            NX_WARNING(this, lit("%1 DB write error").arg(Q_FUNC_INFO));
            return false;
        }
    }

    return true;
}

template<typename Callback>
void QnStorageDb::startVacuumAsync(Callback callback)
{
    if (m_vacuumThreadRunning)
    {
        callback(false);
        return;
    }

    try
    {
        if (m_vacuumThread.joinable())
            m_vacuumThread.join();
    }
    catch (const std::exception& e)
    {
        NX_ERROR(this, lit("Failed to join vacuum thread: %1").arg(QString::fromStdString(e.what())));
    }

    try
    {
        m_vacuumThread = nx::utils::thread(
            [this, callback]()
            {
                m_vacuumThreadRunning = true;
                callback(vacuum());
                m_vacuumThreadRunning = false;
            });
    }
    catch (const std::exception& e)
    {
        NX_ERROR(this, lit("Failed to start vacuum thread: %1").arg(QString::fromStdString(e.what())));
        m_vacuumThread = nx::utils::thread();
    }
}

bool QnStorageDb::addRecord(const QString& cameraUniqueId,
                            QnServer::ChunksCatalog catalog,
                            const DeviceFileCatalog::Chunk& chunk)
{
    {
        QnMutexLocker lk(&m_vacuumMutex);
        auto timeSinceLastVacuum =
            std::chrono::duration_cast<std::chrono::seconds>
                (std::chrono::system_clock::now() - m_vacuumTimePoint);

        if (timeSinceLastVacuum > kVacuumInterval)
        {
            m_vacuumTimePoint = std::chrono::system_clock::now();
            startVacuumAsync(
                [](bool result)
                {
                    if (result)
                    {
                        NX_DEBUG(typeid(QnStorageDb), lit("Sheduled vacuum media DB on storage %1 successfull"));
                    }
                    else
                    {
                        NX_WARNING(typeid(QnStorageDb), lit("Sheduled vacuum media DB on storage %1 failed"));
                    }
                });
        }
    }

    nx::media_db::MediaFileOperation mediaFileOp;
    int cameraId = getCameraIdHash(cameraUniqueId);
    if (cameraId == -1)
    {
        NX_WARNING(this, lit("[media_db, add] camera id hash is not generated. Unable to add record"));
        return false;
    }
    mediaFileOp.setCameraId(cameraId);
    mediaFileOp.setCatalog(catalog);
    mediaFileOp.setDuration(chunk.durationMs);
    mediaFileOp.setFileTypeIndex(chunk.fileIndex);
    mediaFileOp.setFileSize(chunk.getFileSize());
    mediaFileOp.setRecordType(nx::media_db::RecordType::FileOperationAdd);
    mediaFileOp.setStartTime(chunk.startTimeMs);
    mediaFileOp.setTimeZone(chunk.timeZone);

    m_dbHelper.writeRecord(mediaFileOp);

    {
        QnMutexLocker lk(&m_errorMutex);
        if (m_lastWriteError != nx::media_db::Error::NoError &&
            m_lastWriteError != nx::media_db::Error::Eof)
        {
            NX_WARNING(this, lit("%1 DB write error").arg(Q_FUNC_INFO));
            return false;
        }
    }

    return true;
}

bool QnStorageDb::replaceChunks(const QString& cameraUniqueId,
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
    m_ioDevice.reset(m_storage->open(m_dbFileName, QIODevice::ReadWrite | QIODevice::Unbuffered));
    m_dbHelper.setDevice(nullptr);
    if (!m_ioDevice)
    {
        NX_WARNING(this, lit("%1 DB file open failed").arg(Q_FUNC_INFO));
        return false;
    }
    m_dbHelper.setDevice(m_ioDevice.get());
    return true;
}

bool QnStorageDb::createDatabase(const QString &fileName)
{
    m_dbFileName = fileName;
    if (!resetIoDevice())
        return false;

    m_dbHelper.setMode(nx::media_db::Mode::Read);

    if (m_dbHelper.readFileHeader(&m_dbVersion) != nx::media_db::Error::NoError)
    {   // either file has just been created or unrecognized format
        return startDbFile();
    }

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

    m_dbHelper.setMode(nx::media_db::Mode::Write);
    nx::media_db::Error error = m_dbHelper.writeFileHeader(kDbVersion);

    if (error != nx::media_db::Error::NoError && error != nx::media_db::Error::Eof)
    {
        NX_WARNING(this, lit("%1 write DB header failed").arg(Q_FUNC_INFO));
        return false;
    }
    m_dbVersion = kDbVersion;

    return true;
}

QVector<DeviceFileCatalogPtr> QnStorageDb::loadChunksFileCatalog()
{
    QVector<DeviceFileCatalogPtr> result;
    NX_INFO(this, lit("[StorageDb] loading chunks from DB. storage: %1, file: %2")
            .arg(m_storage->getUrl())
            .arg(m_dbFileName));

    if (!vacuum(&result))
    {
        NX_WARNING(this, lit("[StorageDb] loading chunks from DB failed. storage: %1, file: %2")
                .arg(m_storage->getUrl())
                .arg(m_dbFileName));
        return result;
    }

    NX_INFO(this, lit("[StorageDb] finished loading chunks from DB. storage: %1, file: %2")
            .arg(m_storage->getUrl())
            .arg(m_dbFileName));

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

bool QnStorageDb::parseDbContent(QByteArray fileContent)
{
    QBuffer fileBuffer(&fileContent);
    fileBuffer.open(QIODevice::ReadOnly);
    m_dbHelper.setDevice(&fileBuffer);

    auto err = m_dbHelper.readFileHeader(&m_dbVersion);
    if (err != nx::media_db::Error::NoError)
    {
        NX_WARNING(this, lit("%1 read DB header failed").arg(Q_FUNC_INFO));
        return false;
    }

    nx::media_db::Error error;
    int64_t recordCount = 0;
    while ((error = m_dbHelper.readRecord()) == nx::media_db::Error::NoError)
    {
        if (++recordCount % 1000 == 0)
            NX_VERBOSE(this, lm("[vacuum] %1 records read from %2").args(recordCount, m_dbFileName));

        QnMutexLocker lk(&m_errorMutex);
        m_lastReadError = error;
    }

    return true;
}

bool QnStorageDb::vacuum(QVector<DeviceFileCatalogPtr> *data)
{
    QnMutexLocker lk(&m_readMutex);
    auto resetModeGuard = nx::utils::makeScopeGuard(
        [this]()
        {
            m_dbHelper.setMode(nx::media_db::Mode::Write);
            {
                QnMutexLocker lock(&m_syncMutex);
                for (const auto& uth : m_readUuidToHash)
                    m_uuidToHash.insert(uth);
            }
        });

    m_dbHelper.setMode(nx::media_db::Mode::Read);
    m_ioDevice.reset();
    m_readData.clear();

    auto currentFileContent = dbFileContent();
    bool res = m_storage->removeFile(m_dbFileName);
    NX_ASSERT(res);
    if (res)
    {
        if (parseDbContent(std::move(currentFileContent)) && vacuumInternal())
        {
            if (data)
                *data = buildReadResult();
            return true;
        }
    }
    else
    {
        NX_WARNING(this, lit("%1 DB remove file error").arg(Q_FUNC_INFO));
    }

    startDbFile();
    return false;
}

QByteArray QnStorageDb::serializeData() const
{
    QByteArray writeBuf;

    qint64 dataSize = nx::media_db::FileHeader::kSerializedRecordSize;
    dataSize += m_readUuidToHash.size() * nx::media_db::CameraOperation::kSerializedRecordSize;

    for (auto it = m_readUuidToHash.right.begin(); it != m_readUuidToHash.right.end(); ++it)
        dataSize += it->second.size();

    for (auto it = m_readData.cbegin(); it != m_readData.cend(); ++it)
    {
        for (size_t i = 0; i < kCatalogsCount; ++i)
            dataSize += it->second[i].size() *  nx::media_db::MediaFileOperation::kSerializedRecordSize;
    }

    writeBuf.resize(dataSize);

    nx::media_db::FileHeader fh;
    fh.setDbVersion(m_dbVersion);

    char* dst = writeBuf.data();

    AV_WB64(&dst, fh.part1);
    AV_WB64(&dst, fh.part2);

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

    nx::media_db::MediaFileOperation mediaFileOp;

    for (auto it = m_readData.cbegin(); it != m_readData.cend(); ++it)
    {
        const auto cameraIdIt = m_readUuidToHash.left.find(it->first);
        NX_ASSERT(cameraIdIt != m_readUuidToHash.left.end());
        if (cameraIdIt == m_readUuidToHash.left.end())
        {
            NX_DEBUG(this, lit("[media_db] camera id %1 not found in UuidToHash map").arg(it->first));
            continue;
        }
        mediaFileOp.setCameraId(cameraIdIt->second);

        for (size_t i = 0; i < kCatalogsCount; ++i)
        {
            mediaFileOp.setCatalog(i == 0 ? QnServer::ChunksCatalog::LowQualityCatalog :
                QnServer::ChunksCatalog::HiQualityCatalog);

            for (auto chunkIt = it->second[i].cbegin(); chunkIt != it->second[i].cend(); ++chunkIt)
            {
                mediaFileOp.setDuration(chunkIt->durationMs);
                mediaFileOp.setFileSize(chunkIt->getFileSize());
                mediaFileOp.setFileTypeIndex(chunkIt->fileIndex);
                mediaFileOp.setRecordType(nx::media_db::RecordType::FileOperationAdd);
                mediaFileOp.setStartTime(chunkIt->startTimeMs);
                mediaFileOp.setTimeZone(chunkIt->timeZone);

                AV_WB64(&dst, mediaFileOp.part1);
                AV_WB64(&dst, mediaFileOp.part2);
            }
        }
    }
    writeBuf.truncate(dst - writeBuf.data());
    return writeBuf;
}

bool QnStorageDb::vacuumInternal()
{
    using namespace std::chrono;

    nx::utils::ElapsedTimer timer;
    timer.restart();

    NX_DEBUG(this, "QnStorageDb::vacuumInternal begin");
    QByteArray writeBuf = serializeData();
    NX_DEBUG(this, "QnStorageDb::vacuumInternal completed successfully. time = %1ms", timer.elapsedMs());

    if (!resetIoDevice())
        return false;

    m_dbHelper.stream().writeRawData(writeBuf.constData(), writeBuf.size());
    NX_DEBUG(this, "QnStorageDb::vacuumInternal write to disk finished. time = %1ms", timer.elapsedMs());
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

void QnStorageDb::handleCameraOp(const nx::media_db::CameraOperation &cameraOp,
                                 nx::media_db::Error error)
{
    if (error == nx::media_db::Error::ReadError)
        return;

    QString cameraUniqueId = cameraOp.getCameraUniqueId();
    auto uuidIt = m_readUuidToHash.left.find(cameraUniqueId);

    if (uuidIt == m_readUuidToHash.left.end())
        m_readUuidToHash.insert(UuidToHash::value_type(cameraUniqueId, cameraOp.getCameraId()));
}

void QnStorageDb::handleMediaFileOp(const nx::media_db::MediaFileOperation &mediaFileOp,
                                    nx::media_db::Error error)
{
    if (error == nx::media_db::Error::ReadError)
        return;

    uint16_t cameraId = mediaFileOp.getCameraId();
    auto cameraUuidIt = m_readUuidToHash.right.find(cameraId);
    auto opType = mediaFileOp.getRecordType();
    auto opCatalog = mediaFileOp.getCatalog();

    // camera with this ID should have already been found
    NX_ASSERT(cameraUuidIt != m_readUuidToHash.right.end());
    if (cameraUuidIt == m_readUuidToHash.right.end())
    {
        NX_WARNING(this, lit("%1 Got media file with unknown camera ID. Skipping.").arg(Q_FUNC_INFO));
        return;
    }

    auto existCameraIt = m_readData.find(cameraUuidIt->second);
    bool emplaceSuccess;
    if (existCameraIt == m_readData.cend())
        std::tie(existCameraIt, emplaceSuccess) = m_readData.emplace(cameraUuidIt->second, LowHiChunksCatalogs());

    int catalogIndex = opCatalog == QnServer::ChunksCatalog::LowQualityCatalog ? 0 : 1;
    ChunkSet *currentChunkSet = &existCameraIt->second[catalogIndex];

    DeviceFileCatalog::Chunk newChunk(
        DeviceFileCatalog::Chunk(mediaFileOp.getStartTime(), m_storageIndex,
                                 mediaFileOp.getFileTypeIndex(),
                                 mediaFileOp.getDuration(), mediaFileOp.getTimeZone(),
                                 (quint16)(mediaFileOp.getFileSize() >> 32),
                                 (quint32)mediaFileOp.getFileSize()));
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

void QnStorageDb::handleError(nx::media_db::Error error)
{
    QnMutexLocker lk(&m_errorMutex);
    if (error != nx::media_db::Error::NoError && error != nx::media_db::Error::Eof)
    {
        if (error == nx::media_db::Error::ReadError && m_readErrorCount >= kMaxReadErrorCount)
        {
            NX_WARNING(this, lit("%1 DB read error %2. Read errors count = %3")
                       .arg(Q_FUNC_INFO)
                       .arg((int)error)
                       .arg(m_readErrorCount));
            ++m_readErrorCount;
        }
    }
    m_lastReadError = error;
}

void QnStorageDb::handleRecordWrite(nx::media_db::Error error)
{
    QnMutexLocker lk(&m_errorMutex);
    if (error != nx::media_db::Error::NoError && error != nx::media_db::Error::Eof)
        NX_WARNING(this, lit("%1 DB write error: %2").arg(Q_FUNC_INFO).arg((int)error));
    m_lastWriteError = error;
}
