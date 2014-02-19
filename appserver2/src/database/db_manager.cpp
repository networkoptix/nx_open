#include "db_manager.h"
#include <QtSql/QtSql>
#include "nx_ec/data/ec2_business_rule_data.h"

namespace ec2
{

QnDbManager::QnDbTransaction::QnDbTransaction(QSqlDatabase& sdb, QReadWriteLock& mutex): 
    m_sdb(sdb),
    m_mutex(mutex)
{

}

void QnDbManager::QnDbTransaction::beginTran()
{
    m_mutex.lockForWrite();
    QSqlQuery query(m_sdb);
    //query.exec("BEGIN TRANSACTION");
}

void QnDbManager::QnDbTransaction::rollback()
{
    QSqlQuery query(m_sdb);
    //query.exec("ROLLBACK");
    m_mutex.unlock();
}

void QnDbManager::QnDbTransaction::commit()
{
    QSqlQuery query(m_sdb);
    //query.exec("COMMIT");
    m_mutex.unlock();
}

QnDbManager::QnDbTransactionLocker::QnDbTransactionLocker(QnDbTransaction* tran): 
    m_tran(tran), 
    m_committed(false)
{
    m_tran->beginTran();
}

QnDbManager::QnDbTransactionLocker::~QnDbTransactionLocker()
{
    if (!m_committed)
        m_tran->rollback();
}

void QnDbManager::QnDbTransactionLocker::commit()
{
    m_tran->commit();
    m_committed = true;
}

static QnDbManager* globalInstance = 0;


template <class MainData>
void mergeIdListData(QSqlQuery& query, std::vector<MainData>& data, std::vector<QnId> MainData::*subList)
{
    int idx = 0;
    QSqlRecord rec = query.record();
    int idIdx = rec.indexOf("id");
    int parentIdIdx = rec.indexOf("parentId");
    while (query.next())
    {
        QnId id = QnId::fromRfc4122(query.value(idIdx).toByteArray());
        QnId parentId = QnId::fromRfc4122(query.value(parentIdIdx).toByteArray());

        for (; idx < data.size() && data[idx].id != id; idx++);
        if (idx == data.size())
            break;
        (data[idx].*subList).push_back(parentId);
    }
}

template <class MainData, class SubData, class MainSubData, class IdType>
void mergeObjectListData(std::vector<MainData>& data, std::vector<SubData>& subDataList, std::vector<MainSubData> MainData::*subDataListField, IdType SubData::*parentIdField)
{
    int idx = 0;
    foreach(const SubData& subData, subDataList)
    {
        for (; idx < data.size() && subData.*parentIdField != data[idx].id; idx++);
        if (idx == data.size())
            break;
        (data[idx].*subDataListField).push_back(subData);
    }
}

QnDbManager::QnDbManager(QnResourceFactory* factory, StoredFileManagerImpl* const storedFileManagerImpl, const QString& dbFileName ):
    m_tran(m_sdb, m_mutex),
    m_storedFileManagerImpl( storedFileManagerImpl )
{
    m_resourceFactory = factory;
	m_sdb = QSqlDatabase::addDatabase("QSQLITE", "QnDbManager");
    m_sdb.setDatabaseName( dbFileName );
	//m_sdb.setDatabaseName("c:/develop/netoptix_trunk/appserver/db/ecs.db");
    //m_sdb.setDatabaseName("d:/Projects/Serious/NetworkOptix/netoptix_vms/appserver/db/ecs.db");
    //m_sdb.setDatabaseName("c:/Windows/System32/config/systemprofile/AppData/Local/Network Optix/Enterprise Controller/db/ecs.db");
    
	if (m_sdb.open())
	{
		if (!createDatabase()) // create tables is DB is empty
			qWarning() << "can't create tables for sqlLite database!";

        QSqlQuery query(m_sdb);
        query.prepare("select guid from vms_resourcetype where name = 'Storage'");
        Q_ASSERT(query.exec());
        if (query.next())
            m_storageTypeId = QnId::fromRfc4122(query.value("guid").toByteArray());
	}
	else {
		qWarning() << "can't initialize sqlLite database! Actions log is not created!";
	}

	Q_ASSERT(!globalInstance);
	globalInstance = this;
}

bool QnDbManager::isObjectExists(const QString& objectType, const QString& objectName)
{
    QSqlQuery tableList(m_sdb);
    QString request;
    request = QString(lit("SELECT * FROM sqlite_master WHERE type='%1' and name='%2'")).arg(objectType).arg(objectName);
    tableList.prepare(request);
    if (!tableList.exec())
        return false;
    int fieldNo = tableList.record().indexOf(lit("name"));
    if (!tableList.next())
        return false;
    QString value = tableList.value(fieldNo).toString();
    return !value.isEmpty();
}

QList<QByteArray> quotedSplit(const QByteArray& data)
{
    QList<QByteArray> result;
    const char* curPtr = data.data();
    const char* prevPtr = curPtr;
    const char* end = curPtr + data.size();
    bool inQuote1 = false;
    bool inQuote2 = false;
    for (;curPtr < end; ++curPtr) {
        if (*curPtr == '\'')
            inQuote1 = !inQuote1;
        else if (*curPtr == '\"')
            inQuote2 = !inQuote2;
        else if (*curPtr == ';' && !inQuote1 && !inQuote2)
        {
            //*curPtr = 0;
            result << QByteArray::fromRawData(prevPtr, curPtr - prevPtr);
            prevPtr = curPtr+1;
        }
    }

    return result;
}

bool QnDbManager::execSQLFile(const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly))
        return false;
    QByteArray data = file.readAll();
    foreach(const QByteArray& singleCommand, quotedSplit(data))
    {
        if (singleCommand.trimmed().isEmpty())
            continue;
        QSqlQuery ddlQuery(m_sdb);
        ddlQuery.prepare(singleCommand);
        if (!ddlQuery.exec()) {
            qWarning() << "can't create tables for sqlLite database:" << ddlQuery.lastError().text();;
            return false;
        }
    }
    return true;
}

QMap<int, QnId> QnDbManager::getGuidList(const QString& request)
{
    QMap<int, QnId>  result;
    QSqlQuery query(m_sdb);
    query.prepare(request);
    if (!query.exec())
        return result;

    while (query.next())
    {
        qint32 id = query.value(0).toInt();
        QVariant data = query.value(1);
        if (data.toInt())
            result.insert(id, intToGuid(data.toInt()));
        else {
            QnId guid(data.toString());
            if (guid.isNull()) {
                QCryptographicHash md5Hash( QCryptographicHash::Md5 );
                md5Hash.addData(data.toString().toUtf8());
                QByteArray ha2 = md5Hash.result();
                guid = QnId::fromRfc4122(ha2);
            }
            result.insert(id, guid);
        }
    }

    return result;
}

