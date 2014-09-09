#include "db_manager.h"

#include "version.h"

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

using std::nullptr_t;

namespace ec2
{

static QnDbManager* globalInstance = 0; // TODO: #Elric #EC2 use QnSingleton
static const char LICENSE_EXPIRED_TIME_KEY[] = "{4208502A-BD7F-47C2-B290-83017D83CDB7}";
static const char DB_INSTANCE_KEY[] = "DB_INSTANCE_ID";

template <class T>
void assertSorted(std::vector<T> &data) {
#ifdef _DEBUG
    assertSorted(data, &T::id);
#else
    Q_UNUSED(data);
#endif // DEBUG
}

template <class T, class Field>
void assertSorted(std::vector<T> &data, QUuid Field::*idField) {
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
void mergeIdListData(QSqlQuery& query, std::vector<MainData>& data, std::vector<QUuid> MainData::*subList)
{
    assertSorted(data);

    QSqlRecord rec = query.record();
    int idIdx = rec.indexOf("id");
    int parentIdIdx = rec.indexOf("parentId");
    assert(idIdx >=0 && parentIdIdx >= 0);
  
    bool eof = true;
    QUuid id, parentId;
    QByteArray idRfc;

    auto step = [&eof, &id, &idRfc, &parentId, &query, idIdx, parentIdIdx]{
        eof = !query.next();
        if (eof)
            return;
        idRfc = query.value(idIdx).toByteArray();
        assert(idRfc == id.toRfc4122() || idRfc > id.toRfc4122());
        id = QUuid::fromRfc4122(idRfc);
        parentId = QUuid::fromRfc4122(query.value(parentIdIdx).toByteArray());
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

QnDbManager::Locker::Locker(QnDbManager* db)
: 
    m_db(db),
    m_scopedTran( db->getTransaction() )
{
}

QnDbManager::Locker::~Locker()
{
}

void QnDbManager::Locker::commit()
{
    m_scopedTran.commit();
}


// --------------------------------------- QnDbManager -----------------------------------------

QUuid QnDbManager::getType(const QString& typeName)
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
        return QUuid::fromRfc4122(query.value("guid").toByteArray());
    return QUuid();
}

QnDbManager::QnDbManager()
:
    m_licenseOverflowMarked(false),
    m_licenseOverflowTime(0),
    m_initialized(false),
    m_tranStatic(m_sdbStatic, m_mutexStatic),
    m_needResyncLog(false)
{
	Q_ASSERT(!globalInstance);
	globalInstance = this;
}

bool QnDbManager::init(
    QnResourceFactory* factory,
    const QString& dbFilePath,
    const QString& dbFilePathStatic )
{
    m_resourceFactory = factory;

    m_sdb = QSqlDatabase::addDatabase("QSQLITE", "QnDbManager");
    QString dbFileName = closeDirPath(dbFilePath) + QString::fromLatin1("ecs.sqlite");
    m_sdb.setDatabaseName( dbFileName);

    QString backupDbFileName = dbFileName + QString::fromLatin1(".backup");
    if (QFile::exists(backupDbFileName)) {
        QFile::remove(dbFileName);
        QFile::rename(backupDbFileName, dbFileName);
        m_needResyncLog = true;
    }

    m_sdbStatic = QSqlDatabase::addDatabase("QSQLITE", "QnDbManagerStatic");
    QString path2 = dbFilePathStatic.isEmpty() ? dbFilePath : dbFilePathStatic;
    m_sdbStatic.setDatabaseName( closeDirPath(path2) + QString::fromLatin1("ecs_static.sqlite"));

    if( !m_sdb.open() )
    {
        qWarning() << "can't initialize Server sqlLite database " << m_sdb.databaseName() << ". Error: " << m_sdb.lastError().text();
        return false;
    }

    if( !m_sdbStatic.open() )
    {
        qWarning() << "can't initialize Server static sqlLite database " << m_sdbStatic.databaseName() << ". Error: " << m_sdbStatic.lastError().text();
        return false;
    }

    //tuning DB
    {
        QSqlQuery enableWalQuery(m_sdb);
        enableWalQuery.prepare("PRAGMA journal_mode = WAL");
        if( !enableWalQuery.exec() )
        {
            qWarning() << "Failed to enable WAL mode on sqlLite database!" << enableWalQuery.lastError().text();
        }
    }

    bool dbJustCreated = false;
    bool isMigrationFrom2_2 = false;
    if( !createDatabase( &dbJustCreated, &isMigrationFrom2_2 ) )
    {
        // create tables is DB is empty
        qWarning() << "can't create tables for sqlLite database!";
        return false;
    }

    if( !qnCommon->obsoleteServerGuid().isNull() )
    {
        {
            QSqlQuery updateGuidQuery( m_sdb );
            updateGuidQuery.prepare( "UPDATE vms_resource SET guid=? WHERE guid=?" );
            updateGuidQuery.addBindValue( qnCommon->moduleGUID().toRfc4122() );
            updateGuidQuery.addBindValue( qnCommon->obsoleteServerGuid().toRfc4122() );
            if( !updateGuidQuery.exec() )
            {
                qWarning() << "can't initialize sqlLite database!" << updateGuidQuery.lastError().text();
                return false;
            }
        }

        {
            QSqlQuery updateGuidQuery( m_sdb );
            updateGuidQuery.prepare( "UPDATE vms_resource SET parent_guid=? WHERE parent_guid=?" );
            updateGuidQuery.addBindValue( qnCommon->moduleGUID().toRfc4122() );
            updateGuidQuery.addBindValue( qnCommon->obsoleteServerGuid().toRfc4122() );
            if( !updateGuidQuery.exec() )
            {
                qWarning() << "can't initialize sqlLite database!" << updateGuidQuery.lastError().text();
                return false;
            }
        }
    }
    // updateDBVersion();
    QSqlQuery insVersionQuery( m_sdb );
    insVersionQuery.prepare( "INSERT OR REPLACE INTO misc_data (key, data) values (?,?)" );
    insVersionQuery.addBindValue( "VERSION" );
    insVersionQuery.addBindValue( QN_APPLICATION_VERSION );
    if( !insVersionQuery.exec() )
    {
        qWarning() << "can't initialize sqlLite database!" << insVersionQuery.lastError().text();
        return false;
    }
    insVersionQuery.addBindValue( "BUILD" );
    insVersionQuery.addBindValue( QN_APPLICATION_REVISION );
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
        m_adminUserID = QUuid::fromRfc4122( queryAdminUser.value( 0 ).toByteArray() );
        m_adminUserInternalID = queryAdminUser.value( 1 ).toInt();
    }

    QSqlQuery queryServers(m_sdb);
    queryServers.prepare("UPDATE vms_resource set status = ? WHERE xtype_guid = ?"); // todo: only mserver without DB?
    queryServers.bindValue(0, Qn::Offline);
    queryServers.bindValue(1, m_serverTypeId.toRfc4122());
    if( !queryServers.exec() )
    {
        Q_ASSERT( false );
    }

    // read license overflow time
    QSqlQuery query( m_sdb );
    query.prepare( "SELECT data from misc_data where key = ?" );
    query.addBindValue( LICENSE_EXPIRED_TIME_KEY );
    if( query.exec() && query.next() )
    {
        m_licenseOverflowTime = query.value( 0 ).toByteArray().toLongLong();
        m_licenseOverflowMarked = m_licenseOverflowTime > 0;
    }

    QnPeerRuntimeInfo localInfo = QnRuntimeInfoManager::instance()->localInfo();
    if( localInfo.data.prematureLicenseExperationDate != m_licenseOverflowTime )
    {
        localInfo.data.prematureLicenseExperationDate = m_licenseOverflowTime;
        QnRuntimeInfoManager::instance()->items()->updateItem( localInfo.uuid, localInfo );
    }

    query.addBindValue( DB_INSTANCE_KEY );
    if( query.exec() && query.next() )
    {
        m_dbInstanceId = QUuid::fromRfc4122( query.value( 0 ).toByteArray() );
    }
    else
    {
        m_dbInstanceId = QUuid::createUuid();
        QSqlQuery insQuery( m_sdb );
        insQuery.prepare( "INSERT INTO misc_data (key, data) values (?,?)" );
        insQuery.addBindValue( DB_INSTANCE_KEY );
        insQuery.addBindValue( m_dbInstanceId.toRfc4122() );
        if( !insQuery.exec() )
        {
            qWarning() << "can't initialize sqlLite database!";
            return false;
        }
    }

    if( QnTransactionLog::instance() )
        QnTransactionLog::instance()->init();

    if( m_needResyncLog )
        resyncTransactionLog();

    // Set admin user's password
    ApiUserDataList users;
    ErrorCode errCode = doQueryNoLock( nullptr, users );
    if( errCode != ErrorCode::ok )
    {
        return false;
    }

    if( users.empty() )
    {
        return false;
    }

    if( !qnCommon->defaultAdminPassword().isEmpty() )
    {
        QnUserResourcePtr userResource( new QnUserResource() );
        fromApiToResource( users[0], userResource );
        userResource->setPassword( qnCommon->defaultAdminPassword() );

        QnTransaction<ApiUserData> userTransaction( ApiCommand::saveUser );
        userTransaction.fillPersistentInfo();
        fromResourceToApi( userResource, userTransaction.params );
        executeTransactionNoLock( userTransaction, QnUbjson::serialized( userTransaction ) );
    }

    QSqlQuery queryCameras( m_sdb );
    // Update cameras status
    // select cameras from servers without DB and local cameras
    queryCameras.setForwardOnly(true);
    queryCameras.prepare("SELECT r.guid FROM vms_resource r \
                         JOIN vms_camera c on c.resource_ptr_id = r.id \
                         JOIN vms_resource sr on sr.guid = r.parent_guid \
                         JOIN vms_server s on s.resource_ptr_id = sr.id \
                         WHERE r.status != ? AND ((s.flags & 2) or sr.guid = ?)");
    queryCameras.bindValue(0, Qn::Offline);
    queryCameras.bindValue(1, qnCommon->moduleGUID().toRfc4122());
    if (!queryCameras.exec()) {
        qWarning() << Q_FUNC_INFO << __LINE__ << queryCameras.lastError();
        Q_ASSERT( 0 );
    }
    while( queryCameras.next() )
    {
        QnTransaction<ApiSetResourceStatusData> tran( ApiCommand::setResourceStatus );
        tran.fillPersistentInfo();
        tran.params.id = QUuid::fromRfc4122(queryCameras.value(0).toByteArray());
        tran.params.status = Qn::Offline;
        executeTransactionNoLock( tran, QnUbjson::serialized( tran ) );
    }

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

    foreach(const ObjectType& object, objects)
    {
        QnTransaction<ObjectType> transaction(command);
        transaction.fillPersistentInfo();
        transaction.params = object;
        if (transactionLog->saveTransaction(transaction) != ErrorCode::ok)
            return false;
    }
    return true;
}

bool QnDbManager::addTransactionForGeneralSettings()
{
    ApiResourceParamsData object;
    ErrorCode errCode = doQueryNoLock(m_adminUserID, object);
    if (errCode != ErrorCode::ok)
        return false;

    QnTransaction<ApiResourceParamsData> transaction(ApiCommand::setResourceParams);
    transaction.fillPersistentInfo();
    transaction.params = object;
    if (transactionLog->saveTransaction(transaction) != ErrorCode::ok)
        return false;
    return true;
}

bool QnDbManager::resyncTransactionLog()
{
    if (!fillTransactionLogInternal<ApiUserData, ApiUserDataList>(ApiCommand::saveUser))
        return false;
    if (!fillTransactionLogInternal<ApiMediaServerData, ApiMediaServerDataList>(ApiCommand::saveMediaServer))
        return false;
    if (!fillTransactionLogInternal<ApiCameraData, ApiCameraDataList>(ApiCommand::saveCamera))
        return false;
    if (!fillTransactionLogInternal<ApiLayoutData, ApiLayoutDataList>(ApiCommand::saveLayout))
        return false;
    if (!fillTransactionLogInternal<ApiBusinessRuleData, ApiBusinessRuleDataList>(ApiCommand::saveBusinessRule))
        return false;
    if (!addTransactionForGeneralSettings())
        return false;

    return true;
}

bool QnDbManager::isInitialized() const
{
    return m_initialized;
}

QMap<int, QUuid> QnDbManager::getGuidList( const QString& request, GuidConversionMethod method, const QByteArray& intHashPostfix )
{
    QMap<int, QUuid>  result;
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
            result.insert(id, QUuid::fromRfc4122(data.toByteArray()));
            break;
        case CM_MakeHash:
            {
                QCryptographicHash md5Hash( QCryptographicHash::Md5 );
                md5Hash.addData(data.toString().toUtf8());
                QByteArray ha2 = md5Hash.result();
                result.insert(id, QUuid::fromRfc4122(ha2));
                break;
            }
        case CM_INT:
            result.insert(id, intToGuid(id, intHashPostfix));
            break;
        default:
            {
                if (data.isNull())
                    result.insert(id, intToGuid(id, intHashPostfix));
                else {
                    QUuid guid(data.toString());
                    if (guid.isNull()) {
                        QCryptographicHash md5Hash( QCryptographicHash::Md5 );
                        md5Hash.addData(data.toString().toUtf8());
                        QByteArray ha2 = md5Hash.result();
                        guid = QUuid::fromRfc4122(ha2);
                    }
                    result.insert(id, guid);
                }
            }
        }
    }

