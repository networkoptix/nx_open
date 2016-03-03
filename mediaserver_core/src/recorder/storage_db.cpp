#include <cassert>
#include <algorithm>
#include <string>
#include "storage_db.h"

#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

#include <core/resource/storage_plugin_factory.h>
#include <plugins/storage/file_storage/file_storage_resource.h>

#include <utils/serialization/sql.h>
#include <nx/utils/log/log.h>
#include "utils/common/util.h"

const uint8_t kDbVersion = 1;

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

    assert(m_dbHelper.getMode() == nx::media_db::Mode::Write);
    m_dbHelper.writeRecordAsync(mediaFileOp);

    {
        QnMutexLocker lk(&m_syncMutex);
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

    assert(m_dbHelper.getMode() == nx::media_db::Mode::Write);
    m_dbHelper.writeRecordAsync(mediaFileOp);

    {
        QnMutexLocker lk(&m_syncMutex);
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
        m_ioDevice.reset();
        m_storage->removeFile(fileName);

        if (!resetIoDevice())
            return false;

        m_dbHelper.setMode(nx::media_db::Mode::Write);
        m_dbHelper.writeFileHeader(kDbVersion);
        m_dbVersion = kDbVersion;
    }

    //m_lastTranTime.restart();
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

QVector<DeviceFileCatalogPtr> QnStorageDb::loadChunksFileCatalog() 
{
    m_readData.clear();
    m_dbHelper.setMode(nx::media_db::Mode::Read);
    assert(m_dbHelper.getDevice());

    while ((m_lastReadError = m_dbHelper.readRecord()) == nx::media_db::Error::NoError);

    m_dbHelper.setMode(nx::media_db::Mode::Write);
    return buildReadResult();
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
    if ((m_lastReadError = error) == nx::media_db::Error::ReadError)
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
    }
}

void QnStorageDb::handleError(nx::media_db::Error error) 
{
    QnMutexLocker lk(&m_syncMutex);
    m_lastReadError = error;
}

void QnStorageDb::handleRecordWrite(nx::media_db::Error error)
{
    QnMutexLocker lk(&m_syncMutex);
    m_lastWriteError = error;
}