bool QnDbManager::updateTableGuids(const QString& tableName, const QString& fieldName, const QMap<int, QnId>& guids)
{
    for(QMap<int, QnId>::const_iterator itr = guids.begin(); itr != guids.end(); ++itr)
    {
        QSqlQuery query(m_sdb);
        query.prepare(QString("UPDATE %1 SET %2 = :guid WHERE id = :id").arg(tableName).arg(fieldName));
        query.bindValue(":id", itr.key());
        query.bindValue(":guid", itr.value().toRfc4122());
        if (!query.exec())
            return false;
    }
    return true;
}

bool QnDbManager::updateGuids()
{
    QSqlQuery query();
    QMap<int, QnId> guids = getGuidList("SELECT id, guid from vms_resource_tmp order by id");
    if (!updateTableGuids("vms_resource", "guid", guids))
        return false;

    guids = getGuidList("SELECT rt.id, rt.name || coalesce(m.name,'-') as guid from vms_resourcetype rt LEFT JOIN vms_manufacture m on m.id = rt.manufacture_id");
    if (!updateTableGuids("vms_resourcetype", "guid", guids))
        return false;

    guids = getGuidList("SELECT r.id, r2.guid from vms_resource_tmp r JOIN vms_resource_tmp r2 on r2.id = r.parent_id order by r.id");
    if (!updateTableGuids("vms_resource", "parent_guid", guids))
        return false;

    guids = getGuidList("SELECT r.id, rt.guid from vms_resource_tmp r JOIN vms_resourcetype rt on rt.id = r.xtype_id");
    if (!updateTableGuids("vms_resource", "xtype_guid", guids))
        return false;

    guids = getGuidList("SELECT id, id from vms_businessrule ORDER BY id");
    if (!updateTableGuids("vms_businessrule", "guid", guids))
        return false;

    return true;
}

bool QnDbManager::createDatabase()
{
    QnDbTransactionLocker lock(&m_tran);

    if (!isObjectExists(lit("table"), lit("vms_resource")))
    {
        if (!execSQLFile(QLatin1String(":/createdb.sql")))
            return false;
        
        if (!execSQLFile(QLatin1String(":/update_2.2_stage1.sql")))
            return false;
        if (!updateGuids())
            return false;
        if (!execSQLFile(QLatin1String(":/update_2.2_stage2.sql")))
            return false;
    }
    lock.commit();
    return true;
}

QnDbManager::~QnDbManager()
{
	globalInstance = 0;
}

QnDbManager* QnDbManager::instance()
{
    return globalInstance;
}

ErrorCode QnDbManager::insertAddParam(const ApiResourceParam& param, qint32 internalId)
{
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT INTO vms_kvpair (resource_id, name, value) VALUES(:resourceId, :name, :value)");
    insQuery.bindValue(":resourceId", internalId);
    param.autoBindValues(insQuery);
    if (insQuery.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
        return ErrorCode::failure;
    }
}

ErrorCode QnDbManager::removeAddParam(const ApiResourceParam& param)
{
    QSqlQuery delQuery(m_sdb);
    delQuery.prepare("DELETE FROM vms_kvpair where resource_id = :resourceId and name = :name");
    param.autoBindValues(delQuery);
    if (delQuery.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
        return ErrorCode::failure;
    }
}

ErrorCode QnDbManager::deleteAddParams(qint32 resourceId)
{
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("DELETE FROM vms_kvpair WHERE resource_id = :resourceId");
    insQuery.bindValue(":resourceId", resourceId);
    if (insQuery.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
        return ErrorCode::failure;
    }
}

ErrorCode QnDbManager::insertResource(const ApiResourceData& data, qint32* internalId)
{
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT INTO vms_resource (guid, xtype_guid, parent_guid, name, url, status, disabled) VALUES(:id, :typeId, :parentGuid, :name, :url, :status, :disabled)");
    data.autoBindValues(insQuery);
	if (!insQuery.exec()) {
		qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
		return ErrorCode::failure;
	}
    *internalId = insQuery.lastInsertId().toInt();

    foreach(const ApiResourceParam& param, data.addParams) {
        ErrorCode result = insertAddParam(param, *internalId);
        if (result != ErrorCode::ok)
            return result;
    }

    return ErrorCode::ok;
}

ErrorCode QnDbManager::insertOrReplaceResource(const ApiResourceData& data, qint32* internalId)
{
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT OR REPLACE INTO vms_resource (guid, xtype_guid, parent_guid, name, url, status, disabled) VALUES(:id, :typeId, :parentGuid, :name, :url, :status, :disabled)");
    data.autoBindValues(insQuery);
    if (!insQuery.exec()) {
        qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
        return ErrorCode::failure;
    }
    *internalId = insQuery.lastInsertId().toInt();

    if (!data.addParams.empty()) 
    {
        ErrorCode result = deleteAddParams(*internalId);
        if (result != ErrorCode::ok)
            return result;

        foreach(const ApiResourceParam& param, data.addParams) {
            ErrorCode result = insertAddParam(param, *internalId);
            if (result != ErrorCode::ok)
                return result;
        }
    }

    return ErrorCode::ok;
}

