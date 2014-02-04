#include "db_manager.h"
#include <QtSql/QtSql>
#include "nx_ec/data/ec2_business_rule_data.h"

namespace ec2
{

static QnDbManager* globalInstance = 0;


template <class MainData>
void mergeIdListData(QSqlQuery& query, std::vector<MainData>& data, std::vector<qint32> MainData::*subList)
{
    int idx = 0;
    QSqlRecord rec = query.record();
    int idIdx = rec.indexOf("parentId");
    int resourceIdIdx = rec.indexOf("id");
    while (query.next())
    {
        int id = query.value(idIdx).toInt();
        int resourceId = query.value(resourceIdIdx).toInt();

        for (; idx < data.size() && data[idx].id != id; idx++);
        if (idx == data.size())
            break;
        (data[idx].*subList).push_back(resourceId);
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

QnDbManager::QnDbManager(QnResourceFactory* factory)
{
    m_storageTypeId = 0;
    m_resourceFactory = factory;
	m_sdb = QSqlDatabase::addDatabase("QSQLITE", "QnDbManager");
	m_sdb.setDatabaseName("c:/develop/netoptix_vms/appserver/db/ecs.db");
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

ErrorCode QnDbManager::updateResource(const ApiResourceData& data, bool hasAddParams)
{
	QSqlQuery insQuery(m_sdb);
	insQuery.prepare("UPDATE vms_resource SET guid = :guid, xtype_id = :typeId, parent_id = :parentId, name = :name, url = :url, status = :status, disabled = :disabled WHERE id = :id");
	data.autoBindValues(insQuery);

    if (!insQuery.exec()) {
        qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
        return ErrorCode::failure;
    }

    // hasAddParams used for optimization
    if (hasAddParams)
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

ErrorCode QnDbManager::updateStorages(const ApiMediaServerData& data)
{
    QSqlQuery delQuery(m_sdb);
    delQuery.prepare("DELETE FROM vms_storage WHERE resource_ptr_id in (select id from vms_resource where parent_id = :id and xtype_id=:typeId)");
    delQuery.bindValue("id", data.id);
    delQuery.bindValue("typeId", m_storageTypeId);
    if (!delQuery.exec()) {
        qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
        return ErrorCode::failure;
    }

    QSqlQuery delQuery2(m_sdb);
    delQuery2.prepare("DELETE FROM vms_resource WHERE parent_id = :id and xtype_id=:typeId");
    delQuery2.bindValue("id", data.id);
    delQuery2.bindValue("typeId", m_storageTypeId);
    if (!delQuery2.exec()) {
        qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
        return ErrorCode::failure;
    }

    foreach(const ApiStorageData& storage, data.storages)
    {
        ErrorCode result = insertResource(storage);
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
	delQuery.bindValue("id", data.id);
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
	QMutexLocker lock(&m_mutex);

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
    QMutexLocker lock(&m_mutex);

    QSqlQuery query(m_sdb);
    query.prepare("UPDATE vms_resource set status = :status where id = :id");
    query.bindValue(":status", tran.params.status);
    query.bindValue(":id", tran.params.id);
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }
    
    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransaction(const QnTransaction<ApiCameraData>& tran)
{
    QMutexLocker lock(&m_mutex);

	ErrorCode result;
	if (tran.command == ApiCommand::updateCamera) {
		result = updateResource(tran.params, true);
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

    return result;
}

ErrorCode QnDbManager::executeTransaction(const QnTransaction<ApiMediaServerData>& tran)
{
    QMutexLocker lock(&m_mutex);

    ErrorCode result;
    if (tran.command == ApiCommand::updateMediaServer) {
        result = updateResource(tran.params, false);
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

	data.loadFromQuery(queryTypes);

    mergeIdListData<ApiResourceTypeData>(queryParents, data.data, &ApiResourceTypeData::parentId);

	return ErrorCode::ok;
}

// ----------- getCameras --------------------

ErrorCode QnDbManager::doQuery(const QnId& mServerId, ApiCameraDataList& cameraList)
{
	QSqlQuery query(m_sdb);
    QString filterStr;
	if (mServerId.isValid()) {
		filterStr = QString("WHERE r.parent_id = %1").arg(mServerId);
	}
	query.prepare(QString("SELECT r.id, r.guid, r.xtype_id as typeId, r.parent_id as parentId, r.name, r.url, r.status,r. disabled, \
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
                                 JOIN vms_resource r on r.id = kv.resource_id %1 ORDER BY r.id").arg(filterStr2));

	if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
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

	cameraList.loadFromQuery(query);

    ScheduleTaskList sheduleTaskList;
    sheduleTaskList.loadFromQuery(queryScheduleTask);

    std::vector<ApiResourceParam> params;
    QN_QUERY_TO_DATA_OBJECT(ApiResourceParam, params, ApiResourceParamFields);

    mergeObjectListData<ApiCameraData, ScheduleTask>(cameraList.data, sheduleTaskList.data, &ApiCameraData::scheduleTask, &ScheduleTask::sourceId);
    mergeObjectListData<ApiCameraData, ApiResourceParam>(cameraList.data, params, &ApiCameraData::addParams, &ApiResourceParam::resourceId);

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

    userList.loadFromQuery(query);

    return ErrorCode::ok;
}

//getBusinessRules
ErrorCode QnDbManager::doQuery(nullptr_t /*dummy*/, ApiBusinessRuleDataList& businessRuleList)
{
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

}
