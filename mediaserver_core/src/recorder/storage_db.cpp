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
    for (const auto &chunk : chunks)
    {
        bool delResult = deleteRecords(cameraUniqueId, catalog, chunk.startTimeMs);
        bool addResult = addRecord(cameraUniqueId, catalog, chunk);
        result = result && delResult && addResult;
    }
    return result;
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
    m_readResult.clear();
    m_dbHelper.setMode(nx::media_db::Mode::Read);
    assert(m_dbHelper.getDevice());

    while ((m_lastReadError = m_dbHelper.readRecord()) == nx::media_db::Error::NoError);

    m_dbHelper.setMode(nx::media_db::Mode::Write);
    return m_readResult;
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
    auto cameraIt = m_uuidToHash.right.find(cameraId);
    auto opType = mediaFileOp.getRecordType();

    // camera with this ID should have already been found
    assert(cameraIt != m_uuidToHash.right.end());

    auto resultCameraIt = std::find_if(m_readResult.cbegin(), m_readResult.cend(),
                                       [cameraIt](const DeviceFileCatalogPtr &catalog)
                                       {
                                           return catalog->cameraUniqueId() == cameraIt->second;
                                       });
    DeviceFileCatalogPtr fileCatalog;
    if (resultCameraIt != m_readResult.cend())
        fileCatalog = *resultCameraIt;
    else
    {
        //if (opType == nx::media_db::RecordType::FileOperationDelete)
        //    assert(false);

        fileCatalog = DeviceFileCatalogPtr(
            new DeviceFileCatalog(cameraIt->second, 
                                  (QnServer::ChunksCatalog)mediaFileOp.getCatalog(), 
                                  QnServer::StoragePool::None));
        m_readResult.append(fileCatalog);
    }

    if (opType == nx::media_db::RecordType::FileOperationAdd)
    {
        std::deque<DeviceFileCatalog::Chunk> tmpDeque;
        tmpDeque.push_back(DeviceFileCatalog::Chunk(mediaFileOp.getStartTime(), m_storageIndex,
                                                    DeviceFileCatalog::Chunk::FILE_INDEX_WITH_DURATION,
                                                    mediaFileOp.getDuration(), mediaFileOp.getTimeZone(),
                                                    (quint16)(mediaFileOp.getFileSize() >> 32),
                                                    (quint32)mediaFileOp.getFileSize()));
        fileCatalog->addChunks(tmpDeque);
    }
    else if (opType == nx::media_db::RecordType::FileOperationDelete)
    {
    } 
    else
        assert(false);
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
