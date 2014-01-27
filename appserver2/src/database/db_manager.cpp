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
	// todo: implement me
	return false;
}

QnDbManager::~QnDbManager()
{
}

QnDbManager* QnDbManager::instance()
{
    return globalInstance;
}

void QnDbManager::initStaticInstance(QnDbManager* value)
{
    globalInstance = value;
}

ErrorCode QnDbManager::insertResource(ApiResourceData& data)
{
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT INTO vms_resource (id, guid, xtype_id, parent_id, name, url, status, disabled) VALUES(:id, :guid, :typeId, :parentId, :name, :url, :status, :disabled)");
    data.autoBindValues(insQuery);
	if (insQuery.exec()) {
		data.id = insQuery.lastInsertId().toInt();
		return ErrorCode::ok;
	}
	else {
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
	return insQuery.exec() ? ErrorCode::ok : ErrorCode::failure;
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

ErrorCode QnDbManager::executeTransaction( const QnTransaction<ApiCameraData>& tran)
{
	ErrorCode result;
	if (tran.params.id) {
		result = updateResource(tran.params);
		if (result !=ErrorCode::ok)
			return result;
		result = updateCamera(tran.params);
	}
	else {
		ApiCameraData params = tran.params;
		result = insertResource(params);
		if (result !=ErrorCode::ok)
			return result;
		result = insertCamera(params);
	}
    return result;
}

}