ErrorCode QnDbManager::updateResource(const ApiResourceData& data, qint32 internalId)
{
	QSqlQuery insQuery(m_sdb);

	insQuery.prepare("UPDATE vms_resource SET xtype_guid = :typeId, parent_guid = :parentGuid, name = :name, url = :url, status = :status, disabled = :disabled WHERE id = :id");
	data.autoBindValues(insQuery);
    insQuery.bindValue(":id", internalId);

    if (!insQuery.exec()) {
        qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
        return ErrorCode::failure;
    }

    if (!data.addParams.empty()) 
    {
        ErrorCode result = deleteAddParams(internalId);
        if (result != ErrorCode::ok)
            return result;

        foreach(const ApiResourceParam& param, data.addParams) {
            ErrorCode result = insertAddParam(param, internalId);
            if (result != ErrorCode::ok)
                return result;
        }
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::insertOrReplaceCamera(const ApiCameraData& data, qint32 internalId)
{
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT OR REPLACE INTO vms_camera (audio_enabled, control_disabled, firmware, vendor, manually_added, resource_ptr_id, region, schedule_disabled, motion_type, group_name, group_id,\
                     mac, model, secondary_quality, status_flags, physical_id, password, login, dewarping_params, resource_ptr_id) VALUES\
                     (:audioEnabled, :controlDisabled, :firmware, :vendor, :manuallyAdded, :id, :region, :scheduleDisabled, :motionType, :groupName, :groupId,\
                     :mac, :model, :secondaryQuality, :statusFlags, :physicalId, :password, :login, :dewarpingParams, :id)");
    data.autoBindValues(insQuery);
    insQuery.bindValue(":id", internalId);
    if (insQuery.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
        return ErrorCode::failure;
    }
}

ErrorCode QnDbManager::insertOrReplaceMediaServer(const ApiMediaServerData& data, qint32 internalId)
{
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT INTO vms_server (api_url, auth_key, streaming_url, version, net_addr_list, reserve, panic_mode, resource_ptr_id) VALUES\
                     (:apiUrl, :authKey, :streamingUrl, :version, :netAddrList, :reserve, :panicMode, :id)");
    data.autoBindValues(insQuery);
    insQuery.bindValue(":id", internalId);
    if (insQuery.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
        return ErrorCode::failure;
    }
}


ErrorCode QnDbManager::insertOrReplaceLayout(const ApiLayoutData& data, qint32 internalId)
{
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT OR REPLACE INTO vms_layout \
                     (user_can_edit, cell_spacing_height, locked, \
                     cell_aspect_ratio, user_id, background_width, \
                     background_image_filename, background_height, \
                     cell_spacing_width, background_opacity, resource_ptr_id \
                     \
                     VALUES (:userCanEdit, :cellSpacingHeight, :locked, \
                     :cellAspectRatio, :userId, :backgroundWidth, \
                     :backgroundImageFilename, :backgroundHeight, \
                     :cellSpacingWidth, :backgroundOpacity, :id)");
    data.autoBindValues(insQuery);
    insQuery.bindValue(":id", internalId);
    if (insQuery.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
        return ErrorCode::failure;
    }
}

ErrorCode QnDbManager::removeStoragesByServer(const QnId& serverGuid)
{
    QSqlQuery delQuery(m_sdb);
    delQuery.prepare("DELETE FROM vms_storage WHERE resource_ptr_id in (select id from vms_resource where parent_guid = :guid and xtype_guid = :typeId)");
    delQuery.bindValue(":guid", serverGuid.toRfc4122());
    delQuery.bindValue(":typeId", m_storageTypeId.toRfc4122());
    if (!delQuery.exec()) {
        qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
        return ErrorCode::failure;
    }

    QSqlQuery delQuery2(m_sdb);
    delQuery2.prepare("DELETE FROM vms_resource WHERE parent_guid = :guid and xtype_guid=:typeId");
    delQuery2.bindValue(":guid", serverGuid.toRfc4122());
    delQuery2.bindValue(":typeId", m_storageTypeId.toRfc4122());
    if (!delQuery2.exec()) {
        qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
        return ErrorCode::failure;
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::updateStorages(const ApiMediaServerData& data)
{
    ErrorCode result = removeStoragesByServer(data.id);
    if (result != ErrorCode::ok)
        return result;
    
    foreach(const ApiStorageData& storage, data.storages)
    {
        qint32 internalId;
        result = insertResource(storage, &internalId);
        if (result != ErrorCode::ok)
            return result;

        QSqlQuery insQuery(m_sdb);
        insQuery.prepare("INSERT INTO vms_storage (space_limit, used_for_writing, resource_ptr_id) VALUES\
                         (:spaceLimit, :usedForWriting, :id)");
        storage.autoBindValues(insQuery);
        insQuery.bindValue(":id", internalId);

        if (!insQuery.exec()) {
            qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
            return ErrorCode::failure;
        }
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::updateCameraSchedule(const ApiCameraData& data, qint32 internalId)
{
	QSqlQuery delQuery(m_sdb);
	delQuery.prepare("DELETE FROM vms_scheduletask where source_id = :id");
	delQuery.bindValue(":id", internalId);
	if (!delQuery.exec()) {
        qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
        return ErrorCode::failure;
    }

	foreach(const ScheduleTask& task, data.scheduleTask) 
	{
		QSqlQuery insQuery(m_sdb);
		insQuery.prepare("INSERT INTO vms_scheduletask (source_id, start_time, end_time, do_record_audio, record_type, day_of_week, before_threshold, after_threshold, stream_quality, fps) VALUES\
					     (:sourceId, :startTime, :endTime, :doRecordAudio, :recordType, :dayOfWeek, :beforeThreshold, :afterThreshold, :streamQuality, :fps)");
		task.autoBindValues(insQuery);
        insQuery.bindValue(":sourceId", internalId);

		if (!insQuery.exec()) {
            qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
			return ErrorCode::failure;
        }
	}
	return ErrorCode::ok;
}

int QnDbManager::getNextSequence()
{
	QWriteLocker lock(&m_mutex);

	QSqlQuery query(m_sdb);
	query.prepare("update sqlite_sequence set seq = seq + 1 where name=\"vms_resource\"");
	if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
		return 0;
    }
	QSqlQuery query2(m_sdb);
	query.prepare("select seq from sqlite_sequence where name=\"vms_resource\"");
	if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
		return 0;
    }
	query.next();
	int result = query.value(0).toInt();
	return result;
}

ErrorCode QnDbManager::executeTransaction(const QnTransaction<ApiSetResourceStatusData>& tran)
{
    QnDbTransactionLocker lock(&m_tran);

    QSqlQuery query(m_sdb);
    query.prepare("UPDATE vms_resource set status = :status where guid = :id");
    query.bindValue(":status", tran.params.status);
    query.bindValue(":guid", tran.params.id.toRfc4122());
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }
    lock.commit();
    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransaction(const QnTransaction<ApiSetResourceDisabledData>& tran)
{
    QnDbTransactionLocker lock(&m_tran);

    QSqlQuery query(m_sdb);
    query.prepare("UPDATE vms_resource set disabled = :disabled where guid = :guid");
    query.bindValue(":disabled", tran.params.disabled);
    query.bindValue(":guid", tran.params.id.toRfc4122());
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }
    lock.commit();
    return ErrorCode::ok;
}

ErrorCode QnDbManager::saveCamera(const ApiCameraData& params)
{
    qint32 internalId;
    ErrorCode result = insertOrReplaceResource(params, &internalId);
    if (result !=ErrorCode::ok)
        return result;

    result = insertOrReplaceCamera(params, internalId);
    if (result !=ErrorCode::ok)
        return result;

    result = updateCameraSchedule(params, internalId);
    return result;
}

ErrorCode QnDbManager::executeTransaction(const QnTransaction<ApiCameraData>& tran)
{
    QnDbTransactionLocker lock(&m_tran);
    ErrorCode result = saveCamera(tran.params);
    if (result == ErrorCode::ok)
        lock.commit();
    return result;
}

ErrorCode QnDbManager::executeTransaction(const QnTransaction<ApiCameraDataList>& tran)
{
    QnDbTransactionLocker lock(&m_tran);
    foreach(const ApiCameraData& camera, tran.params.data)
    {
        ErrorCode result = saveCamera(camera);
        if (result != ErrorCode::ok)
            return result;
    }
    lock.commit();
    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransaction(const QnTransaction<ApiResourceData>& tran)
{
    QnDbTransactionLocker lock(&m_tran);
    qint32 internalId = getResourceInternalId(tran.params.id);
    ErrorCode err = updateResource(tran.params, internalId);
    if (err != ErrorCode::ok)
        return err;

    lock.commit();
    return ErrorCode::ok;
}

ErrorCode QnDbManager::insertBRuleResource(const QString& tableName, const QnId& ruleGuid, const QnId& resourceGuid)
{
    QSqlQuery query(m_sdb);
    query.prepare(QString("INSERT INTO %1 (businessrule_guid, resource_guid) VALUES (:ruleGuid, :resourceGuid)").arg(tableName));
    query.bindValue(":ruleGuid", ruleGuid.toRfc4122());
    query.bindValue(":resGuid", resourceGuid.toRfc4122());
    if (query.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }
}

ErrorCode QnDbManager::executeTransaction(const QnTransaction<ApiBusinessRuleData>& tran)
{
    QnDbTransactionLocker lock(&m_tran);
    ErrorCode rez;
    if (tran.command == ApiCommand::updateBusinessRule)
        rez = updateBusinessRuleTable(tran.params);
    else {
        qint32 internalId;
        rez = insertBusinessRuleTable(tran.params, &internalId);
    }
    if (rez != ErrorCode::ok)
        return rez;

    ErrorCode err = deleteTableRecord(tran.params.id, "vms_businessrule_action_resources", "businessrule_guid");
    if (err != ErrorCode::ok)
        return err;

    err = deleteTableRecord(tran.params.id, "vms_businessrule_event_resources", "businessrule_guid");
    if (err != ErrorCode::ok)
        return err;

    foreach(const QnId& resourceId, tran.params.eventResource) {
        err = insertBRuleResource("vms_businessrule_event_resources", tran.params.id, resourceId);
        if (err != ErrorCode::ok)
            return err;
    }

    foreach(const QnId& resourceId, tran.params.actionResource) {
        err = insertBRuleResource("vms_businessrule_action_resources", tran.params.id, resourceId);
        if (err != ErrorCode::ok)
            return err;
    }

    lock.commit();
    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransaction(const QnTransaction<ApiMediaServerData>& tran)
{
    QnDbTransactionLocker lock(&m_tran);

    ErrorCode result;
    qint32 internalId;

    result = insertOrReplaceResource(tran.params, &internalId);
    if (result !=ErrorCode::ok)
        return result;

    result = insertOrReplaceMediaServer(tran.params, internalId);
    if (result !=ErrorCode::ok)
        return result;

    if (result !=ErrorCode::ok)
        return result;
    result = updateStorages(tran.params);
    if (result == ErrorCode::ok)
        lock.commit();

    return result;
}

ErrorCode QnDbManager::removeLayoutItems(qint32 id)
{
    QSqlQuery delQuery(m_sdb);
    delQuery.prepare("DELETE FROM vms_layoutitem WHERE layout_id = :id");
    delQuery.bindValue(":id", id);
    if (!delQuery.exec()) {
        qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
        return ErrorCode::failure;
    }

    return ErrorCode::ok;
}

ErrorCode QnDbManager::updateLayoutItems(const ApiLayoutData& data, qint32 internalLayoutId)
{
    ErrorCode result = removeLayoutItems(internalLayoutId);
    if (result != ErrorCode::ok)
        return result;

    foreach(const ApiLayoutItemData& item, data.items)
    {
        QSqlQuery insQuery(m_sdb);
        insQuery.prepare("INSERT INTO vms_layoutitem (zoom_bottom, right, uuid, zoom_left, resource_id, \
                         zoom_right, top, layout_id, bottom, zoom_top, \
                         zoom_target_uuid, flags, contrast_params, rotation, \
                         dewarping_params, left) VALUES \
                         (:zoomBottom, :right, :uuid, :zoomLeft, :resourceId, \
                         :zoomRight, :top, :layoutId, :bottom, :zoomTop, \
                         :zoomTargetUuid, :flags, :contrastParams, :rotation, \
                         :dewarpingParams, :left)");
        item.autoBindValues(insQuery);
        insQuery.bindValue(":layoutId", internalLayoutId);

        if (!insQuery.exec()) {
            qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
            return ErrorCode::failure;
        }
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::saveUser( const ApiUserData& data )
{
    //TODO/IMPL
    return ErrorCode::notImplemented;
}

ErrorCode QnDbManager::deleteUserProfileTable(const qint32 id)
{
    QSqlQuery delQuery(m_sdb);
    delQuery.prepare("DELETE FROM vms_userprofile where user_id = :id");
    delQuery.bindValue(QLatin1String(":id"), id);
    if (delQuery.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
        return ErrorCode::failure;
    }
}

qint32 QnDbManager::getResourceInternalId( const QnId& guid )
{
    QSqlQuery query(m_sdb);
    query.prepare("SELECT id from vms_resource where guid = :guid");
    query.bindValue(":guid", guid.toRfc4122());
    if (!query.exec() || !query.next())
        return 0;
    return query.value("id").toInt();
}

qint32 QnDbManager::getBusinessRuleInternalId( const QnId& guid )
{
    QSqlQuery query(m_sdb);
    query.prepare("SELECT id from vms_businessrule where guid = :guid");
    query.bindValue(":guid", guid.toRfc4122());
    if (!query.exec() || !query.next())
        return 0;
    return query.value("id").toInt();
}

ErrorCode QnDbManager::removeUser( const QnId& guid )
{
    QnDbTransactionLocker tran(&m_tran);

    qint32 internalId = getResourceInternalId(guid);

    QSqlQuery query(m_sdb);
    query.prepare("SELECT resource_ptr_id FROM vms_layout where user_id = :id");
    query.bindValue(":id", internalId);
    if (!query.exec())
        return ErrorCode::failure;

    ErrorCode err;
    while (query.next()) {
        err = removeLayoutNoLock(query.value("resource_ptr_id").toInt());
        if (err != ErrorCode::ok)
            return err;
    }

    err = deleteAddParams(internalId);
    if (err != ErrorCode::ok)
        return err;

    err = deleteUserProfileTable(internalId);
    if (err != ErrorCode::ok)
        return err;

    err = deleteTableRecord(internalId, "auth_user", "id");
    if (err != ErrorCode::ok)
        return err;

    err = deleteResourceTable(internalId);
    if (err != ErrorCode::ok)
        return err;

    tran.commit();
    return ErrorCode::ok;
}

ErrorCode QnDbManager::insertBusinessRuleTable( const ApiBusinessRuleData& businessRule , qint32 *internalId)
{
    QSqlQuery query(m_sdb);
    query.prepare(QString("INSERT INTO vms_businessrule (event_type, event_condition, event_state, action_type, \
                          action_params, aggregation_period, disabled, comments, schedule) VALUES \
                          (:eventType, :eventCondition, :eventState, :actionType, \
                          :actionParams, :aggregationPeriod, :disabled, :comments, :schedule)"));
    businessRule.autoBindValues(query);
    if (query.exec()) {
        *internalId = query.lastInsertId().toInt();
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }
}

ErrorCode QnDbManager::updateBusinessRuleTable( const ApiBusinessRuleData& businessRule)
{
    QSqlQuery query(m_sdb);
    query.prepare(QString("UPDATE vms_businessrule SET event_type = :eventType, event_condition = :eventCondition, event_state = :eventState, action_type = :actionType, \
                          action_params = :actionParams, aggregation_period = :aggregationPeriod, disabled = :disabled, comments = :comments, schedule = :schedule \
                          WHERE guid = :guid"));
    businessRule.autoBindValues(query);
    if (query.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }
}

ErrorCode QnDbManager::removeBusinessRule( const QnId& guid )
{
    qint32 id = getBusinessRuleInternalId(guid);

    ErrorCode err = deleteTableRecord(id, "vms_businessrule_action_resources", "businessrule_id");
    if (err != ErrorCode::ok)
        return err;

    err = deleteTableRecord(id, "vms_businessrule_event_resources", "businessrule_id");
    if (err != ErrorCode::ok)
        return err;

    err = deleteTableRecord(id, "vms_businessrule", "id");
    if (err != ErrorCode::ok)
        return err;

    return ErrorCode::ok;
}


ErrorCode QnDbManager::saveLayout(const ApiLayoutData& params)
{
    qint32 internalId;

    ErrorCode result = insertOrReplaceResource(params, &internalId);
    if (result !=ErrorCode::ok)
        return result;

    result = insertOrReplaceLayout(params, internalId);
    if (result !=ErrorCode::ok)
        return result;

    result = updateLayoutItems(params, internalId);
    return result;
}

ErrorCode QnDbManager::executeTransaction(const QnTransaction<ApiLayoutData>& tran)
{
    QnDbTransactionLocker lock(&m_tran);
    ErrorCode result = saveLayout(tran.params);
    if (result == ErrorCode::ok)
        lock.commit();
    return result;
}

ErrorCode QnDbManager::executeTransaction(const QnTransaction<ApiLayoutDataList>& tran)
{
    QnDbTransactionLocker lock(&m_tran);
    foreach(const ApiLayoutData& layout, tran.params.data)
    {
        ErrorCode err = saveLayout(layout);
        if (err != ErrorCode::ok)
            return err;
    }
    lock.commit();
    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransaction(const QnTransaction<ApiResourceParams>& tran)
{
    QnDbTransactionLocker lock(&m_tran);

    qint32 internalId = getResourceInternalId(tran.params.id);
    ErrorCode result = deleteAddParams(internalId);
    if (result != ErrorCode::ok)
        return result;

    foreach(const ApiResourceParam& param, tran.params.params) 
    {
        result = insertAddParam(param, internalId);
        if (result != ErrorCode::ok)
            return result;
    }
    lock.commit();
    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransaction(const QnTransaction<ApiCameraServerItemData>& tran)
{
    QWriteLocker lock(&m_mutex);

    QSqlQuery query(m_sdb);
    query.prepare("INSERT INTO vms_cameraserveritem (server_guid, timestamp, physical_id) VALUES(:serverGuid, :timestamp, :physicalId)");
    tran.params.autoBindValues(query);
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }

    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransaction(const QnTransaction<ApiPanicModeData>& tran)
{
    QWriteLocker lock(&m_mutex);

    QSqlQuery query(m_sdb);
    query.prepare("UPDATE vms_server SET panic_mode = :mode");
    query.bindValue(QLatin1String(":mode"), (int) tran.params.mode);
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }

    return ErrorCode::ok;
}

ErrorCode QnDbManager::deleteResourceTable(const qint32 id)
{
    QSqlQuery delQuery(m_sdb);
    delQuery.prepare("DELETE FROM vms_resource where id = :id");
    delQuery.bindValue(QLatin1String(":id"), id);
    if (delQuery.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
        return ErrorCode::failure;
    }
}

ErrorCode QnDbManager::deleteCameraServerItemTable(qint32 id)
{
    QSqlQuery query(m_sdb);
    query.prepare("select c.physical_id , (select count(*) from vms_camera where physical_id = c.physical_id) as cnt \
                  FROM vms_camera c WHERE c.resource_ptr_id = :id");
    query.bindValue(QLatin1String(":id"), id);
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }
    query.next();
    if (query.value("cnt").toInt() > 1)
        return ErrorCode::ok; // camera instance on a other media server still present

    QSqlQuery delQuery(m_sdb);
    delQuery.prepare("DELETE FROM vms_cameraserveritem where physical_id = :physical_id");
    delQuery.bindValue(QLatin1String(":physical_id"), query.value("physical_id").toString());
    if (delQuery.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
        return ErrorCode::failure;
    }
}

ErrorCode QnDbManager::deleteTableRecord(const qint32& internalId, const QString& tableName, const QString& fieldName)
{
    QSqlQuery delQuery(m_sdb);
    delQuery.prepare(QString("DELETE FROM %1 where %2 = :id").arg(tableName).arg(fieldName));
    delQuery.bindValue(QLatin1String(":id"), internalId);
    if (delQuery.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
        return ErrorCode::failure;
    }
}

ErrorCode QnDbManager::deleteTableRecord(const QnId& id, const QString& tableName, const QString& fieldName)
{
    QSqlQuery delQuery(m_sdb);
    delQuery.prepare(QString("DELETE FROM %1 where %2 = :guid").arg(tableName).arg(fieldName));
    delQuery.bindValue(QLatin1String(":guid"), id.toRfc4122());
    if (delQuery.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
        return ErrorCode::failure;
    }
}

ErrorCode QnDbManager::removeCamera(const QnId& guid)
{
    QnDbTransactionLocker tran(&m_tran);

    qint32 id = getResourceInternalId(guid);

    ErrorCode err = deleteAddParams(id);
    if (err != ErrorCode::ok)
        return err;

    err = deleteTableRecord(id, "vms_businessrule_action_resources", "resource_id");
    if (err != ErrorCode::ok)
        return err;

    err = deleteTableRecord(id, "vms_businessrule_event_resources", "resource_id");
    if (err != ErrorCode::ok)
        return err;

    err = deleteCameraServerItemTable(id);
    if (err != ErrorCode::ok)
        return err;

    err = deleteTableRecord(id, "vms_camera", "resource_ptr_id");
    if (err != ErrorCode::ok)
        return err;

    err = deleteResourceTable(id);
    if (err != ErrorCode::ok)
        return err;

    tran.commit();
    return ErrorCode::ok;
}

ErrorCode QnDbManager::removeServer(const QnId& guid)
{
    QnDbTransactionLocker tran(&m_tran);

    qint32 id = getResourceInternalId(guid);

    ErrorCode err = deleteAddParams(id);
    if (err != ErrorCode::ok)
        return err;

    err = removeStoragesByServer(guid);
    if (err != ErrorCode::ok)
        return err;

    err = deleteTableRecord(id, "vms_server", "resource_ptr_id");
    if (err != ErrorCode::ok)
        return err;

    err = deleteResourceTable(id);
    if (err != ErrorCode::ok)
        return err;

    tran.commit();
    return ErrorCode::ok;
}

ErrorCode QnDbManager::deleteLayoutItems(const qint32 id)
{
    QSqlQuery delQuery(m_sdb);
    delQuery.prepare("DELETE FROM vms_layoutitem where layout_id = :id");
    delQuery.bindValue(QLatin1String(":id"), id);
    if (delQuery.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
        return ErrorCode::failure;
    }
}

ErrorCode QnDbManager::removeLayout(const QnId& guid)
{
    QnDbTransactionLocker tran(&m_tran);
    ErrorCode err = removeLayoutNoLock(getResourceInternalId(guid));
    if (err == ErrorCode::ok)
        tran.commit();
    return err;
}

ErrorCode QnDbManager::removeLayoutNoLock(qint32 internalId)
{
    ErrorCode err = deleteAddParams(internalId);
    if (err != ErrorCode::ok)
        return err;

    err = deleteLayoutItems(internalId);
    if (err != ErrorCode::ok)
        return err;

    err = deleteTableRecord(internalId, "vms_layout", "resource_ptr_id");
    if (err != ErrorCode::ok)
        return err;

    err = deleteResourceTable(internalId);
    return err;
}

ErrorCode QnDbManager::executeTransaction(const QnTransaction<ApiStoredFileData>& tran)
{
    assert( tran.command == ApiCommand::addStoredFile || tran.command == ApiCommand::updateStoredFile );
    return m_storedFileManagerImpl->saveFile( tran.params );
}

ErrorCode QnDbManager::executeTransaction(const QnTransaction<ApiStoredFilePath>& tran)
{
    assert( tran.command == ApiCommand::removeStoredFile );
    return m_storedFileManagerImpl->removeFile( tran.params );
}

ErrorCode QnDbManager::executeTransaction(const QnTransaction<ApiUserData>& tran)
{
    QnDbTransactionLocker lock(&m_tran);

    ErrorCode result = saveUser( tran.params );
    if (result != ErrorCode::ok)
        return result;
    lock.commit();
    return ErrorCode::ok;
}

ErrorCode QnDbManager::removeResource(const QnId& id)
{
    QnDbTransactionLocker tran(&m_tran);

    QSqlQuery query(m_sdb);
    query.prepare("SELECT \
                    (CASE WHEN c.resource_ptr_id is null then rt.name else 'Camera' end) as name,\
                    FROM vms_resource r\
                    JOIN vms_resourcetype rt on rt.guid = r.xtype_guid\
                    LEFT JOIN vms_camera c on c.resource_ptr_id = r.id\
                    WHERE r.guid = :guid");
    query.bindValue(":guid", id.toRfc4122());
    if (!query.exec())
        return ErrorCode::failure;
    query.next();
    QString objectType = query.value("name").toString();
    if (objectType == "Camera")
        return removeCamera(id);
    else if (objectType == "Server")
        return removeServer(id);
    else if (objectType == "User")
        return removeUser(id);
    else if (objectType == "Layout")
        return removeLayout(id);
    else {
        Q_ASSERT_X(0, "Unknown object type", Q_FUNC_INFO);
        return ErrorCode::notImplemented;
    }
}

ErrorCode QnDbManager::executeTransaction(const QnTransaction<ApiIdData>& tran)
{
    switch (tran.command)
    {
    case ApiCommand::removeCamera:
        return removeCamera(tran.params.id);
    case ApiCommand::removeMediaServer:
        return removeServer(tran.params.id);
    case ApiCommand::removeLayout:
        return removeLayout(tran.params.id);
    case ApiCommand::removeBusinessRule:
        return removeBusinessRule( tran.params.id );
    case ApiCommand::removeUser:
        return removeUser( tran.params.id );
    case ApiCommand::removeResource:
        return removeResource( tran.params.id );
    default:
        qWarning() << "Remove operation is not implemented for command" << toString(tran.command);
        Q_ASSERT_X(0, "Remove operation is not implemented for command", Q_FUNC_INFO);
        break;
    }
    return ErrorCode::unsupported;
}

/* 
-------------------------------------------------------------
-------------------------- getters --------------------------
 ------------------------------------------------------------
*/

// ----------- getResourceTypes --------------------

ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiResourceTypeList& data)
{
	QSqlQuery queryTypes(m_sdb);
	queryTypes.prepare("select rt.guid as id, rt.name, m.name as manufacture \
				  from vms_resourcetype rt \
				  left join vms_manufacture m on m.id = rt.manufacture_id \
				  order by rt.id");
	if (!queryTypes.exec()) {
        qWarning() << Q_FUNC_INFO << queryTypes.lastError().text();
		return ErrorCode::failure;
    }

	QSqlQuery queryParents(m_sdb);
	queryParents.prepare("select t1.guid as id, t2.guid as parentId \
                         from vms_resourcetype_parents p \
                         JOIN vms_resourcetype t1 on t1.id = p.from_resourcetype_id \
                         JOIN vms_resourcetype t2 on t2.id = p.to_resourcetype_id \
                         order by p.from_resourcetype_id, p.to_resourcetype_id desc");
	if (!queryParents.exec()) {
        qWarning() << Q_FUNC_INFO << queryParents.lastError().text();
		return ErrorCode::failure;
    }

    QSqlQuery queryProperty(m_sdb);
    queryProperty.prepare("SELECT rt.guid as resource_type_id, pt.name, pt.type, pt.min, pt.max, pt.step, pt.[values], pt.ui_values, pt.default_value, \
                          pt.netHelper, pt.[group], pt.sub_group, pt.description, pt.ui, pt.readonly \
                          FROM vms_propertytype pt \
                          JOIN vms_resourcetype rt on rt.id = pt.resource_type_id ORDER BY pt.resource_type_id");
    if (!queryProperty.exec()) {
        qWarning() << Q_FUNC_INFO << queryProperty.lastError().text();
        return ErrorCode::failure;
    }

	data.loadFromQuery(queryTypes);
    mergeIdListData(queryParents, data.data, &ApiResourceTypeData::parentId);

    std::vector<ApiPropertyType> allProperties;
    QN_QUERY_TO_DATA_OBJECT(queryProperty, ApiPropertyType, allProperties, ApiPropertyTypeFields);
    mergeObjectListData(data.data, allProperties, &ApiResourceTypeData::propertyTypeList, &ApiPropertyType::resource_type_id);

	return ErrorCode::ok;
}

// ----------- getLayouts --------------------

ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiLayoutDataList& layouts)
{
    QSqlQuery query(m_sdb);
    QString filter; // todo: add data filtering by user here
    query.prepare(QString("SELECT r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentGuid, r.name, r.url, r.status,r. disabled, \
                  l.user_can_edit as userCanEdit, l.cell_spacing_height as cellSpacingHeight, l.locked, \
                  l.cell_aspect_ratio as cellAspectRatio, l.user_id as userId, l.background_width as backgroundWidth, \
                  l.background_image_filename as backgroundImageFilename, l.background_height as backgroundHeight, \
                  l.cell_spacing_width as cellSpacingWidth, l.background_opacity as backgroundOpacity, l.resource_ptr_id as id \
                  FROM vms_layout l \
                  JOIN vms_resource r on r.id = l.resource_ptr_id %1 ORDER BY r.id").arg(filter));
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }

    QSqlQuery queryItems(m_sdb);
    queryItems.prepare("SELECT li.zoom_bottom as zoomBottom, li.right, li.uuid, li.zoom_left as zoomLeft, li.resource_id as resourceId, \
                       li.zoom_right as zoomRight, li.top, layout_id as layoutId, li.bottom, li.zoom_top as zoomTop, \
                       li.zoom_target_uuid as zoomTargetUuid, li.flags, li.contrast_params as contrastParams, li.rotation, li.id, \
                       li.dewarping_params as dewarpingParams, li.left FROM vms_layoutitem li \
                       JOIN vms_resource r on r.id = li.layout_id order by li.layout_id");

    if (!queryItems.exec()) {
        qWarning() << Q_FUNC_INFO << queryItems.lastError().text();
        return ErrorCode::failure;
    }

    layouts.loadFromQuery(query);
    std::vector<ApiLayoutItemDataWithRef> items;
    QN_QUERY_TO_DATA_OBJECT(queryItems, ApiLayoutItemDataWithRef, items, ApiLayoutItemDataFields (layoutId));
    mergeObjectListData(layouts.data, items, &ApiLayoutData::items, &ApiLayoutItemDataWithRef::layoutId);

    return ErrorCode::ok;
}

// ----------- getCameras --------------------

QString guidToSqlString(const QnId& guid)
{
    QByteArray data = guid.toRfc4122().toHex();
    return QString("x'%1'").arg(QString::fromLocal8Bit(data));
}

ErrorCode QnDbManager::doQueryNoLock(const QnId& mServerId, ApiCameraDataList& cameraList)
{
	QSqlQuery queryCameras(m_sdb);
    QString filterStr;
	if (!mServerId.isNull()) {
		filterStr = QString("WHERE r.parent_guid = %1").arg(guidToSqlString(mServerId));
	}
	queryCameras.prepare(QString("SELECT r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentGuid, r.name, r.url, r.status,r. disabled, \
		c.audio_enabled as audioEnabled, c.control_disabled as controlDisabled, c.firmware, c.vendor, c.manually_added as manuallyAdded, \
		c.region, c.schedule_disabled as scheduleDisabled, c.motion_type as motionType, \
		c.group_name as groupName, c.group_id as groupId, c.mac, c. model, c.secondary_quality as secondaryQuality, \
		c.status_flags as statusFlags, c.physical_id as physicalId, c.password, login, c.dewarping_params as dewarpingParams \
		FROM vms_resource r \
		JOIN vms_camera c on c.resource_ptr_id = r.id %1 ORDER BY r.id").arg(filterStr));


    QSqlQuery queryScheduleTask(m_sdb);
    QString filterStr2;
    if (!mServerId.isNull()) 
        filterStr2 = QString("WHERE r.parent_guid = %1").arg(guidToSqlString(mServerId));
    
    queryScheduleTask.prepare(QString("SELECT r.guid as sourceId, st.start_time as startTime, st.end_time as endTime, st.do_record_audio as doRecordAudio, \
                                       st.record_type as recordType, st.day_of_week as dayOfWeek, st.before_threshold as beforeThreshold, st.after_threshold as afterThreshold, \
                                       st.stream_quality as streamQuality, st.fps \
                                       FROM vms_scheduletask st \
                                       JOIN vms_resource r on r.id = st.source_id %1 ORDER BY r.id").arg(filterStr2));

    QSqlQuery queryParams(m_sdb);
    queryParams.prepare(QString("SELECT r.guid as resourceId, kv.value, kv.name \
                                 FROM vms_kvpair kv \
                                 JOIN vms_camera c on c.resource_ptr_id = kv.resource_id \
                                 JOIN vms_resource r on r.id = kv.resource_id \
                                 %1 ORDER BY kv.resource_id").arg(filterStr2));

	if (!queryCameras.exec()) {
        qWarning() << Q_FUNC_INFO << queryCameras.lastError().text();
		return ErrorCode::failure;
    }
    if (!queryScheduleTask.exec()) {
        qWarning() << Q_FUNC_INFO << queryScheduleTask.lastError().text();
        return ErrorCode::failure;
    }

    if (!queryParams.exec()) {
        qWarning() << Q_FUNC_INFO << queryParams.lastError().text();
        return ErrorCode::failure;
    }

	cameraList.loadFromQuery(queryCameras);

    std::vector<ScheduleTaskWithRef> sheduleTaskList;
    QN_QUERY_TO_DATA_OBJECT(queryScheduleTask, ScheduleTaskWithRef, sheduleTaskList, apiScheduleTaskFields (sourceId) );

    mergeObjectListData(cameraList.data, sheduleTaskList, &ApiCameraData::scheduleTask, &ScheduleTaskWithRef::sourceId);

    std::vector<ApiResourceParamWithRef> params;
    QN_QUERY_TO_DATA_OBJECT(queryParams, ApiResourceParamWithRef, params, ApiResourceParamFields (resourceId));
    mergeObjectListData<ApiCameraData>(cameraList.data, params, &ApiCameraData::addParams, &ApiResourceParamWithRef::resourceId);

	return ErrorCode::ok;
}

// ----------- getServers --------------------


ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiMediaServerDataList& serverList)
{
    QSqlQuery query(m_sdb);
    query.prepare(QString("select r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentGuid, r.name, r.url, r.status,r. disabled, \
                          s.api_url as apiUrl, s.auth_key as authKey, s.streaming_url as streamingUrl, s.version, s.net_addr_list as netAddrList, s.reserve, s.panic_mode as panicMode \
                          from vms_resource r \
                          join vms_server s on s.resource_ptr_id = r.id order by r.guid"));

    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }

    QSqlQuery queryStorage(m_sdb);
    queryStorage.prepare(QString("select r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentGuid, r.name, r.url, r.status,r. disabled, \
                          s.space_limit as spaceLimit, s.used_for_writing as usedForWriting \
                          from vms_resource r \
                          join vms_storage s on s.resource_ptr_id = r.id order by r.parent_guid"));

    if (!queryStorage.exec()) {
        qWarning() << Q_FUNC_INFO << queryStorage.lastError().text();
        return ErrorCode::failure;
    }

    serverList.loadFromQuery(query);

    ApiStorageDataList storageList;
    storageList.loadFromQuery(queryStorage);

    mergeObjectListData<ApiMediaServerData, ApiStorageData>(serverList.data, storageList.data, &ApiMediaServerData::storages, &ApiStorageData::parentGuid);

    return ErrorCode::ok;
}

//getCameraServerItems
ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiCameraServerItemDataList& historyList)
{
    QSqlQuery query(m_sdb);
    query.prepare(QString("select server_guid as serverGuid, timestamp, physical_id as physicalId from vms_cameraserveritem"));
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }

    historyList.loadFromQuery(query);

    return ErrorCode::ok;
}

//getUsers
ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiUserDataList& userList)
{
    //digest = md5('%s:%s:%s' % (self.user.username.lower(), 'NetworkOptix', password)).hexdigest()
    QSqlQuery query(m_sdb);
    query.prepare(QString("select r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentGuid, r.name, r.url, r.status,r. disabled, \
                          u.password, u.is_superuser as isAdmin, u.email, p.digest as digest, u.password as hash, p.rights \
                          from vms_resource r \
                          join auth_user u  on u.id = r.id\
                          join vms_userprofile p on p.user_id = u.id\
                          order by r.id"));
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }

    QSqlQuery queryParams(m_sdb);
    queryParams.prepare(QString("SELECT r.guid as resourceId, kv.value, kv.name \
                                FROM vms_kvpair kv \
                                JOIN auth_user u on u.id = kv.resource_id \
                                JOIN vms_resource r on r.id = kv.resource_id \
                                ORDER BY kv.resource_id"));

    if (!queryParams.exec()) {
        qWarning() << Q_FUNC_INFO << queryParams.lastError().text();
        return ErrorCode::failure;
    }

    userList.loadFromQuery(query);
    
    std::vector<ApiResourceParamWithRef> params;
    QN_QUERY_TO_DATA_OBJECT(queryParams, ApiResourceParamWithRef, params, ApiResourceParamFields (resourceId));
    mergeObjectListData<ApiUserData>(userList.data, params, &ApiCameraData::addParams, &ApiResourceParamWithRef::resourceId);
    
    return ErrorCode::ok;
}

