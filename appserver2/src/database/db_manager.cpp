
#include "db_manager.h"

#include "version.h"

#include <QtSql/QtSql>

#include "common/common_module.h"
#include "managers/impl/license_manager_impl.h"
#include "nx_ec/data/ec2_business_rule_data.h"


namespace ec2
{

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

template <class MainData, class SubData, class MainSubData, class MainOrParentType, class IdType, class SubOrParentType>
void mergeObjectListData(std::vector<MainData>& data, std::vector<SubData>& subDataList, std::vector<MainSubData> MainOrParentType::*subDataListField, IdType SubOrParentType::*parentIdField)
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

QnId QnDbManager::getType(const QString& typeName)
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare("select guid from vms_resourcetype where name = ?");
    query.bindValue(0, typeName);
    bool rez = query.exec();
    Q_ASSERT(rez);
    if (query.next())
        return QnId::fromRfc4122(query.value("guid").toByteArray());
    return QnId();
}

QnDbManager::QnDbManager(
    QnResourceFactory* factory,
    LicenseManagerImpl* const licenseManagerImpl,
    const QString& dbFilePath )
:
    QnDbHelper(),
    m_licenseManagerImpl( licenseManagerImpl )
{
    m_resourceFactory = factory;
	m_sdb = QSqlDatabase::addDatabase("QSQLITE", "QnDbManager");
    m_sdb.setDatabaseName( closeDirPath(dbFilePath) + QString::fromLatin1("ecs.sqlite"));
	Q_ASSERT(!globalInstance);
	globalInstance = this;
}

bool QnDbManager::init()
{
	if (!m_sdb.open())
	{
        qWarning() << "can't initialize sqlLite database! Actions log is not created!";
        return false;
    }

	if (!createDatabase())  { 
        // create tables is DB is empty
		qWarning() << "can't create tables for sqlLite database!";
        return false;
    }

    m_storageTypeId = getType("Storage");
    m_serverTypeId = getType("Server");
    m_cameraTypeId = getType("Camera");

    QSqlQuery queryAdminUser(m_sdb);
    queryAdminUser.setForwardOnly(true);
    queryAdminUser.prepare("SELECT r.guid, r.id FROM vms_resource r JOIN auth_user u on u.id = r.id and r.name = 'admin'");
    bool execRez = queryAdminUser.exec();
    Q_ASSERT(execRez);
    if (queryAdminUser.next()) {
        m_adminUserID = QnId::fromRfc4122(queryAdminUser.value(0).toByteArray());
        m_adminUserInternalID = queryAdminUser.value(1).toInt();
    }


    QSqlQuery queryServers(m_sdb);
    queryServers.prepare("UPDATE vms_resource set status = ? WHERE xtype_guid = ?"); // todo: only mserver without DB?
    queryServers.bindValue(0, QnResource::Offline);
    queryServers.bindValue(1, m_serverTypeId.toRfc4122());
    bool rez = queryServers.exec();
    Q_ASSERT(rez);

    QSqlQuery queryCameras(m_sdb);
    // select cameras from media servers without DB and local cameras
    queryCameras.setForwardOnly(true);
    queryCameras.prepare("SELECT r.guid FROM vms_resource r \
                          JOIN vms_camera c on c.resource_ptr_id = r.id \
                          JOIN vms_resource sr on sr.guid = r.parent_guid \
                          JOIN vms_server s on s.resource_ptr_id = sr.id \
                          WHERE r.status != ? AND ((s.flags & 2) or sr.guid = ?)");
    queryCameras.bindValue(0, QnResource::Offline);
    queryCameras.bindValue(1, qnCommon->moduleGUID().toRfc4122());
    if (!queryCameras.exec()) {
        qWarning() << Q_FUNC_INFO << __LINE__ << queryCameras.lastError();
        Q_ASSERT(0);
    }
    while (queryCameras.next()) 
    {
        QnTransaction<ApiSetResourceStatusData> tran(ApiCommand::setResourceStatus, true);
        tran.fillSequence();
        tran.params.id = QnId::fromRfc4122(queryCameras.value(0).toByteArray());
        tran.params.status = QnResource::Offline;
        executeTransactionNoLock(tran);
        QByteArray serializedTran;
        OutputBinaryStream<QByteArray> stream(&serializedTran);
        serialize(tran, &stream);
        transactionLog->saveTransaction(tran, serializedTran);
    }

    return true;
}

