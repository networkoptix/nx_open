#include "db_manager.h"

#include <utils/common/app_info.h>

#include <QtSql/QtSql>

#include <common/common_module.h>

#include "utils/common/util.h"
#include "managers/time_manager.h"
#include "nx_ec/data/api_business_rule_data.h"
#include "nx_ec/data/api_discovery_data.h"
#include <nx/vms/event/event_fwd.h>
#include "utils/common/synctime.h"
#include "utils/crypt/symmetrical.h"

#include "core/resource/user_resource.h"
#include <core/resource/camera_resource.h>

#include <core/resource_management/user_roles_manager.h>

#include <database/api/db_resource_api.h>
#include <database/api/db_layout_api.h>
#include <database/api/db_layout_tour_api.h>
#include <database/api/db_webpage_api.h>

#include <database/migrations/event_rules_db_migration.h>
#include <database/migrations/user_permissions_db_migration.h>
#include <database/migrations/accessible_resources_db_migration.h>
#include <database/migrations/legacy_transaction_migration.h>
#include <database/migrations/add_transaction_type.h>
#include <database/migrations/make_transaction_timestamp_128bit.h>
#include <database/migrations/add_history_attributes_to_transaction.h>
#include <database/migrations/reparent_videowall_layouts.h>
#include <database/migrations/add_default_webpages_migration.h>
#include <database/migrations/cleanup_removed_transactions.h>
#include <database/migrations/access_rights_db_migration.h>

#include <network/system_helpers.h>

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
#include "nx_ec/data/api_media_server_data.h"
#include "nx_ec/data/api_update_data.h"
#include <nx_ec/data/api_time_data.h>
#include "nx_ec/data/api_conversion_functions.h"
#include <nx_ec/data/api_access_rights_data.h>
#include "api/runtime_info_manager.h"
#include <nx/utils/log/log.h>
#include "nx_ec/data/api_camera_data_ex.h"
#include "restype_xml_parser.h"
#include <nx/vms/event/rule.h>
#include "settings.h"
#include <database/api/db_resource_api.h>

#include <nx/fusion/model_functions.h>

static const QString RES_TYPE_MSERVER = "mediaserver";
static const QString RES_TYPE_CAMERA = "camera";
static const QString RES_TYPE_STORAGE = "storage";

using namespace nx;

namespace ec2
{

namespace aux {

bool applyRestoreDbData(const BeforeRestoreDbData& restoreData, const QnUserResourcePtr& admin)
{
    if (!restoreData.isEmpty())
    {
        admin->setHash(restoreData.hash);
        admin->setDigest(restoreData.digest);
        admin->setCryptSha512Hash(restoreData.cryptSha512Hash);
        admin->setRealm(restoreData.realm);
        admin->setEnabled(true);
        return true;
    }

    return false;
}
}

namespace detail
{

static const char LICENSE_EXPIRED_TIME_KEY[] = "{4208502A-BD7F-47C2-B290-83017D83CDB7}";
static const char DB_INSTANCE_KEY[] = "DB_INSTANCE_ID";

using std::nullptr_t;

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

/**
 * Updaters are used to update object fields which are stored as raw json strings.
 * @return True if the object is updated.
 */
bool businessRuleObjectUpdater(ApiBusinessRuleData& data)
{
    if (data.actionParams.size() <= 4) //< keep empty json
        return false;
    auto deserializedData = QJson::deserialized<vms::event::ActionParameters>(data.actionParams);
    data.actionParams = QJson::serialized(deserializedData);
    return true;
}

//-------------------------------------------------------------------------------------------------
// QnDbTransactionExt

bool QnDbManager::QnDbTransactionExt::beginLazyTran()
{
    m_mutex.lockForWrite();

    if (!m_lazyTranInProgress)
        m_database.transaction();
    m_lazyTranInProgress = true;

    m_tranLog->beginTran();
    return true;
}

bool QnDbManager::QnDbTransactionExt::commitLazyTran()
{
    m_tranLog->commit();
    m_mutex.unlock();
    return true;
}

bool QnDbManager::QnDbTransactionExt::beginTran()
{
    m_mutex.lockForWrite();

    if (m_lazyTranInProgress)
        dbCommit(lit("lazy before new"));

    m_lazyTranInProgress = false;
    m_database.transaction();

    m_tranLog->beginTran();
    return true;
}

void QnDbManager::QnDbTransactionExt::rollback()
{
    m_tranLog->rollback();
    m_database.rollback();
    m_lazyTranInProgress = false;
    m_mutex.unlock();
}

bool QnDbManager::QnDbTransactionExt::commit()
{
    const bool rez = dbCommit("ext commit");
    m_lazyTranInProgress = false;
    if (rez)
    {
        // Commit only on success, otherwise rollback is expected.
        m_tranLog->commit();
        m_mutex.unlock();
    }

    return rez;
}

void QnDbManager::QnDbTransactionExt::physicalCommitLazyData()
{
    if (m_lazyTranInProgress)
    {
        m_lazyTranInProgress = false;
        dbCommit("phisical commit lazy");
        m_tranLog->commit(); //< Just in case. It does nothing.
    }
}

QnDbManager::QnLazyTransactionLocker::QnLazyTransactionLocker(QnDbTransactionExt* tran) :
    m_committed(false),
    m_tran(tran)
{
    m_tran->beginLazyTran();
}

QnDbManager::QnLazyTransactionLocker::~QnLazyTransactionLocker()
{
    if (!m_committed)
        m_tran->rollback();

}

bool QnDbManager::QnLazyTransactionLocker::commit()
{
    m_committed = m_tran->commitLazyTran();
    return m_committed;
}


//-------------------------------------------------------------------------------------------------
// QnDbManager

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

QnDbManager::QnDbManager(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule),
    m_licenseOverflowMarked(false),
    m_initialized(false),
    m_tranStatic(m_sdbStatic, m_mutexStatic),
    m_dbJustCreated(false),
    m_isBackupRestore(false),
    m_dbReadOnly(false),
    m_resyncFlags(),
    m_tranLog(nullptr),
    m_timeSyncManager(nullptr),
    m_insertCameraQuery(&m_queryCachePool),
    m_insertCameraUserAttrQuery(&m_queryCachePool),
    m_insertCameraScheduleQuery(&m_queryCachePool),
    m_insertKvPairQuery(&m_queryCachePool),
    m_resourceQueries(m_sdb, &m_queryCachePool)
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
    updateGuidQuery.addBindValue(commonModule()->moduleGUID().toRfc4122() );
    updateGuidQuery.addBindValue(commonModule()->obsoleteServerGuid().toRfc4122() );
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

QString QnDbManager::getDatabaseName(const QString& baseName)
{
    return baseName + commonModule()->moduleGUID().toString();
}

bool QnDbManager::init(const QUrl& dbUrl)
{
    NX_ASSERT(m_tranLog != nullptr);
    m_tran.reset(new QnDbTransactionExt(m_sdb, m_tranLog, m_mutex));

    {
        const QString dbFilePath = dbUrl.toLocalFile();
        const QString dbFilePathStatic = QUrlQuery(dbUrl.query()).queryItemValue("staticdb_path");

        QString dbFileName = closeDirPath(dbFilePath) + QString::fromLatin1("ecs.sqlite");
        addDatabase(dbFileName, getDatabaseName("QnDbManager"));

        QString backupDbFileName = dbFileName + QString::fromLatin1(".backup");
        bool needCleanup = QUrlQuery(dbUrl.query()).hasQueryItem("cleanupDb");
        if (QFile::exists(backupDbFileName) || needCleanup)
        {
            if (!removeDbFile(dbFileName))
                return false;
            if (QFile::exists(backupDbFileName))
            {
                m_resyncFlags |= ResyncLog;
                m_resyncFlags |= ClearLog;
                m_isBackupRestore = true;
                if (!QFile::rename(backupDbFileName, dbFileName)) {
                    qWarning() << "Can't rename database file from" << backupDbFileName << "to" << dbFileName << "Database restore operation canceled";
                    return false;
                }
            }
        }

        m_sdbStatic = QSqlDatabase::addDatabase("QSQLITE", getDatabaseName("QnDbManagerStatic"));
        QString path2 = dbFilePathStatic.isEmpty() ? dbFilePath : dbFilePathStatic;
        m_sdbStatic.setDatabaseName(closeDirPath(path2) + QString::fromLatin1("ecs_static.sqlite"));

        if (!m_sdb.open())
        {
            qWarning() << "can't initialize Server sqlLite database " << m_sdb.databaseName() << ". Error: " << m_sdb.lastError().text();
            return false;
        }

        {
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

                    qint64 currentIdentityTime = commonModule()->systemIdentityTime();
                    commonModule()->setSystemIdentityTime(qMax(currentIdentityTime + 1, dbRestoreTime), commonModule()->moduleGUID());
                }
            }
        }

        if (!m_sdbStatic.open() || !tuneDBAfterOpen(&m_sdbStatic))
        {
            qWarning() << "can't initialize Server static sqlLite database " << m_sdbStatic.databaseName() << ". Error: " << m_sdbStatic.lastError().text();
            return false;
        }

        //tuning DB
        if (!tuneDBAfterOpen(&m_sdb))
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

        if (!createDatabase())
        {
            // create tables is DB is empty
            qWarning() << "can't create tables for sqlLite database!";
            return false;
        }

        QnDbManager::QnDbTransactionLocker locker(getTransaction());

        if (!commonModule()->obsoleteServerGuid().isNull())
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
        if (addedStoredFilesCnt > 0)
            m_resyncFlags |= ResyncFiles;

        removeDirRecursive(storedFilesDir);

        // updateDBVersion();
        QSqlQuery insVersionQuery(m_sdb);
        insVersionQuery.prepare("INSERT OR REPLACE INTO misc_data (key, data) values (?,?)");
        insVersionQuery.addBindValue("VERSION");
        insVersionQuery.addBindValue(QnAppInfo::applicationVersion());
        if (!insVersionQuery.exec())
        {
            qWarning() << "can't initialize sqlLite database!" << insVersionQuery.lastError().text();
            return false;
        }
        insVersionQuery.addBindValue("BUILD");
        insVersionQuery.addBindValue(QnAppInfo::applicationRevision());
        if (!insVersionQuery.exec())
        {
            qWarning() << "can't initialize sqlLite database!" << insVersionQuery.lastError().text();
            return false;
        }

        m_storageTypeId = getType("Storage");
        m_serverTypeId = getType(QnResourceTypePool::kServerTypeId);
        m_cameraTypeId = getType("Camera");

        QSqlQuery queryAdminUser(m_sdb);
        queryAdminUser.setForwardOnly(true);
        queryAdminUser.prepare("SELECT r.guid, r.id FROM vms_resource r JOIN auth_user u on u.id = r.id and r.name = 'admin'"); // TODO: #GDM check owner permission instead
        execSQLQuery(&queryAdminUser, Q_FUNC_INFO);
        if (queryAdminUser.next())
        {
            m_adminUserID = QnUuid::fromRfc4122(queryAdminUser.value(0).toByteArray());
            m_adminUserInternalID = queryAdminUser.value(1).toInt();
        }
        NX_CRITICAL(!m_adminUserID.isNull());


        QSqlQuery queryServers(m_sdb);
        queryServers.prepare("UPDATE vms_resource_status set status = ? WHERE guid in (select guid from vms_resource where xtype_guid = ?)"); // todo: only mserver without DB?
        queryServers.bindValue(0, Qn::Offline);
        queryServers.bindValue(1, m_serverTypeId.toRfc4122());
        if (!queryServers.exec())
        {
            qWarning() << Q_FUNC_INFO << __LINE__ << queryServers.lastError();
            NX_ASSERT(false);
            return false;
        }

