#include "db_manager.h"

#include <utils/common/app_info.h>

#include <QtSql/QtSql>

#include "utils/common/util.h"
#include "common/common_module.h"
#include "managers/impl/license_manager_impl.h"
#include "managers/time_manager.h"
#include "nx_ec/data/api_business_rule_data.h"
#include "nx_ec/data/api_discovery_data.h"
#include "utils/serialization/binary_stream.h"
#include "utils/serialization/sql_functions.h"
#include "business/business_fwd.h"
#include "utils/common/synctime.h"
#include "utils/serialization/json.h"
#include "core/resource/user_resource.h"

#include "nx_ec/data/api_camera_data.h"
#include "nx_ec/data/api_resource_type_data.h"
#include "nx_ec/data/api_stored_file_data.h"
#include "nx_ec/data/api_user_data.h"
#include "nx_ec/data/api_layout_data.h"
#include "nx_ec/data/api_videowall_data.h"
#include "nx_ec/data/api_license_data.h"
#include "nx_ec/data/api_business_rule_data.h"
#include "nx_ec/data/api_full_info_data.h"
#include "nx_ec/data/api_camera_server_item_data.h"
#include "nx_ec/data/api_camera_bookmark_data.h"
#include "nx_ec/data/api_media_server_data.h"
#include "nx_ec/data/api_update_data.h"
#include <nx_ec/data/api_time_data.h>
#include "nx_ec/data/api_conversion_functions.h"
#include "api/runtime_info_manager.h"
#include "utils/common/log.h"
#include "nx_ec/data/api_camera_data_ex.h"
#include "restype_xml_parser.h"

using std::nullptr_t;

static const QString RES_TYPE_MSERVER = "mediaserver";
static const QString RES_TYPE_CAMERA = "camera";

namespace ec2
{

static QnDbManager* globalInstance = 0; // TODO: #Elric #EC2 use QnSingleton
static const char LICENSE_EXPIRED_TIME_KEY[] = "{4208502A-BD7F-47C2-B290-83017D83CDB7}";
static const char DB_INSTANCE_KEY[] = "DB_INSTANCE_ID";

static bool removeDirRecursive(const QString & dirName)
{
    bool result = true;
    QDir dir(dirName);

    if (dir.exists(dirName)) {
        for(const QFileInfo& info: dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden  | QDir::AllDirs | QDir::Files, QDir::DirsFirst)) 
        {
            if (info.isDir())
                result = removeDir(info.absoluteFilePath());
            else
                result = QFile::remove(info.absoluteFilePath());

            if (!result)
                return result;
        }
        result = dir.rmdir(dirName);
    }
    return result;
}

template <class T>
void assertSorted(std::vector<T> &data) {
#ifdef _DEBUG
    assertSorted(data, &T::id);
#else
    Q_UNUSED(data);
#endif // DEBUG
}

template <class T, class Field>
void assertSorted(std::vector<T> &data, QnUuid Field::*idField) {
#ifdef _DEBUG
    if (data.empty())
        return;

    QByteArray prev = (data[0].*idField).toRfc4122();
    for (int i = 1; i < data.size(); ++i) {
        QByteArray next = (data[i].*idField).toRfc4122();
        assert(next >= prev);
        prev = next;
    }
#else
    Q_UNUSED(data);
    Q_UNUSED(idField);
#endif // DEBUG
}

/**
 * Function merges sorted query into the sorted Data list. Data list contains placeholder 
 * field for the data, that is contained in the query. 
 * Data elements should have 'id' field and should be sorted by it.
 * Query should have 'id' and 'parentId' fields and should be sorted by 'id'.
 */
template <class MainData>
void mergeIdListData(QSqlQuery& query, std::vector<MainData>& data, std::vector<QnUuid> MainData::*subList)
{
    assertSorted(data);

    QSqlRecord rec = query.record();
    int idIdx = rec.indexOf("id");
    int parentIdIdx = rec.indexOf("parentId");
    assert(idIdx >=0 && parentIdIdx >= 0);
  
    bool eof = true;
    QnUuid id, parentId;
    QByteArray idRfc;

    auto step = [&eof, &id, &idRfc, &parentId, &query, idIdx, parentIdIdx]{
        eof = !query.next();
        if (eof)
            return;
        idRfc = query.value(idIdx).toByteArray();
        assert(idRfc == id.toRfc4122() || idRfc > id.toRfc4122());
        id = QnUuid::fromRfc4122(idRfc);
        parentId = QnUuid::fromRfc4122(query.value(parentIdIdx).toByteArray());
    };

    step();
    size_t i = 0;
    while (i < data.size() && !eof) {
        if (id == data[i].id) {
            (data[i].*subList).push_back(parentId);
            step();
        } else if (idRfc > data[i].id.toRfc4122()) {
            ++i;
        } else {
            step();
        }
    }
}

/**
 * Function merges two sorted lists. First of them (data) contains placeholder 
 * field for the data, that is contained in the second (subData). 
 * Data elements should have 'id' field and should be sorted by it.
 * SubData elements should be sorted by parentIdField.
 */
template <class MainData, class SubData, class MainSubData, class MainOrParentType, class IdType, class SubOrParentType>
void mergeObjectListData(std::vector<MainData>& data, std::vector<SubData>& subDataList, std::vector<MainSubData> MainOrParentType::*subDataListField, IdType SubOrParentType::*parentIdField)
{
    assertSorted(data);
    assertSorted(subDataList, parentIdField);

    size_t i = 0;
    size_t j = 0;
    while (i < data.size() && j < subDataList.size()) {
        if (subDataList[j].*parentIdField == data[i].id) {
            (data[i].*subDataListField).push_back(subDataList[j]);
            ++j;
        } else if ((subDataList[j].*parentIdField).toRfc4122() > data[i].id.toRfc4122()) {
            ++i;
        } else {
            ++j;
        }
    }
}

template <class MainData, class SubData, class MainOrParentType, class IdType, class SubOrParentType, class Handler>
void mergeObjectListData(
    std::vector<MainData>& data,
    std::vector<SubData>& subDataList,
    IdType MainOrParentType::*idField,
    IdType SubOrParentType::*parentIdField,
    Handler mergeHandler )
{
    assertSorted(data, idField);
    assertSorted(subDataList, parentIdField);

    size_t i = 0;
    size_t j = 0;

    while (i < data.size() && j < subDataList.size()) {
        if (subDataList[j].*parentIdField == data[i].*idField) {
            mergeHandler( data[i], subDataList[j] );
            ++j;
        } else if ((subDataList[j].*parentIdField).toRfc4122() > (data[i].*idField).toRfc4122()) {
            ++i;
        } else {
            ++j;
        }
    }
}

//!Same as above but does not require field "id" of type QnUuid
/**
 * Function merges two sorted lists. First of them (data) contains placeholder 
 * field for the data, that is contained in the second (subData). 
 * Data elements MUST be sorted by \a idField.
 * SubData elements should be sorted by parentIdField.
 * Types MainOrParentType1 and MainOrParentType2 are separated to allow \a subDataListField and \a idField to be pointers to fields of related types
 */
template <class MainData, class SubData, class MainSubData, class MainOrParentType1, class MainOrParentType2, class IdType, class SubOrParentType>
void mergeObjectListData(
    std::vector<MainData>& data,
    std::vector<SubData>& subDataList,
    std::vector<MainSubData> MainOrParentType1::*subDataListField,
    IdType MainOrParentType2::*idField,
    IdType SubOrParentType::*parentIdField )
{
    mergeObjectListData(
        data,
        subDataList,
        idField,
        parentIdField,
        [subDataListField]( MainData& mergeTo, SubData& mergeWhat ){ (mergeTo.*subDataListField).push_back(mergeWhat); } );
}

// --------------------------------------- QnDbTransactionExt -----------------------------------------

bool QnDbManager::QnDbTransactionExt::beginTran()
{
    if( !QnDbTransaction::beginTran() )
        return false;
    transactionLog->beginTran();
    return true;
}

void QnDbManager::QnDbTransactionExt::rollback()
{
    transactionLog->rollback();
    QnDbTransaction::rollback();
}

bool QnDbManager::QnDbTransactionExt::commit()
{
    const bool rez = m_database.commit();
    if (rez) {
        transactionLog->commit();
        m_mutex.unlock();
    }
    else {
        qWarning() << "Commit failed:" << m_database.lastError(); // do not unlock mutex. Rollback is expected
    }
    return rez;
}

// --------------------------------------- QnDbManager -----------------------------------------

QnUuid QnDbManager::getType(const QString& typeName)
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare("select guid from vms_resourcetype where name = ?");
    query.bindValue(0, typeName);
    if( !query.exec() )
    {
        Q_ASSERT(false);
    }
    if (query.next())
        return QnUuid::fromRfc4122(query.value("guid").toByteArray());
    return QnUuid();
}

QnDbManager::QnDbManager()
:
    m_licenseOverflowMarked(false),
    m_initialized(false),
    m_tran(m_sdb, m_mutex),
    m_tranStatic(m_sdbStatic, m_mutexStatic),
    m_needResyncLog(false),
    m_needResyncLicenses(false),
    m_needResyncFiles(false),
    m_dbJustCreated(false),
    m_isBackupRestore(false)
{
	Q_ASSERT(!globalInstance);
	globalInstance = this;
}

bool removeFile(const QString& fileName)
{
    if (QFile::exists(fileName) && !QFile::remove(fileName)) {
        qWarning() << "Can't remove database file" << fileName;
        return false;
    }
    return true;
}

bool QnDbManager::migrateServerGUID(const QString& table, const QString& field)
{
    QSqlQuery updateGuidQuery( m_sdb );
    updateGuidQuery.prepare(lit("UPDATE %1 SET %2=? WHERE %2=?").arg(table).arg(field) );
    updateGuidQuery.addBindValue( qnCommon->moduleGUID().toRfc4122() );
    updateGuidQuery.addBindValue( qnCommon->obsoleteServerGuid().toRfc4122() );
    if( !updateGuidQuery.exec() )
    {
        qWarning() << "can't initialize sqlLite database!" << updateGuidQuery.lastError().text();
        return false;
    }
    return true;
}

