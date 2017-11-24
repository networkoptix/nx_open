#include <cassert>
#include <algorithm>
#include <string>
#include "storage_db.h"
#include <boost/scope_exit.hpp>

#include <plugins/storage/file_storage/file_storage_resource.h>

#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include "utils/common/util.h"

const uint8_t kDbVersion = 1;

/*	It is ok to have at most 2 read errors per storage at mediaserver start.
*	In most cases this means that we've read all data and hit the eof.
*/
const int kMaxReadErrorCount = 2;

namespace
{
const std::chrono::seconds kVacuumInterval(3600 * 24);

class VacuumHandler : public nx::media_db::DbHelperHandler
{
public:
    VacuumHandler(QnStorageDb::UuidToCatalogs &readData) : m_readData(readData) {}

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
            NX_LOG(lit("%1 temporary DB file error: %2").arg(Q_FUNC_INFO).arg((int)error), cl_logWARNING);
        m_error = error;
    }

    void handleRecordWrite(nx::media_db::Error error) override
    {
        if (error != nx::media_db::Error::NoError && error != nx::media_db::Error::Eof)
            NX_LOG(lit("%1 temporary DB file write error: %2").arg(Q_FUNC_INFO).arg((int)error), cl_logWARNING);
        m_error = error;
    }

    nx::media_db::Error getError() const { return m_error; }

private:
    QnStorageDb::UuidToCatalogs &m_readData;
    nx::media_db::Error m_error = nx::media_db::Error::NoError;
};

} // namespace <anonynous>

