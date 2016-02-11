#include "storage_db.h"

#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>


#include <core/resource/storage_plugin_factory.h>
#include <plugins/storage/file_storage/file_storage_resource.h>

#include <utils/serialization/sql.h>
#include <nx/utils/log/log.h>
#include "utils/common/util.h"

static const int COMMIT_INTERVAL = 1000 * 60 * 1;

QnStorageDb::QnStorageDb(const QnStorageResourcePtr& s, int storageIndex):
    QnDbHelper(),
    m_storage(s),
    m_storageIndex(storageIndex),
    m_tran(m_sdb, m_mutex),
    m_needReopenDB(false)
{
    m_lastTranTime.restart();
}

QnStorageDb::~QnStorageDb()
{
    flushRecords();
}

bool QnStorageDb::deleteRecords(const QString& cameraUniqueId, QnServer::ChunksCatalog catalog, qint64 startTimeMs)
{
    QnMutexLocker lock( &m_syncMutex );
    m_recordsToDelete << DeleteRecordInfo(cameraUniqueId, catalog, startTimeMs);
    return true;
}


bool QnStorageDb::deleteRecordsInternal(const DeleteRecordInfo& delRecord)
{
    QSqlQuery query(m_sdb);
    if (delRecord.startTimeMs != -1)
        query.prepare("DELETE FROM storage_data WHERE unique_id = ? and role = ? and start_time = ?");
    else
        query.prepare("DELETE FROM storage_data WHERE unique_id = ? and role = ?");
    query.addBindValue(delRecord.cameraUniqueId);
    query.addBindValue((int) delRecord.catalog);
    if (delRecord.startTimeMs != -1)
        query.addBindValue(delRecord.startTimeMs);
    if (!query.exec())
    {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        m_needReopenDB = true;
        return false;
    }
    return true;
}

bool QnStorageDb::addRecord(const QString& cameraUniqueId, QnServer::ChunksCatalog catalog, const DeviceFileCatalog::Chunk& chunk)
{
    QnMutexLocker locker( &m_syncMutex );

    if (chunk.durationMs <= 0)
        return true;

    m_delayedData << DelayedData(cameraUniqueId, catalog, chunk);
    if (m_lastTranTime.elapsed() < COMMIT_INTERVAL)
        return true;

    return flushRecordsNoLock();
}

bool QnStorageDb::flushRecords()
{
    QnMutexLocker locker( &m_syncMutex );
    return flushRecordsNoLock();
}

bool QnStorageDb::flushRecordsNoLock()
{
    QnDbTransactionLocker tran(getTransaction());
    for(const DelayedData& data: m_delayedData)
        if( !addRecordInternal(data.cameraUniqueId, data.catalog, data.chunk) )
            return false;

    for(const DeleteRecordInfo& delRecord: m_recordsToDelete) {
        if (!deleteRecordsInternal(delRecord)) {
            return false; // keep record list to delete. try to the next time
        }
    }

    if( !tran.commit() )
    {
        qWarning() << Q_FUNC_INFO << m_sdb.lastError().text();
        m_needReopenDB = true;
        return false;
    }
    m_lastTranTime.restart();
    m_delayedData.clear();
    m_recordsToDelete.clear();
    return true;
}

bool QnStorageDb::addRecordInternal(const QString& cameraUniqueId, QnServer::ChunksCatalog catalog, const DeviceFileCatalog::Chunk& chunk)
{
    QSqlQuery query(m_sdb);
    query.prepare("INSERT OR REPLACE INTO storage_data values(?,?,?,?,?,?,?)");

    query.addBindValue(cameraUniqueId); // unique_id
    query.addBindValue(catalog); // role
    query.addBindValue(chunk.startTimeMs); // start_time
    query.addBindValue(chunk.timeZone); // timezone
    query.addBindValue(chunk.fileIndex); // file_index
    query.addBindValue(chunk.durationMs); // duration
    query.addBindValue(chunk.getFileSize()); // filesize

    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        m_needReopenDB = true;
        return false;
    }

    return true;
}

bool QnStorageDb::replaceChunks(const QString& cameraUniqueId, QnServer::ChunksCatalog catalog, const std::deque<DeviceFileCatalog::Chunk>& chunks)
{
    QnDbTransactionLocker tran(getTransaction());

    QSqlQuery query(m_sdb);
    query.prepare("DELETE FROM storage_data WHERE unique_id = ? AND role = ?");
    query.addBindValue(cameraUniqueId);
    query.addBindValue(catalog);

    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        m_needReopenDB = true;
        return false;
    }

    for(const DeviceFileCatalog::Chunk& chunk: chunks)
    {
        if (chunk.durationMs == -1)
            continue;

        if (!addRecordInternal(cameraUniqueId, catalog, chunk)) {
            return false;
        }
    }

    if( !tran.commit() )
    {
        qWarning() << Q_FUNC_INFO << m_sdb.lastError().text();
        m_needReopenDB = true;
        return false;
    }
    return true;
}

bool QnStorageDb::open(const QString& fileName)
{
    // TODO: #akulikov If storage is DBReady, DB should work via Storage::IODevice.
    //       But this change requires DB implementation to be able to work via IODevice, and this is not the case right now.
    addDatabase(fileName, QString("QnStorageManager_%1").arg(fileName));
    return m_sdb.open() && createDatabase();
}