bool QnDbManager::init(QnResourceFactory* factory, const QUrl& dbUrl)
{
    m_resourceFactory = factory;
    const QString dbFilePath = dbUrl.toLocalFile();
    const QString dbFilePathStatic = QUrlQuery(dbUrl.query()).queryItemValue("staticdb_path");

    m_sdb = QSqlDatabase::addDatabase("QSQLITE", "QnDbManager");
    QString dbFileName = closeDirPath(dbFilePath) + QString::fromLatin1("ecs.sqlite");
    m_sdb.setDatabaseName( dbFileName);

    QString backupDbFileName = dbFileName + QString::fromLatin1(".backup");
    bool needCleanup = QUrlQuery(dbUrl.query()).hasQueryItem("cleanupDb");
    if (QFile::exists(backupDbFileName) || needCleanup) 
    {
        if (!removeFile(dbFileName))
            return false;
        if (!removeFile(dbFileName + lit("-shm")))
            return false;
        if (!removeFile(dbFileName + lit("-wal")))
            return false;
        if (QFile::exists(backupDbFileName)) 
        {
            m_needResyncLog = true;
            m_isBackupRestore = true;
            if (!QFile::rename(backupDbFileName, dbFileName)) {
                qWarning() << "Can't rename database file from" << backupDbFileName << "to" << dbFileName << "Database restore operation canceled";
                return false;
            }
        }
    }

    m_sdbStatic = QSqlDatabase::addDatabase("QSQLITE", "QnDbManagerStatic");
    QString path2 = dbFilePathStatic.isEmpty() ? dbFilePath : dbFilePathStatic;
    m_sdbStatic.setDatabaseName( closeDirPath(path2) + QString::fromLatin1("ecs_static.sqlite"));

    if( !m_sdb.open() )
    {
        qWarning() << "can't initialize Server sqlLite database " << m_sdb.databaseName() << ". Error: " << m_sdb.lastError().text();
        return false;
    }


    QSqlQuery identityTimeQuery(m_sdb);
    identityTimeQuery.prepare("SELECT data FROM misc_data WHERE key = ?");
    identityTimeQuery.addBindValue("gotDbDumpTime");
    if (identityTimeQuery.exec() && identityTimeQuery.next()) 
    {
        qint64 dbRestoreTime = identityTimeQuery.value(0).toLongLong();
        if (dbRestoreTime) 
        {
            identityTimeQuery.prepare("DELETE FROM misc_data WHERE key = ?");
            identityTimeQuery.addBindValue("gotDbDumpTime");
            if (!identityTimeQuery.exec())
            {
                qWarning() << "can't initialize Server sqlLite database " << m_sdb.databaseName() << ". Error: " << m_sdb.lastError().text();
                return false;
            }

            qint64 currentIdentityTime = qnCommon->systemIdentityTime();
            qnCommon->setSystemIdentityTime(qMax(currentIdentityTime + 1, dbRestoreTime), qnCommon->moduleGUID());
        }
    }

    if( !m_sdbStatic.open() )
    {
        qWarning() << "can't initialize Server static sqlLite database " << m_sdbStatic.databaseName() << ". Error: " << m_sdbStatic.lastError().text();
        return false;
    }

    //tuning DB
    if( !tuneDBAfterOpen() )
        return false;

    if( !createDatabase() )
    {
        // create tables is DB is empty
        qWarning() << "can't create tables for sqlLite database!";
        return false;
    }

    QnDbManager::QnDbTransactionLocker locker( getTransaction() );
    
    if( !qnCommon->obsoleteServerGuid().isNull() )
    {
        if (!migrateServerGUID("vms_resource", "guid"))
            return false;
        if (!migrateServerGUID("vms_resource", "parent_guid"))
            return false;

        if (!migrateServerGUID("vms_businessrule_action_resources", "resource_guid"))
            return false;
        if (!migrateServerGUID("vms_businessrule_event_resources", "resource_guid"))
            return false;
        if (!migrateServerGUID("vms_cameraserveritem", "server_guid"))
            return false;
        if (!migrateServerGUID("vms_kvpair", "resource_guid"))
            return false;
        if (!migrateServerGUID("vms_resource_status", "guid"))
            return false;
        if (!migrateServerGUID("vms_server_user_attributes", "server_guid"))
            return false;
    }

    QString storedFilesDir = closeDirPath(dbFilePath) + QString(lit("vms_storedfiles/"));
    int addedStoredFilesCnt = 0;
    addStoredFiles(storedFilesDir, &addedStoredFilesCnt);
    m_needResyncFiles = addedStoredFilesCnt > 0;
    removeDirRecursive(storedFilesDir);

    // updateDBVersion();
    QSqlQuery insVersionQuery( m_sdb );
    insVersionQuery.prepare( "INSERT OR REPLACE INTO misc_data (key, data) values (?,?)" );
    insVersionQuery.addBindValue( "VERSION" );
    insVersionQuery.addBindValue( QnAppInfo::applicationVersion() );
    if( !insVersionQuery.exec() )
    {
        qWarning() << "can't initialize sqlLite database!" << insVersionQuery.lastError().text();
        return false;
    }
    insVersionQuery.addBindValue( "BUILD" );
    insVersionQuery.addBindValue( QnAppInfo::applicationRevision() );
    if( !insVersionQuery.exec() )
    {
        qWarning() << "can't initialize sqlLite database!" << insVersionQuery.lastError().text();
        return false;
    }





    m_storageTypeId = getType( "Storage" );
    m_serverTypeId = getType( "Server" );
    m_cameraTypeId = getType( "Camera" );

    QSqlQuery queryAdminUser( m_sdb );
    queryAdminUser.setForwardOnly( true );
    queryAdminUser.prepare( "SELECT r.guid, r.id FROM vms_resource r JOIN auth_user u on u.id = r.id and r.name = 'admin'" );
    if( !queryAdminUser.exec() )
    {
        Q_ASSERT( false );
    }
    if( queryAdminUser.next() )
    {
        m_adminUserID = QnUuid::fromRfc4122( queryAdminUser.value( 0 ).toByteArray() );
        m_adminUserInternalID = queryAdminUser.value( 1 ).toInt();
    }

    QSqlQuery queryServers(m_sdb);
    queryServers.prepare("UPDATE vms_resource_status set status = ? WHERE guid in (select guid from vms_resource where xtype_guid = ?)"); // todo: only mserver without DB?
    queryServers.bindValue(0, Qn::Offline);
    queryServers.bindValue(1, m_serverTypeId.toRfc4122());
    if( !queryServers.exec() )
    {
        qWarning() << Q_FUNC_INFO << __LINE__ << queryServers.lastError();
        Q_ASSERT( false );
        return false;
    }

    // read license overflow time
    QSqlQuery query( m_sdb );
    query.prepare( "SELECT data from misc_data where key = ?" );
    query.addBindValue( LICENSE_EXPIRED_TIME_KEY );
    qint64 licenseOverflowTime = 0;
    if( query.exec() && query.next() )
    {
        licenseOverflowTime = query.value( 0 ).toByteArray().toLongLong();
        m_licenseOverflowMarked = licenseOverflowTime > 0;
    }

    QnPeerRuntimeInfo localInfo = QnRuntimeInfoManager::instance()->localInfo();
    if( localInfo.data.prematureLicenseExperationDate != licenseOverflowTime )
    {
        localInfo.data.prematureLicenseExperationDate = licenseOverflowTime;
        QnRuntimeInfoManager::instance()->updateLocalItem( localInfo );
    }

    query.addBindValue( DB_INSTANCE_KEY );
    if(!m_needResyncLog && query.exec() && query.next())
    {
        m_dbInstanceId = QnUuid::fromRfc4122( query.value( 0 ).toByteArray() );
    }
    else
    {
        m_dbInstanceId = QnUuid::createUuid();
        QSqlQuery insQuery( m_sdb );
        insQuery.prepare( "INSERT OR REPLACE INTO misc_data (key, data) values (?,?)" );
        insQuery.addBindValue( DB_INSTANCE_KEY );
        insQuery.addBindValue( m_dbInstanceId.toRfc4122() );
        if( !insQuery.exec() )
        {
            qWarning() << "can't initialize sqlLite database!";
            return false;
        }
    }

    if( QnTransactionLog::instance() )
        if( !QnTransactionLog::instance()->init() )
        {
            qWarning() << "can't initialize transaction log!";
            return false;
        }

    if( m_needResyncLog ) {
        if (!resyncTransactionLog())
		    return false;
    }
    else {
        if (m_needResyncLicenses) {
            if (!fillTransactionLogInternal<ApiLicenseData, ApiLicenseDataList>(ApiCommand::addLicense))
                return false;
        }
        if (m_needResyncFiles) {
            if (!fillTransactionLogInternal<ApiStoredFileData, ApiStoredFileDataList>(ApiCommand::addStoredFile))
                return false;
        }
    }

    // Set admin user's password
    ApiUserDataList users;
    ErrorCode errCode = doQueryNoLock( m_adminUserID, users );
    if( errCode != ErrorCode::ok )
    {
        return false;
    }

    if( users.empty() )
    {
        return false;
    }

    QByteArray md5Password;
    QByteArray digestPassword;
    qnCommon->adminPasswordData(&md5Password, &digestPassword);
    QString defaultAdminPassword = qnCommon->defaultAdminPassword();
    if( users[0].hash.isEmpty() && defaultAdminPassword.isEmpty() ) {
        defaultAdminPassword = lit("123");
    }

    QnUserResourcePtr userResource( new QnUserResource() );
    fromApiToResource( users[0], userResource );
    bool updateUserResource = false;
    if( !defaultAdminPassword.isEmpty() )
    {
        if (!userResource->checkPassword(defaultAdminPassword)) {
            userResource->setPassword( defaultAdminPassword );
            userResource->generateHash();
            updateUserResource = true;
        }
    }
    if (!md5Password.isEmpty() || !digestPassword.isEmpty()) {
        userResource->setHash(md5Password);
        userResource->setDigest(digestPassword);
        updateUserResource = true;
    }
    if (updateUserResource) 
    {
        // admin user resource has been updated
        QnTransaction<ApiUserData> userTransaction( ApiCommand::saveUser );
        transactionLog->fillPersistentInfo(userTransaction);
        if (qnCommon->useLowPriorityAdminPasswordHach())
            userTransaction.persistentInfo.timestamp = 1; // use hack to declare this change with low proprity in case if admin has been changed in other system (keep other admin user fields unchanged)
        fromResourceToApi( userResource, userTransaction.params );
        executeTransactionNoLock( userTransaction, QnUbjson::serialized( userTransaction ) );
    }

    QSqlQuery queryCameras( m_sdb );
    // Update cameras status
    // select cameras from servers without DB and local cameras
    // In case of database backup restore, mark all cameras as offline (we are going to push our data to all other servers)
    queryCameras.setForwardOnly(true);
    QString serverCondition;
    if (!m_isBackupRestore)
         serverCondition = lit("AND ((s.flags & 2) or sr.guid = ?)");
    queryCameras.prepare(lit("SELECT r.guid FROM vms_resource r \
                         JOIN vms_resource_status rs on rs.guid = r.guid \
                         JOIN vms_camera c on c.resource_ptr_id = r.id \
                         JOIN vms_resource sr on sr.guid = r.parent_guid \
                         JOIN vms_server s on s.resource_ptr_id = sr.id \
                         WHERE coalesce(rs.status,0) != ? %1").arg(serverCondition));
    queryCameras.bindValue(0, Qn::Offline);
    if (!m_isBackupRestore)
        queryCameras.bindValue(1, qnCommon->moduleGUID().toRfc4122());
    if (!queryCameras.exec()) {
        qWarning() << Q_FUNC_INFO << __LINE__ << queryCameras.lastError();
        Q_ASSERT( 0 );
        return false;
    }
    while( queryCameras.next() )
    {
        QnTransaction<ApiResourceStatusData> tran( ApiCommand::setResourceStatus );
        transactionLog->fillPersistentInfo(tran);
        tran.params.id = QnUuid::fromRfc4122(queryCameras.value(0).toByteArray());
        tran.params.status = Qn::Offline;
        if (executeTransactionNoLock( tran, QnUbjson::serialized( tran ) ) != ErrorCode::ok)
            return false;
    }

    if (!locker.commit())
        return false;

    emit initialized();
    m_initialized = true;

    return true;
}

template <class ObjectType, class ObjectListType>
bool QnDbManager::fillTransactionLogInternal(ApiCommand::Value command)
{
    ObjectListType objects;
    ErrorCode errCode = doQueryNoLock(nullptr, objects);
    if (errCode != ErrorCode::ok)
        return false;

    for(const ObjectType& object: objects)
    {
        QnTransaction<ObjectType> transaction(command, object);
        transactionLog->fillPersistentInfo(transaction);
        if (transactionLog->saveTransaction(transaction) != ErrorCode::ok)
            return false;
    }
    return true;
}

bool QnDbManager::resyncTransactionLog()
{
    if (!fillTransactionLogInternal<ApiUserData, ApiUserDataList>(ApiCommand::saveUser))
        return false;
    if (!fillTransactionLogInternal<ApiMediaServerData, ApiMediaServerDataList>(ApiCommand::saveMediaServer))
        return false;
    if (!fillTransactionLogInternal<ApiMediaServerUserAttributesData, ApiMediaServerUserAttributesDataList>(ApiCommand::saveServerUserAttributes))
        return false;
    if (!fillTransactionLogInternal<ApiCameraData, ApiCameraDataList>(ApiCommand::saveCamera))
        return false;
    if (!fillTransactionLogInternal<ApiCameraAttributesData, ApiCameraAttributesDataList>(ApiCommand::saveCameraUserAttributes))
        return false;
    if (!fillTransactionLogInternal<ApiLayoutData, ApiLayoutDataList>(ApiCommand::saveLayout))
        return false;
    if (!fillTransactionLogInternal<ApiBusinessRuleData, ApiBusinessRuleDataList>(ApiCommand::saveBusinessRule))
        return false;
    if (!fillTransactionLogInternal<ApiResourceParamWithRefData, ApiResourceParamWithRefDataList>(ApiCommand::setResourceParam))
        return false;

    if (!fillTransactionLogInternal<ApiStorageData, ApiStorageDataList>(ApiCommand::saveStorage))
        return false;

    if (!fillTransactionLogInternal<ApiLicenseData, ApiLicenseDataList>(ApiCommand::addLicense))
        return false;

    if (!fillTransactionLogInternal<ApiCameraServerItemData, ApiCameraServerItemDataList>(ApiCommand::addCameraHistoryItem))
        return false;

    if (!fillTransactionLogInternal<ApiStoredFileData, ApiStoredFileDataList>(ApiCommand::addStoredFile))
        return false;

    return true;
}

bool QnDbManager::isInitialized() const
{
    return m_initialized;
}

QMap<int, QnUuid> QnDbManager::getGuidList( const QString& request, GuidConversionMethod method, const QByteArray& intHashPostfix )
{
    QMap<int, QnUuid>  result;
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare(request);
    if (!query.exec())
        return result;

    while (query.next())
    {
        qint32 id = query.value(0).toInt();
        QVariant data = query.value(1);
        switch (method)
        {
        case CM_Binary:
            result.insert(id, QnUuid::fromRfc4122(data.toByteArray()));
            break;
        case CM_MakeHash:
            {
                QCryptographicHash md5Hash( QCryptographicHash::Md5 );
                md5Hash.addData(data.toString().toUtf8());
                QByteArray ha2 = md5Hash.result();
                result.insert(id, QnUuid::fromRfc4122(ha2));
                break;
            }
        case CM_INT:
            result.insert(id, intToGuid(id, intHashPostfix));
            break;
        case CM_String:
            result.insert(id, QnUuid(data.toString()));
            break;
        default:
            {
                if (data.toString().isEmpty())
                    result.insert(id, intToGuid(id, intHashPostfix));
                else {
                    QnUuid guid(data.toString());
                    if (guid.isNull()) {
                        QCryptographicHash md5Hash( QCryptographicHash::Md5 );
                        md5Hash.addData(data.toString().toUtf8());
                        QByteArray ha2 = md5Hash.result();
                        guid = QnUuid::fromRfc4122(ha2);
                    }
                    result.insert(id, guid);
                }
            }
        }
    }

    return result;
}

bool QnDbManager::updateTableGuids(const QString& tableName, const QString& fieldName, const QMap<int, QnUuid>& guids)
{
#ifdef DB_DEBUG
    int n = guids.size();
    qDebug() << "updating table guids" << n << "commands queued";
    int i = 0;
#endif // DB_DEBUG
    for(QMap<int, QnUuid>::const_iterator itr = guids.begin(); itr != guids.end(); ++itr)
    {
#ifdef DB_DEBUG
        qDebug() << QString(QLatin1String("processing guid %1 of %2")).arg(++i).arg(n);
#endif // DB_DEBUG
        QSqlQuery query(m_sdb);
        query.prepare(QString("UPDATE %1 SET %2 = :guid WHERE id = :id").arg(tableName).arg(fieldName));
        query.bindValue(":id", itr.key());
        query.bindValue(":guid", itr.value().toRfc4122());
        if (!query.exec()) {
            qWarning() << Q_FUNC_INFO << query.lastError().text();
            return false;
        }
    }
    return true;
}

bool QnDbManager::updateResourceTypeGuids()
{
    QMap<int, QnUuid> guids = 
        getGuidList("SELECT rt.id, rt.name || coalesce(m.name,'-') as guid from vms_resourcetype rt LEFT JOIN vms_manufacture m on m.id = rt.manufacture_id WHERE rt.guid is null", CM_MakeHash);
    return updateTableGuids("vms_resourcetype", "guid", guids);
}

bool QnDbManager::updateGuids()
{
    QMap<int, QnUuid> guids = getGuidList("SELECT id, guid from vms_resource_tmp order by id", CM_Default, QnUuid::createUuid().toByteArray());
    if (!updateTableGuids("vms_resource", "guid", guids))
        return false;

    guids = getGuidList("SELECT resource_ptr_id, physical_id from vms_camera order by resource_ptr_id", CM_MakeHash);
    if (!updateTableGuids("vms_resource", "guid", guids))
        return false;

    guids = getGuidList("SELECT li.id, r.guid FROM vms_layoutitem_tmp li JOIN vms_resource r on r.id = li.resource_id order by li.id", CM_Binary);
    if (!updateTableGuids("vms_layoutitem", "resource_guid", guids))
        return false;

    guids = getGuidList("SELECT li.id, li.uuid FROM vms_layoutitem_tmp li order by li.id", CM_String);
    if (!updateTableGuids("vms_layoutitem", "uuid", guids))
        return false;
    guids = getGuidList("SELECT li.id, li.zoom_target_uuid FROM vms_layoutitem_tmp li order by li.id", CM_String);
    if (!updateTableGuids("vms_layoutitem", "zoom_target_uuid", guids))
        return false;

    if (!updateResourceTypeGuids())
        return false;

    guids = getGuidList("SELECT r.id, r2.guid from vms_resource_tmp r JOIN vms_resource r2 on r2.id = r.parent_id order by r.id", CM_Binary);
    if (!updateTableGuids("vms_resource", "parent_guid", guids))
        return false;

    guids = getGuidList("SELECT r.id, rt.guid from vms_resource_tmp r JOIN vms_resourcetype rt on rt.id = r.xtype_id", CM_Binary);
    if (!updateTableGuids("vms_resource", "xtype_guid", guids))
        return false;

    // update default rules
    guids = getGuidList("SELECT id, id from vms_businessrule WHERE (id between 1 and 19) or (id between 10020 and 10023) ORDER BY id", CM_INT, "DEFAULT_BUSINESS_RULES");
    if (!updateTableGuids("vms_businessrule", "guid", guids))
        return false;

    // update user's rules
    guids = getGuidList("SELECT id, id from vms_businessrule WHERE guid is null ORDER BY id", CM_INT, QnUuid::createUuid().toByteArray());
    if (!updateTableGuids("vms_businessrule", "guid", guids))
        return false;

    return true;
}

void scanDirectoryRecursive(const QString &directory, QStringList &result) {
    QDir sourceDirectory(directory);
    for(const QFileInfo& info: sourceDirectory.entryInfoList(QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Files, QDir::DirsFirst)) {
        if (info.isDir())
            scanDirectoryRecursive(info.absoluteFilePath(), result);
        else
            result.append(info.absoluteFilePath());
    }
};

ErrorCode QnDbManager::insertOrReplaceStoredFile(const QString &fileName, const QByteArray &fileContents) {
    QSqlQuery query(m_sdb);
    query.prepare("INSERT OR REPLACE INTO vms_storedFiles (path, data) values (:path, :data)");
    query.bindValue(":path", fileName);
    query.bindValue(":data", fileContents);
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }
    return ErrorCode::ok;
}


/** Insert sample files into database. */
bool QnDbManager::addStoredFiles(const QString& baseDirectoryName, int* count) 
{
    if (count)
        *count = 0;

    /* Directory name selected equal to the database table name. */

    /* Get all files from the base directory in the resources. Enter folders recursively. */
    QStringList files;
    scanDirectoryRecursive(baseDirectoryName, files);

    /* Add each file to database */
    QDir baseDir(baseDirectoryName);
    for(const QString &fileName: files) {
        QString relativeName = baseDir.relativeFilePath(fileName);
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly))
            return false;
        QByteArray data = file.readAll();

        ErrorCode status = insertOrReplaceStoredFile(relativeName, data);
        if (status != ErrorCode::ok)
            return false;

        if (count)
            ++(*count);
    }
    return true;
}

namespace oldBusinessData // TODO: #Elric #EC2 sane naming
{
    enum BusinessEventType
    {
        NotDefinedEvent,
        Camera_Motion,
        Camera_Input,
        Camera_Disconnect,
        Storage_Failure,
        Network_Issue,
        Camera_Ip_Conflict,
        MediaServer_Failure,
        MediaServer_Conflict,
        MediaServer_Started
    };

