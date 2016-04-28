#include "db_manager.h"

#include <utils/common/app_info.h>

#include <QtSql/QtSql>

#include "utils/common/util.h"
#include "common/common_module.h"
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
#include "nx_ec/data/api_webpage_data.h"
#include "nx_ec/data/api_license_data.h"
#include "nx_ec/data/api_business_rule_data.h"
#include "nx_ec/data/api_full_info_data.h"
#include "nx_ec/data/api_camera_history_data.h"
#include "nx_ec/data/api_client_info_data.h"
#include "nx_ec/data/api_media_server_data.h"
#include "nx_ec/data/api_update_data.h"
#include <nx_ec/data/api_time_data.h>
#include "nx_ec/data/api_conversion_functions.h"
#include "nx_ec/data/api_client_info_data.h"
#include <nx_ec/data/api_access_rights_data.h>
#include "api/runtime_info_manager.h"
#include <nx/utils/log/log.h>
#include "nx_ec/data/api_camera_data_ex.h"
#include "restype_xml_parser.h"
#include "business/business_event_rule.h"
#include "settings.h"

using std::nullptr_t;

static const QString RES_TYPE_MSERVER = "mediaserver";
static const QString RES_TYPE_CAMERA = "camera";
static const QString RES_TYPE_STORAGE = "storage";