QMap<int, QnId> QnDbManager::getGuidList(const QString& request)
{
    QMap<int, QnId>  result;
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare(request);
    if (!query.exec())
        return result;

    while (query.next())
    {
        qint32 id = query.value(0).toInt();
        QVariant data = query.value(1);
        if (data.toString().length() <= 10 && data.toInt())
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
#ifdef DB_DEBUG
    int n = guids.size();
    qDebug() << "updating table guids" << n << "commands queued";
    int i = 0;
#endif // DB_DEBUG
    for(QMap<int, QnId>::const_iterator itr = guids.begin(); itr != guids.end(); ++itr)
    {
#ifdef DB_DEBUG
        qDebug() << QString(QLatin1String("processing guid %1 of %2")).arg(++i).arg(n);
#endif // DB_DEBUG
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
        if (!execSQLFile(QLatin1String(":/01_createdb.sql")))
            return false;

#ifdef EDGE_SERVER
        if (!execSQLFile(QLatin1String(":/02_insert_3thparty_vendor.sql")))
            return false;
#else
        if (!execSQLFile(QLatin1String(":/02_insert_all_vendors.sql")))
            return false;
#endif

        if (!execSQLFile(QLatin1String(":/03_update_2.2_stage1.sql")))
            return false;
        if (!updateGuids())
            return false;
        if (!execSQLFile(QLatin1String(":/04_update_2.2_stage2.sql")))
            return false;

        { //Videowall-related scripts
            if (!execSQLFile(QLatin1String(":/05_videowall.sql")))
                return false;
            QMap<int, QnId> guids = getGuidList("SELECT rt.id, rt.name || '-' as guid from vms_resourcetype rt WHERE rt.name == 'Videowall'");
            if (!updateTableGuids("vms_resourcetype", "guid", guids))
                return false;
        }

    }
    lock.commit();
#ifdef DB_DEBUG
    qDebug() << "database created successfully";
#endif // DB_DEBUG
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

ErrorCode QnDbManager::insertAddParams(const std::vector<ApiResourceParam>& params, qint32 internalId)
{
    QSqlQuery insQuery(m_sdb);
    //insQuery.prepare("INSERT INTO vms_kvpair (resource_id, name, value) VALUES(:resourceId, :name, :value)");
    insQuery.prepare("INSERT OR REPLACE INTO vms_kvpair VALUES(?, NULL, ?, ?, ?)");
    insQuery.bindValue(0, internalId);
    foreach(const ApiResourceParam& param, params) {
        param.autoBindValuesOrdered(insQuery, 1);
        if (!insQuery.exec()) {
            qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
            return ErrorCode::failure;
        }        
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::deleteAddParams(qint32 resourceId)
{
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("DELETE FROM vms_kvpair WHERE resource_id = ?");
    insQuery.addBindValue(resourceId);
    if (insQuery.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
        return ErrorCode::failure;
    }
}

ErrorCode QnDbManager::insertResource(const ApiResource& data, qint32* internalId)
{
    QSqlQuery insQuery(m_sdb);
    //insQuery.prepare("INSERT INTO vms_resource (guid, xtype_guid, parent_guid, name, url, status, disabled) VALUES(:id, :typeId, :parentGuid, :name, :url, :status, :disabled)");
    //data.autoBindValues(insQuery);
    insQuery.prepare("INSERT INTO vms_resource VALUES(NULL, ?,?,?,?,?,?,?)");
    data.autoBindValuesOrdered(insQuery, 0);
	if (!insQuery.exec()) {
		qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
		return ErrorCode::failure;
	}
    *internalId = insQuery.lastInsertId().toInt();

    return insertAddParams(data.addParams, *internalId);
}

qint32 QnDbManager::getResourceInternalId( const QnId& guid ) {
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare("SELECT id from vms_resource where guid = ?");
    query.bindValue(0, guid.toRfc4122());
    if (!query.exec() || !query.next())
        return 0;
    return query.value(0).toInt();
}

QnId QnDbManager::getResourceGuid(const qint32 &internalId) {
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare("SELECT guid from vms_resource where id = ?");
    query.bindValue(0, internalId);
    if (!query.exec() || !query.next())
        return QnId();
    return QnId::fromRfc4122(query.value(0).toByteArray());
}

ErrorCode QnDbManager::insertOrReplaceResource(const ApiResource& data, qint32* internalId)
{
    *internalId = getResourceInternalId(data.id);

    QSqlQuery query(m_sdb);
    if (*internalId) {
        query.prepare("UPDATE vms_resource SET guid = :id, xtype_guid = :typeId, parent_guid = :parentGuid, name = :name, url = :url, status = :status, disabled = :disabled WHERE id = :internalID");
        query.bindValue(":internalID", *internalId);
    }
    else {
        query.prepare("INSERT OR REPLACE INTO vms_resource (guid, xtype_guid, parent_guid, name, url, status, disabled) VALUES(:id, :typeId, :parentGuid, :name, :url, :status, :disabled)");
    }
    data.autoBindValues(query);


    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }
    if (*internalId == 0)
        *internalId = query.lastInsertId().toInt();

    if (!data.addParams.empty()) 
    {
        /*
        ErrorCode result = deleteAddParams(*internalId);
        if (result != ErrorCode::ok)
            return result;
        */
        ErrorCode result = insertAddParams(data.addParams, *internalId);
        if (result != ErrorCode::ok)
            return result;
    }

    return ErrorCode::ok;
}

ErrorCode QnDbManager::updateResource(const ApiResource& data, qint32 internalId)
{
	QSqlQuery insQuery(m_sdb);

	insQuery.prepare("UPDATE vms_resource SET xtype_guid = :typeId, parent_guid = :parentGuid, name = :name, url = :url, status = :status, disabled = :disabled WHERE id = :internalId");
	data.autoBindValues(insQuery);
    insQuery.bindValue(":internalId", internalId);

    if (!insQuery.exec()) {
        qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
        return ErrorCode::failure;
    }

    if (!data.addParams.empty()) 
    {
        /*
        ErrorCode result = deleteAddParams(internalId);
        if (result != ErrorCode::ok)
            return result;
        */
        ErrorCode result = insertAddParams(data.addParams, internalId);
        if (result != ErrorCode::ok)
            return result;
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::insertOrReplaceUser(const ApiUser& data, qint32 internalId)
{
/*        
        "select r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentGuid, r.name, r.url, r.status,r. disabled, \
                          u.is_superuser as isAdmin, u.email, p.digest as digest, u.password as hash, p.rights \
                          from vms_resource r \
                          join auth_user u  on u.id = r.id\
                          join vms_userprofile p on p.user_id = u.id\
                          order by r.id"));
*/


    QSqlQuery insQuery(m_sdb);
    if (!data.hash.isEmpty())
        insQuery.prepare("INSERT OR REPLACE INTO auth_user (id, username, is_superuser, email, password, is_staff, is_active, last_login, date_joined) VALUES (:internalId, :name, :isAdmin, :email, :hash, 1, 1, '', '')");
    else
        insQuery.prepare("UPDATE auth_user SET is_superuser=:isAdmin, email=:email where username=:name");
    data.autoBindValues(insQuery);
    insQuery.bindValue(":internalId", internalId);
    insQuery.bindValue(":name", data.name);
    if (!insQuery.exec())
    {
        qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
        return ErrorCode::failure;
    }

    QSqlQuery insQuery2(m_sdb);
    if (!data.digest.isEmpty())
        insQuery2.prepare("INSERT OR REPLACE INTO vms_userprofile (user_id, resource_ptr_id, digest, rights) VALUES (:internalId, :internalId, :digest, :rights)");
    else
        insQuery2.prepare("UPDATE vms_userprofile SET rights=:rights WHERE user_id=:internalId");
    data.autoBindValues(insQuery2);
    insQuery2.bindValue(":internalId", internalId);
    if (!insQuery2.exec())
    {
        qWarning() << Q_FUNC_INFO << insQuery2.lastError().text();
        return ErrorCode::failure;
    }

    return ErrorCode::ok;
}

ErrorCode QnDbManager::insertOrReplaceCamera(const ApiCamera& data, qint32 internalId)
{
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT OR REPLACE INTO vms_camera (audio_enabled, control_disabled, firmware, vendor, manually_added, resource_ptr_id, region, schedule_disabled, motion_type, group_name, group_id,\
                     mac, model, secondary_quality, status_flags, physical_id, password, login, dewarping_params, resource_ptr_id) VALUES\
                     (:audioEnabled, :controlDisabled, :firmware, :vendor, :manuallyAdded, :id, :region, :scheduleDisabled, :motionType, :groupName, :groupId,\
                     :mac, :model, :secondaryQuality, :statusFlags, :physicalId, :password, :login, :dewarpingParams, :internalId)");
    data.autoBindValues(insQuery);
    insQuery.bindValue(":internalId", internalId);
    if (insQuery.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
        return ErrorCode::failure;
    }
}

ErrorCode QnDbManager::insertOrReplaceMediaServer(const ApiMediaServer& data, qint32 internalId)
{
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT OR REPLACE INTO vms_server (api_url, auth_key, streaming_url, version, system_info, net_addr_list, flags, panic_mode, resource_ptr_id) VALUES\
                     (:apiUrl, :authKey, :streamingUrl, :version, :systemInfo, :netAddrList, :flags, :panicMode, :internalId)");
    data.autoBindValues(insQuery);
    insQuery.bindValue(":internalId", internalId);
    if (insQuery.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
        return ErrorCode::failure;
    }
}


ErrorCode QnDbManager::insertOrReplaceLayout(const ApiLayout& data, qint32 internalId)
{
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT OR REPLACE INTO vms_layout \
                     (user_can_edit, cell_spacing_height, locked, \
                     cell_aspect_ratio, user_id, background_width, \
                     background_image_filename, background_height, \
                     cell_spacing_width, background_opacity, resource_ptr_id) \
                     \
                     VALUES (:userCanEdit, :cellSpacingHeight, :locked, \
                     :cellAspectRatio, :userId, :backgroundWidth, \
                     :backgroundImageFilename, :backgroundHeight, \
                     :cellSpacingWidth, :backgroundOpacity, :internalId)");
    data.autoBindValues(insQuery);
    insQuery.bindValue(":internalId", internalId);
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

ErrorCode QnDbManager::updateStorages(const ApiMediaServer& data)
{
    ErrorCode result = removeStoragesByServer(data.id);
    if (result != ErrorCode::ok)
        return result;
    
    foreach(const ApiStorage& storage, data.storages)
    {
        qint32 internalId;
        result = insertResource(storage, &internalId);
        if (result != ErrorCode::ok)
            return result;

        QSqlQuery insQuery(m_sdb);
        insQuery.prepare("INSERT INTO vms_storage (space_limit, used_for_writing, resource_ptr_id) VALUES\
                         (:spaceLimit, :usedForWriting, :internalId)");
        storage.autoBindValues(insQuery);
        insQuery.bindValue(":internalId", internalId);

        if (!insQuery.exec()) {
            qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
            return ErrorCode::failure;
        }
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::updateCameraSchedule(const ApiCamera& data, qint32 internalId)
{
	QSqlQuery delQuery(m_sdb);
	delQuery.prepare("DELETE FROM vms_scheduletask where source_id = ?");
	delQuery.addBindValue(internalId);
	if (!delQuery.exec()) {
        qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
        return ErrorCode::failure;
    }

    QSqlQuery insQuery(m_sdb);
    //insQuery.prepare("INSERT INTO vms_scheduletask (source_id, start_time, end_time, do_record_audio, record_type, day_of_week, before_threshold, after_threshold, stream_quality, fps) VALUES\
                     //:sourceId, :startTime, :endTime, :doRecordAudio, :recordType, :dayOfWeek, :beforeThreshold, :afterThreshold, :streamQuality, :fps)");
    insQuery.prepare("INSERT INTO vms_scheduletask VALUES (NULL, ?,?,?,?,?,?,?,?,?,?)");
    insQuery.bindValue(0, internalId);
	foreach(const ScheduleTask& task, data.scheduleTask) 
	{
		task.autoBindValuesOrdered(insQuery, 1);
		if (!insQuery.exec()) {
            qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
			return ErrorCode::failure;
        }
	}
	return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransactionNoLock(const QnTransaction<ApiSetResourceStatusData>& tran)
{
    QSqlQuery query(m_sdb);
    query.prepare("UPDATE vms_resource set status = :status where guid = :guid");
    query.bindValue(":status", tran.params.status);
    query.bindValue(":guid", tran.params.id.toRfc4122());
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransactionNoLock(const QnTransaction<ApiSetResourceDisabledData>& tran)
{
    QSqlQuery query(m_sdb);
    query.prepare("UPDATE vms_resource set disabled = :disabled where guid = :guid");
    query.bindValue(":disabled", tran.params.disabled);
    query.bindValue(":guid", tran.params.id.toRfc4122());
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::saveCamera(const ApiCamera& params)
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

ErrorCode QnDbManager::executeTransactionNoLock(const QnTransaction<ApiCamera>& tran)
{
    return saveCamera(tran.params);
}

ErrorCode QnDbManager::executeTransactionNoLock(const QnTransaction<ApiCameraList>& tran)
{
    foreach(const ApiCamera& camera, tran.params.data)
    {
        ErrorCode result = saveCamera(camera);
        if (result != ErrorCode::ok)
            return result;
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransactionNoLock(const QnTransaction<ApiResource>& tran)
{
    qint32 internalId = getResourceInternalId(tran.params.id);
    ErrorCode err = updateResource(tran.params, internalId);
    if (err != ErrorCode::ok)
        return err;

    return ErrorCode::ok;
}

ErrorCode QnDbManager::insertBRuleResource(const QString& tableName, const QnId& ruleGuid, const QnId& resourceGuid)
{
    QSqlQuery query(m_sdb);
    query.prepare(QString("INSERT INTO %1 (businessrule_guid, resource_guid) VALUES (:ruleGuid, :resourceGuid)").arg(tableName));
    query.bindValue(":ruleGuid", ruleGuid.toRfc4122());
    query.bindValue(":resourceGuid", resourceGuid.toRfc4122());
    if (query.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }
}

ErrorCode QnDbManager::updateBusinessRule(const ApiBusinessRule& rule)
{
    ErrorCode rez = insertOrReplaceBusinessRuleTable(rule);
    if (rez != ErrorCode::ok)
        return rez;

    ErrorCode err = deleteTableRecord(rule.id, "vms_businessrule_action_resources", "businessrule_guid");
    if (err != ErrorCode::ok)
        return err;

    err = deleteTableRecord(rule.id, "vms_businessrule_event_resources", "businessrule_guid");
    if (err != ErrorCode::ok)
        return err;

    foreach(const QnId& resourceId, rule.eventResource) {
        err = insertBRuleResource("vms_businessrule_event_resources", rule.id, resourceId);
        if (err != ErrorCode::ok)
            return err;
    }

    foreach(const QnId& resourceId, rule.actionResource) {
        err = insertBRuleResource("vms_businessrule_action_resources", rule.id, resourceId);
        if (err != ErrorCode::ok)
            return err;
    }

    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransactionNoLock(const QnTransaction<ApiBusinessRule>& tran)
{
    return updateBusinessRule(tran.params);
}

ErrorCode QnDbManager::executeTransactionNoLock(const QnTransaction<ApiMediaServer>& tran)
{
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

ErrorCode QnDbManager::updateLayoutItems(const ApiLayout& data, qint32 internalLayoutId)
{
    ErrorCode result = removeLayoutItems(internalLayoutId);
    if (result != ErrorCode::ok)
        return result;

    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT INTO vms_layoutitem (zoom_bottom, right, uuid, zoom_left, resource_id, \
                     zoom_right, top, layout_id, bottom, zoom_top, \
                     zoom_target_uuid, flags, contrast_params, rotation, \
                     dewarping_params, left) VALUES \
                     (:zoomBottom, :right, :uuid, :zoomLeft, :resourceId, \
                     :zoomRight, :top, :layoutId, :bottom, :zoomTop, \
                     :zoomTargetUuid, :flags, :contrastParams, :rotation, \
                     :dewarpingParams, :left)");
    foreach(const ApiLayoutItem& item, data.items)
    {
        item.autoBindValues(insQuery);
        insQuery.bindValue(":layoutId", internalLayoutId);

        if (!insQuery.exec()) {
            qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
            return ErrorCode::failure;
        }
    }
    return ErrorCode::ok;
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

qint32 QnDbManager::getBusinessRuleInternalId( const QnId& guid )
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare("SELECT id from vms_businessrule where guid = :guid");
    query.bindValue(":guid", guid.toRfc4122());
    if (!query.exec() || !query.next())
        return 0;
    return query.value("id").toInt();
}

ErrorCode QnDbManager::removeUser( const QnId& guid )
{
    qint32 internalId = getResourceInternalId(guid);

    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare("SELECT resource_ptr_id FROM vms_layout where user_id = :id");
    query.bindValue(":id", internalId);
    if (!query.exec())
        return ErrorCode::failure;

    ErrorCode err;
    while (query.next()) {
        qint32 layoutInternalId = query.value("resource_ptr_id").toInt();
        QnId layoutId = getResourceGuid(layoutInternalId);
        err = removeLayoutInternal(layoutId, layoutInternalId);
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

    err = deleteRecordFromResourceTable(internalId);
    if (err != ErrorCode::ok)
        return err;

    return ErrorCode::ok;
}

ErrorCode QnDbManager::insertOrReplaceBusinessRuleTable( const ApiBusinessRule& businessRule)
{
    QSqlQuery query(m_sdb);
    query.prepare(QString("INSERT OR REPLACE INTO vms_businessrule (guid, event_type, event_condition, event_state, action_type, \
                          action_params, aggregation_period, disabled, comments, schedule, system) VALUES \
                          (:id, :eventType, :eventCondition, :eventState, :actionType, \
                          :actionParams, :aggregationPeriod, :disabled, :comments, :schedule, :system)"));
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
    ErrorCode err = deleteTableRecord(guid, "vms_businessrule_action_resources", "businessrule_guid");
    if (err != ErrorCode::ok)
        return err;

    err = deleteTableRecord(guid, "vms_businessrule_event_resources", "businessrule_guid");
    if (err != ErrorCode::ok)
        return err;

    err = deleteTableRecord(guid, "vms_businessrule", "guid");
    if (err != ErrorCode::ok)
        return err;

    return ErrorCode::ok;
}


ErrorCode QnDbManager::saveLayout(const ApiLayout& params)
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

ErrorCode QnDbManager::executeTransactionNoLock(const QnTransaction<ApiLayout>& tran)
{
    ErrorCode result = saveLayout(tran.params);
    return result;
}

ErrorCode QnDbManager::executeTransactionNoLock(const QnTransaction<ApiLayoutList>& tran)
{
    foreach(const ApiLayout& layout, tran.params.data)
    {
        ErrorCode err = saveLayout(layout);
        if (err != ErrorCode::ok)
            return err;
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransactionNoLock(const QnTransaction<ApiVideowall>& tran) {
    ErrorCode result = saveVideowall(tran.params);
    return result;
}

ErrorCode QnDbManager::executeTransactionNoLock(const QnTransaction<ApiVideowallList>& tran) {
    foreach(const ApiVideowall& videowall, tran.params.data)
    {
        ErrorCode err = saveVideowall(videowall);
        if (err != ErrorCode::ok)
            return err;
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransactionNoLock(const QnTransaction<ApiUpdateUploadResponceData> &tran) {
    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransactionNoLock(const QnTransaction<ApiResourceParams>& tran)
{
    qint32 internalId = getResourceInternalId(tran.params.id);
    /*
    ErrorCode result = deleteAddParams(internalId);
    if (result != ErrorCode::ok)
        return result;
    */
    return insertAddParams(tran.params.params, internalId);
}

ErrorCode QnDbManager::executeTransactionNoLock(const QnTransaction<ApiCameraServerItem>& tran)
{
    QSqlQuery lastHistory(m_sdb);
    lastHistory.prepare("SELECT server_guid, max(timestamp) FROM vms_cameraserveritem WHERE physical_id = ? AND timestamp < ?");
    lastHistory.addBindValue(tran.params.physicalId);
    lastHistory.addBindValue(tran.params.timestamp);
    if (!lastHistory.exec()) {
        qWarning() << Q_FUNC_INFO << lastHistory.lastError().text();
        return ErrorCode::failure;
    }
    if (lastHistory.next() && lastHistory.value(0).toString() == tran.params.serverGuid)
        return ErrorCode::skipped;

    QSqlQuery query(m_sdb);
    query.prepare("INSERT INTO vms_cameraserveritem (server_guid, timestamp, physical_id) VALUES(:serverGuid, :timestamp, :physicalId)");
    tran.params.autoBindValues(query);
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }

    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransactionNoLock(const QnTransaction<ApiPanicModeData>& tran)
{
    QSqlQuery query(m_sdb);
    query.prepare("UPDATE vms_server SET panic_mode = :mode");
    query.bindValue(QLatin1String(":mode"), (int) tran.params.mode);
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }

    return ErrorCode::ok;
}

ErrorCode QnDbManager::deleteRecordFromResourceTable(const qint32 id)
{
    QSqlQuery delQuery(m_sdb);
    delQuery.prepare("DELETE FROM vms_resource where id = ?");
    delQuery.addBindValue(id);
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
    query.setForwardOnly(true);
    query.prepare("select c.physical_id , (select count(*) from vms_camera where physical_id = c.physical_id) as cnt \
                  FROM vms_camera c WHERE c.resource_ptr_id = :id");
    query.bindValue(QLatin1String(":id"), id);
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }
    if( !query.next() )
        return ErrorCode::ok;   //already deleted
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
    qint32 id = getResourceInternalId(guid);

    ErrorCode err = deleteAddParams(id);
    if (err != ErrorCode::ok)
        return err;

    err = deleteTableRecord(guid, "vms_businessrule_action_resources", "resource_guid");
    if (err != ErrorCode::ok)
        return err;

    err = deleteTableRecord(guid, "vms_businessrule_event_resources", "resource_guid");
    if (err != ErrorCode::ok)
        return err;

    err = deleteCameraServerItemTable(id);
    if (err != ErrorCode::ok)
        return err;

    err = deleteTableRecord(id, "vms_camera", "resource_ptr_id");
    if (err != ErrorCode::ok)
        return err;

    err = deleteRecordFromResourceTable(id);
    if (err != ErrorCode::ok)
        return err;

    return ErrorCode::ok;
}

ErrorCode QnDbManager::removeServer(const QnId& guid)
{
    ErrorCode err;
    qint32 id = getResourceInternalId(guid);

    QSqlQuery queryCameras(m_sdb);
    queryCameras.setForwardOnly(true);
    queryCameras.prepare("SELECT r.guid from vms_camera c JOIN vms_resource r on r.id = c.resource_ptr_id WHERE r.parent_guid = ?");
    queryCameras.addBindValue(guid.toRfc4122());

    ApiCameraList cameraList;
    if (!queryCameras.exec()) {
        qWarning() << Q_FUNC_INFO << queryCameras.lastError().text();
        return ErrorCode::failure;
    }
    while(queryCameras.next()) {
        err = removeCamera(QUuid::fromRfc4122(queryCameras.value(0).toByteArray()));
        if (err != ErrorCode::ok)
            return err;
    }

    err = deleteAddParams(id);
    if (err != ErrorCode::ok)
        return err;

    err = removeStoragesByServer(guid);
    if (err != ErrorCode::ok)
        return err;

    err = deleteTableRecord(id, "vms_server", "resource_ptr_id");
    if (err != ErrorCode::ok)
        return err;

    err = deleteRecordFromResourceTable(id);
    if (err != ErrorCode::ok)
        return err;

    return ErrorCode::ok;
}

ErrorCode QnDbManager::removeLayout(const QnId& id)
{
    return removeLayoutInternal(id, getResourceInternalId(id));
}

ErrorCode QnDbManager::removeLayoutInternal(const QnId& id, const qint32 &internalId) {
    ErrorCode err = deleteAddParams(internalId);
    if (err != ErrorCode::ok)
        return err;

    err = removeLayoutItems(internalId);
    if (err != ErrorCode::ok)
        return err;

    err = removeLayoutFromVideowallItems(id);
    if (err != ErrorCode::ok)
        return err;

    err = deleteTableRecord(internalId, "vms_layout", "resource_ptr_id");
    if (err != ErrorCode::ok)
        return err;

    err = deleteRecordFromResourceTable(internalId);
    return err;
}

ErrorCode QnDbManager::executeTransactionNoLock(const QnTransaction<ApiStoredFileData>& tran)
{
    assert( tran.command == ApiCommand::addStoredFile || tran.command == ApiCommand::updateStoredFile );

    QSqlQuery query(m_sdb);
    query.prepare("INSERT OR REPLACE INTO vms_storedFiles (path, data) values (:path, :data)");
    query.bindValue(":path", tran.params.path);
    query.bindValue(":data", tran.params.data);
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }
    
    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransactionNoLock(const QnTransaction<QString>& tran)
{
    switch (tran.command) {
    case ApiCommand::removeStoredFile: {
        QSqlQuery query(m_sdb);
        query.prepare("DELETE FROM vms_storedFiles WHERE path = :path");
        query.bindValue(":path", tran.params);
        if (!query.exec()) {
            qWarning() << Q_FUNC_INFO << query.lastError().text();
            return ErrorCode::failure;
        }
        break;
    }
    case ApiCommand::installUpdate:
        break;
    default:
        Q_ASSERT_X(0, "Unsupported transaction", Q_FUNC_INFO);
        return ErrorCode::notImplemented;
    }

    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransactionNoLock(const QnTransaction<ApiUser>& tran)
{
    qint32 internalId;

    ErrorCode result = insertOrReplaceResource(tran.params, &internalId);
    if (result !=ErrorCode::ok)
        return result;

    return insertOrReplaceUser(tran.params, internalId);
}

ErrorCode QnDbManager::removeResource(const QnId& id)
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare("SELECT \
                    (CASE WHEN c.resource_ptr_id is null then rt.name else 'Camera' end) as name\
                    FROM vms_resource r\
                    JOIN vms_resourcetype rt on rt.guid = r.xtype_guid\
                    LEFT JOIN vms_camera c on c.resource_ptr_id = r.id\
                    WHERE r.guid = :guid");
    query.bindValue(":guid", id.toRfc4122());
    if (!query.exec())
        return ErrorCode::failure;
    if( !query.next() )
        return ErrorCode::ok;   //Record already deleted. That's exactly what we wanted
    QString objectType = query.value("name").toString();
    ErrorCode result;
    if (objectType == "Camera")
        result = removeCamera(id);
    else if (objectType == "Server")
        result = removeServer(id);
    else if (objectType == "User")
        result = removeUser(id);
    else if (objectType == "Layout")
        result = removeLayout(id);
    else if (objectType == "Videowall")
        result = removeVideowall(id);
    else {
        Q_ASSERT_X(0, "Unknown object type", Q_FUNC_INFO);
        return ErrorCode::notImplemented;
    }

    return result;
}

ErrorCode QnDbManager::executeTransactionNoLock(const QnTransaction<ApiIdData>& tran)
{
    ErrorCode result;
    switch (tran.command)
    {
    case ApiCommand::removeCamera:
        result = removeCamera(tran.params.id);
        break;
    case ApiCommand::removeMediaServer:
        result = removeServer(tran.params.id);
        break;
    case ApiCommand::removeLayout:
        result = removeLayout(tran.params.id);
        break;
    case ApiCommand::removeBusinessRule:
        result = removeBusinessRule( tran.params.id );
        break;
    case ApiCommand::removeUser:
        result = removeUser( tran.params.id );
        break;
    case ApiCommand::removeResource:
        result = removeResource( tran.params.id );
        break;
    default:
        qWarning() << "Remove operation is not implemented for command" << toString(tran.command);
        Q_ASSERT_X(0, "Remove operation is not implemented for command", Q_FUNC_INFO);
        return ErrorCode::unsupported;
    }

    return result;
}


/* 
-------------------------------------------------------------
-------------------------- getters --------------------------
 ------------------------------------------------------------
*/

// ----------- getResourceTypes --------------------

ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiResourceTypeList& data)
{
    if (!m_cachedResTypes.data.empty())
    {
        data = m_cachedResTypes;
        return ErrorCode::ok;
    }

	QSqlQuery queryTypes(m_sdb);
    queryTypes.setForwardOnly(true);
	queryTypes.prepare("select rt.guid as id, rt.name, m.name as manufacture \
				  from vms_resourcetype rt \
				  left join vms_manufacture m on m.id = rt.manufacture_id \
				  order by rt.id");
	if (!queryTypes.exec()) {
        qWarning() << Q_FUNC_INFO << queryTypes.lastError().text();
		return ErrorCode::failure;
    }

	QSqlQuery queryParents(m_sdb);
    queryParents.setForwardOnly(true);
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
    queryProperty.setForwardOnly(true);
    queryProperty.prepare("SELECT rt.guid as resource_type_id, pt.name, pt.type, pt.min, pt.max, pt.step, pt.[values], pt.ui_values, pt.default_value, \
                          pt.netHelper, pt.[group], pt.sub_group, pt.description, pt.ui, pt.readonly \
                          FROM vms_propertytype pt \
                          JOIN vms_resourcetype rt on rt.id = pt.resource_type_id ORDER BY pt.resource_type_id");
    if (!queryProperty.exec()) {
        qWarning() << Q_FUNC_INFO << queryProperty.lastError().text();
        return ErrorCode::failure;
    }

    data.loadFromQuery(queryTypes);
    mergeIdListData<ApiResourceType>(queryParents, data.data, &ApiResourceType::parentId);

    std::vector<ApiPropertyType> allProperties;
    QN_QUERY_TO_DATA_OBJECT(queryProperty, ApiPropertyType, allProperties, ApiPropertyTypeFields);
    mergeObjectListData(data.data, allProperties, &ApiResourceType::propertyTypeList, &ApiPropertyType::resource_type_id);

    m_cachedResTypes = data;

	return ErrorCode::ok;
}

// ----------- getLayouts --------------------

ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiLayoutList& layouts)
{
    QSqlQuery query(m_sdb);
    QString filter; // todo: add data filtering by user here
    query.setForwardOnly(true);
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
    queryItems.setForwardOnly(true);
    queryItems.prepare("SELECT r.guid as layoutId, li.zoom_bottom as zoomBottom, li.right, li.uuid, li.zoom_left as zoomLeft, li.resource_id as resourceId, \
                       li.zoom_right as zoomRight, li.top, li.bottom, li.zoom_top as zoomTop, \
                       li.zoom_target_uuid as zoomTargetUuid, li.flags, li.contrast_params as contrastParams, li.rotation, li.id, \
                       li.dewarping_params as dewarpingParams, li.left FROM vms_layoutitem li \
                       JOIN vms_resource r on r.id = li.layout_id order by li.layout_id");

    if (!queryItems.exec()) {
        qWarning() << Q_FUNC_INFO << queryItems.lastError().text();
        return ErrorCode::failure;
    }

    layouts.loadFromQuery(query);
    std::vector<ApiLayoutItemWithRef> items;
    QN_QUERY_TO_DATA_OBJECT(queryItems, ApiLayoutItemWithRef, items, ApiLayoutItemFields (layoutId));
    mergeObjectListData(layouts.data, items, &ApiLayout::items, &ApiLayoutItemWithRef::layoutId);

    return ErrorCode::ok;
}

// ----------- getCameras --------------------

ErrorCode QnDbManager::doQueryNoLock(const QnId& mServerId, ApiCameraList& cameraList)
{
	QSqlQuery queryCameras(m_sdb);
    QString filterStr;
	if (!mServerId.isNull()) {
		filterStr = QString("WHERE r.parent_guid = %1").arg(guidToSqlString(mServerId));
	}
    queryCameras.setForwardOnly(true);
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
        filterStr2 = QString("AND r.parent_guid = %1").arg(guidToSqlString(mServerId));
    
    queryScheduleTask.setForwardOnly(true);
    queryScheduleTask.prepare(QString("SELECT r.guid as sourceId, st.start_time as startTime, st.end_time as endTime, st.do_record_audio as doRecordAudio, \
                                       st.record_type as recordType, st.day_of_week as dayOfWeek, st.before_threshold as beforeThreshold, st.after_threshold as afterThreshold, \
                                       st.stream_quality as streamQuality, st.fps \
                                       FROM vms_scheduletask st \
                                       JOIN vms_resource r on r.id = st.source_id %1 ORDER BY r.id").arg(filterStr));

    QSqlQuery queryParams(m_sdb);
    queryParams.setForwardOnly(true);
    queryParams.prepare(QString("SELECT r.guid as resourceId, kv.value, kv.name, kv.isResTypeParam \
                                 FROM vms_kvpair kv \
                                 JOIN vms_camera c on c.resource_ptr_id = kv.resource_id \
                                 JOIN vms_resource r on r.id = kv.resource_id \
                                 WHERE kv.isResTypeParam = 1 \
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

    mergeObjectListData(cameraList.data, sheduleTaskList, &ApiCamera::scheduleTask, &ScheduleTaskWithRef::sourceId);

    std::vector<ApiResourceParamWithRef> params;
    QN_QUERY_TO_DATA_OBJECT(queryParams, ApiResourceParamWithRef, params, ApiResourceParamFields (resourceId));
    mergeObjectListData<ApiCamera>(cameraList.data, params, &ApiCamera::addParams, &ApiResourceParamWithRef::resourceId);

	return ErrorCode::ok;
}

// ----------- getServers --------------------


ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiMediaServerList& serverList)
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare(QString("select r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentGuid, r.name, r.url, r.status,r. disabled, \
                          s.api_url as apiUrl, s.auth_key as authKey, s.streaming_url as streamingUrl, s.version, s.system_info as systemInfo, s.net_addr_list as netAddrList, s.flags, s.panic_mode as panicMode \
                          from vms_resource r \
                          join vms_server s on s.resource_ptr_id = r.id order by r.guid"));

    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }

    QSqlQuery queryStorage(m_sdb);
    queryStorage.setForwardOnly(true);
    queryStorage.prepare(QString("select r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentGuid, r.name, r.url, r.status,r. disabled, \
                          s.space_limit as spaceLimit, s.used_for_writing as usedForWriting \
                          from vms_resource r \
                          join vms_storage s on s.resource_ptr_id = r.id order by r.parent_guid"));

    if (!queryStorage.exec()) {
        qWarning() << Q_FUNC_INFO << queryStorage.lastError().text();
        return ErrorCode::failure;
    }

    serverList.loadFromQuery(query);

    ApiStorageList storageList;
    storageList.loadFromQuery(queryStorage);

    mergeObjectListData<ApiMediaServer, ApiStorage>(serverList.data, storageList.data, &ApiMediaServer::storages, &ApiStorage::parentGuid);

    return ErrorCode::ok;
}

//getCameraServerItems
ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiCameraServerItemList& historyList)
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare(QString("select server_guid as serverGuid, timestamp, physical_id as physicalId from vms_cameraserveritem"));
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }

    historyList.loadFromQuery(query);

    return ErrorCode::ok;
}

//getUsers
ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiUserList& userList)
{
    //digest = md5('%s:%s:%s' % (self.user.username.lower(), 'NetworkOptix', password)).hexdigest()
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare(QString("select r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentGuid, r.name, r.url, r.status,r. disabled, \
                          u.is_superuser as isAdmin, u.email, p.digest as digest, u.password as hash, p.rights \
                          from vms_resource r \
                          join auth_user u  on u.id = r.id\
                          join vms_userprofile p on p.user_id = u.id\
                          order by r.id"));
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }

    QSqlQuery queryParams(m_sdb);
    queryParams.setForwardOnly(true);
    queryParams.prepare(QString("SELECT r.guid as resourceId, kv.value, kv.name, kv.isResTypeParam \
                                FROM vms_kvpair kv \
                                JOIN auth_user u on u.id = kv.resource_id \
                                JOIN vms_resource r on r.id = kv.resource_id \
                                WHERE kv.isResTypeParam = 0 \
                                ORDER BY kv.resource_id"));

    if (!queryParams.exec()) {
        qWarning() << Q_FUNC_INFO << queryParams.lastError().text();
        return ErrorCode::failure;
    }

    userList.loadFromQuery(query);
    
    std::vector<ApiResourceParamWithRef> params;
    QN_QUERY_TO_DATA_OBJECT(queryParams, ApiResourceParamWithRef, params, ApiResourceParamFields (resourceId));
    mergeObjectListData<ApiUser>(userList.data, params, &ApiUser::addParams, &ApiResourceParamWithRef::resourceId);
    
    return ErrorCode::ok;
}

//getVideowallList
ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiVideowallList& videowallList) {
    QSqlQuery query(m_sdb);
    QString filter; // todo: add data filtering by user here
    query.setForwardOnly(true);
    query.prepare(QString("SELECT r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentGuid, r.name, r.url, r.status,r. disabled, \
                          l.autorun, l.resource_ptr_id as id \
                          FROM vms_videowall l \
                          JOIN vms_resource r on r.id = l.resource_ptr_id %1 ORDER BY r.id").arg(filter));
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }
    videowallList.loadFromQuery(query);

    QSqlQuery queryItems(m_sdb);
    queryItems.setForwardOnly(true);
    queryItems.prepare("SELECT \
                       item.guid, item.pc_guid, item.layout_guid, item.videowall_guid, \
                       item.name, item.x, item.y, item.w, item.h \
                       FROM vms_videowall_item item");
    if (!queryItems.exec()) {
        qWarning() << Q_FUNC_INFO << queryItems.lastError().text();
        return ErrorCode::failure;
    }
    std::vector<ApiVideowallItemDataWithRef> items;
    QN_QUERY_TO_DATA_OBJECT(queryItems, ApiVideowallItemDataWithRef, items, ApiVideowallItemDataFields (videowall_guid));
    mergeObjectListData(videowallList.data, items, &ApiVideowallData::items, &ApiVideowallItemDataWithRef::videowall_guid);
    
    QSqlQuery queryScreens(m_sdb);
    queryScreens.setForwardOnly(true);
    queryScreens.prepare("SELECT \
                         pc.videowall_guid, pc.pc_guid, \
                         screen.pc_guid, screen.pc_index, \
                         screen.desktop_x, screen.desktop_y, screen.desktop_w, screen.desktop_h, \
                         screen.layout_x, screen.layout_y, screen.layout_w, screen.layout_h \
                         FROM vms_videowall_screen screen \
                         JOIN vms_videowall_pcs pc on pc.pc_guid = screen.pc_guid");
    if (!queryScreens.exec()) {
        qWarning() << Q_FUNC_INFO << queryScreens.lastError().text();
        return ErrorCode::failure;
    }
    std::vector<ApiVideowallScreenDataWithRef> screens;
    QN_QUERY_TO_DATA_OBJECT(queryScreens, ApiVideowallScreenDataWithRef, screens, ApiVideowallScreenDataFields (videowall_guid));
    mergeObjectListData(videowallList.data, screens, &ApiVideowallData::screens, &ApiVideowallScreenDataWithRef::videowall_guid);

    return ErrorCode::ok;
}

//getBusinessRules
ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiBusinessRuleList& businessRuleList)
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare(QString("SELECT guid as id, event_type as eventType, event_condition as eventCondition, event_state as eventState, action_type as actionType, \
                          action_params as actionParams, aggregation_period as aggregationPeriod, disabled, comments, schedule, system \
                          FROM vms_businessrule order by guid"));
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }

    QSqlQuery queryRuleEventRes(m_sdb);
    queryRuleEventRes.setForwardOnly(true);
    queryRuleEventRes.prepare(QString("SELECT businessrule_guid as id, resource_guid as parentId from vms_businessrule_event_resources order by businessrule_guid"));
    if (!queryRuleEventRes.exec()) {
        qWarning() << Q_FUNC_INFO << queryRuleEventRes.lastError().text();
        return ErrorCode::failure;
    }

    QSqlQuery queryRuleActionRes(m_sdb);
    queryRuleActionRes.setForwardOnly(true);
    queryRuleActionRes.prepare(QString("SELECT businessrule_guid as id, resource_guid as parentId from vms_businessrule_action_resources order by businessrule_guid"));
    if (!queryRuleActionRes.exec()) {
        qWarning() << Q_FUNC_INFO << queryRuleActionRes.lastError().text();
        return ErrorCode::failure;
    }

    businessRuleList.loadFromQuery(query);

    // merge data

    mergeIdListData<ApiBusinessRule>(queryRuleEventRes, businessRuleList.data, &ApiBusinessRule::eventResource);
    mergeIdListData<ApiBusinessRule>(queryRuleActionRes, businessRuleList.data, &ApiBusinessRule::actionResource);

    return ErrorCode::ok;
}

// getKVPairs
ErrorCode QnDbManager::doQueryNoLock(const QnId& resourceId, ApiResourceParams& params)
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare(QString("SELECT kv.value, kv.name, kv.isResTypeParam \
                                FROM vms_kvpair kv \
                                JOIN vms_resource r on r.id = kv.resource_id WHERE r.guid = :guid"));
    query.bindValue(QLatin1String(":guid"), resourceId.toRfc4122());
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text() << query.lastQuery();
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

// ApiFullInfo
ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& dummy, ApiFullInfo& data)
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

    if ((err = doQueryNoLock(dummy, data.videowalls)) != ErrorCode::ok)
        return err;

    if ((err = doQueryNoLock(dummy, data.rules)) != ErrorCode::ok)
        return err;

    if ((err = doQueryNoLock(dummy, data.cameraHistory)) != ErrorCode::ok)
        return err;

    if ((err = doQueryNoLock(dummy, data.licenses)) != ErrorCode::ok)
        return err;

    std::vector<ApiResourceParamWithRef> kvPairs;
    QSqlQuery queryParams(m_sdb);
    queryParams.setForwardOnly(true);
    queryParams.prepare(QString("SELECT r.guid as resourceId, kv.value, kv.name, kv.isResTypeParam \
                                FROM vms_kvpair kv \
                                JOIN vms_resource r on r.id = kv.resource_id \
                                WHERE kv.isResTypeParam = 1 \
                                ORDER BY kv.resource_id"));
    if (!queryParams.exec())
        return ErrorCode::failure;
    QN_QUERY_TO_DATA_OBJECT(queryParams, ApiResourceParamWithRef, kvPairs, ApiResourceParamFields (resourceId));


    mergeObjectListData<ApiMediaServer>(data.servers.data, kvPairs, &ApiMediaServer::addParams, &ApiResourceParamWithRef::resourceId);
    mergeObjectListData<ApiCamera>(data.cameras.data,      kvPairs, &ApiCamera::addParams,      &ApiResourceParamWithRef::resourceId);
    mergeObjectListData<ApiUser>(data.users.data,          kvPairs, &ApiUser::addParams,        &ApiResourceParamWithRef::resourceId);
    mergeObjectListData<ApiLayout>(data.layouts.data,      kvPairs, &ApiLayout::addParams,      &ApiResourceParamWithRef::resourceId);
    mergeObjectListData<ApiVideowall>(data.videowalls.data,kvPairs, &ApiVideowall::addParams,   &ApiResourceParamWithRef::resourceId);

    //filling serverinfo
    fillServerInfo( &data.serverInfo );

    return err;
}

//getParams
ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ec2::ApiParamList& data)
{
    ApiResourceParams params;
    ErrorCode rez = doQueryNoLock(m_adminUserID, params);
    if (rez == ErrorCode::ok)
        data.data = params.params;
    return rez;
}

ErrorCode QnDbManager::executeTransactionNoLock(const QnTransaction<ApiResetBusinessRuleData>& tran)
{
    if (!execSQLQuery("DELETE FROM vms_businessrule_action_resources"))
        return ErrorCode::failure;
    if (!execSQLQuery("DELETE FROM vms_businessrule_event_resources"))
        return ErrorCode::failure;
    if (!execSQLQuery("DELETE FROM vms_businessrule"))
        return ErrorCode::failure;

    foreach (const ApiBusinessRule& rule, tran.params.defaultRules.data)
    {
        ErrorCode rez = updateBusinessRule(rule);
        if (rez != ErrorCode::ok)
            return rez;
    }

    return ErrorCode::ok;
}

// save settings
ErrorCode QnDbManager::executeTransactionNoLock(const QnTransaction<ApiParamList>& tran)
{
    /*
    ErrorCode result = deleteAddParams(m_adminUserInternalID);
    if (result != ErrorCode::ok)
        return result;
    */
    return insertAddParams(tran.params.data, m_adminUserInternalID);
}


ErrorCode QnDbManager::saveLicense(const ApiLicense& license) {
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT OR REPLACE INTO vms_license (license_key, license_block) VALUES(:licenseKey, :licenseBlock)");
    insQuery.bindValue(":licenseKey", license.key);
    insQuery.bindValue(":licenseBlock", license.licenseBlock);
    if (insQuery.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
        return ErrorCode::failure;
    }

}

ErrorCode QnDbManager::executeTransactionNoLock(const QnTransaction<ApiLicense>& tran)
{
    return saveLicense(tran.params);
}

ErrorCode QnDbManager::executeTransactionNoLock(const QnTransaction<ApiLicenseList>& tran)
{
    foreach (const ApiLicense& license, tran.params.data) {
        ErrorCode result = saveLicense(license);
        if (result != ErrorCode::ok) {
            return ErrorCode::failure;
        }
    }

    return ErrorCode::ok;

//    return m_licenseManagerImpl->addLicenses( tran.params );
}

ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ec2::ApiLicenseList& data)
{
    QSqlQuery query(m_sdb);

    QString q = QString(lit("SELECT license_key as key, license_block as licenseBlock from vms_license"));
    query.setForwardOnly(true);
    query.prepare(q);

    if (!query.exec())
    {
        qWarning() << Q_FUNC_INFO << __LINE__ << query.lastError();
        return ErrorCode::failure;
    }

    QN_QUERY_TO_DATA_OBJECT_FILTERED(query, ApiLicense, data.data, ApiLicenseFields, m_licenseManagerImpl->validateLicense);

    // m_licenseManagerImpl->getLicenses( &data );
    return ErrorCode::ok;
}

ErrorCode QnDbManager::doQueryNoLock(const ApiStoredFilePath& _path, ApiStoredDirContents& data)
{
    QSqlQuery query(m_sdb);
    QString path;
    if (!_path.isEmpty())
        path = closeDirPath(_path);
    
    QString pathFilter(lit("path"));
    if (!path.isEmpty())
        pathFilter = QString(lit("substr(path, %2)")).arg(path.length()+1);
    QString q = QString(lit("SELECT %1 FROM vms_storedFiles WHERE path LIKE '%2%' ")).arg(pathFilter).arg(path);
    if (!path.isEmpty())
        q += QString(lit("AND substr(path, %2) NOT LIKE '%/%' ")).arg(path.length()+1);

    query.setForwardOnly(true);
    query.prepare(q);
    if (!query.exec())
    {
        qWarning() << Q_FUNC_INFO << __LINE__ << query.lastError();
        return ErrorCode::failure;
    }
    while (query.next()) 
        data.push_back(query.value(0).toString());
    
    return ErrorCode::ok;
}

ErrorCode QnDbManager::doQueryNoLock(const ApiStoredFilePath& path, ApiStoredFileData& data)
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare("SELECT data FROM vms_storedFiles WHERE path = :path");
    query.bindValue(":path", path);
    if (!query.exec())
    {
        qWarning() << Q_FUNC_INFO << __LINE__ << query.lastError();
        return ErrorCode::failure;
    }
    data.path = path;
    if (query.next())
        data.data = query.value(0).toByteArray();
    return ErrorCode::ok;
}

void QnDbManager::fillServerInfo( ServerInfo* const serverInfo )
{
    serverInfo->armBox = QLatin1String(QN_ARM_BOX);
    m_licenseManagerImpl->getHardwareId( serverInfo );
}

ErrorCode QnDbManager::saveVideowall(const ApiVideowall& params) {
    qint32 internalId;

    ErrorCode result = insertOrReplaceResource(params, &internalId);
    if (result != ErrorCode::ok)
        return result;

    result = insertOrReplaceVideowall(params, internalId);
    if (result != ErrorCode::ok)
        return result;

    result = updateVideowallItems(params);
    if (result != ErrorCode::ok)
        return result;

    result = updateVideowallScreens(params);
    return result;
}

ErrorCode QnDbManager::updateVideowallItems(const ApiVideowall& data) {
    ErrorCode result = deleteVideowallItems(data.id);
    if (result != ErrorCode::ok)
        return result;

    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT INTO vms_videowall_item \
                     (guid, pc_guid, layout_guid, videowall_guid, name, x, y, w, h) \
                     VALUES \
                     (:guid, :pc_guid, :layout_guid, :videowall_guid, :name, :x, :y, :w, :h)");
    foreach(const ApiVideowallItem& item, data.items)
    {
        item.autoBindValues(insQuery);
        insQuery.bindValue(":videowall_guid", data.id.toRfc4122());

        if (!insQuery.exec()) {
            qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
            return ErrorCode::failure;
        }
    }
    return ErrorCode::ok;

}

ErrorCode QnDbManager::updateVideowallScreens(const ApiVideowall& data) {
    if (data.screens.size() == 0)
        return ErrorCode::ok;

    QSet<QnId> pcUuids;

    {
        QSqlQuery query(m_sdb);
        query.prepare("INSERT OR REPLACE INTO vms_videowall_screen \
                      (pc_guid, pc_index, \
                      desktop_x, desktop_y, desktop_w, desktop_h, \
                      layout_x, layout_y, layout_w, layout_h) \
                      VALUES \
                      (:pc_guid, :pc_index, \
                      :desktop_x, :desktop_y, :desktop_w, :desktop_h, \
                      :layout_x, :layout_y, :layout_w, :layout_h)");

        foreach(const ApiVideowallScreen& screen, data.screens)
        {
            screen.autoBindValues(query);
            pcUuids << screen.pc_guid;
            if (!query.exec()) {
                qWarning() << Q_FUNC_INFO << query.lastError().text();
                return ErrorCode::failure;
            }
        }
    }

    {
        QSqlQuery query(m_sdb);
        query.prepare("INSERT OR REPLACE INTO vms_videowall_pcs \
                      (videowall_guid, pc_guid) VALUES (:videowall_guid, :pc_guid)");
        foreach (const QnId &pcUuid, pcUuids) {
            query.bindValue(":videowall_guid", data.id.toRfc4122());
            query.bindValue(":pc_guid", pcUuid.toRfc4122());
            if (!query.exec()) {
                qWarning() << Q_FUNC_INFO << query.lastError().text();
                return ErrorCode::failure;
            }
        }
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::deleteVideowallItems(const QnId &videowall_guid) {
    ErrorCode err = deleteTableRecord(videowall_guid, "vms_videowall_item", "videowall_guid");
    if (err != ErrorCode::ok)
        return err;

    { // delete unused PC screens
        QSqlQuery delQuery(m_sdb);
        delQuery.prepare("DELETE FROM vms_videowall_screen WHERE pc_guid NOT IN (SELECT pc_guid from vms_videowall_item) ");
        if (!delQuery.exec()) {
            qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
            return ErrorCode::failure;
        }
    }

    { // delete unused PCs
        QSqlQuery delQuery(m_sdb);
        delQuery.prepare("DELETE FROM vms_videowall_pcs WHERE pc_guid NOT IN (SELECT pc_guid from vms_videowall_screen) ");
        if (!delQuery.exec()) {
            qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
            return ErrorCode::failure;
        }
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::removeVideowall(const QnId& guid) {
    qint32 id = getResourceInternalId(guid);

    ErrorCode err = deleteAddParams(id);
    if (err != ErrorCode::ok)
        return err;

    err = deleteVideowallItems(guid);
    if (err != ErrorCode::ok)
        return err;

    err = deleteTableRecord(id, "vms_videowall", "resource_ptr_id");
    if (err != ErrorCode::ok)
        return err;

    err = deleteRecordFromResourceTable(id);
    if (err != ErrorCode::ok)
        return err;

    return ErrorCode::ok;
}

ErrorCode QnDbManager::insertOrReplaceVideowall(const ApiVideowall& data, qint32 internalId) {
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT OR REPLACE INTO vms_videowall (autorun, resource_ptr_id) VALUES\
                     (:autorun, :internalId)");
    data.autoBindValues(insQuery);
    insQuery.bindValue(":internalId", internalId);
    if (insQuery.exec())
        return ErrorCode::ok;
    
    qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
    return ErrorCode::failure;
}

ErrorCode QnDbManager::removeLayoutFromVideowallItems(const QnId &layout_id) {
    QByteArray emptyId = QUuid().toRfc4122();

    QSqlQuery query(m_sdb);
    query.prepare("UPDATE vms_videowall_item set layout_guid = :empty_id WHERE layout_guid = :layout_id");
    query.bindValue(":empty_id", emptyId);
    query.bindValue(":layout_id", layout_id.toRfc4122());
    if (query.exec())
        return ErrorCode::ok;

    qWarning() << Q_FUNC_INFO << query.lastError().text();
    return ErrorCode::failure;
}

void QnDbManager::beginTran()
{
    m_tran.beginTran();
}

void QnDbManager::commit()
{
    m_tran.commit();
}

void QnDbManager::rollback()
{
    m_tran.rollback();
}

}