    enum BusinessActionType
    {
        UndefinedAction,
        CameraOutput,
        Bookmark,
        CameraRecording,
        PanicRecording,
        SendMail,
        Diagnostics,
        ShowPopup,
        CameraOutputInstant,
        PlaySound,
        SayText,
        PlaySoundRepeated
    };
}

int EventRemapData[][2] =
{
    { oldBusinessData::Camera_Motion,        QnBusiness::CameraMotionEvent     },
    { oldBusinessData::Camera_Input,         QnBusiness::CameraInputEvent      },
    { oldBusinessData::Camera_Disconnect,    QnBusiness::CameraDisconnectEvent },
    { oldBusinessData::Storage_Failure,      QnBusiness::StorageFailureEvent   },
    { oldBusinessData::Network_Issue,        QnBusiness::NetworkIssueEvent     },
    { oldBusinessData::Camera_Ip_Conflict,   QnBusiness::CameraIpConflictEvent },
    { oldBusinessData::MediaServer_Failure,  QnBusiness::ServerFailureEvent    },
    { oldBusinessData::MediaServer_Conflict, QnBusiness::ServerConflictEvent   },
    { oldBusinessData::MediaServer_Started,  QnBusiness::ServerStartEvent      },
    { -1,                                    -1                                }
};

int ActionRemapData[][2] =
{
    { oldBusinessData::CameraOutput,        QnBusiness::CameraOutputAction     },
    { oldBusinessData::Bookmark,            QnBusiness::BookmarkAction         },
    { oldBusinessData::CameraRecording,     QnBusiness::CameraRecordingAction  },
    { oldBusinessData::PanicRecording,      QnBusiness::PanicRecordingAction   },
    { oldBusinessData::SendMail,            QnBusiness::SendMailAction         },
    { oldBusinessData::Diagnostics,         QnBusiness::DiagnosticsAction      },
    { oldBusinessData::ShowPopup,           QnBusiness::ShowPopupAction        },
    { oldBusinessData::CameraOutputInstant, QnBusiness::CameraOutputOnceAction },
    { oldBusinessData::PlaySound,           QnBusiness::PlaySoundOnceAction    },
    { oldBusinessData::SayText,             QnBusiness::SayTextAction          },
    { oldBusinessData::PlaySoundRepeated,   QnBusiness::PlaySoundAction        },
    { -1,                                   -1                                 }
};

int remapValue(int oldVal, const int remapData[][2])
{
    for (int i = 0; remapData[i][0] >= 0; ++i) 
    {
        if (remapData[i][0] == oldVal)
            return remapData[i][1];
    }
    return oldVal;
}

bool QnDbManager::doRemap(int id, int newVal, const QString& fieldName)
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    QString sqlText = QString(lit("UPDATE vms_businessrule set %1 = ? where id = ?")).arg(fieldName);
    query.prepare(sqlText);
    query.addBindValue(newVal);
    query.addBindValue(id);
    return query.exec();
}

struct BeRemapData
{
    BeRemapData(): id(0), eventType(0), actionType(0) {}

    int id;
    int eventType;
    int actionType;
};

bool QnDbManager::migrateBusinessEvents()
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare("SELECT id,event_type, action_type from vms_businessrule");
    if (!query.exec())
        return false;
    
    QVector<BeRemapData> oldData;
    while (query.next())
    {
        BeRemapData data;
        data.id = query.value("id").toInt();
        data.eventType = query.value("event_type").toInt();
        data.actionType = query.value("action_type").toInt();
        oldData << data;
    }

    for(const BeRemapData& remapData: oldData) 
    {
        if (!doRemap(remapData.id, remapValue(remapData.eventType, EventRemapData), "event_type"))
            return false;
        if (!doRemap(remapData.id, remapValue(remapData.actionType, ActionRemapData), "action_type"))
            return false;
    }

    return true;
}

bool QnDbManager::applyUpdates()
{
    QSqlQuery existsUpdatesQuery(m_sdb);
    existsUpdatesQuery.prepare("SELECT migration from south_migrationhistory");
    if (!existsUpdatesQuery.exec())
        return false;
    QStringList existUpdates;
    while (existsUpdatesQuery.next())
        existUpdates << existsUpdatesQuery.value(0).toString();


    QDir dir(":/updates");
    for(const QFileInfo& entry: dir.entryInfoList(QDir::NoDotAndDotDot | QDir::Files, QDir::Name))
    {
        QString fileName = entry.absoluteFilePath();
        if (!existUpdates.contains(fileName)) 
        {
            if (!beforeInstallUpdate(fileName))
                return false;
            if (!execSQLFile(fileName, m_sdb))
                return false;
            if (!afterInstallUpdate(fileName))
                return false;

            QSqlQuery insQuery(m_sdb);
            insQuery.prepare("INSERT INTO south_migrationhistory (app_name, migration, applied) values(?, ?, ?)");
            insQuery.addBindValue(qApp->applicationName());
            insQuery.addBindValue(fileName);
            insQuery.addBindValue(QDateTime::currentDateTime());
            if (!insQuery.exec()) {
                qWarning() << Q_FUNC_INFO << __LINE__ << insQuery.lastError();
                return false;
                }
        }
    }

    return true;
}

bool QnDbManager::beforeInstallUpdate(const QString& updateName)
{
    if (updateName == lit(":/updates/29_update_history_guid.sql")) {
        return updateCameraHistoryGuids(); // perform string->guid conversion before SQL update because of reducing field size to 16 bytes. Probably data would lost if moved it to afterInstallUpdate
    }

    return true;
}

bool QnDbManager::removeServerStatusFromTransactionLog()
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare("SELECT r.guid from vms_server s JOIN vms_resource r on r.id = s.resource_ptr_id");
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << __LINE__ << query.lastError();
        return false;
    }
    QSqlQuery delQuery(m_sdb);
    delQuery.prepare("DELETE from transaction_log WHERE tran_guid = ?");
    while (query.next()) {
        ApiResourceStatusData data;
        data.id = QnUuid::fromRfc4122(query.value(0).toByteArray());
        QnUuid hash = transactionLog->transactionHash(data);
        delQuery.bindValue(0, QnSql::serialized_field(hash));
        if (!delQuery.exec()) {
            qWarning() << Q_FUNC_INFO << __LINE__ << delQuery.lastError();
            return false;
        }
    }

    return true;
}

bool QnDbManager::tuneDBAfterOpen()
{
    QSqlQuery enableWalQuery(m_sdb);
    enableWalQuery.prepare("PRAGMA journal_mode = WAL");
    if( !enableWalQuery.exec() )
    {
        qWarning() << "Failed to enable WAL mode on sqlLite database!" << enableWalQuery.lastError().text();
        return false;
    }

    QSqlQuery enableFKQuery(m_sdb);
    enableFKQuery.prepare("PRAGMA foreign_keys = ON");
    if( !enableFKQuery.exec() )
    {
        qWarning() << "Failed to enable FK support on sqlLite database!" << enableFKQuery.lastError().text();
        return false;
    }

    return true;
}

bool QnDbManager::updateCameraHistoryGuids()
{
    QMap<int, QnUuid> guids = getGuidList("SELECT id, server_guid from vms_cameraserveritem", CM_Default);
    if (!updateTableGuids("vms_cameraserveritem", "server_guid", guids))
        return false;

    // migrate transaction log
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare(QString("SELECT tran_guid, tran_data from transaction_log"));
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    QSqlQuery updQuery(m_sdb);
    updQuery.prepare(QString("UPDATE transaction_log SET tran_data = ? WHERE tran_guid = ?"));

    while (query.next()) {
        QnAbstractTransaction abstractTran;
        QnUuid tranGuid = QnSql::deserialized_field<QnUuid>(query.value(0));
        QByteArray srcData = query.value(1).toByteArray();
        QnUbjsonReader<QByteArray> stream(&srcData);
        if (!QnUbjson::deserialize(&stream, &abstractTran)) {
            qWarning() << Q_FUNC_INFO << "Can' deserialize transaction from transaction log";
            return false;
        }
        if (abstractTran.command != ApiCommand::addCameraHistoryItem) 
            continue;
        ApiCameraServerItemDataOld oldHistoryData;
        if (!QnUbjson::deserialize(&stream, &oldHistoryData))
        {
            qWarning() << Q_FUNC_INFO << "Can' deserialize transaction from transaction log";
            return false;
        }
        QnTransaction<ApiCameraServerItemData> newTran(abstractTran);
        newTran.params.cameraUniqueId = oldHistoryData.cameraUniqueId;
        newTran.params.serverGuid = QnUuid(oldHistoryData.serverId);
        newTran.params.timestamp = oldHistoryData.timestamp;

        updQuery.addBindValue(QnUbjson::serialized(newTran));
        updQuery.addBindValue(QnSql::serialized_field(tranGuid));
        if (!updQuery.exec()) {
            qWarning() << Q_FUNC_INFO << query.lastError().text();
            return false;
        }
    }

    return true;

}

bool QnDbManager::afterInstallUpdate(const QString& updateName)
{
    if (updateName == lit(":/updates/07_videowall.sql")) 
    {
        QMap<int, QnUuid> guids = getGuidList("SELECT rt.id, rt.name || '-' as guid from vms_resourcetype rt WHERE rt.name == 'Videowall'", CM_MakeHash);
        if (!updateTableGuids("vms_resourcetype", "guid", guids))
            return false;
    }
    else if (updateName == lit(":/updates/17_add_isd_cam.sql")) {
        updateResourceTypeGuids();
    }
    else if (updateName == lit(":/updates/20_adding_camera_user_attributes.sql")) {
        if (!m_dbJustCreated)
            m_needResyncLog = true;
    }    
    else if (updateName == lit(":/updates/21_new_dw_cam.sql")) {
        updateResourceTypeGuids();
    }
    else if (updateName == lit(":/updates/24_insert_default_stored_files.sql")) {
        addStoredFiles(lit(":/vms_storedfiles/"));
    }
    else if (updateName == lit(":/updates/27_remove_server_status.sql")) {
        return removeServerStatusFromTransactionLog();
    }

    return true;
}

bool QnDbManager::createDatabase()
{
    QnDbTransactionLocker lock(&m_tran);

    m_dbJustCreated = false;

    if (!isObjectExists(lit("table"), lit("vms_resource"), m_sdb))
    {
        NX_LOG(QString("Create new database"), cl_logINFO);

        m_dbJustCreated = true;

        if (!execSQLFile(lit(":/01_createdb.sql"), m_sdb))
            return false;

        if (!execSQLFile(lit(":/02_insert_all_vendors.sql"), m_sdb))
            return false;
    }

    if (!isObjectExists(lit("table"), lit("transaction_log"), m_sdb))
    {
		NX_LOG(QString("Update database to v 2.3"), cl_logINFO);
        if (!migrateBusinessEvents())
            return false;
        if (!m_dbJustCreated) {
            m_needResyncLog = true;
            if (!execSQLFile(lit(":/02_migration_from_2_2.sql"), m_sdb))
                return false;
        }

        if (!execSQLFile(lit(":/03_update_2.2_stage1.sql"), m_sdb))
            return false;
        if (!updateGuids())
            return false;
        if (!execSQLFile(lit(":/04_update_2.2_stage2.sql"), m_sdb))
            return false;
    }

    QnDbTransactionLocker lockStatic(&m_tranStatic);

    if (!isObjectExists(lit("table"), lit("vms_license"), m_sdbStatic))
    {
        if (!execSQLFile(lit(":/05_staticdb_add_license_table.sql"), m_sdbStatic))
            return false;
    }
    else if (m_dbJustCreated)
        m_needResyncLicenses = true;
    
    // move license table to static DB
    ec2::ApiLicenseDataList licenses;
    QSqlQuery query(m_sdb);
    query.prepare(lit("SELECT license_key as key, license_block as licenseBlock from vms_license"));
    if (!query.exec())
    {
        qWarning() << Q_FUNC_INFO << __LINE__ << query.lastError();
        return false;
    }
    QnSql::fetch_many(query, &licenses);

    for(const ApiLicenseData& data: licenses)
    {
        if (saveLicense(data) != ErrorCode::ok)
            return false;
        m_needResyncLicenses = true;
    }
    if (!execSQLQuery("DELETE FROM vms_license", m_sdb))
        return false;


    if (!applyUpdates())
        return false;

    if (!lockStatic.commit())
        return false;
#ifdef DB_DEBUG
    qDebug() << "database created successfully";
#endif // DB_DEBUG
    return lock.commit();
}

QnDbManager::~QnDbManager()
{
    globalInstance = 0;
}

QnDbManager* QnDbManager::instance()
{
    return globalInstance;
}

ErrorCode QnDbManager::insertAddParam(const ApiResourceParamWithRefData& param)
{
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT OR REPLACE INTO vms_kvpair(resource_guid, name, value) VALUES(?, ?, ?)");

    insQuery.bindValue(0, QnSql::serialized_field(param.resourceId));
    insQuery.bindValue(1, QnSql::serialized_field(param.name));
    insQuery.bindValue(2, QnSql::serialized_field(param.value));
    if (!insQuery.exec()) {
        qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
        return ErrorCode::dbError;
    }        
    return ErrorCode::ok;
}

ErrorCode QnDbManager::fetchResourceParams( const QnQueryFilter& filter, ApiResourceParamWithRefDataList& params )
{
    const QnUuid resID = filter.fields.value( RES_ID_FIELD ).value<QnUuid>();
    const QnUuid resParentID = resID.isNull() ? filter.fields.value( RES_PARENT_ID_FIELD ).value<QnUuid>() : QnUuid();
    const QByteArray resType = filter.fields.value( RES_TYPE_FIELD ).toByteArray();

    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    QString queryStr = "\
            SELECT kv.resource_guid as resourceId, kv.value, kv.name \
            FROM vms_kvpair kv ";
    if( !resType.isEmpty() || !resParentID.isNull())
    {
        queryStr += 
           "JOIN vms_resource r on r.guid = kv.resource_guid ";
        if( resType == RES_TYPE_MSERVER )
            queryStr += 
           "JOIN vms_server s on s.resource_ptr_id = r.id ";
        else if( resType == RES_TYPE_CAMERA )
            queryStr += 
           "JOIN vms_camera c on c.resource_ptr_id = r.id ";
    }
    if( !resID.isNull() )
        queryStr += "\
            WHERE kv.resource_guid = :guid ";
    if( !resParentID.isNull() )
        queryStr += "\
                    WHERE r.parent_guid = :parentGuid ";
    queryStr += 
           "ORDER BY kv.resource_guid ";

    if( !query.prepare( queryStr ) )
    {
        NX_LOG( lit("Could not prepare query %1: %2").arg(queryStr).arg(query.lastError().text()), cl_logWARNING );
        return ErrorCode::dbError;
    }

    if( !resID.isNull() )
        query.bindValue(QLatin1String(":guid"), resID.toRfc4122());
    if( !resParentID.isNull() )
        query.bindValue(QLatin1String(":parentGuid"), resParentID.toRfc4122());
    if (!query.exec())
    {
        NX_LOG( lit("DB error at %1: %2").arg(Q_FUNC_INFO).arg(query.lastError().text()), cl_logWARNING );
        return ErrorCode::dbError;
    }

    QnSql::fetch_many(query, &params);

    return ErrorCode::ok;
}

/*
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
        return ErrorCode::dbError;
    }
}
*/

qint32 QnDbManager::getResourceInternalId( const QnUuid& guid ) {
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare("SELECT id from vms_resource where guid = ?");
    query.bindValue(0, guid.toRfc4122());
    if (!query.exec() || !query.next())
        return 0;
    return query.value(0).toInt();
}

QnUuid QnDbManager::getResourceGuid(const qint32 &internalId) {
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare("SELECT guid from vms_resource where id = ?");
    query.bindValue(0, internalId);
    if (!query.exec() || !query.next())
        return QnUuid();
    return QnUuid::fromRfc4122(query.value(0).toByteArray());
}