    return result;
}

bool QnDbManager::updateTableGuids(const QString& tableName, const QString& fieldName, const QMap<int, QUuid>& guids)
{
#ifdef DB_DEBUG
    int n = guids.size();
    qDebug() << "updating table guids" << n << "commands queued";
    int i = 0;
#endif // DB_DEBUG
    for(QMap<int, QUuid>::const_iterator itr = guids.begin(); itr != guids.end(); ++itr)
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

bool QnDbManager::updateGuids()
{
    QMap<int, QUuid> guids = getGuidList("SELECT id, guid from vms_resource_tmp order by id", CM_Default, QUuid::createUuid().toByteArray());
    if (!updateTableGuids("vms_resource", "guid", guids))
        return false;

    guids = getGuidList("SELECT resource_ptr_id, physical_id from vms_camera order by resource_ptr_id", CM_MakeHash);
    if (!updateTableGuids("vms_resource", "guid", guids))
        return false;

    guids = getGuidList("SELECT li.id, r.guid FROM vms_layoutitem_tmp li JOIN vms_resource r on r.id = li.resource_id order by li.id", CM_Binary);
    if (!updateTableGuids("vms_layoutitem", "resource_guid", guids))
        return false;

    guids = getGuidList("SELECT rt.id, rt.name || coalesce(m.name,'-') as guid from vms_resourcetype rt LEFT JOIN vms_manufacture m on m.id = rt.manufacture_id", CM_MakeHash);
    if (!updateTableGuids("vms_resourcetype", "guid", guids))
        return false;

    guids = getGuidList("SELECT r.id, r2.guid from vms_resource_tmp r JOIN vms_resource r2 on r2.id = r.parent_id order by r.id", CM_Binary);
    if (!updateTableGuids("vms_resource", "parent_guid", guids))
        return false;

    guids = getGuidList("SELECT r.id, rt.guid from vms_resource_tmp r JOIN vms_resourcetype rt on rt.id = r.xtype_id", CM_Binary);
    if (!updateTableGuids("vms_resource", "xtype_guid", guids))
        return false;

    guids = getGuidList("SELECT id, id from vms_businessrule ORDER BY id", CM_INT, QUuid::createUuid().toByteArray());
    if (!updateTableGuids("vms_businessrule", "guid", guids))
        return false;

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

    foreach(const BeRemapData& remapData, oldData) 
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
    foreach(const QFileInfo& entry, dir.entryInfoList(QDir::NoDotAndDotDot | QDir::Files, QDir::Name))
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
            insQuery.addBindValue(QN_APPLICATION_NAME);
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
    return true;
}

bool QnDbManager::afterInstallUpdate(const QString& updateName)
{
    if (updateName == lit(":/updates/07_videowall.sql")) 
    {
        QMap<int, QUuid> guids = getGuidList("SELECT rt.id, rt.name || '-' as guid from vms_resourcetype rt WHERE rt.name == 'Videowall'", CM_MakeHash);
        if (!updateTableGuids("vms_resourcetype", "guid", guids))
            return false;
    }

    return true;
}

bool QnDbManager::createDatabase(bool *dbJustCreated, bool *isMigrationFrom2_2)
{
    QnDbTransactionLocker lock(&m_tran);

    *dbJustCreated = false;
    *isMigrationFrom2_2 = false;

    if (!isObjectExists(lit("table"), lit("vms_resource"), m_sdb))
    {
        NX_LOG(QString("Create new database"), cl_logINFO);

        *dbJustCreated = true;

        if (!execSQLFile(lit(":/01_createdb.sql"), m_sdb))
            return false;

        //#ifdef EDGE_SERVER
        //        if (!execSQLFile(lit(":/02_insert_3thparty_vendor.sql")))
        //            return false;
        //#else
        if (!execSQLFile(lit(":/02_insert_all_vendors.sql"), m_sdb))
            return false;
        //#endif
    }

    if (!isObjectExists(lit("table"), lit("transaction_log"), m_sdb))
    {
		NX_LOG(QString("Update database to v 2.3"), cl_logINFO);
        if (!migrateBusinessEvents())
            return false;
        if (!(*dbJustCreated)) {
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
        if (!execSQLFile(lit(":/06_staticdb_add_license_table.sql"), m_sdbStatic))
            return false;

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

        foreach(const ApiLicenseData& data, licenses)
        {
            if (saveLicense(data) != ErrorCode::ok)
                return false;
        }
        //if (!execSQLQuery("drop table vms_license", m_sdb))
        //    return false;
    }

    if (!applyUpdates())
        return false;

    lockStatic.commit();
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

ErrorCode QnDbManager::insertAddParams(const std::vector<ApiResourceParamData>& params, qint32 internalId)
{
    QSqlQuery insQuery(m_sdb);
    //insQuery.prepare("INSERT INTO vms_kvpair (resource_id, name, value) VALUES(:resourceId, :name, :value)");
    //insQuery.prepare("INSERT OR REPLACE INTO vms_kvpair VALUES(?, NULL, ?, ?, ?)");
    insQuery.prepare("INSERT OR REPLACE INTO vms_kvpair(resource_id, name, value, isResTypeParam) VALUES(?, ?, ?, ?)");

    insQuery.bindValue(0, internalId);
    foreach(const ApiResourceParamData& param, params) {
        assert(!param.name.isEmpty());

        insQuery.bindValue(1, QnSql::serialized_field(param.name));
        insQuery.bindValue(2, QnSql::serialized_field(param.value));
        insQuery.bindValue(3, QnSql::serialized_field(param.predefinedParam));
        if (!insQuery.exec()) {
            qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
            return ErrorCode::dbError;
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
        return ErrorCode::dbError;
    }
}

ErrorCode QnDbManager::insertResource(const ApiResourceData& data, qint32* internalId)
{
    QSqlQuery insQuery(m_sdb);
    //insQuery.prepare("INSERT INTO vms_resource (guid, xtype_guid, parent_guid, name, url, status, disabled) VALUES(:id, :typeId, :parentId, :name, :url, :status, :disabled)");
    //data.autoBindValues(insQuery);
    //insQuery.prepare("INSERT INTO vms_resource VALUES(NULL, ?,?,?,?,?,?,?)");
    insQuery.prepare("INSERT INTO vms_resource(status, name, url, xtype_guid, parent_guid, guid) VALUES(?,?,?,?,?,?)");

    insQuery.bindValue(0, QnSql::serialized_field(data.status));
    insQuery.bindValue(1, QnSql::serialized_field(data.name));
    insQuery.bindValue(2, QnSql::serialized_field(data.url));
    insQuery.bindValue(3, QnSql::serialized_field(data.typeId));
    insQuery.bindValue(4, QnSql::serialized_field(data.parentId));
    insQuery.bindValue(5, QnSql::serialized_field(data.id));

    if (!insQuery.exec()) {
        qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
        return ErrorCode::dbError;
    }
    *internalId = insQuery.lastInsertId().toInt();

    return insertAddParams(data.addParams, *internalId);
}

qint32 QnDbManager::getResourceInternalId( const QUuid& guid ) {
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare("SELECT id from vms_resource where guid = ?");
    query.bindValue(0, guid.toRfc4122());
    if (!query.exec() || !query.next())
        return 0;
    return query.value(0).toInt();
}

QUuid QnDbManager::getResourceGuid(const qint32 &internalId) {
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare("SELECT guid from vms_resource where id = ?");
    query.bindValue(0, internalId);
    if (!query.exec() || !query.next())
        return QUuid();
    return QUuid::fromRfc4122(query.value(0).toByteArray());
}

ErrorCode QnDbManager::insertOrReplaceResource(const ApiResourceData& data, qint32* internalId)
{
    *internalId = getResourceInternalId(data.id);

    Q_ASSERT_X(data.status == Qn::NotDefined, Q_FUNC_INFO, "Status MUST be unchanged for resource modification. Use setStatus instead to modify it!");

    QSqlQuery query(m_sdb);
    if (*internalId) {
        query.prepare("UPDATE vms_resource SET guid = :id, xtype_guid = :typeId, parent_guid = :parentId, name = :name, url = :url WHERE id = :internalID");
        query.bindValue(":internalID", *internalId);
    }
    else {
        query.prepare("INSERT OR REPLACE INTO vms_resource (guid, xtype_guid, parent_guid, name, url, status) VALUES(:id, :typeId, :parentId, :name, :url, :status)");
    }
    QnSql::bind(data, &query);
    //data.autoBindValues(query);


    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
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

ErrorCode QnDbManager::updateResource(const ApiResourceData& data, qint32 internalId)
{
    /* Check that we are updating really existing resource */
    Q_ASSERT(internalId != 0);
    ErrorCode result = checkExistingUser(data.name, internalId);
    if (result != ErrorCode::ok)
        return result;

    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("UPDATE vms_resource SET xtype_guid = :typeId, parent_guid = :parentId, name = :name, url = :url, status = :status WHERE id = :internalId");
    QnSql::bind(data, &insQuery);
    insQuery.bindValue(":internalId", internalId);

    if (!insQuery.exec()) {
        qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
        return ErrorCode::dbError;
    }

    if (!data.addParams.empty()) 
    {
        /*
        ErrorCode result = deleteAddParams(internalId);
        if (result != ErrorCode::ok)
            return result;
        */
        result = insertAddParams(data.addParams, internalId);
        if (result != ErrorCode::ok)
            return result;
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::insertOrReplaceUser(const ApiUserData& data, qint32 internalId)
{
    QSqlQuery insQuery(m_sdb);
    if (!data.hash.isEmpty())
        insQuery.prepare("INSERT OR REPLACE INTO auth_user (id, username, is_superuser, email, password, is_staff, is_active, last_login, date_joined, first_name, last_name) \
                         VALUES (:internalId, :name, :isAdmin, :email, :hash, 1, 1, '', '', '', '')");
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
    if (!data.digest.isEmpty())
        insQuery2.prepare("INSERT OR REPLACE INTO vms_userprofile (user_id, resource_ptr_id, digest, rights) VALUES (:internalId, :internalId, :digest, :permissions)");
    else
        insQuery2.prepare("UPDATE vms_userprofile SET rights=:permissions WHERE user_id=:internalId");
    QnSql::bind(data, &insQuery2);
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
    insQuery.prepare("INSERT OR REPLACE INTO vms_camera (audio_enabled, control_enabled, vendor, manually_added, resource_ptr_id, region, schedule_enabled, motion_type, group_name, group_id,\
                     mac, model, secondary_quality, status_flags, physical_id, password, login, dewarping_params, resource_ptr_id, min_archive_days, max_archive_days, prefered_server_id) VALUES\
                     (:audioEnabled, :controlEnabled, :vendor, :manuallyAdded, :id, :motionMask, :scheduleEnabled, :motionType, :groupName, :groupId,\
                     :mac, :model, :secondaryStreamQuality, :statusFlags, :physicalId, :password, :login, :dewarpingParams, :internalId, :minArchiveDays, :maxArchiveDays, :preferedServerId)");
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

ErrorCode QnDbManager::insertOrReplaceMediaServer(const ApiMediaServerData& data, qint32 internalId)
{
    QSqlQuery insQuery(m_sdb);
    insQuery.prepare("INSERT OR REPLACE INTO vms_server (api_url, auth_key, version, net_addr_list, system_info, flags, panic_mode, max_cameras, redundancy, system_name, resource_ptr_id) VALUES\
                     (:apiUrl, :authKey, :version, :networkAddresses, :systemInfo, :flags, :panicMode, :maxCameras, :allowAutoRedundancy, :systemName, :internalId)");
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
    insQuery.prepare("INSERT OR REPLACE INTO vms_layout \
                     (user_can_edit, cell_spacing_height, locked, \
                     cell_aspect_ratio, background_width, \
                     background_image_filename, background_height, \
                     cell_spacing_width, background_opacity, resource_ptr_id) \
                     \
                     VALUES (:editable, :verticalSpacing, :locked, \
                     :cellAspectRatio, :backgroundWidth, \
                     :backgroundImageFilename, :backgroundHeight, \
                     :horizontalSpacing, :backgroundOpacity, :internalId)");
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

ErrorCode QnDbManager::removeStoragesByServer(const QUuid& serverGuid)
{
    QSqlQuery delQuery(m_sdb);
    delQuery.prepare("DELETE FROM vms_storage WHERE resource_ptr_id in (select id from vms_resource where parent_guid = :guid and xtype_guid = :typeId)");
    delQuery.bindValue(":guid", serverGuid.toRfc4122());
    delQuery.bindValue(":typeId", m_storageTypeId.toRfc4122());
    if (!delQuery.exec()) {
        qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
        return ErrorCode::dbError;
    }

    QSqlQuery delQuery2(m_sdb);
    delQuery2.prepare("DELETE FROM vms_resource WHERE parent_guid = :guid and xtype_guid=:typeId");
    delQuery2.bindValue(":guid", serverGuid.toRfc4122());
    delQuery2.bindValue(":typeId", m_storageTypeId.toRfc4122());
    if (!delQuery2.exec()) {
        qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
        return ErrorCode::dbError;
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
                         (:spaceLimit, :usedForWriting, :internalId)");
        QnSql::bind(storage, &insQuery);
        insQuery.bindValue(":internalId", internalId);

        if (!insQuery.exec()) {
            qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
            return ErrorCode::dbError;
        }
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::removeCameraSchedule(qint32 internalId)
{
    QSqlQuery delQuery(m_sdb);
    delQuery.prepare("DELETE FROM vms_scheduletask where source_id = ?");
    delQuery.addBindValue(internalId);
    if (!delQuery.exec()) {
        qWarning() << Q_FUNC_INFO << delQuery.lastError().text();
        return ErrorCode::dbError;
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::updateCameraSchedule(const ApiCameraData& data, qint32 internalId)
{
    ErrorCode errCode = removeCameraSchedule(internalId);
    if (errCode != ErrorCode::ok)
        return errCode;

    QSqlQuery insQuery(m_sdb);
    //insQuery.prepare("INSERT INTO vms_scheduletask (source_id, start_time, end_time, do_record_audio, record_type, day_of_week, before_threshold, after_threshold, stream_quality, fps) 
    //                  VALUES :sourceId, :startTime, :endTime, :doRecordAudio, :recordType, :dayOfWeek, :beforeThreshold, :afterThreshold, :streamQuality, :fps)");
    insQuery.prepare("INSERT INTO vms_scheduletask(source_id, start_time, end_time, do_record_audio, record_type, day_of_week, before_threshold, after_threshold, stream_quality, fps) VALUES (?,?,?,?,?,?,?,?,?,?)");

    insQuery.bindValue(0, internalId);
    foreach(const ApiScheduleTaskData& task, data.scheduleTasks) 
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

void restartServer();

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
        return ErrorCode::dbError; // invalid back file
    }

    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiSetResourceStatusData>& tran)
{
    QSqlQuery query(m_sdb);
    query.prepare("UPDATE vms_resource set status = :status where guid = :guid");
    query.bindValue(":status", tran.params.status);
    query.bindValue(":guid", tran.params.id.toRfc4122());
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

    result = insertOrReplaceCamera(params, internalId);
    if (result != ErrorCode::ok)
        return result;

    result = updateCameraSchedule(params, internalId);
    return result;
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiCameraData>& tran)
{
    return saveCamera(tran.params);
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiCameraDataList>& tran)
{
    foreach(const ApiCameraData& camera, tran.params)
    {
        ErrorCode result = saveCamera(camera);
        if (result != ErrorCode::ok)
            return result;
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiResourceData>& tran)
{
    qint32 internalId = getResourceInternalId(tran.params.id);
    ErrorCode err = updateResource(tran.params, internalId);
    if (err != ErrorCode::ok)
        return err;

    return ErrorCode::ok;
}

ErrorCode QnDbManager::insertBRuleResource(const QString& tableName, const QUuid& ruleGuid, const QUuid& resourceGuid)
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

    foreach(const QUuid& resourceId, rule.eventResourceIds) {
        err = insertBRuleResource("vms_businessrule_event_resources", rule.id, resourceId);
        if (err != ErrorCode::ok)
            return err;
    }

    foreach(const QUuid& resourceId, rule.actionResourceIds) {
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
    insQuery.prepare("INSERT INTO vms_layoutitem (zoom_bottom, right, uuid, zoom_left, resource_guid, \
                     zoom_right, top, layout_id, bottom, zoom_top, \
                     zoom_target_uuid, flags, contrast_params, rotation, \
                     dewarping_params, left) VALUES \
                     (:zoomBottom, :right, :id, :zoomLeft, :resourceId, \
                     :zoomRight, :top, :layoutId, :bottom, :zoomTop, \
                     :zoomTargetId, :flags, :contrastParams, :rotation, \
                     :dewarpingParams, :left)");
    foreach(const ApiLayoutItemData& item, data.items)
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

qint32 QnDbManager::getBusinessRuleInternalId( const QUuid& guid )
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare("SELECT id from vms_businessrule where guid = :guid");
    query.bindValue(":guid", guid.toRfc4122());
    if (!query.exec() || !query.next())
        return 0;
    return query.value("id").toInt();
}

ErrorCode QnDbManager::removeUser( const QUuid& guid )
{
    qint32 internalId = getResourceInternalId(guid);

    ErrorCode err = ErrorCode::ok;

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

ErrorCode QnDbManager::insertOrReplaceBusinessRuleTable( const ApiBusinessRuleData& businessRule)
{
    QSqlQuery query(m_sdb);
    query.prepare(QString("INSERT OR REPLACE INTO vms_businessrule (guid, event_type, event_condition, event_state, action_type, \
                          action_params, aggregation_period, disabled, comments, schedule, system) VALUES \
                          (:id, :eventType, :eventCondition, :eventState, :actionType, \
                          :actionParams, :aggregationPeriod, :disabled, :comment, :schedule, :system)"));
    QnSql::bind(businessRule, &query);
    if (query.exec()) {
        return ErrorCode::ok;
    }
    else {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }
}

ErrorCode QnDbManager::removeBusinessRule( const QUuid& guid )
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
    foreach(const ApiLayoutData& layout, tran.params)
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
    foreach(const ApiVideowallData& videowall, tran.params)
    {
        ErrorCode err = saveVideowall(videowall);
        if (err != ErrorCode::ok)
            return err;
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiDiscoveryDataList> &tran) {
    if (tran.command == ApiCommand::addDiscoveryInformation) {
        foreach (const ApiDiscoveryData &data, tran.params) {
            QSqlQuery query(m_sdb);
            query.prepare("INSERT OR REPLACE INTO vms_mserver_discovery (server_id, url, ignore) VALUES(:id, :url, :ignore)");
            QnSql::bind(data, &query);
            if (!query.exec()) {
                qWarning() << Q_FUNC_INFO << query.lastError().text();
                return ErrorCode::dbError;
            }
        }
    } else if (tran.command == ApiCommand::removeDiscoveryInformation) {
        foreach (const ApiDiscoveryData &data, tran.params) {
            QSqlQuery query(m_sdb);
            query.prepare("DELETE FROM vms_mserver_discovery WHERE server_id = :id AND url = :url");
            QnSql::bind(data, &query);
            if (!query.exec()) {
                qWarning() << Q_FUNC_INFO << query.lastError().text();
                return ErrorCode::dbError;
            }
        }
    }

    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiUpdateUploadResponceData>& /*tran*/) {
    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiResourceParamsData>& tran)
{
    qint32 internalId = getResourceInternalId(tran.params.id);
    /*
    ErrorCode result = deleteAddParams(internalId);
    if (result != ErrorCode::ok)
        return result;
    */
    return insertAddParams(tran.params.params, internalId);
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiCameraServerItemData>& tran)
{
    QSqlQuery lastHistory(m_sdb);
    lastHistory.prepare("SELECT server_guid, max(timestamp) FROM vms_cameraserveritem WHERE physical_id = ? AND timestamp < ?");
    lastHistory.addBindValue(tran.params.cameraUniqueId);
    lastHistory.addBindValue(tran.params.timestamp);
    if (!lastHistory.exec()) {
        qWarning() << Q_FUNC_INFO << lastHistory.lastError().text();
        return ErrorCode::dbError;
    }
    if (lastHistory.next() && lastHistory.value(0).toByteArray() == tran.params.serverId)
        return ErrorCode::skipped;

    QSqlQuery query(m_sdb);
    query.prepare("INSERT INTO vms_cameraserveritem (server_guid, timestamp, physical_id) VALUES(:serverId, :timestamp, :cameraUniqueId)");
    QnSql::bind(tran.params, &query);
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }

    return ErrorCode::ok;
}

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiPanicModeData>& tran)
{
    QSqlQuery query(m_sdb);
    query.prepare("UPDATE vms_server SET panic_mode = :mode");
    query.bindValue(QLatin1String(":mode"), (int) tran.params.mode);
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
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

ErrorCode QnDbManager::deleteTableRecord(const QUuid& id, const QString& tableName, const QString& fieldName)
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

ErrorCode QnDbManager::removeCamera(const QUuid& guid)
{
    qint32 id = getResourceInternalId(guid);

    ErrorCode err = deleteAddParams(id);
    if (err != ErrorCode::ok)
        return err;

    err = removeCameraSchedule(id);
    if (err != ErrorCode::ok)
        return err;

    err = deleteTableRecord(guid, "vms_businessrule_action_resources", "resource_guid");
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

ErrorCode QnDbManager::removeServer(const QUuid& guid)
{
    ErrorCode err;
    qint32 id = getResourceInternalId(guid);

    err = deleteAddParams(id);
    if (err != ErrorCode::ok)
        return err;

    err = removeStoragesByServer(guid);
    if (err != ErrorCode::ok)
        return err;

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

ErrorCode QnDbManager::removeLayout(const QUuid& id)
{
    return removeLayoutInternal(id, getResourceInternalId(id));
}

ErrorCode QnDbManager::removeLayoutInternal(const QUuid& id, const qint32 &internalId) {
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

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiStoredFileData>& tran)
{
    assert( tran.command == ApiCommand::addStoredFile || tran.command == ApiCommand::updateStoredFile );

    QSqlQuery query(m_sdb);
    query.prepare("INSERT OR REPLACE INTO vms_storedFiles (path, data) values (:path, :data)");
    query.bindValue(":path", tran.params.path);
    query.bindValue(":data", tran.params.data);
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }
    
    return ErrorCode::ok;
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

ApiOjectType QnDbManager::getObjectType(const QUuid& objectId)
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare("SELECT \
                  (CASE WHEN c.resource_ptr_id is null then rt.name else 'Camera' end) as name\
                  FROM vms_resource r\
                  JOIN vms_resourcetype rt on rt.guid = r.xtype_guid\
                  LEFT JOIN vms_camera c on c.resource_ptr_id = r.id\
                  WHERE r.guid = :guid");
    query.bindValue(":guid", objectId.toRfc4122());
    if (!query.exec())
        return ApiObject_NotDefined;
    if( !query.next() )
        return ApiObject_NotDefined;   //Record already deleted. That's exactly what we wanted
    QString objectType = query.value("name").toString();
    if (objectType == "Camera")
        return ApiObject_Camera;
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
            query.prepare("SELECT ?, r.guid from vms_camera c JOIN vms_resource r on r.id = c.resource_ptr_id WHERE r.parent_guid = ?");
            query.addBindValue((int) ApiObject_Camera);
            break;
        case ApiObject_User:
            query.prepare( "SELECT ?, r.guid FROM vms_resource r, vms_layout WHERE r.parent_guid = ? AND r.id = vms_layout.resource_ptr_id" );
            query.addBindValue((int) ApiObject_Layout);
            break;
        default:
            //Q_ASSERT_X(0, "Not implemented!", Q_FUNC_INFO);
            return result;
    }
    query.addBindValue(parentObject.id.toRfc4122());

    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return result;
    }
    while(query.next()) {
        ApiObjectInfo info;
        info.type = (ApiOjectType) query.value(0).toInt();
        info.id = QUuid::fromRfc4122(query.value(1).toByteArray());
        result.push_back(info);
    }

    return result;
}

bool QnDbManager::saveMiscParam( const QByteArray& name, const QByteArray& value )
{
    QnDbManager::Locker locker( this );

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

ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiIdData>& tran)
{
    switch(tran.command) {
        case ApiCommand::removeCamera:
            return removeObject(ApiObjectInfo(ApiObject_Camera, tran.params.id));
        case ApiCommand::removeMediaServer:
            return removeObject(ApiObjectInfo(ApiObject_Server, tran.params.id));
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
/*    
    ApiObjectInfoList nestedList = getNestedObjects(apiObject);
    foreach(const ApiObjectInfo& nestedObject, nestedList)
        removeObject(nestedObject);
*/
    switch (apiObject.type)
    {
    case ApiObject_Camera:
        result = removeCamera(apiObject.id);
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

ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiResourceTypeDataList& data)
{
    if (!m_cachedResTypes.empty())
    {
        data = m_cachedResTypes;
        return ErrorCode::ok;
    }

    QSqlQuery queryTypes(m_sdb);
    queryTypes.setForwardOnly(true);
    queryTypes.prepare("select rt.guid as id, rt.name, m.name as vendor \
                  from vms_resourcetype rt \
                  left join vms_manufacture m on m.id = rt.manufacture_id \
                  order by rt.guid");
    if (!queryTypes.exec()) {
        qWarning() << Q_FUNC_INFO << queryTypes.lastError().text();
        return ErrorCode::dbError;
    }
    QnSql::fetch_many(queryTypes, &data);

    QSqlQuery queryParents(m_sdb);
    queryParents.setForwardOnly(true);
    queryParents.prepare("select t1.guid as id, t2.guid as parentId \
                         from vms_resourcetype_parents p \
                         JOIN vms_resourcetype t1 on t1.id = p.from_resourcetype_id \
                         JOIN vms_resourcetype t2 on t2.id = p.to_resourcetype_id \
                         order by t1.guid, p.to_resourcetype_id desc");
	if (!queryParents.exec()) {
        qWarning() << Q_FUNC_INFO << queryParents.lastError().text();
        return ErrorCode::dbError;
    }
    mergeIdListData<ApiResourceTypeData>(queryParents, data, &ApiResourceTypeData::parentId);

    QSqlQuery queryProperty(m_sdb);
    queryProperty.setForwardOnly(true);
    queryProperty.prepare("SELECT rt.guid as resourceTypeId, pt.name, pt.type, pt.min, pt.max, pt.step, pt.[values], pt.ui_values as uiValues, pt.default_value as defaultValue, \
                          pt.netHelper as internalData, pt.[group], pt.sub_group as subGroup, pt.description, pt.ui, pt.readonly as readOnly \
                          FROM vms_propertytype pt \
                          JOIN vms_resourcetype rt on rt.id = pt.resource_type_id ORDER BY rt.guid");
    if (!queryProperty.exec()) {
        qWarning() << Q_FUNC_INFO << queryProperty.lastError().text();
        return ErrorCode::dbError;
    }

    std::vector<ApiPropertyTypeData> allProperties;
    QnSql::fetch_many(queryProperty, &allProperties);
    mergeObjectListData(data, allProperties, &ApiResourceTypeData::propertyTypes, &ApiPropertyTypeData::resourceTypeId);

    m_cachedResTypes = data;

    return ErrorCode::ok;
}

// ----------- getLayouts --------------------

ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiLayoutDataList& layouts)
{
    QSqlQuery query(m_sdb);
    QString filter; // todo: add data filtering by user here
    query.setForwardOnly(true);
    query.prepare(QString("SELECT r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentId, r.name, r.url, r.status, \
                  l.user_can_edit as editable, l.cell_spacing_height as verticalSpacing, l.locked, \
                  l.cell_aspect_ratio as cellAspectRatio, l.background_width as backgroundWidth, \
                  l.background_image_filename as backgroundImageFilename, l.background_height as backgroundHeight, \
                  l.cell_spacing_width as horizontalSpacing, l.background_opacity as backgroundOpacity, l.resource_ptr_id as id \
                  FROM vms_layout l \
                  JOIN vms_resource r on r.id = l.resource_ptr_id %1 ORDER BY r.guid").arg(filter));
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }

    QSqlQuery queryItems(m_sdb);
    queryItems.setForwardOnly(true);
    queryItems.prepare("SELECT r.guid as layoutId, li.zoom_bottom as zoomBottom, li.right, li.uuid as id, li.zoom_left as zoomLeft, li.resource_guid as resourceId, \
                       li.zoom_right as zoomRight, li.top, li.bottom, li.zoom_top as zoomTop, \
                       li.zoom_target_uuid as zoomTargetId, li.flags, li.contrast_params as contrastParams, li.rotation, li.id, \
                       li.dewarping_params as dewarpingParams, li.left FROM vms_layoutitem li \
                       JOIN vms_resource r on r.id = li.layout_id order by r.guid");

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

ErrorCode QnDbManager::doQueryNoLock(const QUuid& mServerId, ApiCameraDataList& cameraList)
{
    QSqlQuery queryCameras(m_sdb);
    QString filterStr;
    if (!mServerId.isNull()) {
        filterStr = QString("WHERE r.parent_guid = %1").arg(guidToSqlString(mServerId));
    }
    queryCameras.setForwardOnly(true);
    queryCameras.prepare(QString("SELECT r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentId, r.name, r.url, r.status, \
        c.audio_enabled as audioEnabled, c.control_enabled as controlEnabled, c.vendor, c.manually_added as manuallyAdded, \
        c.region as motionMask, c.schedule_enabled as scheduleEnabled, c.motion_type as motionType, \
        c.group_name as groupName, c.group_id as groupId, c.mac, c. model, c.secondary_quality as secondaryStreamQuality, \
		c.status_flags as statusFlags, c.physical_id as physicalId, c.password, login, c.dewarping_params as dewarpingParams, \
        c.min_archive_days as minArchiveDays, c.max_archive_days as maxArchiveDays, c.prefered_server_id as preferedServerId \
        FROM vms_resource r \
        JOIN vms_camera c on c.resource_ptr_id = r.id %1 ORDER BY r.guid").arg(filterStr));


    QSqlQuery queryScheduleTask(m_sdb);
    
    queryScheduleTask.setForwardOnly(true);
    queryScheduleTask.prepare(QString("SELECT r.guid as sourceId, st.start_time as startTime, st.end_time as endTime, st.do_record_audio as recordAudio, \
                                       st.record_type as recordingType, st.day_of_week as dayOfWeek, st.before_threshold as beforeThreshold, st.after_threshold as afterThreshold, \
                                       st.stream_quality as streamQuality, st.fps \
                                       FROM vms_scheduletask st \
                                       JOIN vms_resource r on r.id = st.source_id %1 ORDER BY r.guid").arg(filterStr));

    QSqlQuery queryParams(m_sdb);
    queryParams.setForwardOnly(true);
    QString filterStr2;
    if (!mServerId.isNull())
        filterStr2 = QString("WHERE r.parent_guid = %1").arg(guidToSqlString(mServerId));
    queryParams.prepare(QString("SELECT r.guid as resourceId, kv.value, kv.name, kv.isResTypeParam as predefinedParam\
                                 FROM vms_kvpair kv \
                                 JOIN vms_camera c on c.resource_ptr_id = kv.resource_id \
                                 JOIN vms_resource r on r.id = kv.resource_id \
                                 %1 \
                                 ORDER BY r.guid").arg(filterStr2));

    if (!queryCameras.exec()) {
        qWarning() << Q_FUNC_INFO << queryCameras.lastError().text();
        return ErrorCode::dbError;
    }
    if (!queryScheduleTask.exec()) {
        qWarning() << Q_FUNC_INFO << queryScheduleTask.lastError().text();
        return ErrorCode::dbError;
    }

    if (!queryParams.exec()) {
        qWarning() << Q_FUNC_INFO << queryParams.lastError().text();
        return ErrorCode::dbError;
    }

    QnSql::fetch_many(queryCameras, &cameraList);

    std::vector<ApiScheduleTaskWithRefData> sheduleTaskList;
    QnSql::fetch_many(queryScheduleTask, &sheduleTaskList);
    mergeObjectListData(cameraList, sheduleTaskList, &ApiCameraData::scheduleTasks, &ApiScheduleTaskWithRefData::sourceId);

    std::vector<ApiResourceParamWithRefData> params;
    QnSql::fetch_many(queryParams, &params);
    mergeObjectListData<ApiCameraData>(cameraList, params, &ApiCameraData::addParams, &ApiResourceParamWithRefData::resourceId);

    return ErrorCode::ok;
}

// ----------- getServers --------------------


ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiMediaServerDataList& serverList)
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare(QString("select r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentId, r.name, r.url, r.status, \
                          s.api_url as apiUrl, s.auth_key as authKey, s.version, s.net_addr_list as networkAddresses, s.system_info as systemInfo, \
                          s.flags, s.panic_mode as panicMode, s.max_cameras as maxCameras, s.redundancy as allowAutoRedundancy, s.system_name as systemName \
                          from vms_resource r \
                          join vms_server s on s.resource_ptr_id = r.id order by r.guid"));

    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }

    QSqlQuery queryStorage(m_sdb);
    queryStorage.setForwardOnly(true);
    queryStorage.prepare(QString("select r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentId, r.name, r.url, r.status, \
                          s.space_limit as spaceLimit, s.used_for_writing as usedForWriting \
                          from vms_resource r \
                          join vms_storage s on s.resource_ptr_id = r.id order by r.parent_guid"));

    if (!queryStorage.exec()) {
        qWarning() << Q_FUNC_INFO << queryStorage.lastError().text();
        return ErrorCode::dbError;
    }

    QnSql::fetch_many(query, &serverList);

    ApiStorageDataList storageList;
    QnSql::fetch_many(queryStorage, &storageList);

    mergeObjectListData<ApiMediaServerData, ApiStorageData>(serverList, storageList, &ApiMediaServerData::storages, &ApiStorageData::parentId);

    return ErrorCode::ok;
}

//getCameraServerItems
ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiCameraServerItemDataList& historyList)
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare(QString("select server_guid as serverId, timestamp, physical_id as cameraUniqueId from vms_cameraserveritem"));
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
ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiUserDataList& userList)
{
    //digest = md5('%s:%s:%s' % (self.user.username.lower(), 'NetworkOptix', password)).hexdigest()
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare(QString("select r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentId, r.name, r.url, r.status, \
                          u.is_superuser as isAdmin, u.email, p.digest as digest, u.password as hash, p.rights as permissions \
                          from vms_resource r \
                          join auth_user u  on u.id = r.id\
                          join vms_userprofile p on p.user_id = u.id\
                          order by r.guid"));
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }

    QSqlQuery queryParams(m_sdb);
    queryParams.setForwardOnly(true);
    queryParams.prepare(QString("SELECT r.guid as resourceId, kv.value, kv.name, kv.isResTypeParam as predefinedParam\
                                FROM vms_kvpair kv \
                                JOIN auth_user u on u.id = kv.resource_id \
                                JOIN vms_resource r on r.id = kv.resource_id \
                                WHERE kv.isResTypeParam = 0 \
                                ORDER BY r.guid"));

    if (!queryParams.exec()) {
        qWarning() << Q_FUNC_INFO << queryParams.lastError().text();
        return ErrorCode::dbError;
    }

    QnSql::fetch_many(query, &userList);

    std::vector<ApiResourceParamWithRefData> params;
    QnSql::fetch_many(queryParams, &params);
    mergeObjectListData<ApiUserData>(userList, params, &ApiUserData::addParams, &ApiResourceParamWithRefData::resourceId);
    
    return ErrorCode::ok;
}

//getVideowallList
ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiVideowallDataList& videowallList) {
    QSqlQuery query(m_sdb);
    QString filter; // todo: add data filtering by user here
    query.setForwardOnly(true);
    query.prepare(QString("SELECT r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentId, r.name, r.url, r.status, \
                          l.autorun \
                          FROM vms_videowall l \
                          JOIN vms_resource r on r.id = l.resource_ptr_id %1 ORDER BY r.guid").arg(filter));
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return ErrorCode::dbError;
    }
    QnSql::fetch_many(query, &videowallList);

    QSqlQuery queryItems(m_sdb);
    queryItems.setForwardOnly(true);
    queryItems.prepare("SELECT \
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

    queryScreens.prepare("SELECT \
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
    queryMatrixItems.prepare("SELECT \
                             item.matrix_guid as matrixGuid, \
                             item.item_guid as itemGuid, \
                             item.layout_guid as layoutGuid, \
                             matrix.videowall_guid \
                             FROM vms_videowall_matrix_items item \
                             JOIN vms_videowall_matrix matrix ON matrix.guid = item.matrix_guid \
                             ORDER BY matrix.videowall_guid");
    if (!queryMatrixItems.exec()) {
        qWarning() << Q_FUNC_INFO << queryMatrixItems.lastError().text();
        return ErrorCode::dbError;
    }
    std::vector<ApiVideowallMatrixItemWithRefData> matrixItems;
    QnSql::fetch_many(queryMatrixItems, &matrixItems);

    QSqlQuery queryMatrices(m_sdb);
    queryMatrices.setForwardOnly(true);
    queryMatrices.prepare("SELECT \
                          matrix.guid as id, \
                          matrix.name, \
                          matrix.videowall_guid as videowallGuid \
                          FROM vms_videowall_matrix matrix ORDER BY videowallGuid");
    if (!queryMatrices.exec()) {
        qWarning() << Q_FUNC_INFO << queryMatrices.lastError().text();
        return ErrorCode::dbError;
    }
    std::vector<ApiVideowallMatrixWithRefData> matrices;
    QnSql::fetch_many(queryMatrices, &matrices);
    mergeObjectListData(matrices, matrixItems, &ApiVideowallMatrixData::items, &ApiVideowallMatrixItemWithRefData::matrixGuid);

    mergeObjectListData(videowallList, matrices, &ApiVideowallData::matrices, &ApiVideowallMatrixWithRefData::videowallGuid);

    return ErrorCode::ok;
}

//getBusinessRules
ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ApiBusinessRuleDataList& businessRuleList)
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare(QString("SELECT guid as id, event_type as eventType, event_condition as eventCondition, event_state as eventState, action_type as actionType, \
                          action_params as actionParams, aggregation_period as aggregationPeriod, disabled, comments as comment, schedule, system \
                          FROM vms_businessrule order by guid"));
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
ErrorCode QnDbManager::doQueryNoLock(const QUuid& resourceId, ApiResourceParamsData& params)
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare(QString("SELECT kv.value, kv.name, kv.isResTypeParam as predefinedParam\
                                FROM vms_kvpair kv \
                                JOIN vms_resource r on r.id = kv.resource_id WHERE r.guid = :guid"));
    query.bindValue(QLatin1String(":guid"), resourceId.toRfc4122());
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text() << query.lastQuery();
        return ErrorCode::dbError;
    }

    QnSql::fetch_many(query, &params.params);

    params.id = resourceId;
    return ErrorCode::ok;
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
    QFile f(m_sdb.databaseName());
    if (!f.open(QFile::ReadOnly))
        return ErrorCode::failure;
    data.data = f.readAll();
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

    if ((err = doQueryNoLock(QUuid(), data.cameras)) != ErrorCode::ok)
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

    std::vector<ApiResourceParamWithRefData> kvPairs;
    QSqlQuery queryParams(m_sdb);
    queryParams.setForwardOnly(true);
    queryParams.prepare(QString("SELECT r.guid as resourceId, kv.value, kv.name, kv.isResTypeParam as predefinedParam\
                                FROM vms_kvpair kv \
                                JOIN vms_resource r on r.id = kv.resource_id \
                                ORDER BY r.guid"));
    if (!queryParams.exec())
        return ErrorCode::dbError;

    QnSql::fetch_many(queryParams, &kvPairs);

    mergeObjectListData<ApiMediaServerData>(data.servers,   kvPairs, &ApiMediaServerData::addParams, &ApiResourceParamWithRefData::resourceId);
    mergeObjectListData<ApiCameraData>(data.cameras,        kvPairs, &ApiCameraData::addParams,      &ApiResourceParamWithRefData::resourceId);
    mergeObjectListData<ApiUserData>(data.users,            kvPairs, &ApiUserData::addParams,        &ApiResourceParamWithRefData::resourceId);
    mergeObjectListData<ApiLayoutData>(data.layouts,        kvPairs, &ApiLayoutData::addParams,      &ApiResourceParamWithRefData::resourceId);
    mergeObjectListData<ApiVideowallData>(data.videowalls,  kvPairs, &ApiVideowallData::addParams,   &ApiResourceParamWithRefData::resourceId);

    return err;
}

//getParams
ErrorCode QnDbManager::doQueryNoLock(const nullptr_t& /*dummy*/, ec2::ApiResourceParamDataList& data)
{
    ApiResourceParamsData params;
    ErrorCode rez = doQueryNoLock(m_adminUserID, params);
    if (rez == ErrorCode::ok)
        data = params.params;
    return rez;
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

    foreach (const ApiBusinessRuleData& rule, tran.params.defaultRules)
    {
        ErrorCode rez = updateBusinessRule(rule);
        if (rez != ErrorCode::ok)
            return rez;
    }

    return ErrorCode::ok;
}

// save settings
ErrorCode QnDbManager::executeTransactionInternal(const QnTransaction<ApiResourceParamDataList>& tran)
{
    /*
    ErrorCode result = deleteAddParams(m_adminUserInternalID);
    if (result != ErrorCode::ok)
        return result;
    */
    return insertAddParams(tran.params, m_adminUserInternalID);
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
    foreach (const ApiLicenseData& license, tran.params) {
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
    foreach(const ApiVideowallItemData& item, data.items)
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

    QSet<QUuid> pcUuids;

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

        foreach(const ApiVideowallScreenData& screen, data.screens)
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
        foreach (const QUuid &pcUuid, pcUuids) {
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

    foreach(const ApiVideowallMatrixData &matrix, data.matrices) {
        QnSql::bind(matrix, &insQuery);
        insQuery.bindValue(":videowall_guid", data.id.toRfc4122());

        if (!insQuery.exec()) {
            qWarning() << Q_FUNC_INFO << insQuery.lastError().text();
            return ErrorCode::dbError;
        }

        insItemsQuery.bindValue(":matrix_guid", matrix.id.toRfc4122());
        foreach(const ApiVideowallMatrixItemData &item, matrix.items) {
            QnSql::bind(item, &insItemsQuery);
            if (!insItemsQuery.exec()) {
                qWarning() << Q_FUNC_INFO << insItemsQuery.lastError().text();
                return ErrorCode::dbError;
            }    
        }
    }
    return ErrorCode::ok;
}

ErrorCode QnDbManager::deleteVideowallPcs(const QUuid &videowall_guid) {
    return deleteTableRecord(videowall_guid, "vms_videowall_pcs", "videowall_guid");
}

ErrorCode QnDbManager::deleteVideowallItems(const QUuid &videowall_guid) {
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

ErrorCode QnDbManager::deleteVideowallMatrices(const QUuid &videowall_guid) {
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

ErrorCode QnDbManager::removeVideowall(const QUuid& guid) {
    qint32 id = getResourceInternalId(guid);

    ErrorCode err = deleteAddParams(id);
    if (err != ErrorCode::ok)
        return err;

    err = deleteVideowallMatrices(guid);
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

ErrorCode QnDbManager::removeLayoutFromVideowallItems(const QUuid &layout_id) {
    QByteArray emptyId = QUuid().toRfc4122();

    QSqlQuery query(m_sdb);
    query.prepare("UPDATE vms_videowall_item set layout_guid = :empty_id WHERE layout_guid = :layout_id");
    query.bindValue(":empty_id", emptyId);
    query.bindValue(":layout_id", layout_id.toRfc4122());
    if (query.exec())
        return ErrorCode::ok;

    qWarning() << Q_FUNC_INFO << query.lastError().text();
    return ErrorCode::dbError;
}

bool QnDbManager::markLicenseOverflow(bool value, qint64 time)
{
    if (m_licenseOverflowMarked == value)
        return true;
    m_licenseOverflowMarked = value;
    m_licenseOverflowTime = value ? time : 0;

    QSqlQuery query(m_sdb);
    query.prepare("INSERT OR REPLACE into misc_data (key, data) values(?, ?) ");
    query.addBindValue(LICENSE_EXPIRED_TIME_KEY);
    query.addBindValue(QByteArray::number(m_licenseOverflowTime));
    if (!query.exec()) {
        qWarning() << Q_FUNC_INFO << query.lastError().text();
        return false;
    }

    QnPeerRuntimeInfo localInfo = QnRuntimeInfoManager::instance()->localInfo();
    if (localInfo.data.prematureLicenseExperationDate != m_licenseOverflowTime) {
        localInfo.data.prematureLicenseExperationDate = m_licenseOverflowTime;
        QnRuntimeInfoManager::instance()->items()->updateItem(localInfo.uuid, localInfo);
    }
    
    return true;
}

QUuid QnDbManager::getID() const
{
    return m_dbInstanceId;
}

}