namespace ec2
{

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
    for (size_t i = 1; i < data.size(); ++i) {
        QByteArray next = (data[i].*idField).toRfc4122();
        NX_ASSERT(next >= prev);
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
    NX_ASSERT(idIdx >=0 && parentIdIdx >= 0);

    bool eof = true;
    QnUuid id, parentId;
    QByteArray idRfc;

    auto step = [&eof, &id, &idRfc, &parentId, &query, idIdx, parentIdIdx]{
        eof = !query.next();
        if (eof)
            return;
        idRfc = query.value(idIdx).toByteArray();
        NX_ASSERT(idRfc == id.toRfc4122() || idRfc > id.toRfc4122());
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
        qWarning() << "Commit failed to database" << m_database.databaseName() << "error:"  << m_database.lastError(); // do not unlock mutex. Rollback is expected
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
        NX_ASSERT(false);
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
    m_needResyncCameraUserAttributes(false),
    m_needResyncServerUserAttributes(false),
    m_dbJustCreated(false),
    m_isBackupRestore(false),
    m_needResyncLayout(false),
    m_needResyncbRules(false),
    m_needResyncUsers(false),
    m_needResyncStorages(false),
    m_needResyncClientInfoData(false),
    m_dbReadOnly(false)
{
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

static std::array<QString, 3> DB_POSTFIX_LIST =
{
    QLatin1String(""),
    QLatin1String("-shm"),
    QLatin1String("-wal")
};

bool removeDbFile(const QString& dbFileName)
{
    for(const QString& postfix: DB_POSTFIX_LIST) {
        if (!removeFile(dbFileName + postfix))
            return false;
    }
    return true;
}

bool createCorruptedDbBackup(const QString& dbFileName)
{
    QString newFileName = dbFileName.left(dbFileName.lastIndexOf(L'.') + 1) + lit("corrupted.sqlite");
    if (!removeDbFile(newFileName))
        return false;
    for(const QString& postfix: DB_POSTFIX_LIST)
    {
        if (QFile::exists(dbFileName + postfix) && !QFile::copy(dbFileName + postfix, newFileName + postfix))
            return false;
    }

    return true;
}

bool QnDbManager::init(const QUrl& dbUrl)
{
    const QString dbFilePath = dbUrl.toLocalFile();
    const QString dbFilePathStatic = QUrlQuery(dbUrl.query()).queryItemValue("staticdb_path");

    QString dbFileName = closeDirPath(dbFilePath) + QString::fromLatin1("ecs.sqlite");
    addDatabase(dbFileName, "QnDbManager");

    QString backupDbFileName = dbFileName + QString::fromLatin1(".backup");
    bool needCleanup = QUrlQuery(dbUrl.query()).hasQueryItem("cleanupDb");
    if (QFile::exists(backupDbFileName) || needCleanup)
    {
        if (!removeDbFile(dbFileName))
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
    identityTimeQuery.setForwardOnly(true);
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
    {
        m_sdb.close();
        qWarning() << "Corrupted database file " << m_sdb.databaseName() << "!";
        if (!createCorruptedDbBackup(dbFileName)) {
            qWarning() << "Can't create database backup before removing file";
            return false;
        }
        if (!removeDbFile(dbFileName))
            qWarning() << "Can't delete corrupted database file " << m_sdb.databaseName();
        return false;
    }

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
    m_serverTypeId = getType( QnResourceTypePool::kServerTypeId );
    m_cameraTypeId = getType( "Camera" );

    QSqlQuery queryAdminUser( m_sdb );
    queryAdminUser.setForwardOnly( true );
    queryAdminUser.prepare( "SELECT r.guid, r.id FROM vms_resource r JOIN auth_user u on u.id = r.id and r.name = 'admin'" ); //TODO: #GDM check owner permission instead
    execSQLQuery(&queryAdminUser, Q_FUNC_INFO);
    if (queryAdminUser.next())
    {
        m_adminUserID = QnUuid::fromRfc4122( queryAdminUser.value( 0 ).toByteArray() );
        m_adminUserInternalID = queryAdminUser.value( 1 ).toInt();
    }
    NX_CRITICAL(!m_adminUserID.isNull());


    QSqlQuery queryServers(m_sdb);
    queryServers.prepare("UPDATE vms_resource_status set status = ? WHERE guid in (select guid from vms_resource where xtype_guid = ?)"); // todo: only mserver without DB?
    queryServers.bindValue(0, Qn::Offline);
    queryServers.bindValue(1, m_serverTypeId.toRfc4122());
    if( !queryServers.exec() )
    {
        qWarning() << Q_FUNC_INFO << __LINE__ << queryServers.lastError();
        NX_ASSERT( false );
        return false;
    }

    // read license overflow time
    QSqlQuery query( m_sdb );
    query.setForwardOnly(true);
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
    else
    {
        if (m_needResyncLicenses) {
            if (!fillTransactionLogInternal<ApiLicenseData, ApiLicenseDataList>(ApiCommand::addLicense))
                return false;
        }
        if (m_needResyncFiles) {
            if (!fillTransactionLogInternal<ApiStoredFileData, ApiStoredFileDataList>(ApiCommand::addStoredFile))
                return false;
        }
        if (m_needResyncCameraUserAttributes) {
            if (!fillTransactionLogInternal<ApiCameraAttributesData, ApiCameraAttributesDataList>(ApiCommand::saveCameraUserAttributes))
                return false;
        }
        if (m_needResyncServerUserAttributes) {
            if (!fillTransactionLogInternal<ApiMediaServerUserAttributesData, ApiMediaServerUserAttributesDataList>(ApiCommand::saveServerUserAttributes))
                return false;
        }
        if (m_needResyncLayout) {
            if (!fillTransactionLogInternal<ApiLayoutData, ApiLayoutDataList>(ApiCommand::saveLayout))
                return false;
        }
        if (m_needResyncbRules) {
            if (!fillTransactionLogInternal<ApiBusinessRuleData, ApiBusinessRuleDataList>(ApiCommand::saveBusinessRule))
                return false;
        }
        if (m_needResyncUsers) {
            if (!fillTransactionLogInternal<ApiUserData, ApiUserDataList>(ApiCommand::saveUser))
                return false;
        }
        if (m_needResyncStorages) {
            if (!fillTransactionLogInternal<ApiStorageData, ApiStorageDataList>(ApiCommand::saveStorage))
                return false;
        }
        if(m_needResyncClientInfoData) {
            if (!fillTransactionLogInternal<ApiClientInfoData, ApiClientInfoDataList>(ApiCommand::saveClientInfo))
                return false;
        }

    }

    // Set admin user's password
    QnUserResourcePtr userResource;
    {
        ApiUserDataList users;
        ErrorCode errCode = doQueryNoLock(nullptr, users);
        if (errCode != ErrorCode::ok)
            return false;

        if (users.empty())
            return false;

        auto iter = std::find_if(users.cbegin(), users.cend(), [this](const ec2::ApiUserData& user)
        {
            return user.id == m_adminUserID;
        });

        NX_ASSERT(iter != users.cend(), Q_FUNC_INFO, "Admin must exist");
        if (iter == users.cend())
            return false;

        userResource.reset(new QnUserResource());
        fromApiToResource(*iter, userResource);
        NX_ASSERT(userResource->isOwner(), Q_FUNC_INFO, "Admin must be admin as it is found by name");
    }

    QByteArray md5Password;
    QByteArray digestPassword;
    qnCommon->adminPasswordData(&md5Password, &digestPassword);
    QString defaultAdminPassword = qnCommon->defaultAdminPassword();
    if( (userResource->getHash().isEmpty() || m_dbJustCreated) && defaultAdminPassword.isEmpty() ) {
        defaultAdminPassword = lit("admin");
        if (m_dbJustCreated)
            qnCommon->setUseLowPriorityAdminPasswordHach(true);
    }


    bool updateUserResource = false;
    if( !defaultAdminPassword.isEmpty() )
    {
        if (!userResource->checkPassword(defaultAdminPassword) || userResource->getRealm() != QnAppInfo::realm()) {
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
        NX_ASSERT( 0 );
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

    m_dbReadOnly = ec2::Settings::instance()->dbReadOnly();
    emit initialized();
    m_initialized = true;

    return true;
}

template <>
bool QnDbManager::queryObjects<ApiMediaServerUserAttributesDataList>(ApiMediaServerUserAttributesDataList& objects)
{
    ErrorCode errCode = doQueryNoLock(QnUuid(), objects);
    return errCode == ErrorCode::ok;
}

template <>
bool QnDbManager::queryObjects<ApiClientInfoDataList>(ApiClientInfoDataList& objects)
{
    ErrorCode errCode = doQueryNoLock(QnUuid(), objects);
    return errCode == ErrorCode::ok;
}

template <>
bool QnDbManager::queryObjects<ApiStorageDataList>(ApiStorageDataList& objects)
{
    ErrorCode errCode = doQueryNoLock(QnUuid(), objects);
    return errCode == ErrorCode::ok;
}

template <>
bool QnDbManager::queryObjects<ApiResourceParamWithRefDataList>(ApiResourceParamWithRefDataList& objects)
{
    ErrorCode errCode = doQueryNoLock(QnUuid(), objects);
    return errCode == ErrorCode::ok;
}

template <class ObjectListType>
bool QnDbManager::queryObjects(ObjectListType& objects)
{
    ErrorCode errCode = doQueryNoLock(nullptr, objects);
    return errCode == ErrorCode::ok;
}

template <class ObjectType, class ObjectListType>
bool QnDbManager::fillTransactionLogInternal(ApiCommand::Value command)
{
    ObjectListType objects;
    if (!queryObjects<ObjectListType>(objects))
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

    if (!fillTransactionLogInternal<ApiStoredFileData, ApiStoredFileDataList>(ApiCommand::addStoredFile))
        return false;

    if (!fillTransactionLogInternal<ApiClientInfoData, ApiClientInfoDataList>(ApiCommand::saveClientInfo))
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

bool QnDbManager::updateBusinessActionParameters() {
    QHash<QString, QString> remapTable;
    remapTable["quality"] = "streamQuality";
    remapTable["duration"] = "recordingDuration";
    remapTable["after"] = "recordAfter";
    remapTable["relayOutputID"] = "relayOutputId";

    QMap<int, QByteArray> remapData;

    { /* Reading data from the table. */
        QSqlQuery query(m_sdb);
        query.setForwardOnly(true);
        query.prepare(QString("SELECT id, action_params FROM vms_businessrule order by id"));
        if (!query.exec()) {
            qWarning() << Q_FUNC_INFO << query.lastError().text();
            return false;
        }

        while (query.next()) {
            qint32 id = query.value(0).toInt();
            QByteArray data = query.value(1).toByteArray();

            QJsonValue result;
            QJson::deserialize(data, &result);
            QJsonObject values = result.toObject(); /* Returns empty object in case of deserialization error. */
            if (values.isEmpty())
                continue;

            QJsonObject remappedValues;
            for (const QString &key: values.keys()) {
                QString remappedKey = remapTable.contains(key) ? remapTable[key] : key;
                remappedValues[remappedKey] = values[key];
            }

            QByteArray remappedData;
            QJson::serialize(remappedValues, &remappedData);
            remapData[id] = remappedData;
        }
    }


    for(auto iter = remapData.cbegin(); iter != remapData.cend(); ++iter) {
        QSqlQuery query(m_sdb);
        query.prepare("UPDATE vms_businessrule SET action_params = :value WHERE id = :id");
        query.bindValue(":id", iter.key());
        query.bindValue(":value", iter.value());
        if (!query.exec()) {
            qWarning() << Q_FUNC_INFO << query.lastError().text();
            return false;
        }
    }

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

bool QnDbManager::beforeInstallUpdate(const QString& updateName)
{
    if (updateName == lit(":/updates/29_update_history_guid.sql")) {
        ; //return updateCameraHistoryGuids(); // perform string->guid conversion before SQL update because of reducing field size to 16 bytes. Probably data would lost if moved it to afterInstallUpdate
    }
    else if (updateName == lit(":/updates/30_update_history_guid.sql")) {
        return removeOldCameraHistory();
    }
    else if (updateName == lit(":/updates/33_history_refactor_dummy.sql")) {
        return removeOldCameraHistory();
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

bool QnDbManager::removeEmptyLayoutsFromTransactionLog()
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare("SELECT r.guid from vms_layout l JOIN vms_resource r on r.id = l.resource_ptr_id WHERE NOT EXISTS(SELECT 1 FROM vms_layoutitem li WHERE li.layout_id = l.resource_ptr_id)");
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << __LINE__ << query.lastError();
        return false;
    }
    QSqlQuery delQuery(m_sdb);
    delQuery.prepare("DELETE FROM transaction_log WHERE tran_guid = ?");
    while (query.next()) {
        QnUuid id = QnSql::deserialized_field<QnUuid>(query.value(0));
        delQuery.bindValue(0, QnSql::serialized_field(id));
        if (!delQuery.exec()) {
            qWarning() << Q_FUNC_INFO << __LINE__ << delQuery.lastError();
            return false;
        }
        if (removeLayout(id) != ErrorCode::ok)
            return false;
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

bool QnDbManager::removeOldCameraHistory()
{
    // migrate transaction log
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare(QString("SELECT tran_guid, tran_data from transaction_log"));
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    QSqlQuery updQuery(m_sdb);
    updQuery.prepare(QString("DELETE FROM transaction_log WHERE tran_guid = ?"));

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

        updQuery.addBindValue(QnSql::serialized_field(tranGuid));
        if (!updQuery.exec()) {
            qWarning() << Q_FUNC_INFO << query.lastError().text();
            return false;
        }
    }

    return true;
}

bool QnDbManager::removeWrongSupportedMotionTypeForONVIF()
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare(QString("SELECT tran_guid, tran_data from transaction_log"));
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    QSqlQuery updQuery(m_sdb);
    updQuery.prepare(QString("DELETE FROM transaction_log WHERE tran_guid = ?"));

    while (query.next()) {
        QnAbstractTransaction abstractTran;
        QnUuid tranGuid = QnSql::deserialized_field<QnUuid>(query.value(0));
        QByteArray srcData = query.value(1).toByteArray();
        QnUbjsonReader<QByteArray> stream(&srcData);
        if (!QnUbjson::deserialize(&stream, &abstractTran)) {
            qWarning() << Q_FUNC_INFO << "Can' deserialize transaction from transaction log";
            return false;
        }
        if (abstractTran.command != ApiCommand::setResourceParam)
            continue;
        ApiResourceParamWithRefData data;
        if (!QnUbjson::deserialize(&stream, &data))
        {
            qWarning() << Q_FUNC_INFO << "Can' deserialize transaction from transaction log";
            return false;
        }
        if (data.name == lit("supportedMotion") && data.value.isEmpty())
        {
            updQuery.addBindValue(QnSql::serialized_field(tranGuid));
            if (!updQuery.exec()) {
                qWarning() << Q_FUNC_INFO << query.lastError().text();
                return false;
            }
        }
    }

    return true;
}

bool QnDbManager::fixBusinessRules()
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare(QString("SELECT tran_guid, tran_data from transaction_log"));
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    QSqlQuery delQuery(m_sdb);
    delQuery.prepare(QString("DELETE FROM transaction_log WHERE tran_guid = ?"));

    while (query.next()) {
        QnAbstractTransaction abstractTran;
        QnUuid tranGuid = QnSql::deserialized_field<QnUuid>(query.value(0));
        QByteArray srcData = query.value(1).toByteArray();
        QnUbjsonReader<QByteArray> stream(&srcData);
        if (!QnUbjson::deserialize(&stream, &abstractTran)) {
            qWarning() << Q_FUNC_INFO << "Can' deserialize transaction from transaction log";
            return false;
        }
        if (abstractTran.command == ApiCommand::resetBusinessRules || abstractTran.command == ApiCommand::saveBusinessRule)
        {
            delQuery.addBindValue(QnSql::serialized_field(tranGuid));
            if (!delQuery.exec()) {
                qWarning() << Q_FUNC_INFO << query.lastError().text();
                return false;
            }
        }
    }
    m_needResyncbRules = true;
    return true;
}

bool QnDbManager::isTranAllowed(const QnAbstractTransaction& tran) const
{
    if( !m_dbReadOnly )
        return true;

    switch( tran.command )
    {
        case ApiCommand::addLicense:
        case ApiCommand::addLicenses:
        case ApiCommand::removeLicense:
            return true;

        case ApiCommand::saveMediaServer:
        case ApiCommand::saveStorage:
        case ApiCommand::saveStorages:
        case ApiCommand::saveServerUserAttributes:
        case ApiCommand::saveServerUserAttributesList:
        case ApiCommand::setResourceStatus:
        case ApiCommand::setResourceParam:
        case ApiCommand::setResourceParams:
            //allowing minimum set of transactions required for local server to function properly
            return tran.deliveryInfo.originatorType == QnTranDeliveryInformation::localServer;

        default:
            return false;
    }
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
    else if (updateName == lit(":/updates/21_business_action_parameters.sql")) {
        updateBusinessActionParameters();
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
    else if (updateName == lit(":/updates/31_move_group_name_to_user_attrs.sql")) {
        if (!m_dbJustCreated)
            m_needResyncCameraUserAttributes = true;
    }
    else if (updateName == lit(":/updates/32_default_business_rules.sql")) {
        for(const auto& bRule: QnBusinessEventRule::getSystemRules())
        {
            ApiBusinessRuleData bRuleData;
            fromResourceToApi(bRule, bRuleData);
            if (updateBusinessRule(bRuleData) != ErrorCode::ok)
                return false;
        }
    }
    else if (updateName == lit(":/updates/33_resync_layout.sql")) {
        if (!m_dbJustCreated)
            m_needResyncLayout = true;
    }
    else if (updateName == lit(":/updates/35_fix_onvif_mt.sql")) {
        return removeWrongSupportedMotionTypeForONVIF();
    }
    else if (updateName == lit(":/updates/36_fix_brules.sql")) {
        return fixBusinessRules();
    }
    else if (updateName == lit(":/updates/37_remove_empty_layouts.sql")) {
        return removeEmptyLayoutsFromTransactionLog();
    }
    else if (updateName == lit(":/updates/41_resync_tran_log.sql"))
    {
        if (!m_dbJustCreated) {
            m_needResyncUsers = true;
            m_needResyncStorages = true;
        }
    }
    else if (updateName == lit(":/updates/42_add_license_to_user_attr.sql")) {
        if (!m_dbJustCreated)
            m_needResyncCameraUserAttributes = true;
    }
    else if (updateName == lit(":/updates/43_add_business_rules.sql")) {
        for(const auto& bRule: QnBusinessEventRule::getRulesUpd43())
        {
            ApiBusinessRuleData bRuleData;
            fromResourceToApi(bRule, bRuleData);
            if (updateBusinessRule(bRuleData) != ErrorCode::ok)
                return false;
        }
    }
    else if (updateName == lit(":/updates/48_add_business_rules.sql")) {
        for(const auto& bRule: QnBusinessEventRule::getRulesUpd48())
        {
            ApiBusinessRuleData bRuleData;
            fromResourceToApi(bRule, bRuleData);
            if (updateBusinessRule(bRuleData) != ErrorCode::ok)
                return false;
        }
    }
    else if (updateName == lit(":/updates/43_resync_client_info_data.sql")) {
        if (!m_dbJustCreated)
            m_needResyncClientInfoData = true;
    }
    else if (updateName == lit(":/updates/44_upd_brule_format.sql")) {
        if (!m_dbJustCreated)
            m_needResyncbRules = true;
    }
    else if (updateName == lit(":/updates/49_fix_migration.sql")) {
        if (!m_dbJustCreated)
        {
            m_needResyncCameraUserAttributes = true;
            m_needResyncServerUserAttributes = true;
        }
    }
    else if (updateName == lit(":/updates/49_add_webpage_table.sql"))
    {
        QMap<int, QnUuid> guids = getGuidList("SELECT rt.id, rt.name || '-' as guid from vms_resourcetype rt WHERE rt.name == 'WebPage'", CM_MakeHash);
        if (!updateTableGuids("vms_resourcetype", "guid", guids))
            return false;
    }
    else if (updateName == lit(":/updates/50_add_access_rights.sql"))
    {
        if (!m_dbJustCreated)
        {
            m_needResyncUsers = true;
        }
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

        if (!execSQLFile(lit(":/00_update_2.2_stage0.sql"), m_sdb))
            return false;

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
    query.setForwardOnly(true);
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
    if (!execSQLQuery("DELETE FROM vms_license", m_sdb, Q_FUNC_INFO))
        return false;


    if (!applyUpdates(":/updates"))
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

    //NX_ASSERT(data.status == Qn::NotDefined, Q_FUNC_INFO, "Status MUST be unchanged for resource modification. Use setStatus instead to modify it!");

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
    {
        const QString authQueryStr = data.hash.isEmpty()
            ? "UPDATE auth_user SET is_superuser=:isAdmin, email=:email where username=:name"
            : R"(
                INSERT OR REPLACE
                INTO auth_user
                (id, username, is_superuser, email, password, is_staff, is_active, last_login, date_joined, first_name, last_name)
                VALUES
                (:internalId, :name, :isAdmin, :email, :hash, 1, 1, '', '', '', '')
            )";


        QSqlQuery authQuery(m_sdb);
        if (!prepareSQLQuery(&authQuery, authQueryStr, Q_FUNC_INFO))
            return ErrorCode::dbError;
        QnSql::bind(data, &authQuery);
        authQuery.bindValue(":internalId", internalId);
        if (!execSQLQuery(&authQuery, Q_FUNC_INFO))
            return ErrorCode::dbError;
    }

    {
        const QString profileQueryStr = R"(
            INSERT OR REPLACE
            INTO vms_userprofile
            (user_id, resource_ptr_id, digest, crypt_sha512_hash, realm, rights, is_ldap, is_enabled, group_guid, is_cloud)
            VALUES
            (:internalId, :internalId, :digest, :cryptSha512Hash, :realm, :permissions, :isLdap, :isEnabled, :groupId, :isCloud)
        )";

        QSqlQuery profileQuery(m_sdb);
        if (!prepareSQLQuery(&profileQuery, profileQueryStr, Q_FUNC_INFO))
            return ErrorCode::dbError;
        QnSql::bind(data, &profileQuery);

        if (data.digest.isEmpty() && !data.isLdap)
        {
            // keep current digest value if exists
            QSqlQuery digestQuery(m_sdb);
            digestQuery.setForwardOnly(true);
            if (!prepareSQLQuery(&digestQuery, "SELECT digest, crypt_sha512_hash FROM vms_userprofile WHERE user_id = ?", Q_FUNC_INFO))
                return ErrorCode::dbError;
            digestQuery.addBindValue(internalId);
            if (!execSQLQuery(&digestQuery, Q_FUNC_INFO))
                return ErrorCode::dbError;
            if (digestQuery.next())
            {
                profileQuery.bindValue(":digest", digestQuery.value(0).toByteArray());
                profileQuery.bindValue(":cryptSha512Hash", digestQuery.value(1).toByteArray());
            }
        }
        profileQuery.bindValue(":internalId", internalId);
        if (!execSQLQuery(&profileQuery, Q_FUNC_INFO))
            return ErrorCode::dbError;
    }

    return ErrorCode::ok;
}

ErrorCode QnDbManager::insertOrReplaceUserGroup(const ApiUserGroupData& data)
{
    QSqlQuery query(m_sdb);
    const QString queryStr = R"(
        INSERT OR REPLACE INTO vms_user_groups
        (id, name, permissions)
        VALUES
        (:id, :name, :permissions)
    )";
    if (!prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return ErrorCode::dbError;

    QnSql::bind(data, &query);
    if (!execSQLQuery(&query, Q_FUNC_INFO))
        return ErrorCode::dbError;

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
            group_name,                     \
            audio_enabled,                  \
            control_enabled,                \
            region,                         \
            schedule_enabled,               \
            motion_type,                    \
            secondary_quality,              \
            dewarping_params,               \
            min_archive_days,               \
            max_archive_days,               \
            prefered_server_id,             \
            license_used,                   \
            failover_priority,              \
            backup_type                     \
            )                               \
         VALUES (                           \
            :cameraID,                      \
            :cameraName,                    \
            :userDefinedGroupName,          \
            :audioEnabled,                  \
            :controlEnabled,                \
            :motionMask,                    \
            :scheduleEnabled,               \
            :motionType,                    \
            :secondaryStreamQuality,        \
            :dewarpingParams,               \
            :minArchiveDays,                \
            :maxArchiveDays,                \
            :preferedServerId,              \
            :licenseUsed,                   \
            :failoverPriority,              \
            :backupType                     \
            )                               \
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
        selQuery.setForwardOnly(true);
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
        INSERT OR REPLACE INTO vms_storage ( \
            space_limit, \
            used_for_writing, \
            storage_type, \
            backup, \
            resource_ptr_id) \
        VALUES ( \
            :spaceLimit, \
            :usedForWriting, \
            :storageType, \
            :isBackup, \
            :internalId) \
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
    insQuery.prepare("INSERT INTO vms_scheduletask ("
        "camera_attrs_id, start_time, end_time, do_record_audio, record_type,"
        "day_of_week, before_threshold, after_threshold, stream_quality, fps"
    ") VALUES ("
        ":internalId, :startTime, :endTime, :recordAudio, :recordingType,"
        ":dayOfWeek, :beforeThreshold, :afterThreshold, :streamQuality, :fps"
    ")");

    insQuery.bindValue(":internalId", internalId);
    for(const ApiScheduleTaskData& task: scheduleTasks) {
        QnSql::bind(task, &insQuery);
        if (!insQuery.exec()) {
            qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
            return ErrorCode::dbError;
        }
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiDatabaseDumpData>& tran)
{
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

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiClientInfoData>& tran)
{
    QSqlQuery query(m_sdb);
    query.prepare("INSERT OR REPLACE INTO vms_client_infos VALUES ("
        ":id, :parentId, :skin, :systemInfo, :cpuArchitecture, :cpuModelName,"
        ":phisicalMemory, :openGLVersion, :openGLVendor, :openGLRenderer,"
        ":fullVersion, :systemRuntime)");

    QnSql::bind(tran.params, &query);
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }
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
    insQuery.prepare("                                           \
        INSERT OR REPLACE INTO vms_server_user_attributes (      \
            server_guid,                                         \
            server_name,                                         \
            max_cameras,                                         \
            redundancy,                                          \
            backup_type,                                         \
            backup_days_of_the_week,                             \
            backup_start,                                        \
            backup_duration,                                     \
            backup_bitrate                                       \
        )                                                        \
        VALUES(                                                  \
            :serverID,                                           \
            :serverName,                                         \
            :maxCameras,                                         \
            :allowAutoRedundancy,                                \
            :backupType,                                         \
            :backupDaysOfTheWeek,                                \
            :backupStart,                                        \
            :backupDuration,                                     \
            :backupBitrate                                       \
        )                                                        \
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
        dewarping_params, left, display_info \
        ) VALUES \
        (:zoomBottom, :right, :id, :zoomLeft, :resourceId, \
        :zoomRight, :top, :layoutId, :bottom, :zoomTop, \
        :zoomTargetId, :flags, :contrastParams, :rotation, \
        :dewarpingParams, :left, :displayInfo \
        )\
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

ErrorCode QnDbManager::removeUserGroup(const QnUuid& guid)
{
    /* Cleanup all users, belonging to this group. */
    {
        QSqlQuery query(m_sdb);
        const QString queryStr("UPDATE vms_userprofile SET group = NULL WHERE group = :groupId");
        if (!prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
            return ErrorCode::dbError;

        query.bindValue(":groupId", guid.toRfc4122());
        if (!execSQLQuery(&query, Q_FUNC_INFO))
            return ErrorCode::dbError;
    }

    return deleteTableRecord(guid, "vms_user_groups", "id");
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

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiWebPageData>& tran)
{
    ErrorCode result = saveWebPage(tran.params);
    return result;
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiWebPageDataList>& tran)
{
    for(const ApiWebPageData& webPage: tran.params)
    {
        ErrorCode err = saveWebPage(webPage);
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

ErrorCode QnDbManager::addCameraHistory(const ApiServerFootageData& params)
{
    QSqlQuery query(m_sdb);
    query.prepare("INSERT OR REPLACE INTO vms_used_cameras (server_guid, cameras) VALUES(?, ?)");
    query.addBindValue(QnSql::serialized_field(params.serverGuid));
    query.addBindValue(QnUbjson::serialized(params.archivedCameras));
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }

    return ErrorCode::ok;
}

ErrorCode QnDbManager::removeCameraHistory(const QnUuid& serverId)
{
    QSqlQuery query(m_sdb);
    query.prepare("DELETE FROM vms_usedCameras WHERE server_guid = ?");
    query.addBindValue(QnUbjson::serialized(serverId));
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }

    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiServerFootageData>& tran)
{
    if (tran.command == ApiCommand::addCameraHistoryItem)
        return addCameraHistory(tran.params);
    else {
        NX_ASSERT(1);
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

    // todo: #rvasilenko Do not delete references to the camera. All child object should be deleted with separated transactions
    // It's not big issue now because we keep a lot of camera data after deleting camera object. So, it should be refactored in 1.4 after we introduce full data cleanup
    // for removing camera
    /*
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
    */

    ErrorCode err = deleteTableRecord(id, "vms_camera", "resource_ptr_id");
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

    // This data cannot be removed this way with the current architecture.
//    err = deleteTableRecord(guid, "vms_mserver_discovery", "server_id");
//    if (err != ErrorCode::ok)
//        return err;

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
    NX_ASSERT( tran.command == ApiCommand::addStoredFile || tran.command == ApiCommand::updateStoredFile );
    return insertOrReplaceStoredFile(tran.params.path, tran.params.data);
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiStoredFilePath>& tran)
{
    NX_ASSERT(tran.command == ApiCommand::removeStoredFile);

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
    {
        qWarning() << Q_FUNC_INFO << "Duplicate user with name "<<name<<" found";
        return ErrorCode::failure;  // another user with same name already exists
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::setAccessRights(const ApiAccessRightsData& data)
{
    const QByteArray userOrGroupId = data.userId.toRfc4122();

    /* Get list of resources, user already has access to. */
    QSet<qint32> accessibleResources;

    {
        QString selectQueryString = R"(
            SELECT resource_ptr_id
            FROM vms_access_rights
            WHERE guid = :userOrGroupId
        )";

        QSqlQuery selectQuery(m_sdb);
        selectQuery.setForwardOnly(true);
        if (!prepareSQLQuery(&selectQuery, selectQueryString, Q_FUNC_INFO))
            return ErrorCode::dbError;

        selectQuery.bindValue(":userOrGroupId", userOrGroupId);
        if (!execSQLQuery(&selectQuery, Q_FUNC_INFO))
            return ErrorCode::dbError;

        while (selectQuery.next())
            accessibleResources.insert(selectQuery.value(0).toInt());
    }

    QSet<qint32> newAccessibleResources;
    for (const QnUuid& resourceId : data.resourceIds)
    {
        qint32 resource_ptr_id = getResourceInternalId(resourceId);
        if (resource_ptr_id <= 0)
            return ErrorCode::dbError;
        newAccessibleResources << resource_ptr_id;
    }

    QSet<qint32> resourcesToAdd = newAccessibleResources - accessibleResources;
    if (!resourcesToAdd.empty())
    {
        QString insertQueryString = R"(
            INSERT INTO vms_access_rights
            (guid, resource_ptr_id)
            VALUES
        )";

        QStringList values;

        for (const qint32& resource_ptr_id : resourcesToAdd)
             values << QString("(:userOrGroupId, %1)").arg(resource_ptr_id);
         insertQueryString.append(values.join(L',')).append(L';');

        QSqlQuery insertQuery(m_sdb);
        insertQuery.setForwardOnly(true);
        if (!prepareSQLQuery(&insertQuery, insertQueryString, Q_FUNC_INFO))
            return ErrorCode::dbError;
        insertQuery.bindValue(":userOrGroupId", userOrGroupId);

        if (!execSQLQuery(&insertQuery, Q_FUNC_INFO))
            return ErrorCode::dbError;
    }

    QSet<qint32> resourcesToRemove = accessibleResources - newAccessibleResources;
    if (!resourcesToRemove.empty())
    {
        QSqlQuery removeQuery(m_sdb);
        removeQuery.prepare(R"(
            DELETE FROM vms_access_rights
            WHERE guid = :userOrGroupId
            AND resource_ptr_id IN (:resources);
        )");
        removeQuery.bindValue(":userOrGroupId", userOrGroupId);

        QStringList values;
        for (const qint32& resource_ptr_id : resourcesToRemove)
            values << QString::number(resource_ptr_id);

        removeQuery.bindValue(":resources", values.join(L','));
        if (!execSQLQuery(&removeQuery, Q_FUNC_INFO))
            return ErrorCode::dbError;
    }
    return ErrorCode::ok;
}


ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiUserData>& tran)
{
    /*
    qint32 internalId = getResourceInternalId(tran.params.id);
    ErrorCode result = checkExistingUser(tran.params.name, internalId);
    if (result !=ErrorCode::ok)
        return result;
    */
    qint32 internalId = 0;
    ErrorCode result = insertOrReplaceResource(tran.params, &internalId);
    if (result !=ErrorCode::ok)
        return result;

    return insertOrReplaceUser(tran.params, internalId);
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiUserGroupData>& tran)
{
    NX_ASSERT(tran.command == ApiCommand::saveUserGroup, Q_FUNC_INFO, "Unsupported transaction");
    if (tran.command != ApiCommand::saveUserGroup)
        return ec2::ErrorCode::serverError;
    return insertOrReplaceUserGroup(tran.params);
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiAccessRightsData>& tran)
{
    NX_ASSERT(tran.command == ApiCommand::setAccessRights, Q_FUNC_INFO, "Unsupported transaction");
    if (tran.command != ApiCommand::setAccessRights)
        return ec2::ErrorCode::serverError;

    return setAccessRights(tran.params);
}

ApiObjectType QnDbManager::getObjectTypeNoLock(const QnUuid& objectId)
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
    else if (objectType == QnResourceTypePool::kStorageTypeId)
        return ApiObject_Storage;
    else if (objectType == QnResourceTypePool::kServerTypeId)
        return ApiObject_Server;
    else if (objectType == QnResourceTypePool::kUserTypeId)
        return ApiObject_User;
    else if (objectType == QnResourceTypePool::kLayoutTypeId)
        return ApiObject_Layout;
    else if (objectType == QnResourceTypePool::kVideoWallTypeId)
        return ApiObject_Videowall;
    else if (objectType == QnResourceTypePool::kWebPageTypeId)
        return ApiObject_WebPage;
    else
    {
        NX_ASSERT(false, "Unknown object type", Q_FUNC_INFO);
        return ApiObject_NotDefined;
    }
}

ApiObjectInfoList QnDbManager::getNestedObjectsNoLock(const ApiObjectInfo& parentObject)
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
            //NX_ASSERT(0, "Not implemented!", Q_FUNC_INFO);
            return result;
    }
    query.bindValue(":guid", parentObject.id.toRfc4122());

    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return result;
    }
    while(query.next()) {
        ApiObjectInfo info;
        info.type = (ApiObjectType) query.value(0).toInt();
        info.id = QnUuid::fromRfc4122(query.value(1).toByteArray());
        result.push_back(info);
    }

    return result;
}

ApiObjectInfoList QnDbManager::getObjectsNoLock(const ApiObjectType& objectType)
{
    ApiObjectInfoList result;

    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);

    switch(objectType)
    {
    case ApiObject_BusinessRule:
        query.prepare("SELECT guid from vms_businessrule");
        break;
    default:
        NX_ASSERT(0, "Not implemented!", Q_FUNC_INFO);
        return result;
    }
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return result;
    }
    while(query.next()) {
        ApiObjectInfo info;
        info.type = objectType;
        info.id = QnSql::deserialized_field<QnUuid>(query.value(0));
        result.push_back(std::move(info));
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
    QnWriteLocker lock( &m_mutex );   //locking it here since this method is public

    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
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
    QnWriteLocker lock( &m_mutex );   //locking it here since this method is public

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
    switch(tran.command)
    {
    case ApiCommand::removeCamera:
        return removeCamera(tran.params.id);
    case ApiCommand::removeStorage:
        return removeStorage(tran.params.id);
    case ApiCommand::removeMediaServer:
        return removeServer(tran.params.id);
    case ApiCommand::removeServerUserAttributes:
        return removeMediaServerUserAttributes(tran.params.id);
    case ApiCommand::removeLayout:
        return removeLayout(tran.params.id);
    case ApiCommand::removeBusinessRule:
        return removeBusinessRule(tran.params.id);
    case ApiCommand::removeUser:
        return removeUser(tran.params.id);
    case ApiCommand::removeUserGroup:
        return removeUserGroup(tran.params.id);
    case ApiCommand::removeVideowall:
        return removeVideowall(tran.params.id);
    case ApiCommand::removeWebPage:
        return removeWebPage(tran.params.id);
    default:
        return removeObject(ApiObjectInfo(getObjectTypeNoLock(tran.params.id), tran.params.id));
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
    case ApiObject_WebPage:
        result = removeWebPage(apiObject.id);
        break;
    case ApiObject_NotDefined:
        result = ErrorCode::ok; // object already removed
        break;
    default:
        qWarning() << "Remove operation is not implemented for object type" << apiObject.type;
        NX_ASSERT(0, "Remove operation is not implemented for command", Q_FUNC_INFO);
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
        NX_ASSERT(0, Q_FUNC_INFO, "Can't parse XML file");
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
        li.dewarping_params as dewarpingParams, li.left, li.display_info as displayInfo \
        FROM vms_layoutitem li \
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

ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiCameraDataList& cameraList)
{
    QSqlQuery queryCameras(m_sdb);
    queryCameras.setForwardOnly(true);
    queryCameras.prepare(QString("\
        SELECT r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentId, r.name, r.url, \
        c.vendor, c.manually_added as manuallyAdded, \
        c.group_name as groupName, c.group_id as groupId, c.mac, c.model, \
		c.status_flags as statusFlags, c.physical_id as physicalId \
        FROM vms_resource r \
        LEFT JOIN vms_resource_status rs on rs.guid = r.guid \
        JOIN vms_camera c on c.resource_ptr_id = r.id ORDER BY r.guid\
    "));


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
        s.space_limit as spaceLimit, s.used_for_writing as usedForWriting, s.storage_type as storageType, \
        s.backup as isBackup \
        FROM vms_resource r \
        JOIN vms_storage s on s.resource_ptr_id = r.id \
        %1 \
        ORDER BY r.guid\
    ").arg(filterStr));

    if (!queryStorage.exec()) {
        qWarning() << Q_FUNC_INFO << queryStorage.lastError().text();
        return ErrorCode::dbError;
    }

    QnSql::fetch_many(queryStorage, &storageList);

    QnQueryFilter filter;
    filter.fields.insert( RES_TYPE_FIELD, RES_TYPE_STORAGE );

    ApiResourceParamWithRefDataList params;
    const auto result = fetchResourceParams( filter, params );
    if( result != ErrorCode::ok )
        return result;

    mergeObjectListData<ApiStorageData>(
        storageList, params,
        &ApiStorageData::addParams,
        &ApiResourceParamWithRefData::resourceId);

    // Storages are generally bound to MediaServers,
    // so it's required to be sorted by parent id
    std::sort(storageList.begin(), storageList.end(),
              [](const ApiStorageData& lhs, const ApiStorageData& rhs)
              { return lhs.parentId.toRfc4122() < rhs.parentId.toRfc4122(); });

    return ErrorCode::ok;
}

ErrorCode QnDbManager::getScheduleTasks(std::vector<ApiScheduleTaskWithRefData>& scheduleTaskList)
{
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
        ORDER BY r.camera_guid\
    "));

    if (!queryScheduleTask.exec()) {
        NX_LOG( lit("Db error in %1: %2").arg(Q_FUNC_INFO).arg(queryScheduleTask.lastError().text()), cl_logWARNING );
        return ErrorCode::dbError;
    }

    QnSql::fetch_many(queryScheduleTask, &scheduleTaskList);
    return ErrorCode::ok;
}

ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiCameraAttributesDataList& cameraUserAttributesList)
{
    //fetching camera user attributes
    QSqlQuery queryCameras(m_sdb);
    queryCameras.setForwardOnly(true);

    queryCameras.prepare(lit("\
        SELECT                                           \
            camera_guid as cameraID,                     \
            camera_name as cameraName,                   \
            group_name as userDefinedGroupName,          \
            audio_enabled as audioEnabled,               \
            control_enabled as controlEnabled,           \
            region as motionMask,                        \
            schedule_enabled as scheduleEnabled,         \
            motion_type as motionType,                   \
            secondary_quality as secondaryStreamQuality, \
            dewarping_params as dewarpingParams,         \
            min_archive_days as minArchiveDays,          \
            max_archive_days as maxArchiveDays,          \
            prefered_server_id as preferedServerId,      \
            license_used as licenseUsed,                 \
            failover_priority as failoverPriority,       \
            backup_type as backupType                    \
         FROM vms_camera_user_attributes                 \
         LEFT JOIN vms_resource r on r.guid = camera_guid     \
         ORDER BY camera_guid                            \
        "));

    if (!queryCameras.exec()) {
        NX_LOG( lit("Db error in %1: %2").arg(Q_FUNC_INFO).arg(queryCameras.lastError().text()), cl_logWARNING );
        return ErrorCode::dbError;
    }

    QnSql::fetch_many(queryCameras, &cameraUserAttributesList);

    std::vector<ApiScheduleTaskWithRefData> scheduleTaskList;
    ErrorCode errCode = getScheduleTasks(scheduleTaskList);
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

ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiCameraDataExList& cameraExList)
{
    QSqlQuery queryCameras( m_sdb );
    queryCameras.setForwardOnly(true);

    queryCameras.prepare(QString("\
        SELECT r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentId, \
            coalesce(nullif(cu.camera_name, \"\"), r.name) as name, r.url, \
            coalesce(rs.status, 0) as status, \
            c.vendor, c.manually_added as manuallyAdded, \
            coalesce(nullif(cu.group_name, \"\"), c.group_name) as groupName, \
            c.group_id as groupId, c.mac, c.model, \
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
            cu.prefered_server_id as preferedServerId,         \
            cu.license_used as licenseUsed,                    \
            cu.failover_priority as failoverPriority,          \
            cu.backup_type as backupType                       \
        FROM vms_resource r \
        LEFT JOIN vms_resource_status rs on rs.guid = r.guid \
        JOIN vms_camera c on c.resource_ptr_id = r.id \
        LEFT JOIN vms_camera_user_attributes cu on cu.camera_guid = r.guid \
        ORDER BY r.guid \
    "));

    if (!queryCameras.exec()) {
        NX_LOG( lit("Db error in %1: %2").arg(Q_FUNC_INFO).arg(queryCameras.lastError().text()), cl_logWARNING );
        return ErrorCode::dbError;
    }
    QnSql::fetch_many(queryCameras, &cameraExList);

    //reading resource properties
    QnQueryFilter filter;
    filter.fields.insert( RES_TYPE_FIELD, RES_TYPE_CAMERA );
    ApiResourceParamWithRefDataList params;
    ErrorCode result = fetchResourceParams( filter, params );
    if( result != ErrorCode::ok )
        return result;
    mergeObjectListData<ApiCameraDataEx>(cameraExList, params, &ApiCameraDataEx::addParams, &ApiResourceParamWithRefData::resourceId);

    std::vector<ApiScheduleTaskWithRefData> scheduleTaskList;
    ErrorCode errCode = getScheduleTasks(scheduleTaskList);
    if (errCode != ErrorCode::ok)
        return errCode;

    mergeObjectListData(
        cameraExList,
        scheduleTaskList,
        &ApiCameraDataEx::scheduleTasks,
        &ApiCameraDataEx::id,
        &ApiScheduleTaskWithRefData::sourceId );


    return ErrorCode::ok;
}


// ----------- getServers --------------------


ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiMediaServerDataList& serverList)
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);

    query.prepare(QString("\
        SELECT r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentId, r.name, r.url, \
        s.api_url as apiUrl, s.auth_key as authKey, s.version, s.net_addr_list as networkAddresses, s.system_info as systemInfo, \
        s.flags, s.system_name as systemName \
        FROM vms_resource r \
        LEFT JOIN vms_resource_status rs on rs.guid = r.guid \
        JOIN vms_server s on s.resource_ptr_id = r.id ORDER BY r.guid\
    "));

    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }

    QnSql::fetch_many(query, &serverList);

    return ErrorCode::ok;
}

ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiMediaServerDataExList& serverExList)
{
    {
        //fetching server data
        ApiMediaServerDataList serverList;
        ErrorCode result = doQueryNoLock(nullptr, serverList);
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
        ErrorCode result = doQueryNoLock(QnUuid(), serverAttrsList );
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
    ErrorCode result = doQueryNoLock(QnUuid(), storages );
    if( result != ErrorCode::ok )
        return result;
    //merging storages
    mergeObjectListData( serverExList, storages, &ApiMediaServerDataEx::storages, &ApiMediaServerDataEx::id, &ApiStorageData::parentId );

    //reading properties
    QnQueryFilter filter;
    filter.fields.insert( RES_TYPE_FIELD, RES_TYPE_MSERVER );
    ApiResourceParamWithRefDataList params;
    result = fetchResourceParams( filter, params );
    if( result != ErrorCode::ok )
        return result;
    mergeObjectListData<ApiMediaServerDataEx>(serverExList, params, &ApiMediaServerDataEx::addParams, &ApiResourceParamWithRefData::resourceId);

    //reading status info
    ApiResourceStatusDataList statusList;
    result = doQueryNoLock( QnUuid(), statusList );
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
        SELECT                                                      \
            server_guid as serverID,                                \
            server_name as serverName,                              \
            max_cameras as maxCameras,                              \
            redundancy as allowAutoRedundancy,                      \
            backup_type as backupType,                              \
            backup_days_of_the_week as backupDaysOfTheWeek,         \
            backup_start as backupStart,                            \
            backup_duration as backupDuration,                      \
            backup_bitrate as backupBitrate                         \
        FROM vms_server_user_attributes                             \
        %1                                                          \
        ORDER BY server_guid                                        \
    ").arg(filterStr));
    if( !query.exec() )
    {
        NX_LOG( lit("DB Error at %1: %2").arg(Q_FUNC_INFO).arg(query.lastError().text()), cl_logWARNING );
        return ErrorCode::dbError;
    }
    QnSql::fetch_many(query, &serverAttrsList);

    return ErrorCode::ok;
}

//getCameraHistoryItems
ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiServerFootageDataList& historyList)
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare(QString("SELECT server_guid, cameras FROM vms_used_cameras ORDER BY server_guid"));
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }
    while (query.next()) {
        ApiServerFootageData data;
        data.serverGuid = QnUuid::fromRfc4122(query.value(0).toByteArray());
        data.archivedCameras = QnUbjson::deserialized<std::vector<QnUuid>>(query.value(1).toByteArray());
        historyList.push_back(std::move(data));
    }
    QnSql::fetch_many(query, &historyList);

    return ErrorCode::ok;
}