        // read license overflow time
        QSqlQuery query(m_sdb);
        query.setForwardOnly(true);
        query.prepare("SELECT data from misc_data where key = ?");
        query.addBindValue(LICENSE_EXPIRED_TIME_KEY);
        qint64 licenseOverflowTime = 0;
        if (query.exec() && query.next())
        {
            licenseOverflowTime = query.value(0).toByteArray().toLongLong();
            m_licenseOverflowMarked = licenseOverflowTime > 0;
        }
        const auto& runtimeInfoManager = commonModule()->runtimeInfoManager();
        QnPeerRuntimeInfo localInfo = runtimeInfoManager->localInfo();
        if (localInfo.data.prematureLicenseExperationDate != licenseOverflowTime)
            localInfo.data.prematureLicenseExperationDate = licenseOverflowTime;

        query.addBindValue(DB_INSTANCE_KEY);
        if (!m_resyncFlags.testFlag(ResyncLog) && query.exec() && query.next())
        {
            m_dbInstanceId = QnUuid::fromRfc4122(query.value(0).toByteArray());
        }
        else
        {
            m_dbInstanceId = QnUuid::createUuid();
            QSqlQuery insQuery(m_sdb);
            insQuery.prepare("INSERT OR REPLACE INTO misc_data (key, data) values (?,?)");
            insQuery.addBindValue(DB_INSTANCE_KEY);
            insQuery.addBindValue(m_dbInstanceId.toRfc4122());
            if (!insQuery.exec())
            {
                qWarning() << "can't initialize sqlLite database!";
                return false;
            }
        }

        localInfo.data.peer.persistentId = m_dbInstanceId;
        runtimeInfoManager->updateLocalItem(localInfo);

        if (m_tranLog)
            if (!m_tranLog->init())
            {
                qWarning() << "can't initialize transaction log!";
                return false;
            }

        if (!syncLicensesBetweenDB())
            return false;

        if (m_resyncFlags.testFlag(ClearLog) && !m_tranLog->clear())
            return false;

        if (m_resyncFlags.testFlag(ResyncLog))
        {
            if (!resyncTransactionLog())
                return false;
        }
        else
        {
            if (m_resyncFlags.testFlag(ResyncLicences))
            {
                if (!fillTransactionLogInternal<nullptr_t, ApiLicenseData, ApiLicenseDataList>(ApiCommand::addLicense))
                    return false;
            }
            if (m_resyncFlags.testFlag(ResyncFiles))
            {
                if (!fillTransactionLogInternal<nullptr_t, ApiStoredFileData, ApiStoredFileDataList>(ApiCommand::addStoredFile))
                    return false;
            }
            if (m_resyncFlags.testFlag(ResyncCameraAttributes))
            {
                if (!fillTransactionLogInternal<QnUuid, ApiCameraAttributesData, ApiCameraAttributesDataList>(ApiCommand::saveCameraUserAttributes))
                    return false;
            }
            if (m_resyncFlags.testFlag(ResyncServerAttributes))
            {
                if (!fillTransactionLogInternal<QnUuid, ApiMediaServerUserAttributesData, ApiMediaServerUserAttributesDataList>(ApiCommand::saveMediaServerUserAttributes))
                    return false;
            }
            if (m_resyncFlags.testFlag(ResyncServers))
            {
                if (!fillTransactionLogInternal<QnUuid, ApiMediaServerData, ApiMediaServerDataList>(ApiCommand::saveMediaServer))
                    return false;
            }
            if (m_resyncFlags.testFlag(ResyncLayouts))
            {
                if (!fillTransactionLogInternal<QnUuid, ApiLayoutData, ApiLayoutDataList>(ApiCommand::saveLayout))
                    return false;
            }
            if (m_resyncFlags.testFlag(ResyncRules))
            {
                if (!fillTransactionLogInternal<QnUuid, ApiBusinessRuleData, ApiBusinessRuleDataList>(ApiCommand::saveEventRule, businessRuleObjectUpdater))
                    return false;
            }
            if (m_resyncFlags.testFlag(ResyncUsers))
            {
                if (!fillTransactionLogInternal<QnUuid, ApiUserData, ApiUserDataList>(ApiCommand::saveUser))
                    return false;
            }
            if (m_resyncFlags.testFlag(ResyncStorages))
            {
                if (!fillTransactionLogInternal<QnUuid, ApiStorageData, ApiStorageDataList>(ApiCommand::saveStorage))
                    return false;
            }
            if (m_resyncFlags.testFlag(ResyncVideoWalls))
            {
                if (!fillTransactionLogInternal<QnUuid, ApiVideowallData, ApiVideowallDataList>(ApiCommand::saveVideowall))
                    return false;
            }
            if (m_resyncFlags.testFlag(ResyncWebPages))
            {
                if (!fillTransactionLogInternal<QnUuid, ApiWebPageData, ApiWebPageDataList>(ApiCommand::saveWebPage))
                    return false;
            }

            if (m_resyncFlags.testFlag(ResyncUserAccessRights))
            {
                if (!rebuildUserAccessRightsTransactions())
                    return false;
                if (!fillTransactionLogInternal<nullptr_t, ApiAccessRightsData, ApiAccessRightsDataList>(ApiCommand::setAccessRights))
                    return false;
            }

            if (m_resyncFlags.testFlag(ResyncUserAccessRights))
            {
                if (!rebuildUserAccessRightsTransactions())
                    return false;
                if (!fillTransactionLogInternal<nullptr_t, ApiAccessRightsData, ApiAccessRightsDataList>(ApiCommand::setAccessRights))
                    return false;
            }

        }

        // Set admin user's password
        QnUserResourcePtr userResource;
        {
            ApiUserDataList users;
            ErrorCode errCode = doQueryNoLock(QnUuid(), users);
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

            userResource = fromApiToResource(*iter, commonModule());
            NX_ASSERT(userResource->isOwner(), Q_FUNC_INFO, "Admin must be admin as it is found by name");
        }

        QString defaultAdminPassword = commonModule()->defaultAdminPassword();
        if ((userResource->getHash().isEmpty() || m_dbJustCreated) && defaultAdminPassword.isEmpty())
        {
            defaultAdminPassword = helpers::kFactorySystemPassword;
            if (m_dbJustCreated)
                commonModule()->setUseLowPriorityAdminPasswordHack(true);
        }


        bool updateUserResource = false;

        if (!defaultAdminPassword.isEmpty())
        {
            if (!userResource->checkLocalUserPassword(defaultAdminPassword) ||
                userResource->getRealm() != nx::network::AppInfo::realm() ||
                !userResource->isEnabled())
            {
                userResource->setPasswordAndGenerateHash(defaultAdminPassword);
                userResource->setEnabled(true);
                updateUserResource = true;
            }
        }

        updateUserResource |= aux::applyRestoreDbData(commonModule()->beforeRestoreDbData(), userResource);

        if (updateUserResource)
        {
            // admin user resource has been updated
            QnTransaction<ApiUserData> userTransaction(
                ApiCommand::saveUser,
                commonModule()->moduleGUID());
            m_tranLog->fillPersistentInfo(userTransaction);
            if (commonModule()->useLowPriorityAdminPasswordHack())
                userTransaction.persistentInfo.timestamp = Timestamp::fromInteger(1); // use hack to declare this change with low proprity in case if admin has been changed in other system (keep other admin user fields unchanged)
            fromResourceToApi(userResource, userTransaction.params);
            executeTransactionNoLock(userTransaction, QnUbjson::serialized(userTransaction));
        }

        QSqlQuery queryCameras(m_sdb);
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
            queryCameras.bindValue(1, commonModule()->moduleGUID().toRfc4122());
        if (!queryCameras.exec()) {
            qWarning() << Q_FUNC_INFO << __LINE__ << queryCameras.lastError();
            NX_ASSERT(0);
            return false;
        }
        while (queryCameras.next())
        {
            QnTransaction<ApiResourceStatusData> tran(
                ApiCommand::setResourceStatus,
                commonModule()->moduleGUID());
            m_tranLog->fillPersistentInfo(tran);
            tran.params.id = QnUuid::fromRfc4122(queryCameras.value(0).toByteArray());
            tran.params.status = Qn::Offline;
            if (executeTransactionNoLock(tran, QnUbjson::serialized(tran)) != ErrorCode::ok)
                return false;
        }

        if (!locker.commit())
            return false;
    } // end of DB update

    m_queryCachePool.reset();
    if (!execSQLScript("vacuum;", m_sdb))
        qWarning() << "failed to vacuum ecs database" << Q_FUNC_INFO;

    m_dbReadOnly = commonModule()->isReadOnly();
    emit initialized();
    m_initialized = true;

    return true;
}

bool QnDbManager::syncLicensesBetweenDB()
{
    // move license table to static DB
    ec2::ApiLicenseDataList licensesMainDbVect;
    ec2::ApiLicenseDataList licensesStaticDbVect;
    if (getLicenses(licensesMainDbVect, m_sdb) != ErrorCode::ok)
        return false;
    if (getLicenses(licensesStaticDbVect, m_sdbStatic) != ErrorCode::ok)
        return false;

    std::set<ApiLicenseData> licensesMainDbSet(licensesMainDbVect.begin(), licensesMainDbVect.end());
    std::set<ApiLicenseData> licensesStaticDbSet(licensesStaticDbVect.begin(), licensesStaticDbVect.end());

    ec2::ApiLicenseDataList addToMain;
    std::set_difference(
        licensesStaticDbSet.begin(),
        licensesStaticDbSet.end(),
        licensesMainDbSet.begin(),
        licensesMainDbSet.end(),
        std::inserter(addToMain, addToMain.begin()));

    ec2::ApiLicenseDataList addToStatic;
    std::set_difference(
        licensesMainDbSet.begin(),
        licensesMainDbSet.end(),
        licensesStaticDbSet.begin(),
        licensesStaticDbSet.end(),
        std::inserter(addToStatic, addToStatic.begin()));


    for (const auto& license : addToMain)
    {
        if (saveLicense(license, m_sdb) != ErrorCode::ok)
            return false;
        m_resyncFlags |= ResyncLicences; //< resync transaction log due to data changes
    }

    for (const auto& license : addToStatic)
    {
        if (saveLicense(license, m_sdbStatic) != ErrorCode::ok)
            return false;
    }
    commonModule()->setDbId(m_dbInstanceId);
    return true;
}


template <typename FilterDataType, class ObjectType, class ObjectListType>
bool QnDbManager::fillTransactionLogInternal(ApiCommand::Value command, std::function<bool (ObjectType& data)> updater)
{
    ObjectListType objects;
    if (doQueryNoLock(FilterDataType(), objects) != ErrorCode::ok)
        return false;

    for(const ObjectType& object: objects)
    {
        QnTransaction<ObjectType> transaction(
            command,
            commonModule()->moduleGUID(),
            object);
        auto transactionDescriptor = ec2::getActualTransactionDescriptorByValue<ObjectType>(command);

        if (transactionDescriptor)
            transaction.transactionType = transactionDescriptor->getTransactionTypeFunc(commonModule(), object, this);
        else
            transaction.transactionType = ec2::TransactionType::Unknown;

        m_tranLog->fillPersistentInfo(transaction);
        if (updater && updater(transaction.params))
        {
            if (executeTransactionInternal(transaction) != ErrorCode::ok)
                return false;
        }

        if (m_tranLog->saveTransaction(transaction) != ErrorCode::ok)
            return false;
    }
    return true;
}

