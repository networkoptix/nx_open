#include "storage_db.h"
#include "qsqlquery.h"
#include <QtSql>

static const int COMMIT_INTERVAL = 1000 * 60 * 5;

QnStorageDb::QnStorageDb(int storageIndex):
    QnDbHelper(),
    m_storageIndex(storageIndex)
{
    m_lastTranTime.restart();
}

QnStorageDb::~QnStorageDb()
{
}

bool QnStorageDb::deleteRecords(const QByteArray& mac, QnServer::ChunksCatalog catalog, qint64 startTimeMs)
{
    QSqlQuery query(m_sdb);
    if (startTimeMs != -1)
        query.prepare("DELETE FROM storage_data WHERE unique_id = ? and role = ? and start_time = ?");
    else
        query.prepare("DELETE FROM storage_data WHERE unique_id = ? and role = ?");
    query.addBindValue(mac);
    query.addBindValue((int) catalog);
    if (startTimeMs != -1)
        query.addBindValue(startTimeMs);
    if (!query.exec())
    {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return false;
    }
    return true;
}

void QnStorageDb::addRecord(const QByteArray& mac, QnServer::ChunksCatalog catalog, const DeviceFileCatalog::Chunk& chunk)
{
    if (chunk.durationMs <= 0)
        return;

    if (m_lastTranTime.elapsed() < COMMIT_INTERVAL) 
    {
        m_delayedData << DelayedData(mac, catalog, chunk);
        return;
    }
    flushRecords();

}

void QnStorageDb::flushRecords()
{
    beginTran();
    foreach(const DelayedData& data, m_delayedData)
        addRecordInternal(data.mac, data.catalog, data.chunk);
    commit();
    m_lastTranTime.restart();
    m_delayedData.clear();
}

bool QnStorageDb::addRecordInternal(const QByteArray& mac, QnServer::ChunksCatalog catalog, const DeviceFileCatalog::Chunk& chunk)
{
    QSqlQuery query(m_sdb);
    query.prepare("INSERT OR REPLACE INTO storage_data values(?,?,?,?,?,?,?)");

    query.addBindValue(mac); // unique_id
    query.addBindValue(catalog); // role
    query.addBindValue(chunk.startTimeMs); // start_time
    query.addBindValue(chunk.timeZone); // timezone
    query.addBindValue(chunk.fileIndex); // file_index
    query.addBindValue(chunk.durationMs); // duration
    query.addBindValue(chunk.getFileSize()); // filesize

    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return false;
    }
    return true;
}

bool QnStorageDb::replaceChunks(const QByteArray& mac, QnServer::ChunksCatalog catalog, const QVector<DeviceFileCatalog::Chunk>& chunks)
{
    beginTran();

    QSqlQuery query(m_sdb);
    query.prepare("DELETE FROM storage_data WHERE unique_id = ? AND role = ?");
    query.addBindValue(mac);
    query.addBindValue(catalog);

    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        rollback();
        return false;
    }

    foreach(const DeviceFileCatalog::Chunk& chunk, chunks)
    {
        if (!addRecordInternal(mac, catalog, chunk)) {
            rollback();
            return false;
        }
    }

    commit();
    return true;
}

bool QnStorageDb::open(const QString& fileName)
{
    m_sdb = QSqlDatabase::addDatabase("QSQLITE", QString("QnStorageManager_%1").arg(fileName));
    m_sdb.setDatabaseName(fileName);
    return m_sdb.open() && createDatabase();
}

bool QnStorageDb::createDatabase()
{
    beginTran();
    if (!isObjectExists(lit("table"), lit("storage_data")))
    {
        if (!execSQLFile(QLatin1String(":/create_storage_db.sql"))) {
            rollback();
            return false;
        }
    }
    commit();

    m_lastTranTime.restart();
    return true;
}


QVector<DeviceFileCatalogPtr> QnStorageDb::loadFullFileCatalog()
{
    QVector<DeviceFileCatalogPtr> result;

    QSqlQuery query(m_sdb);
    query.prepare("SELECT * FROM storage_data ORDER BY unique_id, role, start_time");
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return result;
    }
    QSqlRecord queryInfo = query.record();
    int macFieldIdx = queryInfo.indexOf("unique_id");
    int roleFieldIdx = queryInfo.indexOf("role");
    int startTimeFieldIdx = queryInfo.indexOf("start_time");
    int fileNumFieldIdx = queryInfo.indexOf("file_index");
    int timezoneFieldIdx = queryInfo.indexOf("timezone");
    int durationFieldIdx = queryInfo.indexOf("duration");
    int filesizeFieldIdx = queryInfo.indexOf("filesize");

    DeviceFileCatalogPtr fileCatalog;
    QVector<DeviceFileCatalog::Chunk> chunks;
    QnServer::ChunksCatalog prevCatalog = QnServer::ChunksCatalogCount; //should differ form all existing catalogs
    QByteArray prevMac;
    while (query.next())
    {
        QByteArray mac = query.value(macFieldIdx).toByteArray();
        QnServer::ChunksCatalog catalog = (QnServer::ChunksCatalog) query.value(roleFieldIdx).toInt();
        if (mac != prevMac || catalog != prevCatalog) 
        {
            if (fileCatalog) {
                fileCatalog->addChunks(chunks);
                result << fileCatalog;
                chunks.clear();
            }

            prevCatalog = catalog;
            prevMac = mac;
            fileCatalog = DeviceFileCatalogPtr(new DeviceFileCatalog(mac, catalog));
        }
        qint64 startTime = query.value(startTimeFieldIdx).toLongLong();
        qint64 filesize = query.value(filesizeFieldIdx).toLongLong();
        int timezone = query.value(timezoneFieldIdx).toInt();
        int fileNum = query.value(fileNumFieldIdx).toInt();
        int durationMs = query.value(durationFieldIdx).toInt();
        chunks << DeviceFileCatalog::Chunk(startTime, m_storageIndex, fileNum, durationMs, (qint16) timezone, (quint16) (filesize >> 32), (quint32) filesize);
    }
    if (fileCatalog) {
        fileCatalog->addChunks(chunks);
        result << fileCatalog;
    }

    return result;
}

void QnStorageDb::beforeDelete()
{
    beginTran();
}

void QnStorageDb::afterDelete()
{
    commit();
}
