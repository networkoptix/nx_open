#include "db_manager.h"
#include <QtSql/QtSql>

namespace ec2
{

static QnDbManager* globalInstance = 0;

QnDbManager::QnDbManager(QnResourceFactoryPtr factory)
{
	m_resourceFactory = factory;
	m_sdb = QSqlDatabase::addDatabase("QSQLITE", "QnDbManager");
	m_sdb.setDatabaseName("c:/develop/netoptix_new_ec/appserver/db/ecs.db");
	if (m_sdb.open())
	{
		if (!createDatabase()) // create tables is DB is empty
			qWarning() << "can't create tables for sqlLite database!";
	}
	else {
		qWarning() << "can't initialize sqlLite database! Actions log is not created!";
	}
}

bool QnDbManager::createDatabase()
{
	Q_ASSERT(!globalInstance);
	globalInstance = this;

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

ErrorCode QnDbManager::insertResource(const ApiResourceData& data)
{
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT INTO vms_resource (id, guid, xtype_id, parent_id, name, url, status, disabled) VALUES(:id, :guid, :typeId, :parentId, :name, :url, :status, :disabled)");
    data.autoBindValues(insQuery);
	if (insQuery.exec()) {
		//data.id = insQuery.lastInsertId().toInt();
		return ErrorCode::ok;
	}
	else {
		qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
		return ErrorCode::failure;
	}
}

ErrorCode QnDbManager::updateResource(const ApiResourceData& data)
{
	QSqlQuery insQuery(m_sdb);
	insQuery.prepare("UPDATE vms_resource SET guid = :guid, xtype_id = :xtype_id, parent_id = :parent_id, name = :name, url = :url, status = :status, disabled = :disabled WHERE id = :id");
	data.autoBindValues(insQuery);

	QMutexLocker lock(&m_mutex);
	return insQuery.exec() ? ErrorCode::ok : ErrorCode::failure;
}

ErrorCode QnDbManager::insertCamera(const ApiCameraData& data)
{
	QSqlQuery insQuery(m_sdb);
	insQuery.prepare("INSERT INTO vms_camera (audio_enabled, control_disabled, firmware, vendor, manually_added, resource_ptr_id, region, schedule_disabled, motion_type, group_name, group_id,\
					 mac, model, secondary_quality, status_flags, physical_id, password, login, dewarping_params) VALUES\
					 (:audioEnabled, :controlDisabled, :firmware, :vendor, :manuallyAdded, :id, :region, :scheduleDisabled, :motionType, :groupName, :groupId,\
					 :mac, :model, :secondaryQuality, :statusFlags, :physicalId, :password, :login, :dewarpingParams)");
	data.autoBindValues(insQuery);

	QMutexLocker lock(&m_mutex);
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
	QSqlQuery insQuery(m_sdb);
	insQuery.prepare("UPDATE vms_camera SET\
					 audio_enabled = :audioEnabled, control_disabled = :controlDisabled, firmware = :firmware, vendor = :vendor, manually_added = :manuallyAdded, region = :region,\
					 schedule_disabled = :scheduleDisabled, motion_type = :motionType, group_name = :groupName, group_id = :groupId,\
					 mac = :mac, model = :model, secondary_quality = :secondaryQuality, status_flags = :statusFlags, physical_id = :physicalId, \
					 password = :password, login = :login, dewarping_params = :dewarpingParams\
					 WHERE resource_ptr_id = :id");
	data.autoBindValues(insQuery);

	QMutexLocker lock(&m_mutex);
	return insQuery.exec() ? ErrorCode::ok : ErrorCode::failure;
}

ErrorCode QnDbManager::updateCameraSchedule(const ApiCameraData& data)
{
	QMutexLocker lock(&m_mutex);

	QSqlQuery delQuery(m_sdb);
	delQuery.prepare("DELETE FROM vms_scheduletask where source_id = :id");
	delQuery.bindValue("id", data.id);
	if (!delQuery.exec()) 
		return ErrorCode::failure;

	foreach(ScheduleTask task, data.scheduleTask) 
	{
		QSqlQuery insQuery(m_sdb);
		insQuery.prepare("INSERT INTO vms_scheduletask (id, source_id, start_time, end_time, do_record_audio, record_type, day_of_week, before_threshold, after_threshold, stream_quality, fps) VALUES\
					     (:id, :sourceId, :startTime, :endTime, :doRecordAudio, :recordType, :dayOfWeek, :beforeThreshold, :afterThreshold, :streamQuality, :fps)");
		task.autoBindValues(insQuery);

		if (!insQuery.exec()) 
			return ErrorCode::failure;
	}
	return ErrorCode::ok;
}

int QnDbManager::getNextSequence()
{
	QMutexLocker lock(&m_mutex);

	QSqlQuery query(m_sdb);
	query.prepare("update sqlite_sequence set seq = seq + 1 where name=\"vms_resource\"");
	if (!query.exec())
		return 0;
	QSqlQuery query2(m_sdb);
	query.prepare("select seq from sqlite_sequence where name=\"vms_resource\"");
	if (!query.exec())
		return 0;
	query.next();
	int result = query.value(0).toInt();
	return result;
}


ErrorCode QnDbManager::executeTransaction(const QnTransaction<ApiCameraData>& tran)
{
	ErrorCode result;
	if (tran.command == ec2::updateCamera) {
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
	updateCameraSchedule(tran.params);

    return result;
}

// -------------------- getters ----------------------------

// ----------- getResourceTypes --------------------

ErrorCode QnDbManager::doQuery(nullptr_t /*dummy*/, ApiResourceTypeList& data)
{
	QSqlQuery queryTypes(m_sdb);
	queryTypes.prepare("	select rt.id, rt.name, m.name as manufacture \
				  from vms_resourcetype rt \
				  join vms_manufacture m on m.id = rt.manufacture_id \
				  order by rt.id");
	if (!queryTypes.exec())
		return ErrorCode::failure;

	QSqlQuery queryParents(m_sdb);
	queryParents.prepare("select from_resourcetype_id as id, to_resourcetype_id as parentId from vms_resourcetype_parents order by from_resourcetype_id, to_resourcetype_id desc");

	if (!queryParents.exec())
		return ErrorCode::failure;

	data.loadFromQuery(queryTypes);

	int idx = 0;
	QSqlRecord rec = queryParents.record();
	int idIdx = rec.indexOf("id");
	int parentIdIdx = rec.indexOf("parentId");
	while (queryParents.next())
	{
		int id = queryParents.value(idIdx).toInt();
		int parentId = queryParents.value(parentIdIdx).toInt();

		for (; idx < data.data.size() && data.data[idx].id != id; idx++);
		if (idx == data.data.size())
			break;
		data.data[idx].parentId.push_back(parentId);
	}

	return ErrorCode::ok;
}

// ----------- getCameras --------------------

ErrorCode QnDbManager::doQuery(const QnId& mServerId, ApiCameraDataList& cameraList)
{
	QSqlQuery query(m_sdb);
    QString filterStr;
	if (mServerId.isValid()) {
		filterStr = QString("where r.parent_id = %1").arg(mServerId);
	}
	query.prepare(QString("select r.id, r.guid, r.xtype_id as typeId, r.parent_id as parentId, r.name, r.url, r.status,r. disabled, \
		c.audio_enabled as audioEnabled, c.control_disabled as controlDisabled, c.firmware, c.vendor, c.manually_added as manuallyAdded, \
		c.region, c.schedule_disabled as scheduleDisabled, c.motion_type as motionType, \
		c.group_name as groupName, c.group_id as groupId, c.mac, c. model, c.secondary_quality as secondaryQuality, \
		c.status_flags as statusFlags, c.physical_id as physicalId, c.password, login, c.dewarping_params as dewarpingParams \
		from vms_resource r \
		join vms_camera c on c.resource_ptr_id = r.id %1 order by r.id").arg(filterStr));


    QSqlQuery queryScheduleTask(m_sdb);
    QString filterStr2;
    if (mServerId.isValid()) 
        filterStr2 = QString("where r.parent_id = %1").arg(mServerId);
    
    queryScheduleTask.prepare(QString("SELECT st.id, st.source_id as sourceId, st.start_time as startTime, st.end_time as endTime, st.do_record_audio as doRecordAudio, \
                                       st.record_type as recordType, st.day_of_week as dayOfWeek, st.before_threshold as beforeThreshold, st.after_threshold as afterThreshold, \
                                       st.stream_quality as streamQuality, st.fps \
                                       from vms_scheduletask st \
                                       join vms_resource r on r.id = st.source_id %1 order by r.parent_id").arg(filterStr2));


	if (!query.exec())
		return ErrorCode::failure;
    if (!queryScheduleTask.exec())
        return ErrorCode::failure;

	cameraList.loadFromQuery(query);

    ScheduleTaskList sheduleTaskList;
    sheduleTaskList.loadFromQuery(queryScheduleTask);

    int idx = 0;
    foreach(const ScheduleTask& scheduleTaskData, sheduleTaskList.data)
    {
        for (; idx < cameraList.data.size() && scheduleTaskData.sourceId != cameraList.data[idx].id; idx++);
        if (idx == cameraList.data.size())
            break;
        cameraList.data[idx].scheduleTask.push_back(std::move(scheduleTaskData));
    }

	return ErrorCode::ok;
}

// ----------- getServers --------------------

ErrorCode QnDbManager::doQuery(nullptr_t /*dummy*/, ApiMediaServerDataList& serverList)
{
    QSqlQuery query(m_sdb);
    query.prepare(QString("select r.id, r.guid, r.xtype_id as typeId, r.parent_id as parentId, r.name, r.url, r.status,r. disabled, \
                          s.api_url as apiUrl, s.auth_key as authKey, s.streaming_url as streamingUrl, s.version, s.net_addr_list as netAddrList, s.reserve, s.panic_mode as panicMode \
                          from vms_resource r \
                          join vms_server s on s.resource_ptr_id = r.id order by r.id"));

    if (!query.exec())
        return ErrorCode::failure;

    QSqlQuery queryStorage(m_sdb);
    queryStorage.prepare(QString("select r.id, r.guid, r.xtype_id as typeId, r.parent_id as parentId, r.name, r.url, r.status,r. disabled, \
                          s.space_limit as spaceLimit, used_for_writing as usedForWriting \
                          from vms_resource r \
                          join vms_storage s on s.resource_ptr_id = r.id order by r.parent_id"));

    if (!queryStorage.exec())
        return ErrorCode::failure;

    serverList.loadFromQuery(query);

    ApiStorageDataList storageList;
    storageList.loadFromQuery(queryStorage);

    int idx = 0;
    foreach(const ApiStorageData& storageData, storageList.data)
    {
        for (; idx < serverList.data.size() && storageData.parentId != serverList.data[idx].id; idx++);
        if (idx == serverList.data.size())
            break;
        serverList.data[idx].storages.push_back(std::move(storageData));
    }

    return ErrorCode::ok;
}

//getCameraServerItems
ErrorCode QnDbManager::doQuery(nullptr_t /*dummy*/, ApiCameraServerItemDataList& historyList)
{
    QSqlQuery query(m_sdb);
    query.prepare(QString("select server_guid as serverGuid, timestamp, physical_id as physicalId from vms_cameraserveritem"));
    if (!query.exec())
        return ErrorCode::failure;

    historyList.loadFromQuery(query);

    return ErrorCode::ok;
}

}