bool QnDbManager::resyncTransactionLog()
{
    if (!fillTransactionLogInternal<QnUuid, ApiUserData, ApiUserDataList>(ApiCommand::saveUser))
        return false;
    if (!fillTransactionLogInternal<QnUuid, ApiMediaServerData, ApiMediaServerDataList>(ApiCommand::saveMediaServer))
        return false;
    if (!fillTransactionLogInternal<QnUuid, ApiMediaServerUserAttributesData, ApiMediaServerUserAttributesDataList>(ApiCommand::saveMediaServerUserAttributes))
        return false;
    if (!fillTransactionLogInternal<QnUuid, ApiCameraData, ApiCameraDataList>(ApiCommand::saveCamera))
        return false;
    if (!fillTransactionLogInternal<QnUuid, ApiCameraAttributesData, ApiCameraAttributesDataList>(ApiCommand::saveCameraUserAttributes))
        return false;
    if (!fillTransactionLogInternal<QnUuid, ApiLayoutData, ApiLayoutDataList>(ApiCommand::saveLayout))
        return false;
    if (!fillTransactionLogInternal<QnUuid, ApiBusinessRuleData, ApiBusinessRuleDataList>(ApiCommand::saveEventRule, businessRuleObjectUpdater))
        return false;
    if (!fillTransactionLogInternal<QnUuid, ApiResourceParamWithRefData, ApiResourceParamWithRefDataList>(ApiCommand::setResourceParam))
        return false;

    if (!fillTransactionLogInternal<QnUuid, ApiStorageData, ApiStorageDataList>(ApiCommand::saveStorage))
        return false;

    if (!fillTransactionLogInternal<nullptr_t, ApiLicenseData, ApiLicenseDataList>(ApiCommand::addLicense))
        return false;

    if (!fillTransactionLogInternal<nullptr_t, ApiStoredFileData, ApiStoredFileDataList>(ApiCommand::addStoredFile))
        return false;

    if (!fillTransactionLogInternal<QnUuid, ApiResourceStatusData, ApiResourceStatusDataList>(ApiCommand::setResourceStatus))
        return false;

    if (!fillTransactionLogInternal<QnUuid, ApiVideowallData, ApiVideowallDataList>(ApiCommand::saveVideowall))
        return false;

    if (!fillTransactionLogInternal<nullptr_t, ApiAccessRightsData, ApiAccessRightsDataList>(ApiCommand::setAccessRights))
        return false;

    if (!fillTransactionLogInternal<QnUuid, ApiUserRoleData, ApiUserRoleDataList>(ApiCommand::saveUserRole))
        return false;

    if (!fillTransactionLogInternal<QnUuid, ApiWebPageData, ApiWebPageDataList>(ApiCommand::saveWebPage))
        return false;

    if (!fillTransactionLogInternal<nullptr_t, ApiLayoutTourData, ApiLayoutTourDataList>(ApiCommand::saveLayoutTour))
        return false;

    if (!fillTransactionLogInternal<QnUuid, ApiDiscoveryData, ApiDiscoveryDataList>(ApiCommand::addDiscoveryInformation))
        return false;

    return true;
}

bool QnDbManager::isInitialized() const
{
    return m_initialized;
}

QMap<int, QnUuid> QnDbManager::getGuidList(
    const QString& request, GuidConversionMethod method, const QByteArray& intHashPostfix )
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

    if (!updateBusinessRulesGuids())
        return false;

    return true;
}

bool QnDbManager::updateBusinessRulesGuids()
{
    // update default rules
    auto guids = getGuidList(
        R"sql(
            SELECT id, id
            FROM vms_businessrule
            WHERE (id between 1 and 19) or (id between 10020 and 10023)
            ORDER BY id
        )sql",
        CM_INT,
        nx::vms::event::Rule::kGuidPostfix);

    if (!updateTableGuids("vms_businessrule", "guid", guids))
        return false;

    // update user's rules
    guids = getGuidList(
        R"sql(
            SELECT id, id
            FROM vms_businessrule
            WHERE guid is null
            ORDER BY id
        )sql",
        CM_INT,
        QnUuid::createUuid().toByteArray());
    if (!updateTableGuids("vms_businessrule", "guid", guids))
        return false;

    return true;
}