ErrorCode QnDbManager::insertOrReplaceResource(const ApiResourceData& data, qint32* internalId)
{
    *internalId = getResourceInternalId(data.id);

    //Q_ASSERT_X(data.status == Qn::NotDefined, Q_FUNC_INFO, "Status MUST be unchanged for resource modification. Use setStatus instead to modify it!");

    QSqlQuery query(m_sdb);
    if (*internalId) {
        query.prepare("UPDATE vms_resource SET guid = :id, xtype_guid = :typeId, parent_guid = :parentId, name = :name, url = :url WHERE id = :internalId");
        query.bindValue(":internalId", *internalId);
    }
    else {
        query.prepare("INSERT INTO vms_resource (guid, xtype_guid, parent_guid, name, url) VALUES(:id, :typeId, :parentId, :name, :url)");
    }
    QnSql::bind(data, &query);

    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }
    if (*internalId == 0)
        *internalId = query.lastInsertId().toInt();

    return ErrorCode::ok;
}

ErrorCode QnDbManager::insertOrReplaceUser(const ApiUserData& data, qint32 internalId)
{
    QSqlQuery insQuery(m_sdb);
    if (!data.hash.isEmpty())
        insQuery.prepare("\
            INSERT OR REPLACE INTO auth_user (id, username, is_superuser, email, password, is_staff, is_active, last_login, date_joined, first_name, last_name) \
            VALUES (:internalId, :name, :isAdmin, :email, :hash, 1, 1, '', '', '', '')\
        ");
    else
        insQuery.prepare("UPDATE auth_user SET is_superuser=:isAdmin, email=:email where username=:name");
    QnSql::bind(data, &insQuery);
    insQuery.bindValue(":internalId", internalId);
    //insQuery.bindValue(":name", data.name);
    if (!insQuery.exec())
    {
        qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
        return ErrorCode::dbError;
    }

    QSqlQuery insQuery2(m_sdb);
    insQuery2.prepare("INSERT OR REPLACE INTO vms_userprofile (user_id, resource_ptr_id, digest, rights) VALUES (:internalId, :internalId, :digest, :permissions)");
    QnSql::bind(data, &insQuery2);
    if (data.digest.isEmpty())
    {
        // keep current digest value if exists
        QSqlQuery digestQuery(m_sdb);
        digestQuery.prepare("SELECT digest FROM vms_userprofile WHERE user_id = ?");
        digestQuery.addBindValue(internalId);
        if (!digestQuery.exec()) {
            qWarning() << Q_FUNC_INFO << digestQuery.lastError().text();
            return ErrorCode::dbError;
        }
        if (digestQuery.next())
            insQuery2.bindValue(":digest", digestQuery.value(0).toByteArray());
    }
    insQuery2.bindValue(":internalId", internalId);
    if (!insQuery2.exec())
    {
        qWarning() << Q_FUNC_INFO << insQuery2.lastError().text();
        return ErrorCode::dbError;
    }

    return ErrorCode::ok;
}

ErrorCode QnDbManager::insertOrReplaceCamera(const ApiCameraData& data, qint32 internalId)
{
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("\
        INSERT OR REPLACE INTO vms_camera (vendor, manually_added, resource_ptr_id, group_name, group_id,\
        mac, model, status_flags, physical_id) VALUES\
        (:vendor, :manuallyAdded, :internalId, :groupName, :groupId, :mac, :model, :statusFlags, :physicalId)\
    ");
    QnSql::bind(data, &insQuery);
    insQuery.bindValue(":internalId", internalId);
    if (insQuery.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
        return ErrorCode::dbError;
    }
}

ErrorCode QnDbManager::insertOrReplaceCameraAttributes(const ApiCameraAttributesData& data, qint32* const internalId)
{
    //TODO #ak if record already exists have to find id first?

    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("\
        INSERT OR REPLACE INTO vms_camera_user_attributes ( \
            camera_guid,                    \
            camera_name,                    \
            audio_enabled,                  \
            control_enabled,                \
            region,                         \
            schedule_enabled,               \
            motion_type,                    \
            secondary_quality,              \
            dewarping_params,               \
            min_archive_days,               \
            max_archive_days,               \
            prefered_server_id )            \
         VALUES (                           \
            :cameraID,                      \
            :cameraName,                    \
            :audioEnabled,                  \
            :controlEnabled,                \
            :motionMask,                    \
            :scheduleEnabled,               \
            :motionType,                    \
            :secondaryStreamQuality,        \
            :dewarpingParams,               \
            :minArchiveDays,                \
            :maxArchiveDays,                \
            :preferedServerId )             \
        ");
    QnSql::bind(data, &insQuery);
    if( !insQuery.exec() )
    {
        NX_LOG( lit("DB error in %1: %2").arg(Q_FUNC_INFO).arg(insQuery.lastError().text()), cl_logWARNING );
        return ErrorCode::dbError;
    }

    *internalId = insQuery.lastInsertId().toInt();
#if 0
    QSqlQuery renameQuery(m_sdb);
    renameQuery.prepare("UPDATE vms_resource set name = ? WHERE guid = ?");
    renameQuery.addBindValue(data.cameraName);
    renameQuery.addBindValue(QnSql::serialized_field(data.cameraID));
    if( !renameQuery.exec() )
    {
        NX_LOG( lit("DB error in %1: %2").arg(Q_FUNC_INFO).arg(renameQuery.lastError().text()), cl_logWARNING );
        return ErrorCode::dbError;
    }
#endif
    return ErrorCode::ok;
}

ErrorCode QnDbManager::insertOrReplaceMediaServer(const ApiMediaServerData& data, qint32 internalId)
{
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("\
        INSERT OR REPLACE INTO vms_server (api_url, auth_key, version, net_addr_list, system_info, flags, system_name, resource_ptr_id, panic_mode) \
        VALUES (:apiUrl, :authKey, :version, :networkAddresses, :systemInfo, :flags, :systemName, :internalId, 0)\
    ");
    QnSql::bind(data, &insQuery);

    if (data.authKey.isEmpty())
    {
        QSqlQuery selQuery(m_sdb);
        selQuery.prepare("SELECT auth_key from vms_server where resource_ptr_id = ?");
        selQuery.addBindValue(internalId);
        if (selQuery.exec() && selQuery.next())
            insQuery.bindValue(":authKey", selQuery.value(0).toString());
    }

    insQuery.bindValue(":internalId", internalId);
    if (insQuery.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
        return ErrorCode::dbError;
    }
}


ErrorCode QnDbManager::insertOrReplaceLayout(const ApiLayoutData& data, qint32 internalId)
{
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("\
        INSERT OR REPLACE INTO vms_layout \
        (user_can_edit, cell_spacing_height, locked, \
        cell_aspect_ratio, background_width, \
        background_image_filename, background_height, \
        cell_spacing_width, background_opacity, resource_ptr_id) \
        \
        VALUES (:editable, :verticalSpacing, :locked, \
        :cellAspectRatio, :backgroundWidth, \
        :backgroundImageFilename, :backgroundHeight, \
        :horizontalSpacing, :backgroundOpacity, :internalId)\
    ");
    QnSql::bind(data, &insQuery);
    insQuery.bindValue(":internalId", internalId);
    if (insQuery.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
        return ErrorCode::dbError;
    }
}

ErrorCode QnDbManager::removeStorage(const QnUuid& guid)
{
    qint32 id = getResourceInternalId(guid);

    ErrorCode err = deleteTableRecord(id, "vms_storage", "resource_ptr_id");
    if (err != ErrorCode::ok)
        return err;

    err = deleteRecordFromResourceTable(id);
    if (err != ErrorCode::ok)
        return err;

    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiStorageData>& tran)
{
    qint32 internalId;
    ErrorCode result = insertOrReplaceResource(tran.params, &internalId);
    if (result != ErrorCode::ok)
        return result;

    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("\
        INSERT OR REPLACE INTO vms_storage (space_limit, used_for_writing, resource_ptr_id) \
        VALUES (:spaceLimit, :usedForWriting, :internalId)\
    ");
    QnSql::bind(tran.params, &insQuery);
    insQuery.bindValue(":internalId", internalId);
    
    if (!insQuery.exec()) {
        qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
        return ErrorCode::dbError;
    }

    return ErrorCode::ok;
}

ErrorCode QnDbManager::removeCameraSchedule(qint32 internalId)
{
    QSqlQuery delQuery(m_sdb);
    delQuery.prepare("DELETE FROM vms_scheduletask where camera_attrs_id = ?");
    delQuery.addBindValue(internalId);
    if (!delQuery.exec()) {
        qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
        return ErrorCode::dbError;
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::updateCameraSchedule(const std::vector<ApiScheduleTaskData>& scheduleTasks, qint32 internalId)
{
    ErrorCode errCode = removeCameraSchedule(internalId);
    if (errCode != ErrorCode::ok)
        return errCode;

    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT INTO vms_scheduletask(camera_attrs_id, start_time, end_time, do_record_audio, record_type, day_of_week, before_threshold, after_threshold, stream_quality, fps) VALUES (?,?,?,?,?,?,?,?,?,?)");

    insQuery.bindValue(0, internalId);
    for(const ApiScheduleTaskData& task: scheduleTasks) 
    {
        insQuery.bindValue(1, QnSql::serialized_field(task.startTime));
        insQuery.bindValue(2, QnSql::serialized_field(task.endTime));
        insQuery.bindValue(3, QnSql::serialized_field(task.recordAudio));
        insQuery.bindValue(4, QnSql::serialized_field(task.recordingType));
        insQuery.bindValue(5, QnSql::serialized_field(task.dayOfWeek));
        insQuery.bindValue(6, QnSql::serialized_field(task.beforeThreshold));
        insQuery.bindValue(7, QnSql::serialized_field(task.afterThreshold));
        insQuery.bindValue(8, QnSql::serialized_field(task.streamQuality));
        insQuery.bindValue(9, QnSql::serialized_field(task.fps));

        if (!insQuery.exec()) {
            qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
            return ErrorCode::dbError;
        }
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiDatabaseDumpData>& tran)
{
    m_sdb.close();
    QFile f(m_sdb.databaseName() + QString(lit(".backup")));
    if (!f.open(QFile::WriteOnly))
        return ErrorCode::failure;
    f.write(tran.params.data);
    f.close();

    QSqlDatabase testDB = QSqlDatabase::addDatabase("QSQLITE", "QnDbManagerTmp");
    testDB.setDatabaseName( f.fileName());
    if (!testDB.open() || !isObjectExists(lit("table"), lit("transaction_log"), testDB)) {
        QFile::remove(f.fileName());
        qWarning() << "Skipping bad database dump file";
        return ErrorCode::dbError; // invalid back file
    }
    QSqlQuery testDbQuery(testDB);
    testDbQuery.prepare("INSERT OR REPLACE INTO misc_data (key, data) VALUES (?, ?)");
    testDbQuery.addBindValue("gotDbDumpTime");
    testDbQuery.addBindValue(qnSyncTime->currentMSecsSinceEpoch());
    if (!testDbQuery.exec()) {
        qWarning() << "Skipping bad database dump file";
        return ErrorCode::dbError; // invalid back file
    }
    testDB.close();
    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiResourceStatusData>& tran)
{
    QSqlQuery query(m_sdb);
    query.prepare("INSERT OR REPLACE INTO vms_resource_status values (?, ?)");
    query.addBindValue(tran.params.id.toRfc4122());
    query.addBindValue(tran.params.status);
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }
    return ErrorCode::ok;
}

/*
ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiSetResourceDisabledData>& tran)
{
    QSqlQuery query(m_sdb);
    query.prepare("UPDATE vms_resource set disabled = :disabled where guid = :guid");
    query.bindValue(":disabled", tran.params.disabled);
    query.bindValue(":guid", tran.params.id.toRfc4122());
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }
    return ErrorCode::ok;
}
*/

ErrorCode QnDbManager::saveCamera(const ApiCameraData& params)
{
    qint32 internalId;
    ErrorCode result = insertOrReplaceResource(params, &internalId);
    if (result != ErrorCode::ok)
        return result;

    return insertOrReplaceCamera(params, internalId);
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiCameraData>& tran)
{
    return saveCamera(tran.params);
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiCameraAttributesData>& tran)
{
    return saveCameraUserAttributes(tran.params);
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiCameraAttributesDataList>& tran)
{
    for(const ApiCameraAttributesData& attrs: tran.params)
    {
        const ErrorCode result = saveCameraUserAttributes(attrs);
        if (result != ErrorCode::ok)
            return result;
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::saveCameraUserAttributes( const ApiCameraAttributesData& attrs )
{
    qint32 internalId = 0;
    ErrorCode result = insertOrReplaceCameraAttributes(attrs, &internalId);
    if (result != ErrorCode::ok)
        return result;

    return updateCameraSchedule(attrs.scheduleTasks, internalId);
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiResourceData>& tran)
{
    qint32 internalId;
    return insertOrReplaceResource(tran.params, &internalId);
}

ErrorCode QnDbManager::insertBRuleResource(const QString& tableName, const QnUuid& ruleGuid, const QnUuid& resourceGuid)
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
        return ErrorCode::dbError;
    }
}

ErrorCode QnDbManager::updateBusinessRule(const ApiBusinessRuleData& rule)
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

    for(const QnUuid& resourceId: rule.eventResourceIds) {
        err = insertBRuleResource("vms_businessrule_event_resources", rule.id, resourceId);
        if (err != ErrorCode::ok)
            return err;
    }

    for(const QnUuid& resourceId: rule.actionResourceIds) {
        err = insertBRuleResource("vms_businessrule_action_resources", rule.id, resourceId);
        if (err != ErrorCode::ok)
            return err;
    }

    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiBusinessRuleData>& tran)
{
    return updateBusinessRule(tran.params);
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiMediaServerData>& tran)
{
    ErrorCode result;
    qint32 internalId;

    result = insertOrReplaceResource(tran.params, &internalId);
    if (result !=ErrorCode::ok)
        return result;

    result = insertOrReplaceMediaServer(tran.params, internalId);
    if (result !=ErrorCode::ok)
        return result;

    /*
    if (result !=ErrorCode::ok)
        return result;
    result = updateStorages(tran.params);
    */

    return result;
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiMediaServerUserAttributesData>& tran)
{
    return insertOrReplaceMediaServerUserAttributes( tran.params );
}

ErrorCode QnDbManager::insertOrReplaceMediaServerUserAttributes(const ApiMediaServerUserAttributesData& data)
{
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("\
        INSERT OR REPLACE INTO vms_server_user_attributes ( \
            server_guid,                    \
            server_name,                    \
            max_cameras,                    \
            redundancy)                     \
        VALUES(                             \
            :serverID,                      \
            :serverName,                    \
            :maxCameras,                    \
            :allowAutoRedundancy)           \
        ");
    QnSql::bind(data, &insQuery);

    if( !insQuery.exec() )
    {
        NX_LOG( lit("DB Error at %1: %2").arg(Q_FUNC_INFO).arg(insQuery.lastError().text()), cl_logWARNING );
        return ErrorCode::dbError;
    }

    return ErrorCode::ok;
}

ErrorCode QnDbManager::removeMediaServerUserAttributes(const QnUuid& guid)
{
    QSqlQuery query(m_sdb);
    query.prepare("DELETE FROM vms_server_user_attributes WHERE server_guid = :guid");
    query.bindValue(":guid", guid.toRfc4122());
    if( !query.exec() )
    {
        NX_LOG( lit("DB Error at %1: %2").arg(Q_FUNC_INFO).arg(query.lastError().text()), cl_logWARNING );
        return ErrorCode::dbError;
    }

    return ErrorCode::ok;
}

ErrorCode QnDbManager::removeLayoutItems(qint32 id)
{
    QSqlQuery delQuery(m_sdb);
    delQuery.prepare("DELETE FROM vms_layoutitem WHERE layout_id = :id");
    delQuery.bindValue(":id", id);
    if (!delQuery.exec()) {
        qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
        return ErrorCode::dbError;
    }

    return ErrorCode::ok;
}

ErrorCode QnDbManager::updateLayoutItems(const ApiLayoutData& data, qint32 internalLayoutId)
{
    ErrorCode result = removeLayoutItems(internalLayoutId);
    if (result != ErrorCode::ok)
        return result;

    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("\
        INSERT INTO vms_layoutitem (zoom_bottom, right, uuid, zoom_left, resource_guid, \
        zoom_right, top, layout_id, bottom, zoom_top, \
        zoom_target_uuid, flags, contrast_params, rotation, \
        dewarping_params, left) VALUES \
        (:zoomBottom, :right, :id, :zoomLeft, :resourceId, \
        :zoomRight, :top, :layoutId, :bottom, :zoomTop, \
        :zoomTargetId, :flags, :contrastParams, :rotation, \
        :dewarpingParams, :left)\
    ");
    for(const ApiLayoutItemData& item: data.items)
    {
        QnSql::bind(item, &insQuery);
        insQuery.bindValue(":layoutId", internalLayoutId);

        if (!insQuery.exec()) {
            qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
            return ErrorCode::dbError;
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
        return ErrorCode::dbError;
    }
}

qint32 QnDbManager::getBusinessRuleInternalId( const QnUuid& guid )
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare("SELECT id from vms_businessrule where guid = :guid");
    query.bindValue(":guid", guid.toRfc4122());
    if (!query.exec() || !query.next())
        return 0;
    return query.value("id").toInt();
}

ErrorCode QnDbManager::removeUser( const QnUuid& guid )
{
    qint32 internalId = getResourceInternalId(guid);

    ErrorCode err = ErrorCode::ok;

    //err = deleteAddParams(internalId);
    //if (err != ErrorCode::ok)
    //    return err;

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

ErrorCode QnDbManager::insertOrReplaceBusinessRuleTable( const ApiBusinessRuleData& businessRule)
{
    QSqlQuery query(m_sdb);
    query.prepare(QString("\
        INSERT OR REPLACE INTO vms_businessrule (guid, event_type, event_condition, event_state, action_type, \
        action_params, aggregation_period, disabled, comments, schedule, system) VALUES \
        (:id, :eventType, :eventCondition, :eventState, :actionType, \
        :actionParams, :aggregationPeriod, :disabled, :comment, :schedule, :system)\
    "));
    QnSql::bind(businessRule, &query);
    if (query.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }
}

ErrorCode QnDbManager::removeBusinessRule( const QnUuid& guid )
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

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiLayoutData>& tran)
{
    ErrorCode result = saveLayout(tran.params);
    return result;
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiLayoutDataList>& tran)
{
    for(const ApiLayoutData& layout: tran.params)
    {
        ErrorCode err = saveLayout(layout);
        if (err != ErrorCode::ok)
            return err;
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiVideowallData>& tran) {
    ErrorCode result = saveVideowall(tran.params);
    return result;
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiVideowallDataList>& tran) {
    for(const ApiVideowallData& videowall: tran.params)
    {
        ErrorCode err = saveVideowall(videowall);
        if (err != ErrorCode::ok)
            return err;
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiDiscoveryData> &tran) 
{
    if (tran.command == ApiCommand::addDiscoveryInformation) {
        QSqlQuery query(m_sdb);
        query.prepare("INSERT OR REPLACE INTO vms_mserver_discovery (server_id, url, ignore) VALUES(:id, :url, :ignore)");
        QnSql::bind(tran.params, &query);
        if (!query.exec()) {
            qWarning() << Q_FUNC_INFO << query.lastError().text();
            return ErrorCode::dbError;
        }
    } else if (tran.command == ApiCommand::removeDiscoveryInformation) 
    {
        QSqlQuery query(m_sdb);
        query.prepare("DELETE FROM vms_mserver_discovery WHERE server_id = :id AND url = :url");
        QnSql::bind(tran.params, &query);
        if (!query.exec()) {
            qWarning() << Q_FUNC_INFO << query.lastError().text();
            return ErrorCode::dbError;
        }
    }

    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiUpdateUploadResponceData>& /*tran*/) {
    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiResourceParamWithRefData>& tran)
{
    if (tran.command == ApiCommand::setResourceParam)
        return insertAddParam(tran.params);
    else if (tran.command == ApiCommand::removeResourceParam)
        return deleteTableRecord(tran.params.resourceId, "vms_kvpair", "resource_guid");
    else
        return ErrorCode::notImplemented;
}

ErrorCode QnDbManager::addCameraHistory(const ApiCameraServerItemData& params)
{
    QSqlQuery query(m_sdb);
    query.prepare("INSERT OR REPLACE INTO vms_cameraserveritem (server_guid, timestamp, physical_id) VALUES(:serverGuid, :timestamp, :cameraUniqueId)");
    QnSql::bind(params, &query);
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }

    return ErrorCode::ok;
}

ErrorCode QnDbManager::removeCameraHistory(const ApiCameraServerItemData& params)
{
    QSqlQuery query(m_sdb);
    query.prepare("DELETE FROM vms_cameraserveritem WHERE server_guid = :serverGuid AND timestamp = :timestamp AND physical_id = :cameraUniqueId");
    QnSql::bind(params, &query);
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }

    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiCameraServerItemData>& tran)
{
    if (tran.command == ApiCommand::addCameraHistoryItem)
        return addCameraHistory(tran.params);
    else if (tran.command == ApiCommand::removeCameraHistoryItem)
        return removeCameraHistory(tran.params);
    else {
        Q_ASSERT(1);
        return ErrorCode::unsupported;
    }
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
        return ErrorCode::dbError;
    }
}