//getUsers
ErrorCode QnDbManager::doQueryNoLock(const std::nullptr_t& /*dummy*/, ApiUserDataList& userList)
{
    const QString queryStr = R"(
        SELECT r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentId, r.name, r.url,
        u.is_superuser as isAdmin, u.email,
        p.digest as digest, p.crypt_sha512_hash as cryptSha512Hash, p.realm as realm, u.password as hash, p.rights as permissions,
        p.is_ldap as isLdap, p.is_enabled as isEnabled, p.group_guid as groupId, p.is_cloud as isCloud
        FROM vms_resource r
        JOIN auth_user u on u.id = r.id
        JOIN vms_userprofile p on p.user_id = u.id
        ORDER BY r.guid
    )";

    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    if (!prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return ErrorCode::dbError;
    if (!execSQLQuery(&query, Q_FUNC_INFO))
        return ErrorCode::dbError;

    QnSql::fetch_many(query, &userList);

    return ErrorCode::ok;
}

//getUserGroups
ErrorCode QnDbManager::doQueryNoLock(const std::nullptr_t& /*dummy*/, ApiUserGroupDataList& result)
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    const QString queryStr = R"(
        SELECT id, name, permissions
        FROM vms_user_groups
        ORDER BY id
    )";
    if (!prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return ErrorCode::dbError;

    if (!execSQLQuery(&query, Q_FUNC_INFO))
        return ErrorCode::dbError;

    QnSql::fetch_many(query, &result);
    return ErrorCode::ok;
}