//getBusinessRules
ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiBusinessRuleDataList& businessRuleList)
{
    QSqlQuery query(m_sdb);
    query.prepare(QString("SELECT guid as id, event_type as eventType, event_condition as eventCondition, event_state as eventState, action_type as actionType, \
                          action_params as actionParams, aggregation_period as aggregationPeriod, disabled, comments, schedule \
                          FROM vms_businessrule order by guid"));
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }

    QSqlQuery queryRuleEventRes(m_sdb);
    queryRuleEventRes.prepare(QString("SELECT businessrule_guid as parentId, resource_guid as id from vms_businessrule_event_resources order by businessrule_guid"));
    if (!queryRuleEventRes.exec()) {
        qWarning() << Q_FUNC_INFO << queryRuleEventRes.lastError().text();
        return ErrorCode::failure;
    }

    QSqlQuery queryRuleActionRes(m_sdb);
    queryRuleActionRes.prepare(QString("SELECT businessrule_guid as parentId, resource_guid as id from vms_businessrule_action_resources order by businessrule_guid"));
    if (!queryRuleActionRes.exec()) {
        qWarning() << Q_FUNC_INFO << queryRuleActionRes.lastError().text();
        return ErrorCode::failure;
    }

    businessRuleList.loadFromQuery(query);

    // merge data

    mergeIdListData<ApiBusinessRuleData>(queryRuleEventRes, businessRuleList.data, &ApiBusinessRuleData::eventResource);
    mergeIdListData<ApiBusinessRuleData>(queryRuleActionRes, businessRuleList.data, &ApiBusinessRuleData::actionResource);

    return ErrorCode::ok;
}