ErrorCode QnDbManager::deleteCameraServerItemTable(qint32 /*id*/)
{
#if 0
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare("select c.physical_id , (select count(*) from vms_camera where physical_id = c.physical_id) as cnt \
                  FROM vms_camera c WHERE c.resource_ptr_id = :id");
    query.bindValue(QLatin1String(":id"), id);
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }
    if( !query.next() )
        return ErrorCode::ok;   //already deleted
    if (query.value("cnt").toInt() > 1)
        return ErrorCode::ok; // camera instance on a other server still present


    // do not delete because of camera can be found in the future again but camera archive can be still accessible
    QSqlQuery delQuery(m_sdb);
    delQuery.prepare("DELETE FROM vms_cameraserveritem where physical_id = :physical_id");
    delQuery.bindValue(QLatin1String(":physical_id"), query.value("physical_id").toString());
    if (delQuery.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
        return ErrorCode::dbError;
    }
#endif

    return ErrorCode::ok;
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
        return ErrorCode::dbError;
    }
}

ErrorCode QnDbManager::deleteTableRecord(const QnUuid& id, const QString& tableName, const QString& fieldName)
{
    QSqlQuery delQuery(m_sdb);
    delQuery.prepare(QString("DELETE FROM %1 where %2 = :guid").arg(tableName).arg(fieldName));
    delQuery.bindValue(QLatin1String(":guid"), id.toRfc4122());
    if (delQuery.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
        return ErrorCode::dbError;
    }
}

ErrorCode QnDbManager::removeCamera(const QnUuid& guid)
{
    qint32 id = getResourceInternalId(guid);

    ErrorCode err = deleteTableRecord(guid, "vms_businessrule_action_resources", "resource_guid");
    if (err != ErrorCode::ok)
        return err;

    err = deleteTableRecord(guid, "vms_businessrule_event_resources", "resource_guid");
    if (err != ErrorCode::ok)
        return err;

    err = deleteTableRecord(guid, "vms_layoutitem", "resource_guid");
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

ErrorCode QnDbManager::removeServer(const QnUuid& guid)
{
    ErrorCode err;
    qint32 id = getResourceInternalId(guid);

    //err = deleteAddParams(id);
    //if (err != ErrorCode::ok)
    //    return err;

    //err = removeStoragesByServer(guid);
    //if (err != ErrorCode::ok)
    //    return err;

    err = deleteTableRecord(id, "vms_server", "resource_ptr_id");
    if (err != ErrorCode::ok)
        return err;

    err = deleteTableRecord(id, "vms_mserver_discovery", "server_id");
    if (err != ErrorCode::ok)
        return err;

    err = deleteRecordFromResourceTable(id);
    if (err != ErrorCode::ok)
        return err;

    return ErrorCode::ok;
}

ErrorCode QnDbManager::removeLayout(const QnUuid& id)
{
    return removeLayoutInternal(id, getResourceInternalId(id));
}

ErrorCode QnDbManager::removeLayoutInternal(const QnUuid& id, const qint32 &internalId) 
{
    //ErrorCode err = deleteAddParams(internalId);
    //if (err != ErrorCode::ok)
    //    return err;

    ErrorCode err = removeLayoutItems(internalId);
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

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiStoredFileData>& tran) {
    assert( tran.command == ApiCommand::addStoredFile || tran.command == ApiCommand::updateStoredFile );
    return insertOrReplaceStoredFile(tran.params.path, tran.params.data);
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiStoredFilePath>& tran)
{
    assert(tran.command == ApiCommand::removeStoredFile);
  
    QSqlQuery query(m_sdb);
    query.prepare("DELETE FROM vms_storedFiles WHERE path = :path");
    query.bindValue(":path", tran.params.path);
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::checkExistingUser(const QString &name, qint32 internalId) {
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare("SELECT r.id\
                  FROM vms_resource r \
                  JOIN vms_userprofile p on p.resource_ptr_id = r.id \
                  WHERE p.resource_ptr_id != :id and r.name = :name");


    query.bindValue(":id", internalId);
    query.bindValue(":name", name);
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }
    if(query.next())
        return ErrorCode::failure;  // another user with same name already exists
    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiUserData>& tran)
{
    qint32 internalId = getResourceInternalId(tran.params.id);

    ErrorCode result = checkExistingUser(tran.params.name, internalId);
    if (result !=ErrorCode::ok)
        return result;

    result = insertOrReplaceResource(tran.params, &internalId);
    if (result !=ErrorCode::ok)
        return result;

    return insertOrReplaceUser(tran.params, internalId);
}

ApiOjectType QnDbManager::getObjectType(const QnUuid& objectId)
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare("\
        SELECT \
        (CASE WHEN c.resource_ptr_id is null then rt.name else 'Camera' end) as name\
        FROM vms_resource r\
        LEFT JOIN vms_resourcetype rt on rt.guid = r.xtype_guid\
        LEFT JOIN vms_camera c on c.resource_ptr_id = r.id\
        WHERE r.guid = :guid\
    ");
    query.bindValue(":guid", objectId.toRfc4122());
    if (!query.exec())
        return ApiObject_NotDefined;
    if( !query.next() )
        return ApiObject_NotDefined;   //Record already deleted. That's exactly what we wanted
    QString objectType = query.value("name").toString();
    if (objectType == "Camera")
        return ApiObject_Camera;
    else if (objectType == "Storage")
        return ApiObject_Storage;
    else if (objectType == "Server")
        return ApiObject_Server;
    else if (objectType == "User")
        return ApiObject_User;
    else if (objectType == "Layout")
        return ApiObject_Layout;
    else if (objectType == "Videowall")
        return ApiObject_Videowall;
    else 
    {
        Q_ASSERT_X(0, "Unknown object type", Q_FUNC_INFO);
        return ApiObject_NotDefined;
    }
}

ApiObjectInfoList QnDbManager::getNestedObjects(const ApiObjectInfo& parentObject)
{
    ApiObjectInfoList result;

    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);

    switch(parentObject.type)
    {
        case ApiObject_Server:
            query.prepare("\
                SELECT :cameraObjType, r.guid from vms_camera c JOIN vms_resource r on r.id = c.resource_ptr_id WHERE r.parent_guid = :guid \
                UNION \
                SELECT :storageObjType, r.guid from vms_storage s JOIN vms_resource r on r.id = s.resource_ptr_id WHERE r.parent_guid = :guid \
            ");
            query.bindValue(":cameraObjType", (int)ApiObject_Camera);
            query.bindValue(":storageObjType", (int)ApiObject_Storage);
            break;
        case ApiObject_User:
            query.prepare( "SELECT :objType, r.guid FROM vms_resource r, vms_layout WHERE r.parent_guid = :guid AND r.id = vms_layout.resource_ptr_id" );
            query.bindValue(":objType", (int)ApiObject_Layout);
            break;
        default:
            //Q_ASSERT_X(0, "Not implemented!", Q_FUNC_INFO);
            return result;
    }
    query.bindValue(":guid", parentObject.id.toRfc4122());

    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return result;
    }
    while(query.next()) {
        ApiObjectInfo info;
        info.type = (ApiOjectType) query.value(0).toInt();
        info.id = QnUuid::fromRfc4122(query.value(1).toByteArray());
        result.push_back(info);
    }

    return result;
}

bool QnDbManager::saveMiscParam( const QByteArray& name, const QByteArray& value )
{
    QnDbManager::QnDbTransactionLocker locker( getTransaction() );

    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT OR REPLACE INTO misc_data (key, data) values (?,?)");
    insQuery.addBindValue( name );
    insQuery.addBindValue( value );
    if( !insQuery.exec() )
        return false;
    locker.commit();
    return true;
}

bool QnDbManager::readMiscParam( const QByteArray& name, QByteArray* value )
{
    QReadLocker lock(&m_mutex); //locking it here since this method is public

    QSqlQuery query(m_sdb);
    query.prepare("SELECT data from misc_data where key = ?");
    query.addBindValue(name);
    if( query.exec() && query.next() ) {
        *value = query.value(0).toByteArray();
        return true;
    }
    return false;
}

ErrorCode QnDbManager::readSettings(ApiResourceParamDataList& settings)
{
    ApiResourceParamWithRefDataList params;
    ErrorCode rez = doQueryNoLock(m_adminUserID, params);
    settings.reserve( params.size() );
    if (rez == ErrorCode::ok) {
        for(ec2::ApiResourceParamWithRefData& param: params)
            settings.push_back(ApiResourceParamData(std::move(param.name), std::move(param.value)));
    }
    return rez;
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiIdData>& tran)
{
    switch(tran.command) {
        case ApiCommand::removeCamera:
            return removeObject(ApiObjectInfo(ApiObject_Camera, tran.params.id));
        case ApiCommand::removeStorage:
            return removeObject(ApiObjectInfo(ApiObject_Storage, tran.params.id));
        case ApiCommand::removeMediaServer:
            return removeObject(ApiObjectInfo(ApiObject_Server, tran.params.id));
        case ApiCommand::removeServerUserAttributes:
            return removeMediaServerUserAttributes(tran.params.id);
        case ApiCommand::removeLayout:
            return removeObject(ApiObjectInfo(ApiObject_Layout, tran.params.id));
        case ApiCommand::removeBusinessRule:
            return removeObject(ApiObjectInfo(ApiObject_BusinessRule, tran.params.id));
        case ApiCommand::removeUser:
            return removeObject(ApiObjectInfo(ApiObject_User, tran.params.id));
        case ApiCommand::removeVideowall:
            return removeObject(ApiObjectInfo(ApiObject_Videowall, tran.params.id));
        default:
            return removeObject(ApiObjectInfo(ApiObject_Resource, tran.params.id));
    }
}