ec2::ErrorCode QnDbManager::doQueryNoLock(const std::nullptr_t& /*dummy*/, ApiAccessRightsDataList& accessRightsList)
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    const QString queryStr = R"(
        SELECT rights.guid as userId, resource.guid as resourceId
        FROM vms_access_rights rights
        JOIN vms_resource resource on resource.id = rights.resource_ptr_id
        ORDER BY rights.guid
    )";

    if (!prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return ErrorCode::dbError;

    if (!execSQLQuery(&query, Q_FUNC_INFO))
        return ErrorCode::dbError;

    ApiAccessRightsData current;
    while (query.next())
    {
        QnUuid userId = QnUuid::fromRfc4122(query.value(0).toByteArray());
        if (userId != current.userId)
        {
            if (!current.userId.isNull())
                accessRightsList.push_back(current);

            current.userId = userId;
            current.resourceIds.clear();
        }

        QnUuid resourceId = QnUuid::fromRfc4122(query.value(1).toByteArray());
        current.resourceIds.push_back(resourceId);
    }
    if (!current.userId.isNull())
        accessRightsList.push_back(current);

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

// getClientInfos
ErrorCode QnDbManager::doQueryNoLock(const std::nullptr_t&, ApiClientInfoDataList& data)
{
	return doQueryNoLock(QnUuid(), data);
}

ErrorCode QnDbManager::doQueryNoLock(const QnUuid& clientId, ApiClientInfoDataList& data)
{
	QString filterStr;
    if (!clientId.isNull())
        filterStr = QString("WHERE guid = %1").arg(guidToSqlString(clientId));

    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare(QString(
        "SELECT guid as id, parent_guid as parentId, skin, systemInfo,"
            "cpuArchitecture, cpuModelName, cpuModelName, phisicalMemory,"
            "openGLVersion, openGLVendor, openGLRenderer,"
            "full_version as fullVersion, systemRuntime "
        "FROM vms_client_infos %1 ORDER BY guid").arg(filterStr));

    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }

    QnSql::fetch_many(query, &data);
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
        ORDER BY item.matrix_guid\
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
    std::sort(matrices.begin(), matrices.end(), [] (const ApiVideowallMatrixWithRefData& data1, const ApiVideowallMatrixWithRefData& data2) {
        return data1.videowallGuid.toRfc4122() < data2.videowallGuid.toRfc4122();
    });
    mergeObjectListData(videowallList, matrices, &ApiVideowallData::matrices, &ApiVideowallMatrixWithRefData::videowallGuid);

    return ErrorCode::ok;
}