bool QnStorageDb::createDatabase()
{
    QnDbTransactionLocker tran(&m_tran);
    if (!isObjectExists(lit("table"), lit("storage_data"), m_sdb))
    {
        if (!execSQLFile(lit(":/01_create_storage_db.sql"), m_sdb)) {
            return false;
        }
    }

    if (!applyUpdates(":/storage_updates"))
        return false;

    if( !tran.commit() )
    {
        qWarning() << Q_FUNC_INFO << m_sdb.lastError().text();
        return false;
    }

    m_lastTranTime.restart();
    return true;
}

QVector<DeviceFileCatalogPtr> QnStorageDb::loadFullFileCatalog() {
    QVector<DeviceFileCatalogPtr> result;
    result << loadChunksFileCatalog();

    addCatalogFromMediaFolder(
        lit("hi_quality"),
        QnServer::HiQualityCatalog,
        result
    );

    addCatalogFromMediaFolder(
        lit("low_quality"),
        QnServer::LowQualityCatalog,
        result
    );

    return result;
}

bool isCatalogExistInResult(const QVector<DeviceFileCatalogPtr>& result, QnServer::ChunksCatalog catalog, const QString& uniqueId)
{
    for(const DeviceFileCatalogPtr& c: result)
    {
        if (c->getRole() == catalog && c->cameraUniqueId() == uniqueId)
            return true;
    }
    return false;
}

void QnStorageDb::addCatalogFromMediaFolder(
    const QString&                  postfix,
    QnServer::ChunksCatalog         catalog,
    QVector<DeviceFileCatalogPtr>&  result
)
{
    QString root = closeDirPath(QFileInfo(m_sdb.databaseName()).absoluteDir().path()) + postfix;
    QnAbstractStorageResource::FileInfoList files;
    if (m_storage)
        files = m_storage->getFileList(root);
    else
        files = QnAbstractStorageResource::FIListFromQFIList(
            QDir(root).entryInfoList(
                QDir::Dirs | QDir::NoDotAndDotDot,
                QDir::Name
            )
        );

    for (const QnAbstractStorageResource::FileInfo& fi: files)
    {
        QString uniqueId = fi.baseName();
        if (!isCatalogExistInResult(result, catalog, uniqueId)) {
            result
                << DeviceFileCatalogPtr(
                        new DeviceFileCatalog(
                            uniqueId,
                            catalog,
                            QnServer::StoragePool::None
                        )
                    );
        }
    }
}

QVector<DeviceFileCatalogPtr> QnStorageDb::loadChunksFileCatalog() {
    QnWriteLocker lock(&m_mutex);

    QVector<DeviceFileCatalogPtr> result;

    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare("SELECT * FROM storage_data WHERE role <= :max_role ORDER BY unique_id, role, start_time");
    query.bindValue(":max_role", (int)QnServer::HiQualityCatalog);

    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        m_needReopenDB = true;
        return result;
    }
    QSqlRecord queryInfo = query.record();
    int idFieldIdx = queryInfo.indexOf("unique_id");
    int roleFieldIdx = queryInfo.indexOf("role");
    int startTimeFieldIdx = queryInfo.indexOf("start_time");
    int fileNumFieldIdx = queryInfo.indexOf("file_index");
    int timezoneFieldIdx = queryInfo.indexOf("timezone");
    int durationFieldIdx = queryInfo.indexOf("duration");
    int filesizeFieldIdx = queryInfo.indexOf("filesize");

    DeviceFileCatalogPtr fileCatalog;
    std::deque<DeviceFileCatalog::Chunk> chunks;
    QnServer::ChunksCatalog prevCatalog = QnServer::ChunksCatalogCount; //should differ from all existing catalogs
    QByteArray prevId;
    while (query.next())
    {
        QByteArray id = query.value(idFieldIdx).toByteArray();
        QnServer::ChunksCatalog catalog = (QnServer::ChunksCatalog) query.value(roleFieldIdx).toInt();
        if (id != prevId || catalog != prevCatalog)
        {
            if (fileCatalog) {
                fileCatalog->addChunks(chunks);
                result << fileCatalog;
                chunks.clear();
            }

            prevCatalog = catalog;
            prevId = id;
            fileCatalog = DeviceFileCatalogPtr(
                new DeviceFileCatalog(
                    QString::fromUtf8(id),
                    catalog,
                    QnServer::StoragePool::None // It's not important here
                )
            );
        }
        qint64 startTime = query.value(startTimeFieldIdx).toLongLong();
        qint64 filesize = query.value(filesizeFieldIdx).toLongLong();
        int timezone = query.value(timezoneFieldIdx).toInt();
        int fileNum = query.value(fileNumFieldIdx).toInt();
        int durationMs = query.value(durationFieldIdx).toInt();
        chunks.push_back(DeviceFileCatalog::Chunk(startTime, m_storageIndex, fileNum, durationMs, (qint16) timezone, (quint16) (filesize >> 32), (quint32) filesize));
    }
    if (fileCatalog) {
        fileCatalog->addChunks(chunks);
        result << fileCatalog;
    }

    return result;
}

QnStorageDb::QnDbTransaction* QnStorageDb::getTransaction()
{
    if( m_needReopenDB )
    {
        if( m_sdb.open() && createDatabase())
            m_needReopenDB = false;
    }

    return &m_tran;
}