ErrorCode QnDbManager::removeObject(const ApiObjectInfo& apiObject)
{
    ErrorCode result;
    switch (apiObject.type)
    {
    case ApiObject_Camera:
        result = removeCamera(apiObject.id);
        break;
    case ApiObject_Storage:
        result = removeStorage(apiObject.id);
        break;
    case ApiObject_Server:
        result = removeServer(apiObject.id);
        break;
    case ApiObject_Layout:
        result = removeLayout(apiObject.id);
        break;
    case ApiObject_BusinessRule:
        result = removeBusinessRule(apiObject.id);
        break;
    case ApiObject_User:
        result = removeUser(apiObject.id);
        break;
    case ApiObject_Videowall:
        result = removeVideowall(apiObject.id);
        break;
    case ApiObject_Resource:
        result = removeObject(ApiObjectInfo(getObjectType(apiObject.id), apiObject.id));
        break;
    case ApiCommand::NotDefined:
        result = ErrorCode::ok; // object already removed
        break;
    default:
        qWarning() << "Remove operation is not implemented for object type" << apiObject.type;
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

void QnDbManager::loadResourceTypeXML(const QString& fileName, ApiResourceTypeDataList& data)
{
    QFile f(fileName);
    if (!f.open(QFile::ReadOnly))
        return;
    QBuffer xmlData;
    xmlData.setData(f.readAll());
    ResTypeXmlParser parser(data);
    QXmlSimpleReader reader;
    reader.setContentHandler(&parser);
    QXmlInputSource xmlSrc( &xmlData );
    if(!reader.parse( &xmlSrc )) {
        qWarning() << "Can't parse XML file " << fileName << "with additional resource types. Check XML file syntax.";
    }
}

void QnDbManager::addResourceTypesFromXML(ApiResourceTypeDataList& data)
{
    for(const QFileInfo& fi: QDir(":/resources").entryInfoList(QDir::Files))
        loadResourceTypeXML(fi.absoluteFilePath(), data);
    QDir dir2(QCoreApplication::applicationDirPath() + QString(lit("/resources")));
    for(const QFileInfo& fi: dir2.entryInfoList(QDir::Files))
        loadResourceTypeXML(fi.absoluteFilePath(), data);
}

ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiResourceTypeDataList& data)
{
    if (!m_cachedResTypes.empty())
    {
        data = m_cachedResTypes;
        return ErrorCode::ok;
    }

    QSqlQuery queryTypes(m_sdb);
    queryTypes.setForwardOnly(true);
    queryTypes.prepare("\
        SELECT rt.guid as id, rt.name, m.name as vendor \
        FROM vms_resourcetype rt \
        LEFT JOIN vms_manufacture m on m.id = rt.manufacture_id \
        ORDER BY rt.guid\
    ");
    if (!queryTypes.exec()) {
        qWarning() << Q_FUNC_INFO << queryTypes.lastError().text();
        return ErrorCode::dbError;
    }
    QnSql::fetch_many(queryTypes, &data);

    QSqlQuery queryParents(m_sdb);
    queryParents.setForwardOnly(true);
    queryParents.prepare("\
        SELECT t1.guid as id, t2.guid as parentId \
        FROM vms_resourcetype_parents p \
        JOIN vms_resourcetype t1 on t1.id = p.from_resourcetype_id \
        JOIN vms_resourcetype t2 on t2.id = p.to_resourcetype_id \
        ORDER BY t1.guid, p.to_resourcetype_id desc\
    ");
	if (!queryParents.exec()) {
        qWarning() << Q_FUNC_INFO << queryParents.lastError().text();
        return ErrorCode::dbError;
    }
    mergeIdListData<ApiResourceTypeData>(queryParents, data, &ApiResourceTypeData::parentId);

    QSqlQuery queryProperty(m_sdb);
    queryProperty.setForwardOnly(true);
    queryProperty.prepare("\
        SELECT rt.guid as resourceTypeId, pt.name, pt.default_value as defaultValue \
        FROM vms_propertytype pt \
        JOIN vms_resourcetype rt on rt.id = pt.resource_type_id ORDER BY rt.guid\
    ");
    if (!queryProperty.exec()) {
        qWarning() << Q_FUNC_INFO << queryProperty.lastError().text();
        return ErrorCode::dbError;
    }

    std::vector<ApiPropertyTypeData> allProperties;
    QnSql::fetch_many(queryProperty, &allProperties);
    mergeObjectListData(data, allProperties, &ApiResourceTypeData::propertyTypes, &ApiPropertyTypeData::resourceTypeId);

    addResourceTypesFromXML(data);

    m_cachedResTypes = data;

    return ErrorCode::ok;
}

// ----------- getLayouts --------------------

ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiLayoutDataList& layouts)
{
    QSqlQuery query(m_sdb);
    QString filter; // todo: add data filtering by user here
    query.setForwardOnly(true);
    query.prepare(QString("\
        SELECT r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentId, r.name, r.url, \
        l.user_can_edit as editable, l.cell_spacing_height as verticalSpacing, l.locked, \
        l.cell_aspect_ratio as cellAspectRatio, l.background_width as backgroundWidth, \
        l.background_image_filename as backgroundImageFilename, l.background_height as backgroundHeight, \
        l.cell_spacing_width as horizontalSpacing, l.background_opacity as backgroundOpacity, l.resource_ptr_id as id \
        FROM vms_layout l \
        JOIN vms_resource r on r.id = l.resource_ptr_id %1 ORDER BY r.guid\
    ").arg(filter));
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }

    QSqlQuery queryItems(m_sdb);
    queryItems.setForwardOnly(true);
    queryItems.prepare("\
        SELECT r.guid as layoutId, li.zoom_bottom as zoomBottom, li.right, li.uuid as id, li.zoom_left as zoomLeft, li.resource_guid as resourceId, \
        li.zoom_right as zoomRight, li.top, li.bottom, li.zoom_top as zoomTop, \
        li.zoom_target_uuid as zoomTargetId, li.flags, li.contrast_params as contrastParams, li.rotation, li.id, \
        li.dewarping_params as dewarpingParams, li.left FROM vms_layoutitem li \
        JOIN vms_resource r on r.id = li.layout_id order by r.guid\
    ");

    if (!queryItems.exec()) {
        qWarning() << Q_FUNC_INFO << queryItems.lastError().text();
        return ErrorCode::dbError;
    }

    QnSql::fetch_many(query, &layouts);
    std::vector<ApiLayoutItemWithRefData> items;
    QnSql::fetch_many(queryItems, &items);
    mergeObjectListData(layouts, items, &ApiLayoutData::items, &ApiLayoutItemWithRefData::layoutId);

    return ErrorCode::ok;
}

// ----------- getCameras --------------------

ErrorCode QnDbManager::doQueryNoLock(const QnUuid& mServerId, ApiCameraDataList& cameraList)
{
    QSqlQuery queryCameras(m_sdb);
    QString filterStr;
    if (!mServerId.isNull()) {
        filterStr = QString("WHERE r.parent_guid = %1").arg(guidToSqlString(mServerId));
    }
    queryCameras.setForwardOnly(true);
    queryCameras.prepare(QString("\
        SELECT r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentId, r.name, r.url, \
        c.vendor, c.manually_added as manuallyAdded, \
        c.group_name as groupName, c.group_id as groupId, c.mac, c.model, \
		c.status_flags as statusFlags, c.physical_id as physicalId \
        FROM vms_resource r \
        LEFT JOIN vms_resource_status rs on rs.guid = r.guid \
        JOIN vms_camera c on c.resource_ptr_id = r.id %1 ORDER BY r.guid\
    ").arg(filterStr));


    if (!queryCameras.exec()) {
        qWarning() << Q_FUNC_INFO << queryCameras.lastError().text();
        return ErrorCode::dbError;
    }
    QnSql::fetch_many(queryCameras, &cameraList);

    return ErrorCode::ok;
}

// ----------- getResourceStatus --------------------

ErrorCode QnDbManager::doQueryNoLock(const QnUuid& resId, ApiResourceStatusDataList& statusList)
{
    QString filterStr;
    if (!resId.isNull())
        filterStr = QString("WHERE guid = %1").arg(guidToSqlString(resId));

    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare(QString("SELECT guid as id, status from vms_resource_status %1 ORDER BY guid").arg(filterStr));

    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }

    QnSql::fetch_many(query, &statusList);

    return ErrorCode::ok;
}

ErrorCode QnDbManager::doQueryNoLock(const QnUuid& mServerId, ApiStorageDataList& storageList)
{
    QString filterStr;
    if (!mServerId.isNull())
        filterStr = QString("WHERE r.parent_guid = %1").arg(guidToSqlString(mServerId));

    QSqlQuery queryStorage(m_sdb);
    queryStorage.setForwardOnly(true);
    queryStorage.prepare(QString("\
        SELECT r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentId, r.name, r.url, \
        s.space_limit as spaceLimit, s.used_for_writing as usedForWriting \
        FROM vms_resource r \
        JOIN vms_storage s on s.resource_ptr_id = r.id \
        %1 \
        ORDER BY r.parent_guid\
    ").arg(filterStr));

    if (!queryStorage.exec()) {
        qWarning() << Q_FUNC_INFO << queryStorage.lastError().text();
        return ErrorCode::dbError;
    }

    QnSql::fetch_many(queryStorage, &storageList);

    return ErrorCode::ok;
}

ErrorCode QnDbManager::getScheduleTasks(const QnUuid& serverId, std::vector<ApiScheduleTaskWithRefData>& scheduleTaskList)
{
    QString filterStr;
    if( !serverId.isNull() )
        filterStr = QString("WHERE r2.parent_guid = %1").arg(guidToSqlString(serverId));
    
    //fetching recording schedule
    QSqlQuery queryScheduleTask(m_sdb);
    queryScheduleTask.setForwardOnly(true);
    queryScheduleTask.prepare(QString("\
        SELECT\
            r.camera_guid as sourceId,             \
            st.start_time as startTime,            \
            st.end_time as endTime,                \
            st.do_record_audio as recordAudio,     \
            st.record_type as recordingType,       \
            st.day_of_week as dayOfWeek,           \
            st.before_threshold as beforeThreshold,\
            st.after_threshold as afterThreshold,  \
            st.stream_quality as streamQuality,    \
            st.fps                                 \
        FROM vms_scheduletask st \
        JOIN vms_camera_user_attributes r on r.id = st.camera_attrs_id \
        LEFT JOIN vms_resource r2 on r2.guid = r.camera_guid \
        %1\
        ORDER BY r.camera_guid\
    ").arg(filterStr));

    if (!queryScheduleTask.exec()) {
        NX_LOG( lit("Db error in %1: %2").arg(Q_FUNC_INFO).arg(queryScheduleTask.lastError().text()), cl_logWARNING );
        return ErrorCode::dbError;
    }

    QnSql::fetch_many(queryScheduleTask, &scheduleTaskList);
    return ErrorCode::ok;
}

ErrorCode QnDbManager::doQueryNoLock(const QnUuid& serverId, ApiCameraAttributesDataList& cameraUserAttributesList)
{
    //fetching camera user attributes
    QSqlQuery queryCameras(m_sdb);
    QString filterStr;
    if( !serverId.isNull() )
        filterStr = QString("WHERE r.parent_guid = %1").arg(guidToSqlString(serverId));
    queryCameras.setForwardOnly(true);

    queryCameras.prepare(lit("\
        SELECT                                           \
            camera_guid as cameraID,                     \
            camera_name as cameraName,                   \
            audio_enabled as audioEnabled,               \
            control_enabled as controlEnabled,           \
            region as motionMask,                        \
            schedule_enabled as scheduleEnabled,         \
            motion_type as motionType,                   \
            secondary_quality as secondaryStreamQuality, \
            dewarping_params as dewarpingParams,         \
            min_archive_days as minArchiveDays,          \
            max_archive_days as maxArchiveDays,          \
            prefered_server_id as preferedServerId       \
         FROM vms_camera_user_attributes                 \
         LEFT JOIN vms_resource r on r.guid = camera_guid     \
         %1                                              \
         ORDER BY camera_guid                            \
        ").arg(filterStr) );

    if (!queryCameras.exec()) {
        NX_LOG( lit("Db error in %1: %2").arg(Q_FUNC_INFO).arg(queryCameras.lastError().text()), cl_logWARNING );
        return ErrorCode::dbError;
    }
     
    QnSql::fetch_many(queryCameras, &cameraUserAttributesList);

    std::vector<ApiScheduleTaskWithRefData> scheduleTaskList;
    ErrorCode errCode = getScheduleTasks(serverId, scheduleTaskList);
    if (errCode != ErrorCode::ok)
        return errCode;

    //merging data
    mergeObjectListData(
        cameraUserAttributesList,
        scheduleTaskList,
        &ApiCameraAttributesData::scheduleTasks,
        &ApiCameraAttributesData::cameraID,
        &ApiScheduleTaskWithRefData::sourceId );

    return ErrorCode::ok;
}

ErrorCode QnDbManager::doQueryNoLock(const QnUuid& serverId, ApiCameraDataExList& cameraExList)
{
    QSqlQuery queryCameras( m_sdb );
    queryCameras.setForwardOnly(true);

    QString filterStr;
    if (!serverId.isNull()) {
        filterStr = QString("WHERE r.parent_guid = %1").arg(guidToSqlString(serverId));
    }
    
    queryCameras.prepare(QString("\
        SELECT r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentId, \
            coalesce(nullif(cu.camera_name, \"\"), r.name) as name, r.url, \
            coalesce(rs.status, 0) as status, \
            c.vendor, c.manually_added as manuallyAdded, \
            c.group_name as groupName, c.group_id as groupId, c.mac, c.model, \
            c.status_flags as statusFlags, c.physical_id as physicalId, \
            cu.audio_enabled as audioEnabled,                  \
            cu.control_enabled as controlEnabled,              \
            cu.region as motionMask,                           \
            cu.schedule_enabled as scheduleEnabled,            \
            cu.motion_type as motionType,                      \
            cu.secondary_quality as secondaryStreamQuality,    \
            cu.dewarping_params as dewarpingParams,            \
            cu.min_archive_days as minArchiveDays,             \
            cu.max_archive_days as maxArchiveDays,             \
            cu.prefered_server_id as preferedServerId          \
        FROM vms_resource r \
        LEFT JOIN vms_resource_status rs on rs.guid = r.guid \
        JOIN vms_camera c on c.resource_ptr_id = r.id \
        LEFT JOIN vms_camera_user_attributes cu on cu.camera_guid = r.guid \
        %1 \
        ORDER BY r.guid \
    ").arg(filterStr));

    if (!queryCameras.exec()) {
        NX_LOG( lit("Db error in %1: %2").arg(Q_FUNC_INFO).arg(queryCameras.lastError().text()), cl_logWARNING );
        return ErrorCode::dbError;
    }
    QnSql::fetch_many(queryCameras, &cameraExList);

    //reading resource properties
    QnQueryFilter filter;
    if( !serverId.isNull() )
        filter.fields.insert( RES_PARENT_ID_FIELD, QVariant::fromValue(serverId) );    //TODO #ak use QnSql::serialized_field
    filter.fields.insert( RES_TYPE_FIELD, RES_TYPE_CAMERA );
    ApiResourceParamWithRefDataList params;
    ErrorCode result = fetchResourceParams( filter, params );
    if( result != ErrorCode::ok )
        return result;
    mergeObjectListData<ApiCameraDataEx>(cameraExList, params, &ApiCameraDataEx::addParams, &ApiResourceParamWithRefData::resourceId);

    std::vector<ApiScheduleTaskWithRefData> scheduleTaskList;
    ErrorCode errCode = getScheduleTasks(serverId, scheduleTaskList);
    if (errCode != ErrorCode::ok)
        return errCode;

    //merging data
    //mergeObjectListData<ApiCameraDataEx, ApiScheduleTaskWithRefData, ApiScheduleTaskData, ApiCameraDataEx, ApiScheduleTaskWithRefData>(
    mergeObjectListData(
        cameraExList,
        scheduleTaskList,
        &ApiCameraDataEx::scheduleTasks,
        &ApiCameraDataEx::id,
        &ApiScheduleTaskWithRefData::sourceId );


    return ErrorCode::ok;
}


// ----------- getServers --------------------


ErrorCode QnDbManager::doQueryNoLock(const QnUuid& mServerId, ApiMediaServerDataList& serverList)
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    QString filterStr;
    if (!mServerId.isNull()) {
        filterStr = QString("WHERE r.guid = %1").arg(guidToSqlString(mServerId));
    }

    query.prepare(QString("\
        SELECT r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentId, r.name, r.url, \
        s.api_url as apiUrl, s.auth_key as authKey, s.version, s.net_addr_list as networkAddresses, s.system_info as systemInfo, \
        s.flags, s.system_name as systemName \
        FROM vms_resource r \
        LEFT JOIN vms_resource_status rs on rs.guid = r.guid \
        JOIN vms_server s on s.resource_ptr_id = r.id %1 ORDER BY r.guid\
    ").arg(filterStr));

    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }

    QnSql::fetch_many(query, &serverList);

    return ErrorCode::ok;
}

