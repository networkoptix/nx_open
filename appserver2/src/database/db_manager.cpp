#include "db_manager.h"
#include <QtSql/QtSql>

namespace ec2
{

static QnDbManager* globalInstance = 0;

QnDbManager::QnDbManager()
{
	m_sdb = QSqlDatabase::addDatabase("QSQLITE");
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

	return false;
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

}