bool QnDbManager::updateBusinessActionParameters()
{
    QHash<QString, QString> remapTable;
    remapTable["quality"] = "streamQuality";
    remapTable["duration"] = "durationMs";
    remapTable["recordingDuration"] = "durationMs";
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

        while (query.next())
        {
            qint32 id = query.value(0).toInt();
            QByteArray data = query.value(1).toByteArray();

            QJsonValue result;
            QJson::deserialize(data, &result);
            QJsonObject values = result.toObject(); /* Returns empty object in case of deserialization error. */
            if (values.isEmpty())
                continue;

            QJsonObject remappedValues;
            for (const QString &key: values.keys())
            {
                if (remapTable.contains(key))
                {
                    QString remappedKey = remapTable[key];
                    remappedValues[remappedKey] = values[key];
                    if (remappedKey == lit("durationMs"))
                    {
                        // Convert seconds to milliseconds.
                        int valueMs = remappedValues[remappedKey].toInt() * 1000;
                        remappedValues[remappedKey] = QString::number(valueMs);
                    }
                }
                else
                {
                    remappedValues[key] = values[key];
                }
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

bool QnDbManager::beforeInstallUpdate(const QString& updateName)
{
    if (updateName.endsWith(lit("/33_history_refactor_dummy.sql")))
        return removeOldCameraHistory();

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
        QnUuid hash = transactionHash(ApiCommand::removeResourceStatus, data);
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
        migration::legacy::QnAbstractTransactionV1 abstractTran;
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
        migration::legacy::QnAbstractTransactionV1 abstractTran;
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
        if (data.name == lit("supportedMotion") && (data.value.isEmpty() || data.value.toInt() == 1))
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

bool QnDbManager::cleanupDanglingDbObjects()
{
    const QString kCleanupScript(":/updates/68_cleanup_db.sql");
    return nx::utils::db::SqlQueryExecutionHelper::execSQLFile(kCleanupScript, m_sdb);
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
        migration::legacy::QnAbstractTransactionV1 abstractTran;
        QnUuid tranGuid = QnSql::deserialized_field<QnUuid>(query.value(0));
        QByteArray srcData = query.value(1).toByteArray();
        QnUbjsonReader<QByteArray> stream(&srcData);
        if (!QnUbjson::deserialize(&stream, &abstractTran)) {
            qWarning() << Q_FUNC_INFO << "Can' deserialize transaction from transaction log";
            return false;
        }
        if (abstractTran.command == ApiCommand::resetEventRules || abstractTran.command == ApiCommand::saveEventRule)
        {
            delQuery.addBindValue(QnSql::serialized_field(tranGuid));
            if (!delQuery.exec()) {
                qWarning() << Q_FUNC_INFO << query.lastError().text();
                return false;
            }
        }
    }
    return true;
}

bool QnDbManager::encryptKvPairs()
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    QString queryStr = "SELECT rowid, value, name FROM vms_kvpair";

    if(!query.prepare(queryStr))
    {
        NX_LOG(lit("Could not prepare query %1: %2").arg(queryStr).arg(query.lastError().text()), cl_logERROR);
        return false;
    }

    if (!query.exec())
    {
        NX_LOG(lit("Could not execute query %1: %2").arg(queryStr).arg(query.lastError().text()), cl_logERROR);
        return false;
    }

    QSqlQuery insQuery(m_sdb);
    QString insQueryString = "UPDATE vms_kvpair SET value = :value WHERE rowid = :rowid";

    while (query.next())
    {
        int rowid     = query.value(0).toInt();
        QString value = query.value(1).toString();
        QString name  = query.value(2).toString();

        bool allowed = ec2::access_helpers::kvSystemOnlyFilter(
            ec2::access_helpers::Mode::read,
            Qn::UserAccessData(),
            name);

        if (!allowed) // need encrypt
        {
            value = nx::utils::encodeHexStringFromStringAES128CBC(value);

            insQuery.prepare(insQueryString);

            insQuery.bindValue(":name",  name);
            insQuery.bindValue(":value", value);
            insQuery.bindValue(":rowid", rowid);

            if (!insQuery.exec())
            {
                NX_LOG(lit("Could not execute query %1: %2").arg(insQueryString).arg(insQuery.lastError().text()), cl_logERROR);
                return false;
            }
        }
    }

    return true;
}

bool QnDbManager::afterInstallUpdate(const QString& updateName)
{
    if (updateName.endsWith(lit("/07_videowall.sql")))
    {
        // TODO: #GDM move to migration?
        auto guids = getGuidList("SELECT rt.id, rt.name || '-' as guid from vms_resourcetype rt WHERE rt.name == 'Videowall'", CM_MakeHash);
        return updateTableGuids("vms_resourcetype", "guid", guids);
    }

    if (updateName.endsWith(lit("/17_add_isd_cam.sql")))
        return updateResourceTypeGuids();

    if (updateName.endsWith(lit("/20_adding_camera_user_attributes.sql")))
        return resyncIfNeeded(ResyncLog);

    if (updateName.endsWith(lit("/21_business_action_parameters.sql")))
        return updateBusinessActionParameters();

    if (updateName.endsWith(lit("/21_new_dw_cam.sql")))
        return updateResourceTypeGuids();

    if (updateName.endsWith(lit("/24_insert_default_stored_files.sql")))
        return addStoredFiles(lit(":/vms_storedfiles/"));

    if (updateName.endsWith(lit("/27_remove_server_status.sql")))
        return removeServerStatusFromTransactionLog();

    if (updateName.endsWith(lit("/31_move_group_name_to_user_attrs.sql")))
        return resyncIfNeeded(ResyncCameraAttributes);

    if (updateName.endsWith(lit("/32_default_business_rules.sql")))
    {
        // TODO: #GDM move to migration
        for (const auto& rule: vms::event::Rule::getSystemRules())
        {
            ApiBusinessRuleData ruleData;
            fromResourceToApi(rule, ruleData);
            if (updateBusinessRule(ruleData) != ErrorCode::ok)
                return false;
        }
        return resyncIfNeeded(ResyncRules);
    }

    if (updateName.endsWith(lit("/33_resync_layout.sql")))
        return resyncIfNeeded(ResyncLayouts);

    if (updateName.endsWith(lit("/35_fix_onvif_mt.sql")))
        return removeWrongSupportedMotionTypeForONVIF();

    if (updateName.endsWith(lit("/36_fix_brules.sql")))
        return fixBusinessRules() && resyncIfNeeded(ResyncRules);

    if (updateName.endsWith(lit("/37_remove_empty_layouts.sql")))
        return removeEmptyLayoutsFromTransactionLog();

    if (updateName.endsWith(lit("/41_resync_tran_log.sql")))
        return resyncIfNeeded({ResyncUsers, ResyncStorages});

    if (updateName.endsWith(lit("/42_add_license_to_user_attr.sql")))
        return resyncIfNeeded(ResyncCameraAttributes);

    if (updateName.endsWith(lit("/43_add_business_rules.sql")))
    {
        // TODO: #GDM move to migration
        for (const auto& rule: vms::event::Rule::getRulesUpd43())
        {
            ApiBusinessRuleData ruleData;
            fromResourceToApi(rule, ruleData);
            if (updateBusinessRule(ruleData) != ErrorCode::ok)
                return false;
        }
        return resyncIfNeeded(ResyncRules);
    }

    if (updateName.endsWith(lit("/48_add_business_rules.sql")))
    {
        // TODO: #GDM move to migration
        for (const auto& rule: vms::event::Rule::getRulesUpd48())
        {
            ApiBusinessRuleData ruleData;
            fromResourceToApi(rule, ruleData);
            if (updateBusinessRule(ruleData) != ErrorCode::ok)
                return false;
        }
        return resyncIfNeeded(ResyncRules);
    }

    if (updateName.endsWith(lit("/43_resync_client_info_data.sql")))
        return resyncIfNeeded(ResyncClientInfo);

    if (updateName.endsWith(lit("/44_upd_brule_format.sql")))
        return resyncIfNeeded(ResyncRules);

    if (updateName.endsWith(lit("/49_fix_migration.sql")))
        return resyncIfNeeded({ResyncCameraAttributes, ResyncServerAttributes});

    if (updateName.endsWith(lit("/49_add_webpage_table.sql")))
    {
        QMap<int, QnUuid> guids = getGuidList("SELECT rt.id, rt.name || '-' as guid from vms_resourcetype rt WHERE rt.name == 'WebPage'", CM_MakeHash);
        return updateTableGuids("vms_resourcetype", "guid", guids);
    }

    if (updateName.endsWith(lit("/50_add_access_rights.sql")))
        return resyncIfNeeded(ResyncUsers);

    if (updateName.endsWith(lit("/50_fix_migration.sql")))
        return resyncIfNeeded({ResyncRules, ResyncUsers});

    if (updateName.endsWith(lit("/51_http_business_action.sql")))
        return resyncIfNeeded(ResyncRules);

    if (updateName.endsWith(lit("/53_reset_ldap_users.sql")))
        return resyncIfNeeded(ResyncUsers);

    if (updateName.endsWith(lit("/54_migrate_permissions.sql")))
        return ec2::db::migrateV25UserPermissions(m_sdb) && resyncIfNeeded(ResyncUsers);

    if (updateName.endsWith(lit("/55_migrate_accessible_resources.sql")))
        return ec2::db::migrateAccessibleResources(m_sdb) && resyncIfNeeded(ResyncUsers);

    if (updateName.endsWith(lit("/56_remove_layout_editable.sql")))
        return resyncIfNeeded(ResyncLayouts);

    if (updateName.endsWith(lit("/58_migrate_camera_output_action.sql")))
        return ec2::db::migrateRulesToV30(m_sdb) && resyncIfNeeded(ResyncRules);

    if (updateName.endsWith(lit("/52_fix_onvif_mt.sql")))
        return removeWrongSupportedMotionTypeForONVIF(); // TODO: #rvasilenko consistency break

    if (updateName.endsWith(lit("/65_transaction_log_add_fields.sql")))
        return resyncIfNeeded(ResyncLog);

    if (updateName.endsWith(lit("/68_add_transaction_type.sql")))
        return migration::addTransactionType::migrate(&m_sdb); // TODO: #rvasilenko namespaces consistency break

    if (updateName.endsWith(lit("/68_cleanup_db.sql")))
        return resyncIfNeeded({ClearLog, ResyncLog});

    if (updateName.endsWith(lit("/70_make_transaction_timestamp_128bit.sql")))
        return migration::timestamp128bit::migrate(&m_sdb); // TODO: #rvasilenko namespaces consistency break

    if (updateName.endsWith(lit("/73_encrypt_kvpairs.sql")))
        return encryptKvPairs();

    if (updateName.endsWith(lit("/74_remove_server_deprecated_columns.sql")))
        return resyncIfNeeded(ResyncServers);

    if (updateName.endsWith(lit("/75_add_user_id_to_tran.sql")))
        return migration::add_history::migrate(&m_sdb);

    if (updateName.endsWith(lit("/76_rename_discovery_attribute.sql")))
        return resyncIfNeeded({ClearLog, ResyncLog});

    if (updateName.endsWith(lit("/77_fix_custom_permission_flag.sql")))
        return ec2::db::fixCustomPermissionFlag(m_sdb) && resyncIfNeeded(ResyncUsers);

    if (updateName.endsWith(lit("/81_changed_status_stransaction_hash.sql")))
        return resyncIfNeeded({ClearLog, ResyncLog});

    if (updateName.endsWith(lit("/82_optera_trash_cleanup.sql")))
    {
        if (!m_dbJustCreated)
            cleanupDanglingDbObjects();
        return resyncIfNeeded({ClearLog, ResyncLog});
    }

    if (updateName.endsWith(lit("/85_add_default_webpages.sql")))
    {
        return ec2::database::migrations::addDefaultWebpages(&m_resourceQueries)
            && resyncIfNeeded(ResyncWebPages);
    }

    if (updateName.endsWith(lit("/86_fill_cloud_user_digest.sql")))
        return resyncIfNeeded(ResyncUsers);

    if (updateName.endsWith(lit("/87_migrate_videowall_layouts.sql")))
    {
        return ec2::database::migrations::reparentVideoWallLayouts(&m_resourceQueries)
            && resyncIfNeeded({ResyncLayouts, ResyncVideoWalls});
    }
    if (updateName.endsWith(lit("/92_rename_recording_param_name.sql")))
        return updateBusinessActionParameters();

    if (updateName.endsWith(lit("/93_migrate_show_popup_action.sql")))
        return ec2::db::migrateRulesToV31Alpha(m_sdb) && resyncIfNeeded(ResyncRules);

    if (updateName.endsWith(lit("/94_migrate_business_actions_all_users.sql")))
        return ec2::db::migrateActionsAllUsers(m_sdb) && resyncIfNeeded(ResyncRules);

    if (updateName.endsWith(lit("/95_migrate_business_events_all_users.sql")))
        return ec2::db::migrateEventsAllUsers(m_sdb) && resyncIfNeeded(ResyncRules);

    if (updateName.endsWith(lit("/99_20170802_cleanup_client_info_list.sql")))
        return ec2::db::cleanupClientInfoList(m_sdb);

    if (updateName.endsWith(lit("/99_20170928_update_business_rules_guids.sql")))
    {
        return fixDefaultBusinessRuleGuids() && resyncIfNeeded(ResyncRules);
    }

    if (updateName.endsWith(lit("/99_20170926_refactor_user_access_rights.sql")))
        return ec2::db::migrateAccessRightsToUbjsonFormat(m_sdb, this) && resyncIfNeeded(ResyncUserAccessRights);

    NX_LOG(lit("SQL update %1 does not require post-actions.").arg(updateName), cl_logDEBUG1);
    return true;
}

bool QnDbManager::fixDefaultBusinessRuleGuids()
{
    QSqlQuery query(m_sdb);
    QString queryStr(lit("SELECT 1 FROM vms_businessrule WHERE guid = :guid"));
    if (!prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    QSqlQuery updQuery(m_sdb);
    QString udpQueryStr(lit("\
        UPDATE vms_businessrule \
        SET guid = :newValue  \
        WHERE guid = :oldValue"));
    if (!prepareSQLQuery(&updQuery, udpQueryStr, Q_FUNC_INFO))
        return false;

    QSqlQuery delQuery(m_sdb);
    QString delQueryStr(lit("DELETE FROM vms_businessrule where guid = :guid"));
    if (!prepareSQLQuery(&delQuery, delQueryStr, Q_FUNC_INFO))
        return false;

    auto remapTable = nx::vms::event::Rule::remappedGuidsToFix();
    for (auto itr = remapTable.begin(); itr != remapTable.end(); ++itr)
    {
        auto oldValue = itr.key();
        auto newValue = itr.value();

        query.addBindValue(QnSql::serialized_field(newValue));
        if (!execSQLQuery(&query, Q_FUNC_INFO))
            return false;
        const bool isNewValueExists = query.next();
        if (isNewValueExists)
        {
            delQuery.addBindValue(QnSql::serialized_field(oldValue));
            if (!execSQLQuery(&delQuery, Q_FUNC_INFO))
                return false;
        }
        else
        {
            updQuery.addBindValue(QnSql::serialized_field(newValue));
            updQuery.addBindValue(QnSql::serialized_field(oldValue));
            if (!execSQLQuery(&updQuery, Q_FUNC_INFO))
                return false;
        }
    }

    return true;
}

bool QnDbManager::createDatabase()
{
    QnDbTransactionLocker lock(m_tran.get());

    m_dbJustCreated = false;

    if (!isObjectExists(lit("table"), lit("vms_resource"), m_sdb))
    {
        NX_LOG(lit("Create new database"), cl_logINFO);

        m_dbJustCreated = true;

        if (!execSQLFile(lit(":/01_createdb.sql"), m_sdb))
            return false;

        if (!execSQLFile(lit(":/02_insert_all_vendors.sql"), m_sdb))
            return false;
    }

    if (!isObjectExists(lit("table"), lit("transaction_log"), m_sdb))
    {
        NX_LOG(lit("Update database to v 2.3"), cl_logDEBUG1);

        if (!execSQLFile(lit(":/00_update_2.2_stage0.sql"), m_sdb))
            return false;

        if (!ec2::db::migrateRulesToV23(m_sdb))
            return false;

        if (!m_dbJustCreated)
        {
            m_resyncFlags |= ResyncLog;
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

    if (!applyUpdates(":/updates"))
    {
        NX_LOG(lit("%1 Applying migration updates failed").arg(Q_FUNC_INFO), cl_logWARNING);
        return false;
    }

    NX_LOG(lit("%1 Applying migration updates succeded").arg(Q_FUNC_INFO), cl_logDEBUG2);

    if (!lockStatic.commit())
        return false;
#ifdef DB_DEBUG
    qDebug() << "database created successfully";
#endif // DB_DEBUG
    return lock.commit();
}

QnDbManager::~QnDbManager()
{
    if (m_sdbStatic.isOpen())
    {
        m_sdbStatic = QSqlDatabase();
        QSqlDatabase::removeDatabase(getDatabaseName("QnDbManagerStatic"));
    }
}

ErrorCode QnDbManager::insertAddParam(const ApiResourceParamWithRefData& param)
{
    const auto query = m_insertKvPairQuery.get(m_sdb,
        "INSERT OR REPLACE INTO vms_kvpair(resource_guid, name, value) VALUES(?, ?, ?)");

    query->bindValue(0, QnSql::serialized_field(param.resourceId));
    query->bindValue(1, QnSql::serialized_field(param.name));
    query->bindValue(2, QnSql::serialized_field(param.value));

    if (!query->exec())
    {
        qWarning() << Q_FUNC_INFO << query->lastError().text();
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
        NX_LOG( lit("Could not prepare query %1: %2").arg(queryStr).arg(query.lastError().text()), cl_logERROR);
        return ErrorCode::dbError;
    }

    if( !resID.isNull() )
        query.bindValue(QLatin1String(":guid"), resID.toRfc4122());
    if( !resParentID.isNull() )
        query.bindValue(QLatin1String(":parentGuid"), resParentID.toRfc4122());
    if (!query.exec())
    {
        NX_LOG( lit("DB error at %1: %2").arg(Q_FUNC_INFO).arg(query.lastError().text()), cl_logERROR);
        return ErrorCode::dbError;
    }

    QnSql::fetch_many(query, &params);

    return ErrorCode::ok;
}

qint32 QnDbManager::getResourceInternalId( const QnUuid& guid )
{
    return database::api::getResourceInternalId(&m_resourceQueries, guid);
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
    if (!database::api::insertOrReplaceResource(&m_resourceQueries, data, internalId))
        return ErrorCode::dbError;
    return ErrorCode::ok;
}

ErrorCode QnDbManager::insertOrReplaceUser(const ApiUserData& data, qint32 internalId)
{
    {
        const QString authQueryStr =
            R"(
            INSERT OR REPLACE
            INTO auth_user
            (id, username, is_superuser, email,
                password,
                is_staff, is_active, last_login, date_joined, first_name, last_name)
            VALUES
            (:internalId, :name, :isAdmin, :email,
            coalesce(:hash, coalesce((SELECT password FROM auth_user WHERE id = :internalId), '')),
            1, 1, '', '', '', '')
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
            (user_id, resource_ptr_id, digest, crypt_sha512_hash, realm, rights, is_ldap, is_enabled, user_role_guid, is_cloud, full_name)
            VALUES
            (:internalId, :internalId, :digest, :cryptSha512Hash, :realm, :permissions, :isLdap, :isEnabled, :userRoleId, :isCloud, :fullName)
        )";

        QSqlQuery profileQuery(m_sdb);
        if (!prepareSQLQuery(&profileQuery, profileQueryStr, Q_FUNC_INFO))
            return ErrorCode::dbError;

        QnSql::bind(data, &profileQuery);
        if (data.isLdap && data.isCloud)
        {
            NX_LOG(lit("Warning at %1: user data has both LDAP and cloud flags set; cloud flag will be ignored. Internal id=%2").arg(Q_FUNC_INFO).arg(internalId), cl_logWARNING);
            profileQuery.bindValue(":isCloud", false);
        }

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

ErrorCode QnDbManager::insertOrReplaceUserRole(const ApiUserRoleData& data)
{
    QSqlQuery query(m_sdb);
    const QString queryStr = R"(
        INSERT OR REPLACE INTO vms_user_roles
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
    const auto query = m_insertCameraQuery.get(m_sdb, R"sql(
        INSERT OR REPLACE INTO vms_camera (
            vendor, manually_added, resource_ptr_id, group_name, group_id,
            mac, model, status_flags, physical_id
        ) VALUES (
            :vendor, :manuallyAdded, :internalId, :groupName, :groupId,
            :mac, :model, :statusFlags, :physicalId
        ))sql");

    QnSql::bind(data, query.get());
    query->bindValue(":internalId", internalId);
    if (query->exec())
    {
        return ErrorCode::ok;
    }
    else
    {
        qWarning() << Q_FUNC_INFO << query->lastError().text();
        return ErrorCode::dbError;
    }
}

ErrorCode QnDbManager::removeCameraAttributes(const QnUuid& id)
{
    QSqlQuery delQuery(m_sdb);
    delQuery.prepare("DELETE FROM vms_camera_user_attributes where camera_guid = ?");
    delQuery.addBindValue(id.toRfc4122());
    if (!delQuery.exec()) {
        qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
        return ErrorCode::dbError;
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::insertOrReplaceCameraAttributes(const ApiCameraAttributesData& data, qint32* const internalId)
{
    const auto query = m_insertCameraUserAttrQuery.get(m_sdb, R"sql(
        INSERT OR REPLACE INTO vms_camera_user_attributes (
            camera_guid,
            camera_name,
            group_name,
            audio_enabled,
            control_enabled,
            region,
            schedule_enabled,
            motion_type,
            secondary_quality,
            dewarping_params,
            min_archive_days,
            max_archive_days,
            preferred_server_id,
            license_used,
            failover_priority,
            backup_type
        ) VALUES (
            :cameraId,
            :cameraName,
            :userDefinedGroupName,
            :audioEnabled,
            :controlEnabled,
            :motionMask,
            :scheduleEnabled,
            :motionType,
            :secondaryStreamQuality,
            :dewarpingParams,
            :minArchiveDays,
            :maxArchiveDays,
            :preferredServerId,
            :licenseUsed,
            :failoverPriority,
            :backupType
        ))sql");

    QnSql::bind(data, query.get());
    if( !query->exec() )
    {
        NX_LOG( lit("DB error in %1: %2").arg(Q_FUNC_INFO).arg(query->lastError().text()), cl_logERROR);
        return ErrorCode::dbError;
    }

    *internalId = query->lastInsertId().toInt();
    return ErrorCode::ok;
}

ErrorCode QnDbManager::insertOrReplaceMediaServer(const ApiMediaServerData& data, qint32 internalId)
{
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("\
        INSERT OR REPLACE INTO vms_server (auth_key, version, net_addr_list, system_info, flags, resource_ptr_id) \
        VALUES (:authKey, :version, :networkAddresses, :systemInfo, :flags, :internalId)\
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
    if (tran.params.parentId.isNull())
    {
        // TODO: Improve error reporting. Currently HTTP 500 with empty error text is returned.
        NX_LOG(lit("saveStorage: parentId is null"), cl_logERROR);
        return ErrorCode::unsupported;
    }

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

    const auto query = m_insertCameraScheduleQuery.get(m_sdb, R"sql(
        INSERT INTO vms_scheduletask (
            camera_attrs_id, start_time, end_time, do_record_audio, record_type,
            day_of_week, before_threshold, after_threshold, stream_quality, fps
        ) VALUES (
            :internalId, :startTime, :endTime, :recordAudio, :recordingType,
            :dayOfWeek, :beforeThreshold, :afterThreshold, :streamQuality, :fps
        ))sql");

    query->bindValue(":internalId", internalId);
    for(const ApiScheduleTaskData& task: scheduleTasks)
    {
        QnSql::bind(task, query.get());
        if (!query->exec())
        {
            qWarning() << Q_FUNC_INFO << query->lastError().text();
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

    QSqlDatabase testDB = QSqlDatabase::addDatabase("QSQLITE", getDatabaseName("QnDbManagerTmp"));
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
    if (params.physicalId.isEmpty())
        return ec2::ErrorCode::forbidden;

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
            :serverId,                                           \
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
        NX_LOG( lit("DB Error at %1: %2").arg(Q_FUNC_INFO).arg(insQuery.lastError().text()), cl_logERROR);
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
        NX_LOG( lit("DB Error at %1: %2").arg(Q_FUNC_INFO).arg(query.lastError().text()), cl_logERROR);
        return ErrorCode::dbError;
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

ErrorCode QnDbManager::removeUser( const QnUuid& guid )
{
    qint32 internalId = getResourceInternalId(guid);

    ErrorCode err = ErrorCode::ok;

    err = deleteUserProfileTable(internalId);
    if (err != ErrorCode::ok)
        return err;

    err = deleteTableRecord(internalId, "auth_user", "id");
    if (err != ErrorCode::ok)
        return err;

    err = deleteRecordFromResourceTable(internalId);
    return ErrorCode::ok;
}

ErrorCode QnDbManager::removeUserRole(const QnUuid& userRoleId)
{
    /* Cleanup all users having this role. */
    {
        QSqlQuery query(m_sdb);
        const QString queryStr("UPDATE vms_userprofile SET user_role_guid = NULL WHERE user_role_guid = ?");
        if (!prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
            return ErrorCode::dbError;

        query.addBindValue(userRoleId.toRfc4122());
        if (!execSQLQuery(&query, Q_FUNC_INFO))
            return ErrorCode::dbError;
    }

    /* Cleanup user role shared resources. */
    auto err = cleanAccessRights(userRoleId);
    if (err != ErrorCode::ok)
        return err;

    return deleteTableRecord(userRoleId, "vms_user_roles", "id");
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
    if (!database::api::saveLayout(&m_resourceQueries, params))
        return ErrorCode::dbError;
    return ErrorCode::ok;
}

ec2::ErrorCode QnDbManager::removeLayoutTour(const QnUuid& id)
{
    if (!database::api::removeLayoutTour(m_sdb, id))
        return ErrorCode::dbError;
    return ErrorCode::ok;
}

ec2::ErrorCode QnDbManager::saveLayoutTour(const ApiLayoutTourData& params)
{
    if (!database::api::saveLayoutTour(m_sdb, params))
        return ErrorCode::dbError;
    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiLayoutData>& tran)
{
    return saveLayout(tran.params);
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

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiLayoutTourData>& tran)
{
    return saveLayoutTour(tran.params);
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
        return removeParam(tran.params);
    else
        return ErrorCode::notImplemented;
}

ErrorCode QnDbManager::removeParam(const ApiResourceParamWithRefData& data)
{
    QSqlQuery query(m_sdb);
    query.prepare("DELETE FROM vms_kvpair WHERE resource_guid = :id AND name = :name");
    query.bindValue(lit(":id"), data.resourceId.toRfc4122());
    query.bindValue(lit(":name"), data.name);
    if (!query.exec())
    {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }
    return ErrorCode::ok;
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
    return database::api::removeLayout(&m_resourceQueries, id)
        ? ErrorCode::ok
        : ErrorCode::dbError;
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
    if (data.resourceIds.empty())
        return cleanAccessRights(data.userId);

    const auto userOrRoleId = data.userId.toRfc4122();

    const QString insertQueryString = R"sql(
            INSERT OR REPLACE
            INTO vms_access_rights
            (userOrRoleId, resourceIds)
            values
           (:userOrRoleId, :resourceIds)
        )sql";

    QSqlQuery insertQuery(m_sdb);
    insertQuery.setForwardOnly(true);
    if (!prepareSQLQuery(&insertQuery, insertQueryString, Q_FUNC_INFO))
        return ErrorCode::dbError;

    insertQuery.addBindValue(QnSql::serialized_field(data.userId));
    insertQuery.addBindValue(QnUbjson::serialized(data.resourceIds));

    if (!execSQLQuery(&insertQuery, Q_FUNC_INFO))
        return ErrorCode::dbError;

    return ErrorCode::ok;
}

ec2::ErrorCode QnDbManager::cleanAccessRights(const QnUuid& userOrRoleId)
{
    QSqlQuery query(m_sdb);
    QString queryStr
    (R"(
        DELETE FROM vms_access_rights
        WHERE userOrRoleId = :userOrRoleId;
     )");

    if (!prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return ErrorCode::dbError;

    query.bindValue(":userOrRoleId", userOrRoleId.toRfc4122());
    if (!execSQLQuery(&query, Q_FUNC_INFO))
        return ErrorCode::dbError;

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

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiUserRoleData>& tran)
{
    NX_ASSERT(tran.command == ApiCommand::saveUserRole, Q_FUNC_INFO, "Unsupported transaction");
    if (tran.command != ApiCommand::saveUserRole)
        return ec2::ErrorCode::serverError;
    return insertOrReplaceUserRole(tran.params);
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

namespace {
QString getObjectInfoSelectString(const QString& objType, const QString& objTable)
{
    return lit("SELECT :%1, r.guid \
                FROM %2 o JOIN vms_resource r  \
                    on r.id = o.resource_ptr_id WHERE r.parent_guid = :guid")
                .arg(objType)
                .arg(objTable);
}

QString getMultiObjectsInfoSelectString(const QStringList& selectStrings)
{
    QString result;
    for (int i = 0; i < selectStrings.size(); ++i)
    {
        result.append(selectStrings[i]);
        if (i != selectStrings.size() - 1)
            result.append(lit(" UNION "));
    }

    return result;
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
            query.prepare(
                getMultiObjectsInfoSelectString(
                      QStringList() << getObjectInfoSelectString(lit("cameraObjType"), lit("vms_camera"))
                                    << getObjectInfoSelectString(lit("storageObjType"), lit("vms_storage"))
                                    << getObjectInfoSelectString(lit("layoutObjType"), lit("vms_layout"))));
            query.bindValue(":cameraObjType", (int)ApiObject_Camera);
            query.bindValue(":storageObjType", (int)ApiObject_Storage);
            query.bindValue(":layoutObjType", (int)ApiObject_Layout);
            break;
        case ApiObject_Videowall:
        case ApiObject_User:
            query.prepare(getObjectInfoSelectString(lit("objType"), lit("vms_layout")));
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

ErrorCode QnDbManager::saveMiscParam(const ApiMiscData &params)
{
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT OR REPLACE INTO misc_data (key, data) values (?,?)");
    insQuery.addBindValue( params.name );
    insQuery.addBindValue( params.value );
    if( !insQuery.exec() )
        return ErrorCode::dbError;
    return ErrorCode::ok;
}

bool QnDbManager::readMiscParam( const QByteArray& name, QByteArray* value )
{
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
    case ApiCommand::removeLayoutTour:
        return removeLayoutTour(tran.params.id);
    case ApiCommand::removeEventRule:
        return removeBusinessRule(tran.params.id);
    case ApiCommand::removeUser:
        return removeUser(tran.params.id);
    case ApiCommand::removeUserRole:
        return removeUserRole(tran.params.id);
    case ApiCommand::removeVideowall:
        return removeVideowall(tran.params.id);
    case ApiCommand::removeWebPage:
        return removeWebPage(tran.params.id);
    case ApiCommand::removeCameraUserAttributes:
        return removeCameraAttributes(tran.params.id);
    case ApiCommand::removeAccessRights:
        return cleanAccessRights(tran.params.id);
    case ApiCommand::removeResourceStatus:
        return removeResourceStatus(tran.params.id);
    default:
        return removeObject(ApiObjectInfo(getObjectTypeNoLock(tran.params.id), tran.params.id));
    }
}

ErrorCode QnDbManager::removeResourceStatus(const QnUuid& id)
{
    QSqlQuery removeQuery(m_sdb);
    QString removeQueryStr("DELETE FROM vms_resource_status WHERE guid = :resourceId");

    if (!prepareSQLQuery(&removeQuery, removeQueryStr, Q_FUNC_INFO))
        return ErrorCode::dbError;

    removeQuery.bindValue(":resourceId", QnSql::serialized_field(id));
    if (!execSQLQuery(&removeQuery, Q_FUNC_INFO))
        return ErrorCode::dbError;

    return ErrorCode::ok;
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
    const auto cameraTypesDir = lit("/resources/camera_types");
    const auto nameFilters = QStringList{lit("*.xml")};

    const auto qrcDir = QDir(lit(":") + cameraTypesDir);
    for(const auto& fileInfo: qrcDir.entryInfoList(nameFilters, QDir::Files))
        loadResourceTypeXML(fileInfo.absoluteFilePath(), data);

    const auto applicationDir = QDir(QCoreApplication::applicationDirPath() + cameraTypesDir);
    for(const auto& fileInfo: applicationDir.entryInfoList(nameFilters, QDir::Files))
        loadResourceTypeXML(fileInfo.absoluteFilePath(), data);
}

ErrorCode QnDbManager::doQueryNoLock(const QByteArray &name, ApiMiscData& miscData)
{
    if (!readMiscParam(name, &miscData.value))
        return ErrorCode::dbError;
    miscData.name = name;
    return ErrorCode::ok;
}

ErrorCode QnDbManager::doQueryNoLock(nullptr_t /*dummy*/, ApiResourceParamDataList& data)
{
    return readSettings(data);
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

/**
 * /ec2/getLayouts
 */
ErrorCode QnDbManager::doQueryNoLock(const QnUuid& id, ApiLayoutDataList& layouts)
{
    if (!database::api::fetchLayouts(m_sdb, id, layouts))
        return ErrorCode::dbError;
    return ErrorCode::ok;
}

/**
 * /ec2/getCameras
 */
ErrorCode QnDbManager::doQueryNoLock(const QnUuid& id, ApiCameraDataList& cameraList)
{
    QString filterStr;
    if (!id.isNull())
        filterStr = QString("WHERE r.guid = %1").arg(guidToSqlString(id));

    QSqlQuery queryCameras(m_sdb);
    queryCameras.setForwardOnly(true);
    queryCameras.prepare(lit(" \
        SELECT r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentId, r.name, r.url, \
        c.vendor, c.manually_added as manuallyAdded, \
        c.group_name as groupName, c.group_id as groupId, c.mac, c.model, \
        c.status_flags as statusFlags, c.physical_id as physicalId \
        FROM vms_resource r \
        LEFT JOIN vms_resource_status rs on rs.guid = r.guid \
        JOIN vms_camera c on c.resource_ptr_id = r.id %1 ORDER BY r.guid \
    ").arg(filterStr));

    if (!queryCameras.exec()) {
        qWarning() << Q_FUNC_INFO << queryCameras.lastError().text();
        return ErrorCode::dbError;
    }
    QnSql::fetch_many(queryCameras, &cameraList);

    return ErrorCode::ok;
}

/**
 * /ec2/getResourceStatus
 */
ErrorCode QnDbManager::doQueryNoLock(const QnUuid& resId, ApiResourceStatusDataList& statusList)
{
    QString filterStr;
    if (!resId.isNull())
        filterStr = QString("WHERE guid = %1").arg(guidToSqlString(resId));

    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare(lit("SELECT guid as id, status from vms_resource_status %1 ORDER BY guid")
        .arg(filterStr));

    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }

    QnSql::fetch_many(query, &statusList);

    return ErrorCode::ok;
}

ec2::ErrorCode QnDbManager::doQueryNoLock(const QnUuid& /*id*/, ApiUpdateUploadResponceDataList& data)
{
    data.clear();
    return ErrorCode::ok;
}

ErrorCode QnDbManager::getStorages(const QString& filterStr, ApiStorageDataList& storageList)
{
    QSqlQuery queryStorage(m_sdb);
    queryStorage.setForwardOnly(true);
    queryStorage.prepare(lm(R"sql(
        SELECT r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentId, r.name,
            r.url, s.space_limit as spaceLimit, s.used_for_writing as usedForWriting,
            s.storage_type as storageType, s.backup as isBackup
        FROM vms_resource r
        JOIN vms_storage s on s.resource_ptr_id = r.id
        %1
        ORDER BY r.guid
    )sql").arg(filterStr));

    if (!queryStorage.exec())
    {
        qWarning() << Q_FUNC_INFO << queryStorage.lastError().text();
        return ErrorCode::dbError;
    }

    QnSql::fetch_many(queryStorage, &storageList);

    QnQueryFilter filter;
    filter.fields.insert(RES_TYPE_FIELD, RES_TYPE_STORAGE);

    ApiResourceParamWithRefDataList params;
    const auto result = fetchResourceParams(filter, params);
    if (result != ErrorCode::ok)
        return result;

    mergeObjectListData<ApiStorageData>(
        storageList, params,
        &ApiStorageData::addParams,
        &ApiResourceParamWithRefData::resourceId);

    // Storages are generally bound to mediaservers, so it's required to be sorted by parent id.
    std::sort(storageList.begin(), storageList.end(),
        [](const ApiStorageData& lhs, const ApiStorageData& rhs)
        {
            return lhs.parentId.toRfc4122() < rhs.parentId.toRfc4122();
        });

    return ErrorCode::ok;
}

/**
 * /ec2/getStorages: get storages filtered by parentServerId.
 */
ErrorCode QnDbManager::doQueryNoLock(
    const ParentId& parentId, ApiStorageDataList& storageList)
{
    QString filterStr;
    if (!parentId.id.isNull())
        filterStr = QString("WHERE r.parent_guid = %1").arg(guidToSqlString(parentId.id));

    return getStorages(filterStr, storageList);
}

/**
 * Used for API Merge by /ec2/saveStorages: get storage by id.
 */
ErrorCode QnDbManager::doQueryNoLock(const QnUuid& storageId, ApiStorageDataList& storageList)
{
    QString filterStr;
    if (!storageId.isNull())
        filterStr = QString("WHERE r.guid = %1").arg(guidToSqlString(storageId));

    return getStorages(filterStr, storageList);
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
        NX_LOG( lit("Db error in %1: %2").arg(Q_FUNC_INFO).arg(queryScheduleTask.lastError().text()), cl_logERROR);
        return ErrorCode::dbError;
    }

    QnSql::fetch_many(queryScheduleTask, &scheduleTaskList);
    return ErrorCode::ok;
}

ErrorCode QnDbManager::doQueryNoLock(
    const QnUuid& cameraId, ApiCameraAttributesDataList& cameraUserAttributesList)
{
    //fetching camera user attributes
    QSqlQuery queryCameras(m_sdb);
    queryCameras.setForwardOnly(true);
    QString filterStr;
    if (!cameraId.isNull())
        filterStr = QString("WHERE camera_guid = %1").arg(guidToSqlString(cameraId));

    queryCameras.prepare(lit("\
        SELECT                                           \
            camera_guid as cameraId,                     \
            camera_name as cameraName,                   \
            group_name as userDefinedGroupName,          \
            audio_enabled as audioEnabled,               \
            control_enabled as controlEnabled,           \
            region as motionMask,                        \
            schedule_enabled as scheduleEnabled,         \
            motion_type as motionType,                   \
            secondary_quality as secondaryStreamQuality, \
            dewarping_params as dewarpingParams,         \
            coalesce(min_archive_days, %1) as minArchiveDays,             \
            coalesce(max_archive_days, %2) as maxArchiveDays,             \
            preferred_server_id as preferredServerId,    \
            license_used as licenseUsed,                 \
            failover_priority as failoverPriority,       \
            backup_type as backupType                    \
         FROM vms_camera_user_attributes \
         LEFT JOIN vms_resource r on r.guid = camera_guid \
         %3 \
         ORDER BY camera_guid \
        ").arg(-ec2::kDefaultMinArchiveDays)
          .arg(-ec2::kDefaultMaxArchiveDays)
          .arg(filterStr));

    if (!queryCameras.exec()) {
        NX_LOG( lit("Db error in %1: %2").arg(Q_FUNC_INFO).arg(queryCameras.lastError().text()), cl_logERROR);
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
        &ApiCameraAttributesData::cameraId,
        &ApiScheduleTaskWithRefData::sourceId );

    return ErrorCode::ok;
}

ErrorCode QnDbManager::doQueryNoLock(const QnUuid& id, ApiCameraDataExList& cameraExList)
{
    QSqlQuery queryCameras( m_sdb );
    queryCameras.setForwardOnly(true);

    QString filterStr;
    if (!id.isNull())
        filterStr = QString("WHERE r.guid = %1").arg(guidToSqlString(id));

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
            coalesce(cu.min_archive_days, %1) as minArchiveDays,             \
            coalesce(cu.max_archive_days, %2) as maxArchiveDays,             \
            cu.preferred_server_id as preferredServerId,       \
            cu.license_used as licenseUsed,                    \
            cu.failover_priority as failoverPriority,          \
            cu.backup_type as backupType                       \
        FROM vms_resource r \
        LEFT JOIN vms_resource_status rs on rs.guid = r.guid \
        JOIN vms_camera c on c.resource_ptr_id = r.id \
        LEFT JOIN vms_camera_user_attributes cu on cu.camera_guid = r.guid \
        %3 \
        ORDER BY r.guid \
    ").arg(-ec2::kDefaultMinArchiveDays)
      .arg(-ec2::kDefaultMaxArchiveDays)
      .arg(filterStr));

    if (!queryCameras.exec()) {
        NX_LOG( lit("Db error in %1: %2").arg(Q_FUNC_INFO).arg(queryCameras.lastError().text()), cl_logERROR);
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

/**
 * /ec2/getServers
 */
ErrorCode QnDbManager::doQueryNoLock(const QnUuid& id, ApiMediaServerDataList& serverList)
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);

    QString filterStr;
    if (!id.isNull())
        filterStr = QString("WHERE r.guid = %1").arg(guidToSqlString(id));

    query.prepare(lit("\
        SELECT r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentId, r.name, r.url, \
        s.auth_key as authKey, s.version, s.net_addr_list as networkAddresses, s.system_info as systemInfo, \
        s.flags \
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

ErrorCode QnDbManager::doQueryNoLock(const QnUuid& id, ApiMediaServerDataExList& serverExList)
{
    {
        //fetching server data
        ApiMediaServerDataList serverList;
        ErrorCode result = doQueryNoLock(id, serverList);
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
        ErrorCode result = doQueryNoLock(id, serverAttrsList);
        if( result != ErrorCode::ok )
            return result;

        //merging data & attributes
        mergeObjectListData(
            serverExList,
            serverAttrsList,
            &ApiMediaServerDataEx::id,
            &ApiMediaServerUserAttributesData::serverId,
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
    result = doQueryNoLock( id, statusList );
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
            server_guid as serverId,                                \
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
        NX_LOG( lit("DB Error at %1: %2").arg(Q_FUNC_INFO).arg(query.lastError().text()), cl_logERROR);
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
ErrorCode QnDbManager::doQueryNoLock(const QnUuid& id, ApiUserDataList& userList)
{
    QString filterStr;
    if( !id.isNull() )
        filterStr = QString("WHERE r.guid = %1").arg(guidToSqlString(id));

    QString queryStr = lit(" \
        SELECT r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentId, r.name, r.url, \
        u.is_superuser as isAdmin, u.email, \
        p.digest as digest, p.crypt_sha512_hash as cryptSha512Hash, p.realm as realm, u.password as hash, p.rights as permissions, \
        p.is_ldap as isLdap, p.is_enabled as isEnabled, p.user_role_guid as userRoleId, p.is_cloud as isCloud, \
        coalesce((SELECT value from vms_kvpair WHERE resource_guid = r.guid and name = '%1'), p.full_name) as fullName \
        FROM vms_resource r \
        JOIN auth_user u on u.id = r.id \
        JOIN vms_userprofile p on p.user_id = u.id \
        %2 \
        ORDER BY r.guid \
    ").arg(Qn::USER_FULL_NAME).arg(filterStr);

    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    if (!prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return ErrorCode::dbError;
    if (!execSQLQuery(&query, Q_FUNC_INFO))
        return ErrorCode::dbError;

    QnSql::fetch_many(query, &userList);

    return ErrorCode::ok;
}

//getUserRoles
ErrorCode QnDbManager::doQueryNoLock(const QnUuid& id, ApiUserRoleDataList& result)
{
    QString filterStr;
    if (!id.isNull())
        filterStr = QString("WHERE id = %1").arg(guidToSqlString(id));

    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    const QString queryStr = QString(R"(
        SELECT id, name, permissions
        FROM vms_user_roles
        %1
        ORDER BY id
    )").arg(filterStr);

    if (!prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return ErrorCode::dbError;

    if (!execSQLQuery(&query, Q_FUNC_INFO))
        return ErrorCode::dbError;

    QnSql::fetch_many(query, &result);
    return ErrorCode::ok;
}

ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiPredefinedRoleDataList& result)
{
    result = QnUserRolesManager::getPredefinedRoles();
    return ErrorCode::ok;
}

ec2::ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiAccessRightsDataList& accessRightsList)
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    const QString queryStr = R"(
        SELECT userOrRoleId, resourceIds
        FROM vms_access_rights
        ORDER BY userOrRoleId
    )";

    if (!prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return ErrorCode::dbError;

    if (!execSQLQuery(&query, Q_FUNC_INFO))
        return ErrorCode::dbError;

    while (query.next())
    {
        ApiAccessRightsData data;
        data.userId = QnUuid::fromRfc4122(query.value(0).toByteArray());
        data.resourceIds = QnUbjson::deserialized<std::vector<QnUuid>>(query.value(1).toByteArray());
        accessRightsList.push_back(std::move(data));
    }

    return ErrorCode::ok;
}


//getTransactionLog
ErrorCode QnDbManager::doQueryNoLock(const ApiTranLogFilter& filter, ApiTransactionDataList& tranList)
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    QString whereClause;
    if (filter.cloudOnly)
        whereClause = lit("WHERE tran_type=%1").arg(TransactionType::Cloud);
    query.prepare(lit("SELECT tran_guid, tran_data from transaction_log %1 order by peer_guid, db_guid, sequence").arg(whereClause));
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }

    while (query.next()) {
        ApiTransactionData tran;
        tran.tranGuid = QnSql::deserialized_field<QnUuid>(query.value(0));
        QByteArray srcData = query.value(1).toByteArray();
        tran.dataSize = srcData.size();
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
ErrorCode QnDbManager::doQueryNoLock(const QnUuid& id, ApiVideowallDataList& videowallList) {
    QSqlQuery query(m_sdb);
    QString filterStr;
    if (!id.isNull())
        filterStr = QString("WHERE r.guid = %1").arg(guidToSqlString(id));
    query.setForwardOnly(true);
    query.prepare(lit("\
        SELECT r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentId, r.name, r.url, l.autorun \
        FROM vms_videowall l \
        JOIN vms_resource r on r.id = l.resource_ptr_id %1 ORDER BY r.guid \
    ").arg(filterStr));
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
ErrorCode QnDbManager::doQueryNoLock(const QnUuid& id, ApiWebPageDataList& webPageList)
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    QString filterStr;
    if (!id.isNull())
        filterStr = QString("WHERE r.guid = %1").arg(guidToSqlString(id));
    query.prepare(lit("\
        SELECT r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentId, r.name, r.url \
        FROM vms_webpage l \
        JOIN vms_resource r on r.id = l.resource_ptr_id %1 ORDER BY r.guid \
        ").arg(filterStr));
    if (!query.exec())
    {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }
    QnSql::fetch_many(query, &webPageList);

    return ErrorCode::ok;
}

//getEventRules
ErrorCode QnDbManager::doQueryNoLock(const QnUuid& id, ApiBusinessRuleDataList& businessRuleList)
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    QString filterStr;
    if (!id.isNull())
        filterStr = QString("WHERE guid = %1").arg(guidToSqlString(id));
    query.prepare(lit(" \
        SELECT guid as id, event_type as eventType, event_condition as eventCondition, event_state as eventState, action_type as actionType, \
        action_params as actionParams, aggregation_period as aggregationPeriod, disabled, comments as comment, schedule, system \
        FROM vms_businessrule %1 ORDER BY guid \
    ").arg(filterStr));
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
    currentTime = m_timeSyncManager->getTimeInfo();
    return ErrorCode::ok;
}

// dumpDatabase
ErrorCode QnDbManager::doQuery(const nullptr_t& /*dummy*/, ApiDatabaseDumpData& data)
{
    QnWriteLocker lock(&m_mutex);

    if (!execSQLScript("vacuum;", m_sdb))
        qWarning() << "failed to vacuum database" << Q_FUNC_INFO;

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
        NX_LOG( lit("Can't reopen ec2 DB (%1). Error %2").arg(m_sdb.databaseName()).arg(m_sdb.lastError().text()), cl_logERROR);
        return ErrorCode::dbError;
    }

    if( !m_sdbStatic.open() || !tuneDBAfterOpen(&m_sdbStatic) )
    {
        NX_LOG( lit("Can't reopen ec2 license DB (%1). Error %2").arg(m_sdbStatic.databaseName()).arg(m_sdbStatic.lastError().text()), cl_logERROR);
        return ErrorCode::dbError;
    }

    //tuning DB
    if( !tuneDBAfterOpen(&m_sdb) )
        return ErrorCode::dbError;
    return ErrorCode::ok;
}

ErrorCode QnDbManager::doQuery(const ApiStoredFilePath& dumpFilePath, ApiDatabaseDumpToFileData& databaseDumpToFileData)
{
    QnWriteLocker lock(&m_mutex);
    m_tran->physicalCommitLazyData();

    //have to close/open DB to dump journals to .db file
    m_sdb.close();
    m_sdbStatic.close();

    ErrorCode result = ErrorCode::ok;
    QFile::remove(dumpFilePath.path);
    if( !QFile::copy( m_sdb.databaseName(), dumpFilePath.path ) )
        result = ErrorCode::ioError;

    QFileInfo dumpFileInfo( dumpFilePath.path );
    databaseDumpToFileData.size = dumpFileInfo.size();

    if( !m_sdb.open() )
    {
        NX_LOG( lit("Can't reopen ec2 DB (%1). Error %2").arg(m_sdb.databaseName()).arg(m_sdb.lastError().text()), cl_logERROR);
        return ErrorCode::dbError;
    }

    if( !m_sdbStatic.open() || !tuneDBAfterOpen(&m_sdbStatic) )
    {
        NX_LOG( lit("Can't reopen ec2 license DB (%1). Error %2").arg(m_sdbStatic.databaseName()).arg(m_sdbStatic.lastError().text()), cl_logERROR);
        return ErrorCode::dbError;
    }

    //tuning DB
    if( !tuneDBAfterOpen(&m_sdb) )
        return ErrorCode::dbError;
    return result;
}

bool QnDbManager::tuneDBAfterOpen(QSqlDatabase* const sqlDb)
{
    if (!QnDbHelper::tuneDBAfterOpen(sqlDb))
        return false;

    m_queryCachePool.reset();
    return true;
}

ec2::database::api::QueryCache::Pool* QnDbManager::queryCachePool()
{
    return &m_queryCachePool;
}

// TODO: Change to a function. ATTENTION: Macro contains "return".
#define DB_LOAD(ID, DATA) do \
{ \
    const ErrorCode errorCode = doQueryNoLock(ID, DATA); \
    if (errorCode != ErrorCode::ok) \
        return errorCode; \
} while (0)

ErrorCode QnDbManager::readApiFullInfoDataComplete(ApiFullInfoData* data)
{
    QnWriteLocker lock(&m_mutex);

    DB_LOAD(nullptr, data->resourceTypes);
    DB_LOAD(QnUuid(), data->servers);
    DB_LOAD(QnUuid(), data->serversUserAttributesList);
    DB_LOAD(QnUuid(), data->cameras);
    DB_LOAD(QnUuid(), data->cameraUserAttributesList);
    DB_LOAD(QnUuid(), data->users);
    DB_LOAD(QnUuid(), data->userRoles);
    DB_LOAD(QnUuid(), data->layouts);
    DB_LOAD(QnUuid(), data->videowalls);
    DB_LOAD(QnUuid(), data->webPages);
    DB_LOAD(QnUuid(), data->rules);
    DB_LOAD(nullptr, data->cameraHistory);
    DB_LOAD(nullptr, data->licenses);
    DB_LOAD(QnUuid(), data->discoveryData);
    DB_LOAD(QnUuid(), data->allProperties);
    DB_LOAD(QnUuid(), data->storages);
    DB_LOAD(QnUuid(), data->resStatusList);
    DB_LOAD(nullptr, data->accessRights);
    DB_LOAD(nullptr, data->layoutTours);

    return ErrorCode::ok;
}

ErrorCode QnDbManager::readApiFullInfoDataForMobileClient(
    ApiFullInfoData* data, const QnUuid& userId)
{
    QnWriteLocker lock(&m_mutex);

    DB_LOAD(QnUuid(), data->servers);
    DB_LOAD(QnUuid(), data->serversUserAttributesList);
    DB_LOAD(QnUuid(), data->cameras);
    DB_LOAD(QnUuid(), data->cameraUserAttributesList);

    DB_LOAD(userId, data->users);
    const ApiUserData* user = nullptr;
    if (data->users.size() == 1)
        user = &data->users[0];

    if (user) // Do not load user roles if there is no current user.
        DB_LOAD(user->userRoleId, data->userRoles);

    DB_LOAD(QnUuid(), data->layouts);
    if (user) // Remove layouts belonging to other users.
    {
        QSet<QnUuid> serverIds;
        for (const auto& server: data->servers)
            serverIds.insert(server.id);

        data->layouts.erase(std::remove_if(data->layouts.begin(), data->layouts.end(),
            [user, &serverIds](const ApiLayoutData& layout)
            {
                return !layout.parentId.isNull()
                    && layout.parentId != user->id
                    && !serverIds.contains(layout.parentId);
            }),
            data->layouts.end());
    }

    // Admin user is required for global properties.
    if (userId != QnUserResource::kAdminGuid)
        DB_LOAD(QnUserResource::kAdminGuid, data->users);

    DB_LOAD(nullptr, data->cameraHistory);
    DB_LOAD(QnUuid(), data->discoveryData);
    DB_LOAD(QnUuid(), data->allProperties);
    DB_LOAD(QnUuid(), data->resStatusList);
    DB_LOAD(nullptr, data->accessRights);

    return ErrorCode::ok;
}

#undef DB_LOAD

ErrorCode QnDbManager::doQueryNoLock(const QnUuid& id, ApiDiscoveryDataList &data) {
    QSqlQuery query(m_sdb);

    QString filterStr;
    if (!id.isNull())
        filterStr = QString("WHERE server_id = %1").arg(guidToSqlString(id));

    QString q = QString(lit(
        "SELECT server_id as id, url, ignore from vms_mserver_discovery %1 ORDER BY server_id")
        .arg(filterStr));
    query.setForwardOnly(true);
    query.prepare(q);

    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << __LINE__ << query.lastError();
        return ErrorCode::dbError;
    }

    QnSql::fetch_many(query, &data);

    return ErrorCode::ok;
}

ErrorCode QnDbManager::saveLicense(const ApiLicenseData& license)
{
    QnDbTransactionLocker lockStatic(&m_tranStatic);
    auto result = saveLicense(license, m_sdbStatic);
    if (result == ErrorCode::ok)
    {
        result = saveLicense(license, m_sdb);
        if (result == ErrorCode::ok)
            lockStatic.commit();
    }
    return result;
}

ErrorCode QnDbManager::saveLicense(const ApiLicenseData& license, QSqlDatabase& database)
{
    QSqlQuery insQuery(database);
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

ErrorCode QnDbManager::removeLicense(const ApiLicenseData& license)
{
    QnDbTransactionLocker lockStatic(&m_tranStatic);
    auto result = removeLicense(license, m_sdbStatic);
    if (result == ErrorCode::ok)
    {
        result = removeLicense(license, m_sdb);
        if (result == ErrorCode::ok)
            lockStatic.commit();
    }
    return result;
}

ErrorCode QnDbManager::removeLicense(const ApiLicenseData& license, QSqlDatabase& database)
{
    QSqlQuery delQuery(database);
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

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiMiscData>& tran)
{
    return saveMiscParam(tran.params);
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
    return getLicenses(data, m_sdb);
}

ErrorCode QnDbManager::getLicenses(ec2::ApiLicenseDataList& data, QSqlDatabase& database)
{
    QSqlQuery query(database);

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

bool QnDbManager::resyncIfNeeded(ResyncFlags flags)
{
    if (!m_dbJustCreated)
        m_resyncFlags |= flags;
    if (flags.testFlag(ResyncWebPages))
        m_resyncFlags |= ResyncWebPages; //< Resync web pages always. Media servers could have different list even it started with empty DB.
    return true;
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

/**
* /ec2/getLayouts
*/
ec2::ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiLayoutTourDataList& data)
{
    if (!database::api::fetchLayoutTours(m_sdb, data))
        return ErrorCode::dbError;
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



ErrorCode QnDbManager::saveWebPage(const ApiWebPageData& params)
{
    if (!database::api::saveWebPage(&m_resourceQueries, params))
        return ErrorCode::dbError;
    return ErrorCode::ok;
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

    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiCleanupDatabaseData>& tran)
{
    ErrorCode result = ErrorCode::ok;
    if (tran.params.cleanupDbObjects)
        result = cleanupDanglingDbObjects() ? ErrorCode::ok : ErrorCode::failure;

    if (result != ErrorCode::ok)
        return result;

    if (tran.params.cleanupTransactionLog)
        result = m_tranLog->clear() && resyncTransactionLog() ? ErrorCode::ok : ErrorCode::failure;

    return result;
}

QnUuid QnDbManager::getID() const
{
    return m_dbInstanceId;
}

bool QnDbManager::updateId()
{
    auto newDbInstanceId = QnUuid::createUuid();

    QSqlQuery query(m_sdb);
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT OR REPLACE INTO misc_data (key, data) values (?,?)");
    insQuery.addBindValue(DB_INSTANCE_KEY);
    insQuery.addBindValue(newDbInstanceId.toRfc4122());
    if (!insQuery.exec())
    {
        qWarning() << "can't update db instance ID";
        return false;
    }

    m_dbInstanceId = newDbInstanceId;
    return true;
}

QnDbManager::QnDbTransactionExt* QnDbManager::getTransaction()
{
    return m_tran.get();
}

void QnDbManager::setTransactionLog(QnTransactionLog* tranLog)
{
    m_tranLog = tranLog;
}

void QnDbManager::setTimeSyncManager(TimeSynchronizationManager* timeSyncManager)
{
    m_timeSyncManager = timeSyncManager;
}

QnTransactionLog* QnDbManager::transactionLog() const
{
    return m_tranLog;
}

bool QnDbManager::rebuildUserAccessRightsTransactions()
{
    // migrate transaction log
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    if (!SqlQueryExecutionHelper::prepareSQLQuery(
        &query,
        lit("SELECT tran_guid, tran_data from transaction_log"),
        Q_FUNC_INFO))
    {
        return false;
    }
    if (!SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO))
        return false;

    QVector<QnUuid> recordsToDelete;
    QVector<ApiIdData> recordsToAdd;
    while (query.next())
    {
        QnUuid tranGuid = QnSql::deserialized_field<QnUuid>(query.value(0));
        QnAbstractTransaction abstractTran;
        QByteArray srcData = query.value(1).toByteArray();
        QnUbjsonReader<QByteArray> stream(&srcData);
        if (!QnUbjson::deserialize(&stream, &abstractTran))
        {
            NX_WARNING(this, "Can't deserialize transaction from transaction log");
            return false;
        }

        if (abstractTran.command == ApiCommand::removeAccessRights
            || abstractTran.command == ApiCommand::setAccessRights)
        {
            recordsToDelete << tranGuid;
        }
        else if (abstractTran.command == ApiCommand::removeUser
            || abstractTran.command == ApiCommand::removeUserRole)
        {
            ApiIdData data;
            if (!QnUbjson::deserialize(&stream, &data))
            {
                NX_WARNING(this, "Can't deserialize transaction from transaction log");
                return false;
            }
            recordsToAdd << data;
        }
    }

    for (const auto& data: recordsToDelete)
    {
        QSqlQuery delQuery(m_sdb);
        if (!SqlQueryExecutionHelper::prepareSQLQuery(
            &delQuery,
            lit("DELETE FROM transaction_log WHERE tran_guid = ?"),
            Q_FUNC_INFO))
        {
            return false;
        }

        delQuery.addBindValue(QnSql::serialized_field(data));
        if (!SqlQueryExecutionHelper::execSQLQuery(&delQuery, Q_FUNC_INFO))
            return false;
    }

    for (const auto& data: recordsToAdd)
    {
        QnTransaction<ApiIdData> transaction(
            ApiCommand::removeAccessRights,
            commonModule()->moduleGUID(),
            data);
        transactionLog()->fillPersistentInfo(transaction);
        if (transactionLog()->saveTransaction(transaction) != ErrorCode::ok)
            return false;
    }

    return true;
}

} // namespace detail

QnDbManagerAccess::QnDbManagerAccess(
    detail::QnDbManager* dbManager,
    const Qn::UserAccessData &userAccessData)
    :
    m_dbManager(dbManager),
    m_userAccessData(userAccessData)
{}

ApiObjectType QnDbManagerAccess::getObjectType(const QnUuid& objectId)
{
    return m_dbManager->getObjectType(objectId);
}

QnDbHelper::QnDbTransaction* QnDbManagerAccess::getTransaction()
{
    return m_dbManager->getTransaction();
}

ApiObjectType QnDbManagerAccess::getObjectTypeNoLock(const QnUuid& objectId)
{
    return m_dbManager->getObjectTypeNoLock(objectId);
}

ApiObjectInfoList QnDbManagerAccess::getNestedObjectsNoLock(const ApiObjectInfo& parentObject)
{
    return m_dbManager->getNestedObjectsNoLock(parentObject);
}

ApiObjectInfoList QnDbManagerAccess::getObjectsNoLock(const ApiObjectType& objectType)
{
    return m_dbManager->getObjectsNoLock(objectType);
}

ApiIdDataList QnDbManagerAccess::getLayoutToursNoLock(const QnUuid& parentId)
{
    ApiIdDataList result;
    ApiLayoutTourDataList tours;
    database::api::fetchLayoutTours(m_dbManager->getDB(), tours);
    for (const auto& tour: tours)
    {
        if (tour.parentId == parentId)
            result.push_back(tour.id);
    }
    return result;
}

void QnDbManagerAccess::getResourceParamsNoLock(const QnUuid& resourceId, ApiResourceParamWithRefDataList& resourceParams)
{
    m_dbManager->doQueryNoLock(resourceId, resourceParams);
}

bool QnDbManagerAccess::isTranAllowed(const QnAbstractTransaction& tran) const
{
    if (!m_dbManager->isReadOnly())
        return true;

    switch (tran.command)
    {
        case ApiCommand::addLicense:
        case ApiCommand::addLicenses:
        case ApiCommand::removeLicense:
            return true;

        case ApiCommand::saveMediaServer:
        case ApiCommand::saveStorage:
        case ApiCommand::saveStorages:
        case ApiCommand::saveMediaServerUserAttributes:
        case ApiCommand::saveMediaServerUserAttributesList:
        case ApiCommand::setResourceStatus:
        case ApiCommand::setResourceParam:
        case ApiCommand::setResourceParams:
            // Allowing minimum set of transactions required for local server to function properly.
            return m_userAccessData == Qn::kSystemAccess;

        default:
            return false;
    }
}

ErrorCode QnDbManagerAccess::readApiFullInfoDataForMobileClient(ApiFullInfoData* data, const QnUuid& userId)
{
    const ErrorCode errorCode =
        m_dbManager->readApiFullInfoDataForMobileClient(data, userId);
    if (errorCode != ErrorCode::ok)
        return errorCode;

    filterData(data->servers);
    filterData(data->serversUserAttributesList);
    filterData(data->cameras);
    filterData(data->cameraUserAttributesList);
    filterData(data->users);
    filterData(data->userRoles);
    filterData(data->userRoles);
    filterData(data->accessRights);
    filterData(data->layouts);
    filterData(data->cameraHistory);
    filterData(data->discoveryData);
    filterData(data->allProperties);
    filterData(data->resStatusList);

    return ErrorCode::ok;
}

} // namespace ec2