ErrorCode QnDbManager::doQueryNoLock(const QnUuid& mServerId, ApiMediaServerDataExList& serverExList)
{
    {
        //fetching server data
        ApiMediaServerDataList serverList;
        ErrorCode result = doQueryNoLock( mServerId, serverList );
        if( result != ErrorCode::ok )
            return result;

        //moving server data to dataex
        serverExList.reserve( serverList.size() );
        for( ApiMediaServerDataList::size_type i = 0; i < serverList.size(); ++i )
            serverExList.push_back( ApiMediaServerDataEx(std::move(serverList[i])) );
    }

    {
        //fetching server attributes
        ApiMediaServerUserAttributesDataList serverAttrsList;
        ErrorCode result = doQueryNoLock( mServerId, serverAttrsList );
        if( result != ErrorCode::ok )
            return result;

        //merging data & attributes
        mergeObjectListData(
            serverExList,
            serverAttrsList,
            &ApiMediaServerDataEx::id,
            &ApiMediaServerUserAttributesData::serverID,
            []( ApiMediaServerDataEx& server, ApiMediaServerUserAttributesData& serverAttrs )
                { 
                    ((ApiMediaServerUserAttributesData&)server) = std::move(serverAttrs);
                    if (!server.serverName.isEmpty())
                        server.name = server.serverName;
                });
    }

    //fetching storages
    ApiStorageDataList storages;
    ErrorCode result = doQueryNoLock( mServerId, storages );
    if( result != ErrorCode::ok )
        return result;
    //merging storages
    mergeObjectListData( serverExList, storages, &ApiMediaServerDataEx::storages, &ApiMediaServerDataEx::id, &ApiStorageData::parentId );

    //reading properties
    QnQueryFilter filter;
    if( !mServerId.isNull() )
        filter.fields.insert( RES_ID_FIELD, QVariant::fromValue(mServerId) );
    filter.fields.insert( RES_TYPE_FIELD, RES_TYPE_MSERVER );
    ApiResourceParamWithRefDataList params;
    result = fetchResourceParams( filter, params );
    if( result != ErrorCode::ok )
        return result;
    mergeObjectListData<ApiMediaServerDataEx>(serverExList, params, &ApiMediaServerDataEx::addParams, &ApiResourceParamWithRefData::resourceId);

    //reading status info
    ApiResourceStatusDataList statusList;
    result = doQueryNoLock( mServerId, statusList );
    if( result != ErrorCode::ok )
        return result;
    
    mergeObjectListData(
        serverExList,
        statusList,
        &ApiMediaServerDataEx::id,
        &ApiResourceStatusData::id,
        []( ApiMediaServerDataEx& server, ApiResourceStatusData& statusData ) { server.status = statusData.status; } );

    return ErrorCode::ok;
}

ErrorCode QnDbManager::doQueryNoLock(const QnUuid& mServerId, ApiMediaServerUserAttributesDataList& serverAttrsList)
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    QString filterStr;
    if( !mServerId.isNull() )
        filterStr = QString("WHERE server_guid = %1").arg(guidToSqlString(mServerId));

    query.prepare( lit("\
        SELECT                                \
            server_guid as serverID,          \
            server_name as serverName,        \
            max_cameras as maxCameras,        \
            redundancy as allowAutoRedundancy \
        FROM vms_server_user_attributes       \
        %1                                    \
        ORDER BY server_guid                  \
    ").arg(filterStr));
    if( !query.exec() )
    {
        NX_LOG( lit("DB Error at %1: %2").arg(Q_FUNC_INFO).arg(query.lastError().text()), cl_logWARNING );
        return ErrorCode::dbError;
    }
    QnSql::fetch_many(query, &serverAttrsList);

    return ErrorCode::ok;
}

//getCameraServerItems
ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiCameraServerItemDataList& historyList)
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare(QString("SELECT server_guid as serverGuid, timestamp, physical_id as cameraUniqueId FROM vms_cameraserveritem ORDER BY physical_id, timestamp"));
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }

    QnSql::fetch_many(query, &historyList);

    return ErrorCode::ok;
}

ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiCameraBookmarkTagDataList& tags) {
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare("SELECT name FROM vms_camera_bookmark_tag");
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }
    QnSql::fetch_many(query, &tags);

    return ErrorCode::ok;
}

//getUsers
ErrorCode QnDbManager::doQueryNoLock(const QnUuid& userId, ApiUserDataList& userList)
{
    QString filterStr;
    if (!userId.isNull())
        filterStr = QString("WHERE r.guid = %1").arg(guidToSqlString(userId));

    //digest = md5('%s:%s:%s' % (self.user.username.lower(), 'NetworkOptix', password)).hexdigest()
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare(QString("\
        SELECT r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentId, r.name, r.url, \
        u.is_superuser as isAdmin, u.email, p.digest as digest, u.password as hash, p.rights as permissions \
        FROM vms_resource r \
        JOIN auth_user u  on u.id = r.id\
        JOIN vms_userprofile p on p.user_id = u.id\
        %1\
        ORDER BY r.guid\
    ").arg(filterStr));
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }

    QnSql::fetch_many(query, &userList);

    return ErrorCode::ok;
}

//getTransactionLog
ErrorCode QnDbManager::doQueryNoLock(const std::nullptr_t&, ApiTransactionDataList& tranList)
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare(QString("SELECT tran_guid, tran_data from transaction_log order by peer_guid, db_guid, sequence"));
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }
    
    while (query.next()) {
        ApiTransactionData tran;
        tran.tranGuid = QnSql::deserialized_field<QnUuid>(query.value(0));
        QByteArray srcData = query.value(1).toByteArray();
        QnUbjsonReader<QByteArray> stream(&srcData);
        if (QnUbjson::deserialize(&stream, &tran.tran)) {
            tranList.push_back(std::move(tran));
        }
        else {
            qWarning() << "Can' deserialize transaction from transaction log";
            return ErrorCode::dbError;
        }
    }

    return ErrorCode::ok;
}

//getVideowallList
ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiVideowallDataList& videowallList) {
    QSqlQuery query(m_sdb);
    QString filter; // todo: add data filtering by user here
    query.setForwardOnly(true);
    query.prepare(QString("\
        SELECT r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentId, r.name, r.url, l.autorun \
        FROM vms_videowall l \
        JOIN vms_resource r on r.id = l.resource_ptr_id %1 ORDER BY r.guid\
    ").arg(filter));
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }
    QnSql::fetch_many(query, &videowallList);

    QSqlQuery queryItems(m_sdb);
    queryItems.setForwardOnly(true);
    queryItems.prepare("\
        SELECT \
            item.guid, item.pc_guid as pcGuid, item.layout_guid as layoutGuid, \
            item.videowall_guid as videowallGuid, item.name, \
            item.snap_left as snapLeft, item.snap_top as snapTop, item.snap_right as snapRight, item.snap_bottom as snapBottom \
        FROM vms_videowall_item item ORDER BY videowallGuid");
    if (!queryItems.exec()) {
        qWarning() << Q_FUNC_INFO << queryItems.lastError().text();
        return ErrorCode::dbError;
    }
    std::vector<ApiVideowallItemWithRefData> items;
    QnSql::fetch_many(queryItems, &items);

    mergeObjectListData(videowallList, items, &ApiVideowallData::items, &ApiVideowallItemWithRefData::videowallGuid);
    
    QSqlQuery queryScreens(m_sdb);
    queryScreens.setForwardOnly(true);

    queryScreens.prepare("\
        SELECT \
            pc.videowall_guid as videowallGuid, \
            screen.pc_guid as pcGuid, screen.pc_index as pcIndex, \
            screen.desktop_x as desktopLeft, screen.desktop_y as desktopTop, \
            screen.desktop_w as desktopWidth, screen.desktop_h as desktopHeight, \
            screen.layout_x as layoutLeft, screen.layout_y as layoutTop, \
            screen.layout_w as layoutWidth, screen.layout_h as layoutHeight \
        FROM vms_videowall_screen screen \
        JOIN vms_videowall_pcs pc on pc.pc_guid = screen.pc_guid ORDER BY videowallGuid");
    if (!queryScreens.exec()) {
        qWarning() << Q_FUNC_INFO << queryScreens.lastError().text();
        return ErrorCode::dbError;
    }
    std::vector<ApiVideowallScreenWithRefData> screens;
    QnSql::fetch_many(queryScreens, &screens);
    mergeObjectListData(videowallList, screens, &ApiVideowallData::screens, &ApiVideowallScreenWithRefData::videowallGuid);

    QSqlQuery queryMatrixItems(m_sdb);
    queryMatrixItems.setForwardOnly(true);
    queryMatrixItems.prepare("\
        SELECT \
            item.matrix_guid as matrixGuid, \
            item.item_guid as itemGuid, \
            item.layout_guid as layoutGuid, \
            matrix.videowall_guid \
        FROM vms_videowall_matrix_items item \
        JOIN vms_videowall_matrix matrix ON matrix.guid = item.matrix_guid \
        ORDER BY matrix.videowall_guid\
    ");
    if (!queryMatrixItems.exec()) {
        qWarning() << Q_FUNC_INFO << queryMatrixItems.lastError().text();
        return ErrorCode::dbError;
    }
    std::vector<ApiVideowallMatrixItemWithRefData> matrixItems;
    QnSql::fetch_many(queryMatrixItems, &matrixItems);

    QSqlQuery queryMatrices(m_sdb);
    queryMatrices.setForwardOnly(true);
    queryMatrices.prepare("\
        SELECT \
            matrix.guid as id, \
            matrix.name, \
            matrix.videowall_guid as videowallGuid \
        FROM vms_videowall_matrix matrix ORDER BY matrix.guid\
    ");
    if (!queryMatrices.exec()) {
        qWarning() << Q_FUNC_INFO << queryMatrices.lastError().text();
        return ErrorCode::dbError;
    }
    std::vector<ApiVideowallMatrixWithRefData> matrices;
    QnSql::fetch_many(queryMatrices, &matrices);
    mergeObjectListData(matrices, matrixItems, &ApiVideowallMatrixData::items, &ApiVideowallMatrixItemWithRefData::matrixGuid);
    qSort(matrices.begin(), matrices.end(), [] (const ApiVideowallMatrixWithRefData& data1, const ApiVideowallMatrixWithRefData& data2) {
        return data1.videowallGuid.toRfc4122() < data2.videowallGuid.toRfc4122();
    });
    mergeObjectListData(videowallList, matrices, &ApiVideowallData::matrices, &ApiVideowallMatrixWithRefData::videowallGuid);

    return ErrorCode::ok;
}

//getBusinessRules
ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiBusinessRuleDataList& businessRuleList)
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare(QString("\
        SELECT guid as id, event_type as eventType, event_condition as eventCondition, event_state as eventState, action_type as actionType, \
        action_params as actionParams, aggregation_period as aggregationPeriod, disabled, comments as comment, schedule, system \
        FROM vms_businessrule order by guid\
    "));
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }

    QSqlQuery queryRuleEventRes(m_sdb);
    queryRuleEventRes.setForwardOnly(true);
    queryRuleEventRes.prepare(QString("SELECT businessrule_guid as id, resource_guid as parentId from vms_businessrule_event_resources order by businessrule_guid"));
    if (!queryRuleEventRes.exec()) {
        qWarning() << Q_FUNC_INFO << queryRuleEventRes.lastError().text();
        return ErrorCode::dbError;
    }

    QSqlQuery queryRuleActionRes(m_sdb);
    queryRuleActionRes.setForwardOnly(true);
    queryRuleActionRes.prepare(QString("SELECT businessrule_guid as id, resource_guid as parentId from vms_businessrule_action_resources order by businessrule_guid"));
    if (!queryRuleActionRes.exec()) {
        qWarning() << Q_FUNC_INFO << queryRuleActionRes.lastError().text();
        return ErrorCode::dbError;
    }

    QnSql::fetch_many(query, &businessRuleList);

    // merge data

    mergeIdListData<ApiBusinessRuleData>(queryRuleEventRes, businessRuleList, &ApiBusinessRuleData::eventResourceIds);
    mergeIdListData<ApiBusinessRuleData>(queryRuleActionRes, businessRuleList, &ApiBusinessRuleData::actionResourceIds);

    return ErrorCode::ok;
}

// getKVPairs
ErrorCode QnDbManager::doQueryNoLock(const QnUuid& resourceId, ApiResourceParamWithRefDataList& params)
{
    QnQueryFilter filter;
    if( !resourceId.isNull() )
        filter.fields.insert( RES_ID_FIELD, QVariant::fromValue(resourceId) );
    return fetchResourceParams( filter, params );
}

// getCurrentTime
ErrorCode QnDbManager::doQuery(const nullptr_t& /*dummy*/, ApiTimeData& currentTime)
{
    currentTime.value = TimeSynchronizationManager::instance()->getSyncTime();
    return ErrorCode::ok;
}

// dumpDatabase
ErrorCode QnDbManager::doQuery(const nullptr_t& /*dummy*/, ApiDatabaseDumpData& data)
{
    QWriteLocker lock(&m_mutex);

    //have to close/open DB to dump journals to .db file
    m_sdb.close();
    m_sdbStatic.close();

    //creating dump
    QFile f(m_sdb.databaseName());
    if (!f.open(QFile::ReadOnly))
        return ErrorCode::ioError;
    data.data = f.readAll();

    //TODO #ak add license db to backup

    if( !m_sdb.open() )
    {
        NX_LOG( lit("Can't reopen ec2 DB (%1). Error %2").arg(m_sdb.databaseName()).arg(m_sdb.lastError().text()), cl_logWARNING );
        return ErrorCode::dbError;
    }

    if( !m_sdbStatic.open() )
    {
        NX_LOG( lit("Can't reopen ec2 license DB (%1). Error %2").arg(m_sdbStatic.databaseName()).arg(m_sdbStatic.lastError().text()), cl_logWARNING );
        return ErrorCode::dbError;
    }

    //tuning DB
    if( !tuneDBAfterOpen() )
        return ErrorCode::dbError;

    return ErrorCode::ok;
}

ErrorCode QnDbManager::doQuery(const ApiStoredFilePath& dumpFilePath, qint64& dumpFileSize)
{
    QWriteLocker lock(&m_mutex);

    //have to close/open DB to dump journals to .db file
    m_sdb.close();
    m_sdbStatic.close();

    if( !QFile::copy( m_sdb.databaseName(), dumpFilePath.path ) )
        return ErrorCode::ioError;

    //TODO #ak add license db to backup

    QFileInfo dumpFileInfo( dumpFilePath.path );
    dumpFileSize = dumpFileInfo.size();

    if( !m_sdb.open() )
    {
        NX_LOG( lit("Can't reopen ec2 DB (%1). Error %2").arg(m_sdb.databaseName()).arg(m_sdb.lastError().text()), cl_logWARNING );
        return ErrorCode::dbError;
    }

    if( !m_sdbStatic.open() )
    {
        NX_LOG( lit("Can't reopen ec2 license DB (%1). Error %2").arg(m_sdbStatic.databaseName()).arg(m_sdbStatic.lastError().text()), cl_logWARNING );
        return ErrorCode::dbError;
    }

    //tuning DB
    if( !tuneDBAfterOpen() )
        return ErrorCode::dbError;

    return ErrorCode::ok;
}


