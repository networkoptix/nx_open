#include <cassert>
#include <algorithm>
#include <string>
#include "storage_db.h"

#include <plugins/storage/file_storage/file_storage_resource.h>

#include <nx/utils/log/log.h>
#include "utils/common/util.h"

const uint8_t kDbVersion = 1;

namespace
{

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
        m_error = error;
    }

    void handleRecordWrite(nx::media_db::Error error) override
    {
        m_error = error;
    }

    nx::media_db::Error getError() const { return m_error; }

private:
    QnStorageDb::UuidToCatalogs &m_readData;
    nx::media_db::Error m_error;
};

} // namespace <anonynous>

QnStorageDb::QnStorageDb(const QnStorageResourcePtr& s, int storageIndex):
    m_storage(s),
    m_storageIndex(storageIndex),
    m_dbHelper(this),
    m_ioDevice(nullptr),
    m_lastReadError(nx::media_db::Error::NoError),
    m_lastWriteError(nx::media_db::Error::NoError)
{
}

QnStorageDb::~QnStorageDb()
{
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

bool QnStorageDb::deleteRecords(const QString& cameraUniqueId,
                                QnServer::ChunksCatalog catalog,
                                qint64 startTimeMs)
{
    return deleteRecordsUnsafe(cameraUniqueId, catalog, startTimeMs);
}

bool QnStorageDb::deleteRecordsUnsafe(const QString& cameraUniqueId,
                                      QnServer::ChunksCatalog catalog,
                                      qint64 startTimeMs)
{
    int cameraId;
    {
        QnMutexLocker lk(&m_syncMutex);
        auto it = m_uuidToHash.left.find(cameraUniqueId);
        if (it == m_uuidToHash.left.end())
        {
            nx::media_db::CameraOperation cameraOp;
            cameraId = fillCameraOp(cameraOp, cameraUniqueId);
            m_uuidToHash.insert(UuidToHash::value_type(cameraUniqueId, cameraId));
            m_dbHelper.writeRecordAsync(cameraOp);
        }
        else
            cameraId = it->second;
    }
    nx::media_db::MediaFileOperation mediaFileOp;
    mediaFileOp.setCameraId(cameraId);
    mediaFileOp.setCatalog(catalog);
    mediaFileOp.setStartTime(startTimeMs);
    mediaFileOp.setRecordType(nx::media_db::RecordType::FileOperationDelete);

    //NX_ASSERT(m_dbHelper.getMode() == nx::media_db::Mode::Write);
    m_dbHelper.writeRecordAsync(mediaFileOp);

    {
        QnMutexLocker lk(&m_errorMutex);
        if (m_lastWriteError != nx::media_db::Error::NoError &&
            m_lastWriteError != nx::media_db::Error::Eof)
        {
            return false;
        }
    }

    return true;
}

bool QnStorageDb::addRecord(const QString& cameraUniqueId,
                            QnServer::ChunksCatalog catalog,
                            const DeviceFileCatalog::Chunk& chunk)
{
    return addRecordUnsafe(cameraUniqueId, catalog, chunk);
}

bool QnStorageDb::addRecordUnsafe(const QString& cameraUniqueId,
                                  QnServer::ChunksCatalog catalog,
                                  const DeviceFileCatalog::Chunk& chunk)
{
    int cameraId;
    {
        QnMutexLocker lk(&m_syncMutex);
        auto it = m_uuidToHash.left.find(cameraUniqueId);
        if (it == m_uuidToHash.left.end())
        {
            nx::media_db::CameraOperation cameraOp;
            cameraId = fillCameraOp(cameraOp, cameraUniqueId);
            m_uuidToHash.insert(UuidToHash::value_type(cameraUniqueId, cameraId));
            m_dbHelper.writeRecordAsync(cameraOp);
        }
        else
            cameraId = it->second;
    }
    nx::media_db::MediaFileOperation mediaFileOp;
    mediaFileOp.setCameraId(cameraId);
    mediaFileOp.setCatalog(catalog);
    mediaFileOp.setDuration(chunk.durationMs);
    mediaFileOp.setFileSize(chunk.getFileSize());
    mediaFileOp.setRecordType(nx::media_db::RecordType::FileOperationAdd);
    mediaFileOp.setStartTime(chunk.startTimeMs);
    mediaFileOp.setTimeZone(chunk.timeZone);

    //NX_ASSERT(m_dbHelper.getMode() == nx::media_db::Mode::Write);
    m_dbHelper.writeRecordAsync(mediaFileOp);

    {
        QnMutexLocker lk(&m_errorMutex);
        if (m_lastWriteError != nx::media_db::Error::NoError &&
            m_lastWriteError != nx::media_db::Error::Eof)
        {
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
    bool delResult = deleteRecordsUnsafe(cameraUniqueId, catalog, -1);

    for (const auto &chunk : chunks)
    {
        bool addResult = addRecordUnsafe(cameraUniqueId, catalog, chunk);
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
        return false;
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
    m_dbHelper.writeFileHeader(kDbVersion);
    m_dbVersion = kDbVersion;

    return true;
}

QVector<DeviceFileCatalogPtr> QnStorageDb::loadChunksFileCatalog() 
{
    m_readData.clear();
    m_dbHelper.setMode(nx::media_db::Mode::Read);

    if (!resetIoDevice())
    {
        startDbFile();
        return QVector<DeviceFileCatalogPtr>();
    }

    if (m_dbHelper.readFileHeader(&m_dbVersion) != nx::media_db::Error::NoError)
    {
        startDbFile();
        return QVector<DeviceFileCatalogPtr>();
    }

    NX_ASSERT(m_dbHelper.getDevice());

    if ((!m_dbHelper.getDevice() || !m_ioDevice->isOpen()))
    {
        startDbFile();
        return QVector<DeviceFileCatalogPtr>();
    }

    nx::media_db::Error error;
    while ((error = m_dbHelper.readRecord()) == nx::media_db::Error::NoError)
    {
        QnMutexLocker lk(&m_errorMutex);
        m_lastReadError = error;
    }

    if (!vacuum() && !startDbFile())
        return QVector<DeviceFileCatalogPtr>();

    m_dbHelper.setMode(nx::media_db::Mode::Write);
    return buildReadResult();
}

bool QnStorageDb::vacuum()
{
    QString tmpDbFileName = m_dbFileName + ".tmp";
    std::unique_ptr<QIODevice> tmpFile(m_storage->open(tmpDbFileName,
                                                       QIODevice::ReadWrite));
    if (!tmpFile)
        return false;

    VacuumHandler vh(m_readData);
    nx::media_db::DbHelper tmpDbHelper(&vh);
    tmpDbHelper.setDevice(tmpFile.get());
    tmpDbHelper.setMode(nx::media_db::Mode::Write);

    nx::media_db::Error error = tmpDbHelper.writeFileHeader(m_dbVersion);
    if (error == nx::media_db::Error::WriteError)
        return false;

    for (auto it = m_uuidToHash.right.begin(); it != m_uuidToHash.right.end(); ++it)
    {
        nx::media_db::CameraOperation camOp;
        camOp.setCameraId(it->first);
        camOp.setCameraUniqueId(QByteArray(it->second.toLatin1().constData(),
                                           it->second.size()));
        camOp.setRecordType(nx::media_db::RecordType::CameraOperationAdd);
        camOp.setCameraUniqueIdLen(it->second.size());

        tmpDbHelper.writeRecordAsync(camOp);
    }

    for (auto it = m_readData.cbegin(); it != m_readData.cend(); ++it)
    {
        for (size_t i = 0; i < 2; ++i)
        {
            for (auto chunkIt = it->second[i].cbegin(); chunkIt != it->second[i].cend(); ++chunkIt)
            {
                nx::media_db::MediaFileOperation mediaFileOp;
                auto cameraIdIt = m_uuidToHash.left.find(it->first);
                assert(cameraIdIt != m_uuidToHash.left.end());

                mediaFileOp.setCameraId(cameraIdIt->second);
                mediaFileOp.setCatalog(i == 0 ? QnServer::ChunksCatalog::LowQualityCatalog :
                                       QnServer::ChunksCatalog::HiQualityCatalog);
                mediaFileOp.setDuration(chunkIt->durationMs);
                mediaFileOp.setFileSize(chunkIt->getFileSize());
                mediaFileOp.setRecordType(nx::media_db::RecordType::FileOperationAdd);
                mediaFileOp.setStartTime(chunkIt->startTimeMs);
                mediaFileOp.setTimeZone(chunkIt->timeZone);

                tmpDbHelper.writeRecordAsync(mediaFileOp);
            }
        }
    }
    if (vh.getError() == nx::media_db::Error::WriteError)
        return false;
    tmpDbHelper.setMode(nx::media_db::Mode::Read); // flush

    m_ioDevice.reset();
    bool res = m_storage->removeFile(m_dbFileName);
    assert(res);

    tmpFile.reset();
    res = m_storage->renameFile(tmpDbFileName, m_dbFileName);
    assert(res);

    res = resetIoDevice();
    assert(res);
    
    auto readDataCopy = m_readData;
    uint8_t dbVersion;

    m_readData.clear();
    m_dbHelper.setMode(nx::media_db::Mode::Read);
    assert(m_dbHelper.getDevice());

    error = m_dbHelper.readFileHeader(&dbVersion);
    assert(error == nx::media_db::Error::NoError);
    assert(dbVersion == m_dbVersion);
    if (error == nx::media_db::Error::ReadError || dbVersion != m_dbVersion)
        return false;

    while ((error= m_dbHelper.readRecord()) == nx::media_db::Error::NoError)
    {
        QnMutexLocker lk(&m_errorMutex);
        m_lastReadError = error;
    }

    bool isDataConsistent = checkDataConsistency(readDataCopy);
    assert(isDataConsistent);
    if (!isDataConsistent)
        return false;

    return true;
}

bool QnStorageDb::checkDataConsistency(const UuidToCatalogs &readDataCopy) const
{
    for (auto it = readDataCopy.cbegin(); it != readDataCopy.cend(); ++it)
    {
        auto otherIt = m_readData.find(it->first);
        if (otherIt == m_readData.cend())
        {
            if (it->second[0].size() != 0 || it->second[1].size() != 0)
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
    auto uuidIt = m_uuidToHash.left.find(cameraUniqueId);

    if (uuidIt == m_uuidToHash.left.end())
        m_uuidToHash.insert(UuidToHash::value_type(cameraUniqueId, cameraOp.getCameraId()));
}

void QnStorageDb::handleMediaFileOp(const nx::media_db::MediaFileOperation &mediaFileOp,
                                    nx::media_db::Error error) 
{
    {
        QnMutexLocker lk(&m_errorMutex);
        m_lastReadError = error;
    }

    if (error == nx::media_db::Error::ReadError)
        return;

    uint16_t cameraId = mediaFileOp.getCameraId();
    auto cameraUuidIt = m_uuidToHash.right.find(cameraId);
    auto opType = mediaFileOp.getRecordType();
    auto opCatalog = mediaFileOp.getCatalog();

    // camera with this ID should have already been found
    assert(cameraUuidIt != m_uuidToHash.right.end());

    auto existCameraIt = m_readData.find(cameraUuidIt->second);
    bool emplaceSuccess;
    if (existCameraIt == m_readData.cend())
        std::tie(existCameraIt, emplaceSuccess) = m_readData.emplace(cameraUuidIt->second, LowHiChunksCatalogs());

    int catalogIndex = opCatalog == QnServer::ChunksCatalog::LowQualityCatalog ? 0 : 1;
    ChunkSet *currentChunkSet = &existCameraIt->second[catalogIndex];

    DeviceFileCatalog::Chunk newChunk(
        DeviceFileCatalog::Chunk(mediaFileOp.getStartTime(), m_storageIndex,
                                 DeviceFileCatalog::Chunk::FILE_INDEX_WITH_DURATION,
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
        assert(false);
        break;
    }
}

void QnStorageDb::handleError(nx::media_db::Error error) 
{
    QnMutexLocker lk(&m_errorMutex);
    m_lastReadError = error;
}

void QnStorageDb::handleRecordWrite(nx::media_db::Error error)
{
    QnMutexLocker lk(&m_errorMutex);
    m_lastWriteError = error;
}