// getKVPairs
ErrorCode QnDbManager::doQueryNoLock(const QnId& resourceId, ApiResourceParams& params)
{
    QSqlQuery query(m_sdb);
    query.prepare(QString("SELECT kv.value, kv.name \
                                FROM vms_kvpair kv \
                                JOIN vms_resource r on r.id = kv.resource_id WHERE r.guid = :guid"));
    query.bindValue(QLatin1String(":guid"), resourceId.toRfc4122());
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }

    QN_QUERY_TO_DATA_OBJECT(query, ApiResourceParam, params.params, ApiResourceParamFields);
    params.id = resourceId;
    return ErrorCode::ok;
}

// getCurrentTime
ErrorCode QnDbManager::doQuery(const nullptr_t& /*dummy*/, qint64& currentTime)
{
    currentTime = QDateTime::currentMSecsSinceEpoch();
    return ErrorCode::ok;
}

// ApiFullData
ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& dummy, ApiFullData& data)
{
    ErrorCode err;

    if ((err = doQueryNoLock(dummy, data.resTypes)) != ErrorCode::ok)
        return err;

    if ((err = doQueryNoLock(dummy, data.servers)) != ErrorCode::ok)
        return err;

    if ((err = doQueryNoLock(QnId(), data.cameras)) != ErrorCode::ok)
        return err;

    if ((err = doQueryNoLock(dummy, data.users)) != ErrorCode::ok)
        return err;

    if ((err = doQueryNoLock(dummy, data.layouts)) != ErrorCode::ok)
        return err;

    if ((err = doQueryNoLock(dummy, data.rules)) != ErrorCode::ok)
        return err;

    if ((err = doQueryNoLock(dummy, data.cameraHistory)) != ErrorCode::ok)
        return err;

    std::vector<ApiResourceParamWithRef> kvPairs;
    QSqlQuery queryParams(m_sdb);
    queryParams.prepare(QString("SELECT r.guid as resourceId, kv.value, kv.name \
                                FROM vms_kvpair kv \
                                JOIN vms_resource r on r.id = kv.resource_id \
                                %1 ORDER BY kv.resource_id"));
    if (!queryParams.exec())
        return ErrorCode::failure;
    QN_QUERY_TO_DATA_OBJECT(queryParams, ApiResourceParamWithRef, kvPairs, ApiResourceParamFields (resourceId));


    mergeObjectListData<ApiMediaServerData>(data.servers.data, kvPairs, &ApiMediaServerData::addParams, &ApiResourceParamWithRef::resourceId);
    mergeObjectListData<ApiCameraData>(data.cameras.data,      kvPairs, &ApiCameraData::addParams,      &ApiResourceParamWithRef::resourceId);
    mergeObjectListData<ApiUserData>(data.users.data,          kvPairs, &ApiUserData::addParams,        &ApiResourceParamWithRef::resourceId);
    mergeObjectListData<ApiLayoutData>(data.layouts.data,      kvPairs, &ApiLayoutData::addParams,      &ApiResourceParamWithRef::resourceId);

    return err;
}

ErrorCode QnDbManager::doQuery(const ApiStoredFilePath& path, ApiStoredDirContents& data)
{
    return m_storedFileManagerImpl->listDirectory( path, data );
}

ErrorCode QnDbManager::doQuery(const ApiStoredFilePath& path, ApiStoredFileData& data)
{
    return m_storedFileManagerImpl->readFile( path, data );
}


}
