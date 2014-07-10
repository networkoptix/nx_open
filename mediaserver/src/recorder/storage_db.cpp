#include "storage_db.h"
#include "qsqlquery.h"
#include <QtSql>

#include <core/resource/camera_bookmark.h>

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
    QMutexLocker locker(&m_mutex);

    if (chunk.durationMs <= 0)
        return;

    m_delayedData << DelayedData(mac, catalog, chunk);
    QByteArray countKey = mac + QByteArray("-") + QByteArray::number((int)catalog);
    if (m_addCount[countKey]++ > 0 &&  m_lastTranTime.elapsed() < COMMIT_INTERVAL) 
        return;

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

bool QnStorageDb::replaceChunks(const QByteArray& mac, QnServer::ChunksCatalog catalog, const std::deque<DeviceFileCatalog::Chunk>& chunks)
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
        if (chunk.durationMs == -1)
            continue;

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
    if (!isObjectExists(lit("table"), lit("storage_data"), m_sdb))
    {
        if (!execSQLFile(lit(":/01_create_storage_db.sql"), m_sdb)) {
            rollback();
            return false;
        }

        if (!execSQLFile(lit(":/02_storage_bookmarks.sql"), m_sdb)) {
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
    insQuery.prepare("INSERT OR REPLACE INTO storage_bookmark ( \
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

    QSqlQuery cleanTagQuery(m_sdb);
    cleanTagQuery.prepare("DELETE FROM storage_bookmark_tag WHERE bookmark_guid = ?");
    cleanTagQuery.addBindValue(bookmark.guid.toRfc4122());
    if (!cleanTagQuery.exec()) {
        qWarning() << Q_FUNC_INFO << cleanTagQuery.lastError().text();
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

bool QnStorageDb::deleteCameraBookmark(const QnCameraBookmark &bookmark, const QByteArray &mac) {
    QSqlQuery cleanTagQuery(m_sdb);
    cleanTagQuery.prepare("DELETE FROM storage_bookmark_tag WHERE bookmark_guid = ?");
    cleanTagQuery.addBindValue(bookmark.guid.toRfc4122());
    if (!cleanTagQuery.exec()) {
        qWarning() << Q_FUNC_INFO << cleanTagQuery.lastError().text();
        return false;
    }

    QSqlQuery cleanQuery(m_sdb);
    cleanQuery.prepare("DELETE FROM storage_bookmark WHERE guid = ?");
    cleanQuery.addBindValue(bookmark.guid.toRfc4122());
    if (!cleanQuery.exec()) {
        qWarning() << Q_FUNC_INFO << cleanQuery.lastError().text();
        return false;
    }

    return true;
}

bool QnStorageDb::getBookmarks(const QByteArray &cameraGuid, const QnCameraBookmarkSearchFilter &filter, QnCameraBookmarkList &result) {

    QString filterStr;
    QStringList bindings;

    auto addFilter = [&](const QString &text, bool skipBinding) {
        if (filterStr.isEmpty())
            filterStr = "WHERE " + text;
        else
            filterStr = filterStr + " AND " + text;
        if (!skipBinding)
            bindings.append(text.mid(text.lastIndexOf(':')));
    };

    if (!cameraGuid.isEmpty())
        addFilter("book.camera_id = :cameraGuid", false);
    if (filter.minStartTimeMs > 0)
        addFilter("startTimeMs >= :minStartTimeMs", false);
    if (filter.maxStartTimeMs < INT64_MAX)
        addFilter("startTimeMs <= :maxStartTimeMs", false);
    if (filter.minDurationMs > 0)
        addFilter("durationMs >= :minDurationMs", false);

    //TODO: #GDM #Bookmarks implement name search
    // hack because QT is unable to bind list values properly --gdm
    if (!filter.text.isEmpty())
        addFilter("tagName in ('" + filter.text + "')", true);

    QString queryStr("SELECT \
                     book.guid as guid, \
                     book.start_time as startTimeMs, \
                     book.duration as durationMs, \
                     book.name as name, \
                     book.description as description, \
                     book.timeout as timeout, \
                     tag.name as tagName \
                     FROM storage_bookmark book \
                     LEFT JOIN storage_bookmark_tag tag \
                     ON book.guid = tag.bookmark_guid " 
                     + filterStr +
                     " ORDER BY startTimeMs ASC, guid");

    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare(queryStr);
    
    auto checkedBind = [&](const QString &placeholder, const QVariant &value) {
        if (!bindings.contains(placeholder))
            return;
        query.bindValue(placeholder, value);
    };

    checkedBind(":cameraGuid", cameraGuid);
    checkedBind(":minStartTimeMs", filter.minStartTimeMs);
    checkedBind(":maxStartTimeMs", filter.maxStartTimeMs);
    checkedBind(":minDurationMs", filter.minDurationMs);

    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    QnSqlIndexMapping mapping = QnSql::mapping<QnCameraBookmark>(query);

    QSqlRecord queryInfo = query.record();
    int guidFieldIdx = queryInfo.indexOf("guid");
    int tagNameFieldIdx = queryInfo.indexOf("tagName");

    QUuid prevGuid;

    while (query.next()) {
        QUuid guid = QUuid::fromRfc4122(query.value(guidFieldIdx).toByteArray());
        if (guid != prevGuid) {
            prevGuid = guid;
            result.push_back(QnCameraBookmark());
            QnSql::fetch(mapping, query.record(), &result.back());
        }

        QString tag = query.value(tagNameFieldIdx).toString();
        if (!tag.isEmpty())
            result.back().tags.append(tag);
    }

    return true;
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
    std::deque<DeviceFileCatalog::Chunk> chunks;
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
        chunks.push_back(DeviceFileCatalog::Chunk(startTime, m_storageIndex, fileNum, durationMs, (qint16) timezone, (quint16) (filesize >> 32), (quint32) filesize));
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
    std::deque<DeviceFileCatalog::Chunk> chunks;
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
        chunks.push_back(DeviceFileCatalog::Chunk(startTime, m_storageIndex, 0, durationMs, 0, 0, 0));
    }
    if (fileCatalog) {
        fileCatalog->addChunks(chunks);
        result << fileCatalog;
    }

    return result;
}