// ApiFullInfo
ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& dummy, ApiFullInfoData& data)
{
    ErrorCode err;

    if ((err = doQueryNoLock(dummy, data.resourceTypes)) != ErrorCode::ok)
        return err;

    if ((err = doQueryNoLock(dummy, data.servers)) != ErrorCode::ok)
        return err;

    if ((err = doQueryNoLock(dummy, data.serversUserAttributesList)) != ErrorCode::ok)
        return err;

    if ((err = doQueryNoLock(QnUuid(), data.cameras)) != ErrorCode::ok)
        return err;

    if ((err = doQueryNoLock(QnUuid(), data.cameraUserAttributesList)) != ErrorCode::ok)
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

    if ((err = doQueryNoLock(dummy, data.discoveryData)) != ErrorCode::ok)
        return err;
		
    if ((err = doQueryNoLock(dummy, data.allProperties)) != ErrorCode::ok)
        return err;
	
    if ((err = doQueryNoLock(dummy, data.storages)) != ErrorCode::ok)
        return err;

    if ((err = doQueryNoLock(dummy, data.resStatusList)) != ErrorCode::ok)
        return err;
    
    /*
    QSqlQuery queryParams(m_sdb);
    queryParams.setForwardOnly(true);
    queryParams.prepare(QString("SELECT r.guid as resourceId, kv.value, kv.name \
                                FROM vms_kvpair kv \
                                JOIN vms_resource r on r.id = kv.resource_id \
                                ORDER BY r.guid"));
    if (!queryParams.exec())
        return ErrorCode::dbError;

    QnSql::fetch_many(queryParams, &data.allResParams);
    */
/*
    mergeObjectListData<ApiMediaServerData>(data.servers,   kvPairs, &ApiMediaServerData::addParams, &ApiResourceParamWithRefData::resourceId);
    mergeObjectListData<ApiCameraData>(data.cameras,        kvPairs, &ApiCameraData::addParams,      &ApiResourceParamWithRefData::resourceId);
    mergeObjectListData<ApiUserData>(data.users,            kvPairs, &ApiUserData::addParams,        &ApiResourceParamWithRefData::resourceId);
    mergeObjectListData<ApiLayoutData>(data.layouts,        kvPairs, &ApiLayoutData::addParams,      &ApiResourceParamWithRefData::resourceId);
    mergeObjectListData<ApiVideowallData>(data.videowalls,  kvPairs, &ApiVideowallData::addParams,   &ApiResourceParamWithRefData::resourceId);
*/
    return err;
}

ErrorCode QnDbManager::doQueryNoLock(const std::nullptr_t &, ApiDiscoveryDataList &data) {
    QSqlQuery query(m_sdb);

    QString q = QString(lit("SELECT server_id as id, url, ignore from vms_mserver_discovery"));
    query.setForwardOnly(true);
    query.prepare(q);

    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << __LINE__ << query.lastError();
        return ErrorCode::dbError;
    }

    QnSql::fetch_many(query, &data);

    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiResetBusinessRuleData>& tran)
{
    if (!execSQLQuery("DELETE FROM vms_businessrule_action_resources", m_sdb))
        return ErrorCode::dbError;
    if (!execSQLQuery("DELETE FROM vms_businessrule_event_resources", m_sdb))
        return ErrorCode::dbError;
    if (!execSQLQuery("DELETE FROM vms_businessrule", m_sdb))
        return ErrorCode::dbError;

    for (const ApiBusinessRuleData& rule: tran.params.defaultRules)
    {
        ErrorCode rez = updateBusinessRule(rule);
        if (rez != ErrorCode::ok)
            return rez;
    }

    return ErrorCode::ok;
}

ErrorCode QnDbManager::saveLicense(const ApiLicenseData& license) {
    QSqlQuery insQuery(m_sdbStatic);
    insQuery.prepare("INSERT OR REPLACE INTO vms_license (license_key, license_block) VALUES(:licenseKey, :licenseBlock)");
    insQuery.bindValue(":licenseKey", license.key);
    insQuery.bindValue(":licenseBlock", license.licenseBlock);
    if (insQuery.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
        return ErrorCode::dbError;
    }
}

ErrorCode QnDbManager::removeLicense(const ApiLicenseData& license) {
    QSqlQuery delQuery(m_sdbStatic);
    delQuery.prepare("DELETE FROM vms_license WHERE license_key = ?");
    delQuery.addBindValue(license.key);
    if (delQuery.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
        return ErrorCode::dbError;
    }
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiLicenseData>& tran)
{
    if (tran.command == ApiCommand::addLicense)
        return saveLicense(tran.params);
    else if (tran.command == ApiCommand::removeLicense)
        return removeLicense(tran.params);
    else {
        Q_ASSERT_X(1, Q_FUNC_INFO, "Unexpected command!");
        return ErrorCode::notImplemented;
    }
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiLicenseDataList>& tran)
{
    for (const ApiLicenseData& license: tran.params) {
        ErrorCode result = saveLicense(license);
        if (result != ErrorCode::ok) {
            return ErrorCode::dbError;
        }
    }

    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiCameraBookmarkTagDataList> &tran) {

    std::function<ec2::ErrorCode (const ApiCameraBookmarkTagData &)> processTag;

    switch (tran.command) {
    case ApiCommand::addCameraBookmarkTags:
        processTag = [this](const ApiCameraBookmarkTagData &tag) {return addCameraBookmarkTag(tag);};
        break;
    case ApiCommand::removeCameraBookmarkTags:
        processTag = [this](const ApiCameraBookmarkTagData &tag) {return removeCameraBookmarkTag(tag);};
        break;
    default:
        assert(false); //should never get here
        break;
    }

    for (const ApiCameraBookmarkTagData &tag: tran.params) {
        ErrorCode result = processTag(tag);
        if (result != ErrorCode::ok)
            return result;
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::addCameraBookmarkTag(const ApiCameraBookmarkTagData &tag) {
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT OR REPLACE INTO vms_camera_bookmark_tag (name) VALUES (:name)");
    QnSql::bind(tag, &insQuery);
    if (insQuery.exec())
        return ErrorCode::ok;

    qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
    return ErrorCode::dbError;
}

ErrorCode QnDbManager::removeCameraBookmarkTag(const ApiCameraBookmarkTagData &tag) {
    QSqlQuery delQuery(m_sdb);
    delQuery.prepare("DELETE FROM vms_camera_bookmark_tag WHERE name=:name");
    QnSql::bind(tag, &delQuery);
    if (delQuery.exec())
        return ErrorCode::ok;

    qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
    return ErrorCode::dbError;
}

ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ec2::ApiLicenseDataList& data)
{
    QSqlQuery query(m_sdbStatic);

    QString q = QString(lit("SELECT license_key as key, license_block as licenseBlock from vms_license"));
    query.setForwardOnly(true);
    query.prepare(q);

    if (!query.exec())
    {
        qWarning() << Q_FUNC_INFO << __LINE__ << query.lastError();
        return ErrorCode::dbError;
    }

    QnSql::fetch_many(query, &data);
    return ErrorCode::ok;
}

ErrorCode QnDbManager::doQueryNoLock(const ApiStoredFilePath& _path, ApiStoredDirContents& data)
{
    QSqlQuery query(m_sdb);
    QString path;
    if (!_path.path.isEmpty())
        path = closeDirPath(_path.path);
    
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
        return ErrorCode::dbError;
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
    query.bindValue(":path", path.path);
    if (!query.exec())
    {
        qWarning() << Q_FUNC_INFO << __LINE__ << query.lastError();
        return ErrorCode::dbError;
    }
    data.path = path.path;
    if (query.next())
        data.data = query.value(0).toByteArray();
    return ErrorCode::ok;
}

ErrorCode QnDbManager::doQueryNoLock(const ApiStoredFilePath& path, ApiStoredFileDataList& data)
{
    QString filter;
    if (!path.path.isEmpty())
        filter = QString("WHERE path = '%1'").arg(path.path);

    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare(QString("SELECT path, data FROM vms_storedFiles %1").arg(filter));
    if (!query.exec())
    {
        qWarning() << Q_FUNC_INFO << __LINE__ << query.lastError();
        return ErrorCode::dbError;
    }
    QnSql::fetch_many(query, &data);
    return ErrorCode::ok;
}

ErrorCode QnDbManager::saveVideowall(const ApiVideowallData& params) {
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
    if (result != ErrorCode::ok)
        return result;

    result = updateVideowallMatrices(params);
    return result;
}

ErrorCode QnDbManager::updateVideowallItems(const ApiVideowallData& data) {
    ErrorCode result = deleteVideowallItems(data.id);
    if (result != ErrorCode::ok)
        return result;

    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT INTO vms_videowall_item \
                     (guid, pc_guid, layout_guid, videowall_guid, name, snap_left, snap_top, snap_right, snap_bottom) \
                     VALUES \
                     (:guid, :pcGuid, :layoutGuid, :videowall_guid, :name, :snapLeft, :snapTop, :snapRight, :snapBottom)");
    for(const ApiVideowallItemData& item: data.items)
    {
        QnSql::bind(item, &insQuery);
        insQuery.bindValue(":videowall_guid", data.id.toRfc4122());

        if (!insQuery.exec()) {
            qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
            return ErrorCode::dbError;
        }
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::updateVideowallScreens(const ApiVideowallData& data) {
    if (data.screens.size() == 0)
        return ErrorCode::ok;

    QSet<QnUuid> pcUuids;

    {
        QSqlQuery query(m_sdb);
        query.prepare("INSERT OR REPLACE INTO vms_videowall_screen \
                      (pc_guid, pc_index, \
                      desktop_x, desktop_y, desktop_w, desktop_h, \
                      layout_x, layout_y, layout_w, layout_h) \
                      VALUES \
                      (:pcGuid, :pcIndex, \
                      :desktopLeft, :desktopTop, :desktopWidth, :desktopHeight, \
                      :layoutLeft, :layoutTop, :layoutWidth, :layoutHeight)");

        for(const ApiVideowallScreenData& screen: data.screens)
        {
            QnSql::bind(screen, &query);
            pcUuids << screen.pcGuid;
            if (!query.exec()) {
                qWarning() << Q_FUNC_INFO << query.lastError().text();
                return ErrorCode::dbError;
            }
        }
    }

    {
        QSqlQuery query(m_sdb);
        query.prepare("INSERT OR REPLACE INTO vms_videowall_pcs \
                      (videowall_guid, pc_guid) VALUES (:videowall_guid, :pc_guid)");
        for (const QnUuid &pcUuid: pcUuids) {
            query.bindValue(":videowall_guid", data.id.toRfc4122());
            query.bindValue(":pc_guid", pcUuid.toRfc4122());
            if (!query.exec()) {
                qWarning() << Q_FUNC_INFO << query.lastError().text();
                return ErrorCode::dbError;
            }
        }
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::updateVideowallMatrices(const ApiVideowallData &data) {
    ErrorCode result = deleteVideowallMatrices(data.id);
    if (result != ErrorCode::ok)
        return result;

    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT INTO vms_videowall_matrix \
                     (guid, videowall_guid, name) \
                     VALUES \
                     (:id, :videowall_guid, :name)");

    QSqlQuery insItemsQuery(m_sdb);
    insItemsQuery.prepare("INSERT INTO vms_videowall_matrix_items \
                     (matrix_guid, item_guid, layout_guid) \
                     VALUES \
                     (:matrix_guid, :itemGuid, :layoutGuid)");

    for(const ApiVideowallMatrixData &matrix: data.matrices) {
        QnSql::bind(matrix, &insQuery);
        insQuery.bindValue(":videowall_guid", data.id.toRfc4122());

        if (!insQuery.exec()) {
            qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
            return ErrorCode::dbError;
        }

        insItemsQuery.bindValue(":matrix_guid", matrix.id.toRfc4122());
        for(const ApiVideowallMatrixItemData &item: matrix.items) {
            QnSql::bind(item, &insItemsQuery);
            if (!insItemsQuery.exec()) {
                qWarning() << Q_FUNC_INFO << insItemsQuery.lastError().text();
                return ErrorCode::dbError;
            }    
        }
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::deleteVideowallPcs(const QnUuid &videowall_guid) {
    return deleteTableRecord(videowall_guid, "vms_videowall_pcs", "videowall_guid");
}

ErrorCode QnDbManager::deleteVideowallItems(const QnUuid &videowall_guid) {
    ErrorCode err = deleteTableRecord(videowall_guid, "vms_videowall_item", "videowall_guid");
    if (err != ErrorCode::ok)
        return err;

    { // delete unused PC screens
        QSqlQuery delQuery(m_sdb);
        delQuery.prepare("DELETE FROM vms_videowall_screen WHERE pc_guid NOT IN (SELECT pc_guid from vms_videowall_item) ");
        if (!delQuery.exec()) {
            qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
            return ErrorCode::dbError;
        }
    }

    { // delete unused PCs
        QSqlQuery delQuery(m_sdb);
        delQuery.prepare("DELETE FROM vms_videowall_pcs WHERE pc_guid NOT IN (SELECT pc_guid from vms_videowall_screen) ");
        if (!delQuery.exec()) {
            qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
            return ErrorCode::dbError;
        }
    }

    return ErrorCode::ok;
}

ErrorCode QnDbManager::deleteVideowallMatrices(const QnUuid &videowall_guid) {
    ErrorCode err = deleteTableRecord(videowall_guid, "vms_videowall_matrix", "videowall_guid");
    if (err != ErrorCode::ok)
        return err;

    { // delete unused matrix items
        QSqlQuery delQuery(m_sdb);
        delQuery.prepare("DELETE FROM vms_videowall_matrix_items WHERE matrix_guid NOT IN (SELECT guid from vms_videowall_matrix) ");
        if (!delQuery.exec()) {
            qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
            return ErrorCode::dbError;
        }
    }

    return ErrorCode::ok;
}

ErrorCode QnDbManager::removeVideowall(const QnUuid& guid) {
    qint32 id = getResourceInternalId(guid);

    //ErrorCode err = deleteAddParams(id);
    //if (err != ErrorCode::ok)
    //    return err;

    ErrorCode err = deleteVideowallMatrices(guid);
    if (err != ErrorCode::ok)
        return err;

    err = deleteVideowallPcs(guid);
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

ErrorCode QnDbManager::insertOrReplaceVideowall(const ApiVideowallData& data, qint32 internalId) {
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT OR REPLACE INTO vms_videowall (autorun, resource_ptr_id) VALUES\
                     (:autorun, :internalId)");
    QnSql::bind(data, &insQuery);
    insQuery.bindValue(":internalId", internalId);
    if (insQuery.exec())
        return ErrorCode::ok;
    
    qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
    return ErrorCode::dbError;
}

ErrorCode QnDbManager::removeLayoutFromVideowallItems(const QnUuid &layout_id) {
    QByteArray emptyId = QnUuid().toRfc4122();

    QSqlQuery query(m_sdb);
    query.prepare("UPDATE vms_videowall_item set layout_guid = :empty_id WHERE layout_guid = :layout_id");
    query.bindValue(":empty_id", emptyId);
    query.bindValue(":layout_id", layout_id.toRfc4122());
    if (query.exec())
        return ErrorCode::ok;

    qWarning() << Q_FUNC_INFO << query.lastError().text();
    return ErrorCode::dbError;
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiLicenseOverflowData>& tran)
{
    if (m_licenseOverflowMarked == tran.params.value)
        return ErrorCode::ok;
    m_licenseOverflowMarked = tran.params.value;

    QSqlQuery query(m_sdb);
    query.prepare("INSERT OR REPLACE into misc_data (key, data) values(?, ?) ");
    query.addBindValue(LICENSE_EXPIRED_TIME_KEY);
    query.addBindValue(QByteArray::number(tran.params.time));
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::failure;
    }

    QnPeerRuntimeInfo localInfo = QnRuntimeInfoManager::instance()->localInfo();
    if (localInfo.data.prematureLicenseExperationDate != tran.params.time) {
        localInfo.data.prematureLicenseExperationDate = tran.params.time;
        QnRuntimeInfoManager::instance()->updateLocalItem(localInfo);
    }
    
    return ErrorCode::ok;
}

QnUuid QnDbManager::getID() const
{
    return m_dbInstanceId;
}

QnDbManager::QnDbTransaction* QnDbManager::getTransaction()
{
    return &m_tran;
}

}