//getWebPageList
ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiWebPageDataList& webPageList)
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare(QString("\
                          SELECT r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentId, r.name, r.url \
                          FROM vms_webpage l \
                          JOIN vms_resource r on r.id = l.resource_ptr_id ORDER BY r.guid\
                          "));
    if (!query.exec())
    {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }
    QnSql::fetch_many(query, &webPageList);

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
    currentTime = TimeSynchronizationManager::instance()->getTimeInfo();
    return ErrorCode::ok;
}

// dumpDatabase
ErrorCode QnDbManager::doQuery(const nullptr_t& /*dummy*/, ApiDatabaseDumpData& data)
{
    QnWriteLocker lock(&m_mutex);

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

ErrorCode QnDbManager::doQuery(const ApiStoredFilePath& dumpFilePath, ApiDatabaseDumpToFileData& databaseDumpToFileData)
{
    QnWriteLocker lock(&m_mutex);

    //have to close/open DB to dump journals to .db file
    m_sdb.close();
    m_sdbStatic.close();

    if( !QFile::copy( m_sdb.databaseName(), dumpFilePath.path ) )
        return ErrorCode::ioError;

    //TODO #ak add license db to backup

    QFileInfo dumpFileInfo( dumpFilePath.path );
    databaseDumpToFileData.size = dumpFileInfo.size();

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
//    ErrorCode status;

#define db_load(target)      { ErrorCode status = doQueryNoLock(dummy, target);     if (status != ErrorCode::ok) return status; }
#define db_load_uuid(target) { ErrorCode status = doQueryNoLock(QnUuid(), target);  if (status != ErrorCode::ok) return status; }

    db_load(data.resourceTypes);

    db_load(data.servers);
    db_load_uuid(data.serversUserAttributesList);
    db_load(data.cameras);
    db_load(data.cameraUserAttributesList);
    db_load(data.users);
    db_load(data.userGroups);
    db_load(data.layouts);
    db_load(data.videowalls);
    db_load(data.webPages);
    db_load(data.rules);
    db_load(data.cameraHistory);
    db_load(data.licenses);
    db_load(data.discoveryData);
    db_load_uuid(data.allProperties);
    db_load_uuid(data.storages);
    db_load_uuid(data.resStatusList);
    db_load(data.accessRights);

#undef db_load_uuid
#undef db_load

    return ErrorCode::ok;
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
        NX_ASSERT(1, Q_FUNC_INFO, "Unexpected command!");
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

ErrorCode QnDbManager::saveWebPage(const ApiWebPageData& params)
{
    qint32 internalId;

    ErrorCode result = insertOrReplaceResource(params, &internalId);
    if (result != ErrorCode::ok)
        return result;

    result = insertOrReplaceWebPage(params, internalId);
    return result;
}


ErrorCode QnDbManager::removeWebPage(const QnUuid& guid)
{
    qint32 id = getResourceInternalId(guid);

    ErrorCode err = deleteTableRecord(id, "vms_webpage", "resource_ptr_id");
    if (err != ErrorCode::ok)
        return err;

    err = deleteRecordFromResourceTable(id);
    if (err != ErrorCode::ok)
        return err;

    return ErrorCode::ok;
}

ErrorCode QnDbManager::insertOrReplaceWebPage(const ApiWebPageData& data, qint32 internalId)
{
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT OR REPLACE INTO vms_webpage (resource_ptr_id) VALUES (:internalId)");
    QnSql::bind(data, &insQuery);
    insQuery.bindValue(":internalId", internalId);
    if (insQuery.exec())
        return ErrorCode::ok;

    qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
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
