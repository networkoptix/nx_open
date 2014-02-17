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
    query.exec("BEGIN TRANSACTION");
}

void QnDbManager::QnDbTransaction::rollback()
{
    QSqlQuery query(m_sdb);
    query.exec("ROLLBACK");
    m_mutex.unlock();
}

void QnDbManager::QnDbTransaction::commit()
{
    QSqlQuery query(m_sdb);
    query.exec("COMMIT");
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
void mergeIdListData(QSqlQuery& query, std::vector<MainData>& data, std::vector<qint32> MainData::*subList)
{
    int idx = 0;
    QSqlRecord rec = query.record();
    int idIdx = rec.indexOf("id");
    int parentIdIdx = rec.indexOf("parentId");
    while (query.next())
    {
        int id = query.value(idIdx).toInt();
        int parentId = query.value(parentIdIdx).toInt();

        for (; idx < data.size() && data[idx].id != id; idx++);
        if (idx == data.size())
            break;
        (data[idx].*subList).push_back(parentId);
    }
}

template <class MainData, class SubData>
void mergeObjectListData(std::vector<MainData>& data, std::vector<SubData>& subDataList, std::vector<SubData> MainData::*subDataListField, qint32 SubData::*parentIdField)
{
    int idx = 0;
    foreach(const SubData& subData, subDataList)
    {
        for (; idx < data.size() && subData.*parentIdField != data[idx].id; idx++);
        if (idx == data.size())
            break;
        (data[idx].*subDataListField).push_back(std::move(subData));
    }
}

QnDbManager::QnDbManager(QnResourceFactory* factory, StoredFileManagerImpl* const storedFileManagerImpl ):
    m_tran(m_sdb, m_mutex),
    m_storedFileManagerImpl( storedFileManagerImpl )

{
    m_storageTypeId = 0;
    m_resourceFactory = factory;
	m_sdb = QSqlDatabase::addDatabase("QSQLITE", "QnDbManager");
	m_sdb.setDatabaseName("c:/develop/netoptix_trunk/appserver/db/ecs.db");
    //m_sdb.setDatabaseName("c:/Windows/System32/config/systemprofile/AppData/Local/Network Optix/Enterprise Controller/db/ecs.db");
    
	if (m_sdb.open())
	{
		if (!createDatabase()) // create tables is DB is empty
			qWarning() << "can't create tables for sqlLite database!";

        QSqlQuery query(m_sdb);
        query.prepare("select * from vms_resourcetype where name = 'Storage'");
        Q_ASSERT(query.exec());
        if (query.next())
            m_storageTypeId = query.value("id").toInt();
	}
	else {
		qWarning() << "can't initialize sqlLite database! Actions log is not created!";
	}

	Q_ASSERT(!globalInstance);
	globalInstance = this;
}

bool QnDbManager::createDatabase()
{
    //TODO/IMPL
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

ErrorCode QnDbManager::insertAddParam(const ApiResourceParam& param)
{
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT INTO vms_kvpair (resource_id, name, value) VALUES(:resourceId, :name, :value)");
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

ErrorCode QnDbManager::insertResource(const ApiResourceData& data)
{
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT INTO vms_resource (id, guid, xtype_id, parent_id, name, url, status, disabled) VALUES(:id, :guid, :typeId, :parentId, :name, :url, :status, :disabled)");
    data.autoBindValues(insQuery);
	if (!insQuery.exec()) {
		qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
		return ErrorCode::failure;
	}
    foreach(const ApiResourceParam& param, data.addParams) {
        ErrorCode result = insertAddParam(param);
        if (result != ErrorCode::ok)
            return result;
    }

    return ErrorCode::ok;
}

ErrorCode QnDbManager::insertOrReplaceResource(const ApiResourceData& data)
{
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT OR REPLACE INTO vms_resource (id, guid, xtype_id, parent_id, name, url, status, disabled) VALUES(:id, :guid, :typeId, :parentId, :name, :url, :status, :disabled)");
    data.autoBindValues(insQuery);
    if (!insQuery.exec()) {
        qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
        return ErrorCode::failure;
    }
    foreach(const ApiResourceParam& param, data.addParams) {
        ErrorCode result = insertAddParam(param);
        if (result != ErrorCode::ok)
            return result;
    }

    return ErrorCode::ok;
}

ErrorCode QnDbManager::updateResource(const ApiResourceData& data)
{
	QSqlQuery insQuery(m_sdb);
	insQuery.prepare("UPDATE vms_resource SET guid = :guid, xtype_id = :typeId, parent_id = :parentId, name = :name, url = :url, status = :status, disabled = :disabled WHERE id = :id");
	data.autoBindValues(insQuery);

    if (!insQuery.exec()) {
        qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
        return ErrorCode::failure;
    }

    if (!data.addParams.empty()) 
    {
        ErrorCode result = deleteAddParams(data.id);
        if (result != ErrorCode::ok)
            return result;

        foreach(const ApiResourceParam& param, data.addParams) {
            ErrorCode result = insertAddParam(param);
            if (result != ErrorCode::ok)
                return result;
        }
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::insertCamera(const ApiCameraData& data)
{
	QSqlQuery insQuery(m_sdb);
	insQuery.prepare("INSERT INTO vms_camera (audio_enabled, control_disabled, firmware, vendor, manually_added, resource_ptr_id, region, schedule_disabled, motion_type, group_name, group_id,\
					 mac, model, secondary_quality, status_flags, physical_id, password, login, dewarping_params) VALUES\
					 (:audioEnabled, :controlDisabled, :firmware, :vendor, :manuallyAdded, :id, :region, :scheduleDisabled, :motionType, :groupName, :groupId,\
					 :mac, :model, :secondaryQuality, :statusFlags, :physicalId, :password, :login, :dewarpingParams)");
	data.autoBindValues(insQuery);

	if (insQuery.exec()) {
		return ErrorCode::ok;
	}
	else {
		qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
		return ErrorCode::failure;
	}
}

ErrorCode QnDbManager::updateCamera(const ApiCameraData& data)
{
	QSqlQuery query(m_sdb);
	query.prepare("UPDATE vms_camera SET\
					 audio_enabled = :audioEnabled, control_disabled = :controlDisabled, firmware = :firmware, vendor = :vendor, manually_added = :manuallyAdded, region = :region,\
					 schedule_disabled = :scheduleDisabled, motion_type = :motionType, group_name = :groupName, group_id = :groupId,\
					 mac = :mac, model = :model, secondary_quality = :secondaryQuality, status_flags = :statusFlags, physical_id = :physicalId, \
					 password = :password, login = :login, dewarping_params = :dewarpingParams\
					 WHERE resource_ptr_id = :id");
	data.autoBindValues(query);

	return query.exec() ? ErrorCode::ok : ErrorCode::failure;
}

ErrorCode QnDbManager::insertMediaServer(const ApiMediaServerData& data)
{
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT INTO vms_server (api_url, auth_key, streaming_url, version, net_addr_list, reserve, panic_mode, resource_ptr_id) VALUES\
                     (:apiUrl, :authKey, :streamingUrl, :version, :netAddrList, :reserve, :panicMode, :id)");
    data.autoBindValues(insQuery);
    if (insQuery.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
        return ErrorCode::failure;
    }
}

ErrorCode QnDbManager::updateMediaServer(const ApiMediaServerData& data)
{
    QSqlQuery query(m_sdb);
    query.prepare("UPDATE vms_server SET\
                     api_url = :apiUrl, auth_key = :authKey, streaming_url = :streamingUrl, version = :version, \
                     net_addr_list = :netAddrList, reserve = :reserve, panic_mode= :panicMode \
                     WHERE resource_ptr_id = :id");
    data.autoBindValues(query);

    if (query.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }
}

ErrorCode QnDbManager::insertLayout(const ApiLayoutData& data)
{
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT INTO vms_layout \
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
    if (insQuery.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
        return ErrorCode::failure;
    }
}

ErrorCode QnDbManager::insertOrReplaceLayout(const ApiLayoutData& data)
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
    if (insQuery.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
        return ErrorCode::failure;
    }
}

ErrorCode QnDbManager::updateLayout(const ApiLayoutData& data)
{
    QSqlQuery query(m_sdb);
    query.prepare("UPDATE vms_layout SET\
                  user_can_edit = :userCanEdit, \
                  cell_spacing_height = :cellSpacingHeight, locked = :locked, \
                  cell_aspect_ratio = :cellAspectRatio, user_id = :userId, background_width = :backgroundWidth, \
                  background_image_filename = :backgroundImageFilename, background_height = :backgroundHeight, \
                  cell_spacing_width = :cellSpacingWidth, background_opacity = :backgroundOpacity \
                  WHERE resource_ptr_id = :id");
    data.autoBindValues(query);

    if (query.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }
}

ErrorCode QnDbManager::removeStoragesByServer(qint32 id)
{
    QSqlQuery delQuery(m_sdb);
    delQuery.prepare("DELETE FROM vms_storage WHERE resource_ptr_id in (select id from vms_resource where parent_id = :id and xtype_id = :typeId)");
    delQuery.bindValue(":id", id);
    delQuery.bindValue(":typeId", m_storageTypeId);
    if (!delQuery.exec()) {
        qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
        return ErrorCode::failure;
    }

    QSqlQuery delQuery2(m_sdb);
    delQuery2.prepare("DELETE FROM vms_resource WHERE parent_id = :id and xtype_id=:typeId");
    delQuery2.bindValue(":id", id);
    delQuery2.bindValue(":typeId", m_storageTypeId);
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
        result = insertResource(storage);
        if (result != ErrorCode::ok)
            return result;

        QSqlQuery insQuery(m_sdb);
        insQuery.prepare("INSERT INTO vms_storage (space_limit, used_for_writing, resource_ptr_id) VALUES\
                         (:spaceLimit, :usedForWriting, :id)");
        storage.autoBindValues(insQuery);

        if (!insQuery.exec()) {
            qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
            return ErrorCode::failure;
        }
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::updateCameraSchedule(const ApiCameraData& data)
{
	QSqlQuery delQuery(m_sdb);
	delQuery.prepare("DELETE FROM vms_scheduletask where source_id = :id");
	delQuery.bindValue(":id", data.id);
	if (!delQuery.exec()) {
        qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
        return ErrorCode::failure;
    }

	foreach(const ScheduleTask& task, data.scheduleTask) 
	{
		QSqlQuery insQuery(m_sdb);
		insQuery.prepare("INSERT INTO vms_scheduletask (id, source_id, start_time, end_time, do_record_audio, record_type, day_of_week, before_threshold, after_threshold, stream_quality, fps) VALUES\
					     (:id, :sourceId, :startTime, :endTime, :doRecordAudio, :recordType, :dayOfWeek, :beforeThreshold, :afterThreshold, :streamQuality, :fps)");
		task.autoBindValues(insQuery);

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
    query.prepare("UPDATE vms_resource set status = :status where id = :id");
    query.bindValue(":status", tran.params.status);
    query.bindValue(":id", tran.params.id);
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
    query.prepare("UPDATE vms_resource set disabled = :disabled where id = :id");
    query.bindValue(":disabled", tran.params.disabled);
    query.bindValue(":id", tran.params.id);
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }
    lock.commit();
    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransaction(const QnTransaction<ApiCameraData>& tran)
{
    QnDbTransactionLocker lock(&m_tran);

	ErrorCode result;
	if (tran.command == ApiCommand::updateCamera) {
		result = updateResource(tran.params);
		if (result !=ErrorCode::ok)
			return result;
		result = updateCamera(tran.params);
	}
	else {
		result = insertResource(tran.params);
		if (result !=ErrorCode::ok)
			return result;
		result = insertCamera(tran.params);
	}
	if (result !=ErrorCode::ok)
		return result;
	result = updateCameraSchedule(tran.params);
    if (result == ErrorCode::ok)
        lock.commit();
    return result;
}

ErrorCode QnDbManager::executeTransaction(const QnTransaction<ApiCameraDataList>& tran)
{
    QnDbTransactionLocker lock(&m_tran);
    ErrorCode result;
    foreach(const ApiCameraData& camera, tran.params.data)
    {
        result = updateResource(camera);
        if (result !=ErrorCode::ok)
            return result;
        result = updateCamera(camera);
        if (result !=ErrorCode::ok)
            return result;
        result = updateCameraSchedule(camera);
        if (result != ErrorCode::ok)
            return result;
    }
    lock.commit();
    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransaction(const QnTransaction<ApiResourceData>& tran)
{
    QnDbTransactionLocker lock(&m_tran);
    
    ErrorCode err = updateResource(tran.params);
    if (err != ErrorCode::ok)
        return err;

    lock.commit();
    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransaction(const QnTransaction<ApiBusinessRuleData>& tran)
{
    if( tran.command == ApiCommand::addBusinessRule )
        return insertBusinessRule(tran.params);
    else if( tran.command == ApiCommand::updateBusinessRule )
        return updateBusinessRule(tran.params);

    return ErrorCode::notImplemented;
}

ErrorCode QnDbManager::executeTransaction(const QnTransaction<ApiMediaServerData>& tran)
{
    QnDbTransactionLocker lock(&m_tran);

    ErrorCode result;
    if (tran.command == ApiCommand::updateMediaServer) {
        result = updateResource(tran.params);
        if (result !=ErrorCode::ok)
            return result;
        result = updateMediaServer(tran.params);
    }
    else {
        result = insertResource(tran.params);
        if (result !=ErrorCode::ok)
            return result;
        result = insertMediaServer(tran.params);
    }
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

ErrorCode QnDbManager::updateLayoutItems(const ApiLayoutData& data)
{
    ErrorCode result = removeLayoutItems(data.id);
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

        if (!insQuery.exec()) {
            qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
            return ErrorCode::failure;
        }
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::insertUser( const ApiUserData& data )
{
    //TODO/IMPL
    return ErrorCode::notImplemented;
}

ErrorCode QnDbManager::updateUser( const ApiUserData& data )
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

ErrorCode QnDbManager::removeUser( qint32 id )
{
    QnDbTransactionLocker tran(&m_tran);

    QSqlQuery query(m_sdb);
    query.prepare("SELECT resource_ptr_id FROM vms_layout where user_id = :id");
    query.bindValue(":id", id);
    if (!query.exec())
        return ErrorCode::failure;

    ErrorCode err;
    while (query.next()) {
        err = removeLayoutNoLock(query.value("resource_ptr_id").toInt());
        if (err != ErrorCode::ok)
            return err;
    }

    err = deleteAddParams(id);
    if (err != ErrorCode::ok)
        return err;

    err = deleteUserProfileTable(id);
    if (err != ErrorCode::ok)
        return err;

    err = deleteTableRecord(id, "auth_user", "id");
    if (err != ErrorCode::ok)
        return err;

    err = deleteResourceTable(id);
    if (err != ErrorCode::ok)
        return err;

    tran.commit();
    return ErrorCode::ok;
}

ErrorCode QnDbManager::insertBusinessRule( const ApiBusinessRuleData& /*businessRule*/ )
{
    //TODO/IMPL
    return ErrorCode::notImplemented;
}

ErrorCode QnDbManager::updateBusinessRule( const ApiBusinessRuleData& /*businessRule*/ )
{
    //TODO/IMPL
    return ErrorCode::notImplemented;
}

ErrorCode QnDbManager::removeBusinessRule( qint32 id )
{
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


ErrorCode QnDbManager::executeTransaction(const QnTransaction<ApiLayoutData>& tran)
{
    QnDbTransactionLocker lock(&m_tran);

    ErrorCode result;
    if (tran.command == ApiCommand::updateLayout) {
        result = updateResource(tran.params);
        if (result !=ErrorCode::ok)
            return result;
        result = updateLayout(tran.params);
    }
    else {
        result = insertResource(tran.params);
        if (result !=ErrorCode::ok)
            return result;
        result = insertLayout(tran.params);
    }
    if (result !=ErrorCode::ok)
        return result;
    result = updateLayoutItems(tran.params);
    if (result == ErrorCode::ok)
        lock.commit();

    return result;
}

ErrorCode QnDbManager::executeTransaction(const QnTransaction<ApiLayoutDataList>& tran)
{
    QnDbTransactionLocker lock(&m_tran);

    foreach(const ApiLayoutData& layout, tran.params.data)
    {
        ErrorCode err = insertOrReplaceResource(layout);
        if (err != ErrorCode::ok)
            return err;

        err = insertOrReplaceLayout(layout);
        if (err != ErrorCode::ok)
            return err;

        err = updateLayoutItems(layout);
        if (err != ErrorCode::ok)
            return err;
    }

    lock.commit();
    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransaction(const QnTransaction<ApiResourceParams>& tran)
{
    QnDbTransactionLocker lock(&m_tran);

    foreach(const ApiResourceParam& param, tran.params) 
    {
        ErrorCode result = removeAddParam(param);
        if (result != ErrorCode::ok)
            return result;
        result = insertAddParam(param);
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

ErrorCode QnDbManager::deleteTableRecord(qint32 id, const QString& tableName, const QString& fieldName)
{
    QSqlQuery delQuery(m_sdb);
    delQuery.prepare(QString("DELETE FROM %1 where %2 = :id").arg(tableName).arg(fieldName));
    delQuery.bindValue(QLatin1String(":id"), id);
    if (delQuery.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
        return ErrorCode::failure;
    }
}

ErrorCode QnDbManager::removeCamera(const qint32 id)
{
    QnDbTransactionLocker tran(&m_tran);

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

ErrorCode QnDbManager::removeServer(const qint32 id)
{
    QnDbTransactionLocker tran(&m_tran);

    ErrorCode err = deleteAddParams(id);
    if (err != ErrorCode::ok)
        return err;

    err = removeStoragesByServer(id);
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

ErrorCode QnDbManager::removeLayout(const qint32 id)
{
    QnDbTransactionLocker tran(&m_tran);
    ErrorCode err = removeLayoutNoLock(id);
    if (err == ErrorCode::ok)
        tran.commit();
    return err;
}

ErrorCode QnDbManager::removeLayoutNoLock(const qint32 id)
{
    ErrorCode err = deleteAddParams(id);
    if (err != ErrorCode::ok)
        return err;

    err = deleteLayoutItems(id);
    if (err != ErrorCode::ok)
        return err;

    err = deleteTableRecord(id, "vms_layout", "resource_ptr_id");
    if (err != ErrorCode::ok)
        return err;

    err = deleteResourceTable(id);
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
    if( tran.command == ApiCommand::addUser )
        return insertUser( tran.params );
    else if( tran.command == ApiCommand::updateUser )
        return updateUser( tran.params );

    return ErrorCode::notImplemented;
}

ErrorCode QnDbManager::removeResource(const qint32 id)
{
    QnDbTransactionLocker tran(&m_tran);

    QSqlQuery query(m_sdb);
    query.prepare("SELECT \
                    (CASE WHEN c.resource_ptr_id is null then rt.name else 'Camera' end) as name,\
                    FROM vms_resource r\
                    JOIN vms_resourcetype rt on rt.id = r.xtype_id\
                    LEFT JOIN vms_camera c on c.resource_ptr_id = r.id\
                    WHERE r.id = :id");
    query.bindValue(":id", id);
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

ErrorCode QnDbManager::doQuery(nullptr_t /*dummy*/, ApiResourceTypeList& data)
{
    QReadLocker lock(&m_mutex);

	QSqlQuery queryTypes(m_sdb);
	queryTypes.prepare("select rt.id, rt.name, m.name as manufacture \
				  from vms_resourcetype rt \
				  left join vms_manufacture m on m.id = rt.manufacture_id \
				  order by rt.id");
	if (!queryTypes.exec()) {
        qWarning() << Q_FUNC_INFO << queryTypes.lastError().text();
		return ErrorCode::failure;
    }

	QSqlQuery queryParents(m_sdb);
	queryParents.prepare("select from_resourcetype_id as id, to_resourcetype_id as parentId from vms_resourcetype_parents order by from_resourcetype_id, to_resourcetype_id desc");
	if (!queryParents.exec()) {
        qWarning() << Q_FUNC_INFO << queryParents.lastError().text();
		return ErrorCode::failure;
    }

    QSqlQuery queryProperty(m_sdb);
    queryProperty.prepare("SELECT * FROM vms_propertytype order by resource_type_id");
    if (!queryProperty.exec()) {
        qWarning() << Q_FUNC_INFO << queryProperty.lastError().text();
        return ErrorCode::failure;
    }

	data.loadFromQuery(queryTypes);
    mergeIdListData<ApiResourceTypeData>(queryParents, data.data, &ApiResourceTypeData::parentId);

    std::vector<ApiPropertyType> allProperties;
    QN_QUERY_TO_DATA_OBJECT(queryProperty, ApiPropertyType, allProperties, ApiPropertyTypeFields);
    mergeObjectListData<ApiResourceTypeData, ApiPropertyType>(data.data, allProperties, &ApiResourceTypeData::propertyTypeList, &ApiPropertyType::resource_type_id);

	return ErrorCode::ok;
}

// ----------- getLayouts --------------------

ErrorCode QnDbManager::doQuery(nullptr_t /*dummy*/, ApiLayoutDataList& layouts)
{
    QReadLocker lock(&m_mutex);

    QSqlQuery query(m_sdb);
    QString filter; // todo: add data filtering by user here
    query.prepare(QString("SELECT r.id, r.guid, r.xtype_id as typeId, r.parent_id as parentId, r.name, r.url, r.status,r. disabled, \
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
    queryItems.prepare("SELECT zoom_bottom as zoomBottom, right, uuid, zoom_left as zoomLeft, resource_id as resourceId, \
                       zoom_right as zoomRight, top, layout_id as layoutId, bottom, zoom_top as zoomTop, \
                       zoom_target_uuid as zoomTargetUuid, flags, contrast_params as contrastParams, rotation, id, \
                       dewarping_params as dewarpingParams, left FROM vms_layoutitem order by layout_id");

    if (!queryItems.exec()) {
        qWarning() << Q_FUNC_INFO << queryItems.lastError().text();
        return ErrorCode::failure;
    }

    layouts.loadFromQuery(query);
    std::vector<ApiLayoutItemData> items;
    QN_QUERY_TO_DATA_OBJECT(queryItems, ApiLayoutItemData, items, ApiLayoutItemDataFields);
    mergeObjectListData<ApiLayoutData, ApiLayoutItemData>(layouts.data, items, &ApiLayoutData::items, &ApiLayoutItemData::layoutId);

    return ErrorCode::ok;
}

// ----------- getCameras --------------------

ErrorCode QnDbManager::doQuery(const QnId& mServerId, ApiCameraDataList& cameraList)
{
    QReadLocker lock(&m_mutex);

	QSqlQuery queryCameras(m_sdb);
    QString filterStr;
	if (mServerId.isValid()) {
		filterStr = QString("WHERE r.parent_id = %1").arg(mServerId);
	}
	queryCameras.prepare(QString("SELECT r.id, r.guid, r.xtype_id as typeId, r.parent_id as parentId, r.name, r.url, r.status,r. disabled, \
		c.audio_enabled as audioEnabled, c.control_disabled as controlDisabled, c.firmware, c.vendor, c.manually_added as manuallyAdded, \
		c.region, c.schedule_disabled as scheduleDisabled, c.motion_type as motionType, \
		c.group_name as groupName, c.group_id as groupId, c.mac, c. model, c.secondary_quality as secondaryQuality, \
		c.status_flags as statusFlags, c.physical_id as physicalId, c.password, login, c.dewarping_params as dewarpingParams \
		FROM vms_resource r \
		JOIN vms_camera c on c.resource_ptr_id = r.id %1 ORDER BY r.id").arg(filterStr));


    QSqlQuery queryScheduleTask(m_sdb);
    QString filterStr2;
    if (mServerId.isValid()) 
        filterStr2 = QString("WHERE r.parent_id = %1").arg(mServerId);
    
    queryScheduleTask.prepare(QString("SELECT st.id, st.source_id as sourceId, st.start_time as startTime, st.end_time as endTime, st.do_record_audio as doRecordAudio, \
                                       st.record_type as recordType, st.day_of_week as dayOfWeek, st.before_threshold as beforeThreshold, st.after_threshold as afterThreshold, \
                                       st.stream_quality as streamQuality, st.fps \
                                       FROM vms_scheduletask st \
                                       JOIN vms_resource r on r.id = st.source_id %1 ORDER BY r.parent_id").arg(filterStr2));

    QSqlQuery queryParams(m_sdb);
    queryParams.prepare(QString("SELECT kv.resource_id as resourceId, kv.value, kv.name \
                                 FROM vms_kvpair kv \
                                 JOIN vms_camera c on c.resource_ptr_id = kv.resource_id \
                                 %1 ORDER BY r.id").arg(filterStr2));

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

    ScheduleTaskList sheduleTaskList;
    sheduleTaskList.loadFromQuery(queryScheduleTask);

    mergeObjectListData<ApiCameraData, ScheduleTask>(cameraList.data, sheduleTaskList.data, &ApiCameraData::scheduleTask, &ScheduleTask::sourceId);

    std::vector<ApiResourceParam> params;
    QN_QUERY_TO_DATA_OBJECT(queryParams, ApiResourceParam, params, ApiResourceParamFields);
    mergeObjectListData<ApiCameraData, ApiResourceParam>(cameraList.data, params, &ApiCameraData::addParams, &ApiResourceParam::resourceId);

	return ErrorCode::ok;
}

// ----------- getServers --------------------


ErrorCode QnDbManager::doQuery(nullptr_t /*dummy*/, ApiMediaServerDataList& serverList)
{
    QReadLocker lock(&m_mutex);

    QSqlQuery query(m_sdb);
    query.prepare(QString("select r.id, r.guid, r.xtype_id as typeId, r.parent_id as parentId, r.name, r.url, r.status,r. disabled, \
                          s.api_url as apiUrl, s.auth_key as authKey, s.streaming_url as streamingUrl, s.version, s.net_addr_list as netAddrList, s.reserve, s.panic_mode as panicMode \
                          from vms_resource r \
                          join vms_server s on s.resource_ptr_id = r.id order by r.id"));

    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }

    QSqlQuery queryStorage(m_sdb);
    queryStorage.prepare(QString("select r.id, r.guid, r.xtype_id as typeId, r.parent_id as parentId, r.name, r.url, r.status,r. disabled, \
                          s.space_limit as spaceLimit, used_for_writing as usedForWriting \
                          from vms_resource r \
                          join vms_storage s on s.resource_ptr_id = r.id order by r.parent_id"));

    if (!queryStorage.exec()) {
        qWarning() << Q_FUNC_INFO << queryStorage.lastError().text();
        return ErrorCode::failure;
    }

    serverList.loadFromQuery(query);

    ApiStorageDataList storageList;
    storageList.loadFromQuery(queryStorage);

    mergeObjectListData<ApiMediaServerData, ApiStorageData>(serverList.data, storageList.data, &ApiMediaServerData::storages, &ApiStorageData::parentId);

    return ErrorCode::ok;
}

//getCameraServerItems
ErrorCode QnDbManager::doQuery(nullptr_t /*dummy*/, ApiCameraServerItemDataList& historyList)
{
    QReadLocker lock(&m_mutex);

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
ErrorCode QnDbManager::doQuery(nullptr_t /*dummy*/, ApiUserDataList& userList)
{
    QReadLocker lock(&m_mutex);

    //digest = md5('%s:%s:%s' % (self.user.username.lower(), 'NetworkOptix', password)).hexdigest()
    QSqlQuery query(m_sdb);
    query.prepare(QString("select r.id, r.guid, r.xtype_id as typeId, r.parent_id as parentId, r.name, r.url, r.status,r. disabled, \
                          u.password, u.is_superuser as isAdmin, u.email, p.digest as digest, u.password as hash, p.rights \
                          from vms_resource r \
                          join auth_user u  on u.id = r.id\
                          join vms_userprofile p on p.user_id = u.id"));
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }

    QSqlQuery queryParams(m_sdb);
    queryParams.prepare(QString("SELECT kv.resource_id as resourceId, kv.value, kv.name \
                                FROM vms_kvpair kv \
                                JOIN auth_user u on u.id = kv.resource_id \
                                ORDER BY r.id"));

    if (!queryParams.exec()) {
        qWarning() << Q_FUNC_INFO << queryParams.lastError().text();
        return ErrorCode::failure;
    }

    userList.loadFromQuery(query);
    
    std::vector<ApiResourceParam> params;
    QN_QUERY_TO_DATA_OBJECT(queryParams, ApiResourceParam, params, ApiResourceParamFields);
    mergeObjectListData<ApiUserData, ApiResourceParam>(userList.data, params, &ApiCameraData::addParams, &ApiResourceParam::resourceId);
    
    return ErrorCode::ok;
}

//getBusinessRules
ErrorCode QnDbManager::doQuery(nullptr_t /*dummy*/, ApiBusinessRuleDataList& businessRuleList)
{
    QReadLocker lock(&m_mutex);

    QSqlQuery query(m_sdb);
    query.prepare(QString("SELECT id, event_type as eventType, event_condition as eventCondition, event_state as eventState, action_type as actionType, \
                          action_params as actionParams, aggregation_period as aggregationPeriod, disabled, comments, schedule \
                          FROM vms_businessrule order by id"));
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }

    QSqlQuery queryRuleEventRes(m_sdb);
    queryRuleEventRes.prepare(QString("SELECT businessrule_id as parentId, resource_id as id from vms_businessrule_event_resources order by businessrule_id"));
    if (!queryRuleEventRes.exec()) {
        qWarning() << Q_FUNC_INFO << queryRuleEventRes.lastError().text();
        return ErrorCode::failure;
    }

    QSqlQuery queryRuleActionRes(m_sdb);
    queryRuleActionRes.prepare(QString("SELECT businessrule_id as parentId, resource_id as id from vms_businessrule_action_resources order by businessrule_id"));
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
ErrorCode QnDbManager::doQuery(const QnId& resourceId, ApiResourceParams& params)
{
    QReadLocker lock(&m_mutex);

    QSqlQuery query(m_sdb);
    query.prepare(QString("SELECT kv.resource_id as resourceId, kv.value, kv.name \
                                FROM vms_kvpair kv \
                                JOIN vms_resource r on r.id = kv.resource_id WHERE r.id = :id ORDER BY r.id"));
    query.bindValue(QLatin1String(":id"), resourceId.toInt());
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }

    QN_QUERY_TO_DATA_OBJECT(query, ApiResourceParam, params, ApiResourceParamFields);
    return ErrorCode::ok;
}

// getCurrentTime
ErrorCode QnDbManager::doQuery(nullptr_t /*dummy*/, qint64& currentTime)
{
    currentTime = QDateTime::currentMSecsSinceEpoch();
    return ErrorCode::ok;
}

// ApiFullData
ErrorCode QnDbManager::doQuery(nullptr_t dummy, ApiFullData& data)
{
    QReadLocker lock(&m_mutex);
    ErrorCode err;

    if ((err = doQuery(dummy, data.resTypes)) != ErrorCode::ok)
        return err;

    if ((err = doQuery(dummy, data.servers)) != ErrorCode::ok)
        return err;

    if ((err = doQuery(dummy, data.cameras)) != ErrorCode::ok)
        return err;

    if ((err = doQuery(dummy, data.users)) != ErrorCode::ok)
        return err;

    if ((err = doQuery(dummy, data.layouts)) != ErrorCode::ok)
        return err;

    if ((err = doQuery(dummy, data.rules)) != ErrorCode::ok)
        return err;

    if ((err = doQuery(dummy, data.cameraHistory)) != ErrorCode::ok)
        return err;

    ApiResourceParams kvPairs;
    if ((err = doQuery(dummy, kvPairs)) != ErrorCode::ok)
        return err;

    mergeObjectListData<ApiMediaServerData, ApiResourceParam>(data.servers.data, kvPairs, &ApiMediaServerData::addParams, &ApiResourceParam::resourceId);
    mergeObjectListData<ApiCameraData,      ApiResourceParam>(data.cameras.data, kvPairs, &ApiCameraData::addParams,      &ApiResourceParam::resourceId);
    mergeObjectListData<ApiUserData,        ApiResourceParam>(data.users.data,   kvPairs, &ApiUserData::addParams,        &ApiResourceParam::resourceId);
    mergeObjectListData<ApiLayoutData,      ApiResourceParam>(data.layouts.data, kvPairs, &ApiLayoutData::addParams,      &ApiResourceParam::resourceId);

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