QnStorageDb::QnStorageDb(const QnStorageResourcePtr& s, int storageIndex):
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
                    NX_LOG(lit("[media_db] Unable to generate unique hash for camera id %1").arg(cameraUniqueId), cl_logWARNING);
                    return -1;
                }
                cameraId = nx::utils::random::number<uint16_t>();
                existHashIdIt = m_uuidToHash.right.find(cameraId);
                triesSoFar++;
            }

            NX_ASSERT(existHashIdIt == m_uuidToHash.right.end());
            if (existHashIdIt != m_uuidToHash.right.end())
                NX_LOG(lit("%1 Bad camera hash").arg(Q_FUNC_INFO), cl_logWARNING);

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
        NX_LOG(lit("[media_db, delete] camera id hash is not generated. Unable to delete"), cl_logWARNING);
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
            NX_LOG(lit("%1 DB write error").arg(Q_FUNC_INFO), cl_logWARNING);
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
        NX_LOG(
            lit("Failed to join vacuum thread: %1").arg(QString::fromStdString(e.what())),
            cl_logERROR);
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
        NX_LOG(
            lit("Failed to start vacuum thread: %1").arg(QString::fromStdString(e.what())),
            cl_logERROR);
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
                        NX_LOG(lit("Sheduled vacuum media DB on storage %1 successfull"), cl_logDEBUG1);
                    }
                    else
                    {
                        NX_LOG(lit("Sheduled vacuum media DB on storage %1 failed"), cl_logWARNING);
                    }
                });
        }
    }

    nx::media_db::MediaFileOperation mediaFileOp;
    int cameraId = getCameraIdHash(cameraUniqueId);
    if (cameraId == -1)
    {
        NX_LOG(lit("[media_db, add] camera id hash is not generated. Unable to add record"), cl_logWARNING);
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
            NX_LOG(lit("%1 DB write error").arg(Q_FUNC_INFO), cl_logWARNING);
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
        NX_LOG(lit("%1 DB file open failed").arg(Q_FUNC_INFO), cl_logWARNING);
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
            result << DeviceFileCatalogPtr(new DeviceFileCatalog(uniqueId, catalog,
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
        NX_LOG(lit("%1 write DB header failed").arg(Q_FUNC_INFO), cl_logWARNING);
        return false;
    }
    m_dbVersion = kDbVersion;

    return true;
}

QVector<DeviceFileCatalogPtr> QnStorageDb::loadChunksFileCatalog()
{
    QVector<DeviceFileCatalogPtr> result;
    NX_LOG(lit("[StorageDb] loading chunks from DB. storage: %1, file: %2")
            .arg(m_storage->getUrl())
            .arg(m_dbFileName), cl_logINFO);

    if (!vacuum(&result))
    {
        NX_LOG(lit("[StorageDb] loading chunks from DB failed. storage: %1, file: %2")
                .arg(m_storage->getUrl())
                .arg(m_dbFileName), cl_logWARNING);
        return result;
    }

    NX_LOG(lit("[StorageDb] finished loading chunks from DB. storage: %1, file: %2")
            .arg(m_storage->getUrl())
            .arg(m_dbFileName), cl_logINFO);

    return result;
}

bool QnStorageDb::vacuum(QVector<DeviceFileCatalogPtr> *data)
{
    QnMutexLocker lk(&m_readMutex);

    BOOST_SCOPE_EXIT(this_)
    {
        this_->m_dbHelper.setMode(nx::media_db::Mode::Write);
        {
            QnMutexLocker lock(&this_->m_syncMutex);
            for (const auto& uth : this_->m_readUuidToHash)
                this_->m_uuidToHash.insert(uth);
        }
    }
    BOOST_SCOPE_EXIT_END

    m_readData.clear();
    m_dbHelper.setMode(nx::media_db::Mode::Read);

    if (!resetIoDevice())
    {
        startDbFile();
        return false;
    }

    if (m_dbHelper.readFileHeader(&m_dbVersion) != nx::media_db::Error::NoError)
    {
        NX_LOG(lit("%1 read DB header failed").arg(Q_FUNC_INFO), cl_logWARNING);
        startDbFile();
        return false;
    }

    NX_ASSERT(m_dbHelper.getDevice());

    if ((!m_dbHelper.getDevice() || !m_ioDevice->isOpen()))
    {
        NX_LOG(lit("%1 DB file open error").arg(Q_FUNC_INFO), cl_logWARNING);
        startDbFile();
        return false;
    }

    nx::media_db::Error error;
    while ((error = m_dbHelper.readRecord()) == nx::media_db::Error::NoError)
    {
        QnMutexLocker lk(&m_errorMutex);
        m_lastReadError = error;
    }

    if (!vacuumInternal() && !startDbFile())
        return false;

    if (data)
        *data = buildReadResult();

    return true;
}

bool QnStorageDb::vacuumInternal()
{
    NX_LOG("QnStorageDb::vacuumInternal begin", cl_logDEBUG1);

    QString tmpDbFileName = m_dbFileName + ".tmp";
    std::unique_ptr<QIODevice> tmpFile(m_storage->open(tmpDbFileName, QIODevice::ReadWrite | QIODevice::Unbuffered));
    if (!tmpFile)
    {
        NX_LOG(lit("%1 temporary DB file open error").arg(Q_FUNC_INFO), cl_logWARNING);
        return false;
    }

    VacuumHandler vh(m_readData);
    nx::media_db::DbHelper tmpDbHelper(&vh);
    tmpDbHelper.setDevice(tmpFile.get());
    tmpDbHelper.setMode(nx::media_db::Mode::Write);

    nx::media_db::Error error = tmpDbHelper.writeFileHeader(m_dbVersion);
    if (error == nx::media_db::Error::WriteError)
    {
        NX_LOG(lit("%1 temporary DB file write header error").arg(Q_FUNC_INFO), cl_logWARNING);
        return false;
    }

    for (auto it = m_readUuidToHash.right.begin(); it != m_readUuidToHash.right.end(); ++it)
    {
        nx::media_db::CameraOperation camOp;
        camOp.setCameraId(it->first);
        camOp.setCameraUniqueId(QByteArray(it->second.toLatin1().constData(),
                                           it->second.size()));
        camOp.setRecordType(nx::media_db::RecordType::CameraOperationAdd);
        camOp.setCameraUniqueIdLen(it->second.size());

        tmpDbHelper.writeRecord(camOp);
    }

    static const size_t kCatalogsCount = 2;

    for (auto it = m_readData.cbegin(); it != m_readData.cend(); ++it)
    {
        for (size_t i = 0; i < kCatalogsCount; ++i)
        {
            for (auto chunkIt = it->second[i].cbegin(); chunkIt != it->second[i].cend(); ++chunkIt)
            {
                nx::media_db::MediaFileOperation mediaFileOp;
                auto cameraIdIt = m_readUuidToHash.left.find(it->first);
                NX_ASSERT(cameraIdIt != m_readUuidToHash.left.end());
                if (cameraIdIt == m_readUuidToHash.left.end())
                {
                    NX_LOG(lit("[media_db] camera id %1 not found in UuidToHash map").arg(it->first), cl_logDEBUG1);
                    continue;
                }

                mediaFileOp.setCameraId(cameraIdIt->second);
                mediaFileOp.setCatalog(i == 0 ? QnServer::ChunksCatalog::LowQualityCatalog :
                                       QnServer::ChunksCatalog::HiQualityCatalog);
                mediaFileOp.setDuration(chunkIt->durationMs);
                mediaFileOp.setFileSize(chunkIt->getFileSize());
                mediaFileOp.setFileTypeIndex(chunkIt->fileIndex);
                mediaFileOp.setRecordType(nx::media_db::RecordType::FileOperationAdd);
                mediaFileOp.setStartTime(chunkIt->startTimeMs);
                mediaFileOp.setTimeZone(chunkIt->timeZone);

                tmpDbHelper.writeRecord(mediaFileOp);
            }
        }
    }
    if (vh.getError() == nx::media_db::Error::WriteError)
        return false;
    tmpDbHelper.setMode(nx::media_db::Mode::Read); // flush

    m_ioDevice.reset();
    bool res = m_storage->removeFile(m_dbFileName);
    NX_ASSERT(res);
    if (!res)
        NX_LOG(lit("%1 temporary DB remove file error").arg(Q_FUNC_INFO), cl_logWARNING);

    tmpFile.reset();
    res = m_storage->renameFile(tmpDbFileName, m_dbFileName);
    //NX_ASSERT(res);
    if (!res)
        NX_LOG(lit("%1 temporary DB rename file error").arg(Q_FUNC_INFO), cl_logWARNING);

    res = resetIoDevice();
    NX_ASSERT(res);
    if (!res)
        return false;

    auto readDataCopy = m_readData;
    uint8_t dbVersion;

    m_readData.clear();
    m_dbHelper.setMode(nx::media_db::Mode::Read);
    NX_ASSERT(m_dbHelper.getDevice());

    error = m_dbHelper.readFileHeader(&dbVersion);
    NX_ASSERT(error == nx::media_db::Error::NoError);
    NX_ASSERT(dbVersion == m_dbVersion);
    if (error == nx::media_db::Error::ReadError || dbVersion != m_dbVersion)
    {
        NX_LOG(lit("%1 DB file read header error after vacuum").arg(Q_FUNC_INFO), cl_logWARNING);
        return false;
    }

    while ((error= m_dbHelper.readRecord()) == nx::media_db::Error::NoError)
    {
        QnMutexLocker lk(&m_errorMutex);
        m_lastReadError = error;
    }

    bool isDataConsistent = checkDataConsistency(readDataCopy);
    NX_ASSERT(isDataConsistent);
    if (!isDataConsistent)
    {
        NX_LOG(lit("%1 DB is not consistent after vacuum").arg(Q_FUNC_INFO), cl_logWARNING);
        return false;
    }

    NX_LOG("QnStorageDb::vacuumInternal completed successfully", cl_logDEBUG1);

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
        DeviceFileCatalogPtr newFileCatalog(new DeviceFileCatalog(it->first,
                                                                  QnServer::ChunksCatalog::LowQualityCatalog,
                                                                  QnServer::StoragePool::None));
        newFileCatalog->assignChunksUnsafe(it->second[0].cbegin(), it->second[0].cend());
        result.push_back(newFileCatalog);

        newFileCatalog = DeviceFileCatalogPtr(new DeviceFileCatalog(it->first,
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
        NX_LOG(lit("%1 Got media file with unknown camera ID. Skipping.").arg(Q_FUNC_INFO), cl_logWARNING);
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
            for (auto it = currentChunkSet->begin(); it != currentChunkSet->end();)
            {
                if (it->startTimeMs == newChunk.startTimeMs)
                    it = currentChunkSet->erase(it);
                else
                    ++it;
            }
        break;
    }
    default:
        NX_ASSERT(false);
        NX_LOG(lit("%1 Unknown record type.").arg(Q_FUNC_INFO), cl_logWARNING);
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
			NX_LOG(lit("%1 DB read error %2. Read errors count = %3")
					   .arg(Q_FUNC_INFO)
					   .arg((int)error)
					   .arg(m_readErrorCount),
				   cl_logWARNING);
			++m_readErrorCount;
		}
	}
    m_lastReadError = error;
}

void QnStorageDb::handleRecordWrite(nx::media_db::Error error)
{
    QnMutexLocker lk(&m_errorMutex);
    if (error != nx::media_db::Error::NoError && error != nx::media_db::Error::Eof)
        NX_LOG(lit("%1 DB write error: %2").arg(Q_FUNC_INFO).arg((int)error), cl_logWARNING);
    m_lastWriteError = error;
}
