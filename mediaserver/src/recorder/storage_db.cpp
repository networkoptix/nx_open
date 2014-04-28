#include "storage_db.h"
#include "qsqlquery.h"
#include <QtSql>

#include <utils/serialization/sql.h>

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
        if (!execSQLFile(lit(":/01_create_storage_db.sql"))) {
            rollback();
            return false;
        }

        if (!execSQLFile(lit(":/02_storage_bookmarks.sql"))) {
            rollback();
            return false;
        }
    }
    commit();

    m_lastTranTime.restart();
    return true;
}


QVector<DeviceFileCatalogPtr> QnStorageDb::loadFullFileCatalog() {
    QVector<DeviceFileCatalogPtr> result;
    result << loadChunksFileCatalog();
    result << loadBookmarksFileCatalog();
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

bool QnStorageDb::removeCameraBookmarks(const QByteArray &mac) {
    {
        QSqlQuery delQuery(m_sdb);
        delQuery.prepare("DELETE FROM storage_bookmark_tag WHERE bookmark_guid IN (SELECT guid from storage_bookmark WHERE camera_id = :id)");
        delQuery.bindValue(":id", mac);
        if (!delQuery.exec()) {
            qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
            return false;
        }
    }

    {
        QSqlQuery delQuery(m_sdb);
        delQuery.prepare("DELETE FROM storage_bookmark WHERE camera_id = :id");
        delQuery.bindValue(":id", mac);
        if (!delQuery.exec()) {
            qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
            return false;
        }
    }

    return true;
}

bool QnStorageDb::addOrUpdateCameraBookmark(const QnCameraBookmark& bookmark, const QByteArray &mac) {
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT INTO storage_bookmark ( \
                     guid, camera_id, start_time, duration, \
                     name, description, timeout \
                     ) VALUES ( \
                     :guid, :cameraId, :startTimeMs, :durationMs, \
                     :name, :description, :timeout \
                     )");
    QnSql::bind(bookmark, &insQuery);
    insQuery.bindValue(":cameraId", mac); // unique_id
    if (!insQuery.exec()) {
        qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
        return false;
    }

    QSqlQuery tagQuery(m_sdb);
    tagQuery.prepare("INSERT INTO storage_bookmark_tag ( bookmark_guid, name ) VALUES ( :bookmark_guid, :name )");

    tagQuery.bindValue(":bookmark_guid", bookmark.guid.toRfc4122());
    for (const QString tag: bookmark.tags) {
        tagQuery.bindValue(":name", tag);
        if (!tagQuery.exec()) {
            qWarning() << Q_FUNC_INFO << tagQuery.lastError().text();
            return false;
        }
    }

    return true;
}

QList<QnCameraBookmark> QnStorageDb::getBookmarks(const QByteArray &mac)
{
    QList<QnCameraBookmark> result;
    return result;
    /*
    {   // load tags

        QSqlQuery queryTags(m_sdb);
        queryTags.setForwardOnly(true);
        queryTags.prepare("SELECT \
                          tag.bookmark_guid as bookmarkGuid, tag.name \
                          FROM storage_bookmark_tag tag");
        if (!queryTags.exec()) {
            qWarning() << Q_FUNC_INFO << queryTags.lastError().text();
            return result;
        }
        

        // load bookmarks
        QSqlQuery queryBookmarks(m_sdb);
        queryBookmarks.setForwardOnly(true);
        queryBookmarks.prepare("SELECT \
                               r.guid as cameraId, \
                               bookmark.guid, bookmark.start_time as startTime, bookmark.duration, \
                               bookmark.name, bookmark.description, bookmark.timeout \
                               FROM vms_bookmark bookmark \
                               JOIN vms_resource r on r.id = bookmark.camera_id");
        if (!queryBookmarks.exec()) {
            qWarning() << Q_FUNC_INFO << queryBookmarks.lastError().text();
            return ErrorCode::failure;
        }


        {   // merge tags
            foreach(const ApiCameraBookmarkTag& tag, tags) {
                int idx = 0;
                for (; idx < bookmarks.size() && tag.bookmarkGuid != bookmarks[idx].guid; idx++);
                if (idx == bookmarks.size())
                    break;
                bookmarks[idx].tags.push_back(tag.name);
            }
        }

        
    }*/
}

QVector<DeviceFileCatalogPtr> QnStorageDb::loadChunksFileCatalog() {
    QVector<DeviceFileCatalogPtr> result;

    QSqlQuery query(m_sdb);
    query.prepare("SELECT * FROM storage_data WHERE role <= :max_role ORDER BY unique_id, role, start_time");
    query.bindValue(":max_role", (int)QnServer::HiQualityCatalog);

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
    QnServer::ChunksCatalog prevCatalog = QnServer::ChunksCatalogCount; //should differ from all existing catalogs
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

QVector<DeviceFileCatalogPtr> QnStorageDb::loadBookmarksFileCatalog() {
    QVector<DeviceFileCatalogPtr> result;

    QSqlQuery query(m_sdb);
    query.prepare("SELECT camera_id, start_time, duration FROM storage_bookmark ORDER BY camera_id, start_time");
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return result;
    }
    QSqlRecord queryInfo = query.record();
    int macFieldIdx = queryInfo.indexOf("camera_id");
    int startTimeFieldIdx = queryInfo.indexOf("start_time");
    int durationFieldIdx = queryInfo.indexOf("duration");

    DeviceFileCatalogPtr fileCatalog;
    QVector<DeviceFileCatalog::Chunk> chunks;
    QByteArray prevMac;
    while (query.next())
    {
        QByteArray mac = query.value(macFieldIdx).toByteArray();
        if (mac != prevMac) 
        {
            if (fileCatalog) {
                fileCatalog->addChunks(chunks);
                result << fileCatalog;
                chunks.clear();
            }
            prevMac = mac;
            fileCatalog = DeviceFileCatalogPtr(new DeviceFileCatalog(mac, QnServer::BookmarksCatalog));
        }
        qint64 startTime = query.value(startTimeFieldIdx).toLongLong();
        int durationMs = query.value(durationFieldIdx).toInt();
        chunks << DeviceFileCatalog::Chunk(startTime, m_storageIndex, 0, durationMs, 0, 0, 0);
    }
    if (fileCatalog) {
        fileCatalog->addChunks(chunks);
        result << fileCatalog;
    }

    return result;
}
