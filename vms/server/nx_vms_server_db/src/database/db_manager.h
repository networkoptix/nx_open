#pragma once

#include <memory>

#include <QtSql/QSqlError>

#include <common/common_module_aware.h>
#include <common/common_module.h>
#include <core/resource_access/user_access_data.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource/user_resource.h>
#include <database/api/db_resource_api.h>
#include <nx_ec/ec_api.h>
#include <nx_ec/data/api_fwd.h>
#include <transaction/transaction.h>
#include <transaction/transaction_log.h>
#include <utils/db/db_helper.h>

#include <nx/fusion/serialization/sql.h>
#include <nx/utils/log/log.h>
#include <nx/utils/singleton.h>
#include <nx/vms/api/data_fwd.h>
#include <rest/request_type_wrappers.h>
#include <nx/vms/api/data/runtime_data.h>
#include <nx/vms/api/data/lock_data.h>
#include <nx/vms/event/event_fwd.h>
#include <nx/metrics/metrics_storage.h>

struct BeforeRestoreDbData;

namespace ec2
{

class TimeSynchronizationManager;

namespace db
{
    bool migrateAccessRightsToUbjsonFormat(QSqlDatabase& database, detail::QnDbManager* db);
}

namespace aux {
bool applyRestoreDbData(const BeforeRestoreDbData& restoreData, const QnUserResourcePtr& admin);
}

class LicenseManagerImpl;

enum ApiObjectType
{
    ApiObject_NotDefined,
    ApiObject_Server,
    ApiObject_Camera,
    ApiObject_User,
    ApiObject_Layout,
    ApiObject_Videowall,
    ApiObject_BusinessRule,
    ApiObject_Storage,
    ApiObject_WebPage,
    ApiObject_AnalyticsPlugin,
    ApiObject_AnalyticsEngine,
    ApiObjectUserRole,
};

struct ApiObjectInfo
{
    ApiObjectInfo() {}
    ApiObjectInfo(const ApiObjectType& type, const QnUuid& id): type(type), id(id) {}

    ApiObjectType type;
    QnUuid id;
};

class ApiObjectInfoList: public std::vector<ApiObjectInfo>
{
public:
    std::vector<nx::vms::api::IdData> toIdList() const
    {
        std::vector<nx::vms::api::IdData> result;
        result.reserve(size());
        for (size_t i = 0; i < size(); ++i)
            result.push_back(nx::vms::api::IdData(at(i).id));
        return result;
    }
};

class QnDbManagerAccess;
class ServerQueryProcessor;

namespace detail
{

    class QnDbManager
    :
        public QObject,
        public QnDbHelper,
        public QnCommonModuleAware
    {
        Q_OBJECT

        friend class ::ec2::QnDbManagerAccess;
        friend bool ::ec2::db::migrateAccessRightsToUbjsonFormat(QSqlDatabase& database, detail::QnDbManager* db);
        friend class PersistentStorage;
    public:
        QnDbManager(QnCommonModule* commonModule);
        virtual ~QnDbManager();

        bool init(const nx::utils::Url& dbUrl);
        bool isInitialized() const;
        static QString ecsDbFileName(const QString& basePath);
        static int currentBuildNumber(const QString& basePath);

        template <class T>
        ErrorCode executeTransactionNoLock(const QnTransaction<T>& tran, const QByteArray& serializedTran)
        {
            NX_ASSERT(!tran.persistentInfo.isNull(), "You must register transaction command in persistent command list!");
            if (!tran.isLocal()) {
                QnTransactionLog::ContainsReason isContains = m_tranLog->contains(tran);
                if (isContains == QnTransactionLog::Reason_Timestamp)
                    return ErrorCode::containsBecauseTimestamp;
                else if (isContains == QnTransactionLog::Reason_Sequence)
                    return ErrorCode::containsBecauseSequence;
            }
            ErrorCode result = executeTransactionInternal(tran);
            if (result != ErrorCode::ok)
            {
                commonModule()->metrics()->transactions().errors()++;
                return result;
            }
            commonModule()->metrics()->transactions().success()++;
            if (tran.isLocal())
            {
                commonModule()->metrics()->transactions().local()++;
                return ErrorCode::ok;
            }
            return m_tranLog->saveTransaction( tran, serializedTran);
        }

        ErrorCode executeTransactionNoLock(
            const QnTransaction<nx::vms::api::DatabaseDumpData>& tran,
            const QByteArray& /*serializedTran*/)
        {
            return executeTransactionInternal(tran);
        }

        template <class T>
        ErrorCode executeTransaction(const QnTransaction<T>& tran, const QByteArray& serializedTran)
        {
            NX_ASSERT(!tran.persistentInfo.isNull(), "You must register transaction command in persistent command list!");
            QnDbTransactionLocker lock(getTransaction());
            ErrorCode result = executeTransactionNoLock(tran, serializedTran);
            if (result == ErrorCode::ok) {
                if (!lock.commit())
                {
                    NX_WARNING(QnLog::EC2_TRAN_LOG, lit("Commit error while executing transaction %1: %2").
                        arg(toString(result)).arg(m_sdb.lastError().text()));
                    return ErrorCode::dbError;
                }
            }
            return result;
        }


        // --------- get methods ---------------------

        template <class T1, class T2>
        ErrorCode doQuery(const T1& t1, T2& t2)
        {
            QnWriteLocker lock(&m_mutex);
            return doQueryNoLock(t1, t2);
        }

        template <class OutputData>
        ErrorCode execSqlQuery(const QString& queryStr, OutputData* outputData)
        {
            QnWriteLocker lock(&m_mutex);
            QSqlQuery query(m_sdb);
            query.setForwardOnly(true);
            if (!prepareSQLQuery(&query, queryStr, Q_FUNC_INFO) || !query.exec())
                return ErrorCode::dbError;
            QnSql::fetch_many(query, outputData);
            return ErrorCode::ok;
        }

        //dumpDatabase
        ErrorCode doQuery(const std::nullptr_t& /*dummy*/, nx::vms::api::DatabaseDumpData& data);
        ErrorCode doQuery(
            const nx::vms::api::StoredFilePath& path,
            nx::vms::api::DatabaseDumpToFileData& dumpFileSize);

        // --------- misc -----------------------------
        QnUuid getID() const;
        bool updateId();

        ApiObjectType getObjectType(const QnUuid& objectId)
        {
            QnWriteLocker lock( &m_mutex );
            return getObjectTypeNoLock( objectId );
        }
        /*!
            \note This overload should be called within transaction
        */
        ApiObjectType getObjectTypeNoLock(const QnUuid& objectId);
        ApiObjectInfoList getNestedObjectsNoLock(const ApiObjectInfo& parentObject);
        ApiObjectInfoList getObjectsNoLock(const ApiObjectType& objectType);

        //!Reads settings (properties of user 'admin')

        void setTransactionLog(QnTransactionLog* tranLog);
        QnTransactionLog* transactionLog() const;
        virtual bool tuneDBAfterOpen(QSqlDatabase* const sqlDb) override;
        ec2::database::api::QueryCache::Pool* queryCachePool();

    signals:
        //!Emitted after \a QnDbManager::init was successfully executed
        void initialized();

    private:
        enum FilterType
        {
            RES_ID_FIELD,
            RES_TYPE_FIELD,
            RES_PARENT_ID_FIELD
        };

        //!query filter
        class QnQueryFilter
        {
        public:
            //filtered field,
            QMap<int, QVariant> fields;
        };


        friend class ::ec2::QnTransactionLog;
        QSqlDatabase& getDB() { return m_sdb; }
        QnReadWriteLock& getMutex() { return m_mutex; }

        // ------------ data retrieval --------------------------------------
        ErrorCode doQueryNoLock(std::nullptr_t /*dummy*/, nx::vms::api::ResourceParamDataList& data);

        //listDirectory
        ErrorCode doQueryNoLock(
            const nx::vms::api::StoredFilePath& path,
            nx::vms::api::StoredFilePathList& data);
        //getStorageData
        ErrorCode doQueryNoLock(
            const nx::vms::api::StoredFilePath& path,
            nx::vms::api::StoredFileData& data);

        //getStoredFiles
        ErrorCode doQueryNoLock(
            const nx::vms::api::StoredFilePath& path,
            nx::vms::api::StoredFileDataList& data);

        ErrorCode doQueryNoLock(const std::nullptr_t&, nx::vms::api::StoredFileDataList& data)
        {
            return doQueryNoLock(nx::vms::api::StoredFilePath(), data);
        }

        ErrorCode doQueryNoLock(const QByteArray &paramName, nx::vms::api::MiscData& miscData);
        ErrorCode doQueryNoLock(
            const std::nullptr_t& /*dummy*/,
            nx::vms::api::SystemMergeHistoryRecordList& systemMergeHistory);
        //getResourceTypes
        ErrorCode doQueryNoLock(
            const std::nullptr_t& /*dummy*/,
            nx::vms::api::ResourceTypeDataList& resourceTypeList);

        //getCameras
        ErrorCode doQueryNoLock(const QnUuid& id, nx::vms::api::CameraDataList& cameraList);

        //getStorages
        ErrorCode getStorages(const QString& filterStr, nx::vms::api::StorageDataList& storageList);
        ErrorCode doQueryNoLock(
            const nx::vms::api::StorageParentId& parentId, nx::vms::api::StorageDataList& storageList);
        ErrorCode doQueryNoLock(
            const QnUuid& storageId, nx::vms::api::StorageDataList& storageList);

        //get resource status
        ErrorCode doQueryNoLock(
            const QnUuid& resId,
            nx::vms::api::ResourceStatusDataList& statusList);

        //getCameraUserAttributesList
        ErrorCode doQueryNoLock(
            const QnUuid& cameraId,
            nx::vms::api::CameraAttributesDataList& cameraUserAttributesList);

        //getCamerasEx
        ErrorCode doQueryNoLock(const QnCameraDataExQuery& query,
			nx::vms::api::CameraDataExList& cameraList);

        //getServers
        ErrorCode doQueryNoLock(const QnUuid& id, nx::vms::api::MediaServerDataList& serverList);

        //getServersEx
        ErrorCode doQueryNoLock(const QnUuid& id, nx::vms::api::MediaServerDataExList& serverList);

        //getCameraServerItems
        ErrorCode doQueryNoLock(
            const std::nullptr_t& /*dummy*/,
            nx::vms::api::ServerFootageDataList& historyList);

        //getUserList
        ErrorCode doQueryNoLock(const QnUuid& id, nx::vms::api::UserDataList& userList);

        //getUserRoleList
        ErrorCode doQueryNoLock(const QnUuid& id, nx::vms::api::UserRoleDataList& userRoleList);

        //getPredefinedRoles
        ErrorCode doQueryNoLock(const std::nullptr_t& /*dummy*/,
            nx::vms::api::PredefinedRoleDataList& rolesList);

        //getAccessRights
        ErrorCode doQueryNoLock(const std::nullptr_t& /*dummy*/,
            nx::vms::api::AccessRightsDataList& accessRightsList);

        //getVideowallList
        ErrorCode doQueryNoLock(const QnUuid& id, nx::vms::api::VideowallDataList& videowallList);

        //getWebPageList
        ErrorCode doQueryNoLock(const QnUuid& id, nx::vms::api::WebPageDataList& webPageList);

        //getAnalyticsPluginList
        ErrorCode doQueryNoLock(
            const QnUuid& id,
            nx::vms::api::AnalyticsPluginDataList& outAnalyticsPluginList);

        //getAnalyticsEnginesList
        ErrorCode doQueryNoLock(
            const QnUuid& id,
            nx::vms::api::AnalyticsEngineDataList& outAnalyticsEngineList);

        //getBusinessRuleList
        ErrorCode doQueryNoLock(const QnUuid& id, nx::vms::api::EventRuleDataList& userList);

        //getBusinessRuleList
        ErrorCode doQueryNoLock(const QnUuid& id, nx::vms::api::LayoutDataList& layoutList);

        //getResourceParams
        ErrorCode doQueryNoLock(
            const QnUuid& resourceId,
            nx::vms::api::ResourceParamWithRefDataList& params);

        // FullInfoData
        ErrorCode readFullInfoDataComplete(nx::vms::api::FullInfoData* data);

        // FullInfoData abridged for Mobile Client
        ErrorCode readFullInfoDataForMobileClient(
            nx::vms::api::FullInfoData* data, const QnUuid& userId);

        //getLicenses
        ErrorCode doQueryNoLock(
            const std::nullptr_t& /*dummy*/, nx::vms::api::LicenseDataList& data);

        // DiscoveryDataList
        ErrorCode doQueryNoLock(const QnUuid& id, nx::vms::api::DiscoveryDataList& data);

        //getMediaServerUserAttributesList
        ErrorCode doQueryNoLock(const QnUuid& mServerId,
            nx::vms::api::MediaServerUserAttributesDataList& serverAttrsList);

        //getTransactionLog
        ErrorCode doQueryNoLock(const ApiTranLogFilter&, ApiTransactionDataList& tranList);

        // Stub - acts as if nothing is found in the database. Needed for merge algorithm.
        ErrorCode doQueryNoLock(
            const QnUuid& /*id*/, nx::vms::api::UpdateUploadResponseDataList& data);

        // getLayoutTours
        ErrorCode doQueryNoLock(
            const QnUuid& id,
            nx::vms::api::LayoutTourDataList& data);

        // ------------ transactions --------------------------------------

        template<typename T>
        ErrorCode executeTransactionInternal(const QnTransaction<T>&)
        {
            NX_ASSERT(0, "This function should be explicitely specialized");
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<nx::vms::api::CameraData>& tran);
        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::CameraAttributesData>& tran);
        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::CameraAttributesDataList>& tran);
        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::MediaServerData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<nx::vms::api::StorageData>& tran);
        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::MediaServerUserAttributesData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<nx::vms::api::LayoutData>& tran);
        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::LayoutTourData>& tran);
        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::LayoutDataList>& tran);
        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::ResourceStatusData>& tran);
        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::ResourceParamWithRefData>& tran);
        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::ServerFootageData>& tran);
        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::StoredFileData>& tran);
        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::StoredFilePath>& tran);
        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::EventRuleData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<nx::vms::api::UserData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<nx::vms::api::UserRoleData>& tran);
        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::AccessRightsData>& tran);

        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::ResetEventRulesData>& /*tran*/)
        {
            NX_ASSERT(false, "This transaction can't be executed directly!");
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::VideowallData>& tran);
        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::UpdateUploadResponseData>& tran);
        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::VideowallDataList>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<nx::vms::api::WebPageData>& tran);
        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::WebPageDataList>& tran);

        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::AnalyticsPluginData>& tran);
        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::AnalyticsEngineData>& tran);

        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::AnalyticsPluginDataList>& tran);
        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::AnalyticsEngineDataList>& tran);

        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::DiscoveryData>& tran);
        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::DatabaseDumpData>& tran);
        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::ClientInfoData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<nx::vms::api::MiscData>& tran);
        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::SystemMergeHistoryRecord>& tran);

        // delete camera, server, layout, any resource, etc.
        ErrorCode executeTransactionInternal(const QnTransaction<nx::vms::api::IdData>& tran);

        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::LicenseDataList>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<nx::vms::api::LicenseData>& tran);

        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::IdDataList>& /*tran*/)
        {
            NX_ASSERT(false, "This is a non persistent transaction!");
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::MediaServerUserAttributesDataList>& /*tran*/)
        {
            NX_ASSERT(false, "This is a non persistent transaction!");
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::CameraDataList>& /*tran*/)
        {
            NX_ASSERT(false, "This is a non persistent transaction!");
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::StorageDataList>& /*tran*/)
        {
            NX_ASSERT(false, "This is a non persistent transaction!");
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::ResourceParamDataList>& /*tran*/)
        {
            NX_ASSERT(false, "This is a non persistent transaction!");
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::ResourceParamWithRefDataList>& /*tran*/)
        {
            NX_ASSERT(false, "This is a non persistent transaction!");
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<nx::vms::api::EmailSettingsData>&)
        {
            NX_ASSERT(false, "This is a non persistent transaction!");
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<nx::vms::api::FullInfoData>&)
        {
            NX_ASSERT(false, "This is a non persistent transaction!");
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<nx::vms::api::EventActionData>&)
        {
            NX_ASSERT(false, "This is a non persistent transaction!");
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::VideowallControlMessageData>&)
        {
            NX_ASSERT(false, "This is a non persistent transaction!");
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<nx::vms::api::UpdateUploadData>&)
        {
            NX_ASSERT(false, "This is a non persistent transaction!");
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::DiscoveredServerData>&)
        {
            NX_ASSERT(false, "This is a non persistent transaction!");
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::DiscoveredServerDataList>&)
        {
            NX_ASSERT(false, "This is a non persistent transaction!");
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<nx::vms::api::DiscoverPeerData>&)
        {
            NX_ASSERT(false, "This is a non persistent transaction!");
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<nx::vms::api::SystemIdData>&)
        {
            NX_ASSERT(false, "This is a non persistent transaction!");
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<nx::vms::api::LockData>&)
        {
            NX_ASSERT(false, "This is a non persistent transaction!");
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<nx::vms::api::RuntimeData>&)
        {
            NX_ASSERT(false, "This is a non persistent transaction!");
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<nx::vms::api::PeerAliveData>&)
        {
            NX_ASSERT(false, "This is a non persistent transaction!");
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<nx::vms::api::UpdateInstallData>&)
        {
            NX_ASSERT(false, "This is a non persistent transaction!");
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<nx::vms::api::SyncRequestData>&)
        {
            NX_ASSERT(false, "This is a non persistent transaction!");
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<nx::vms::api::TranStateResponse>&)
        {
            NX_ASSERT(false, "This is a non persistent transaction!");
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<nx::vms::api::LicenseOverflowData>&);
        ErrorCode executeTransactionInternal(const QnTransaction<nx::vms::api::CleanupDatabaseData>& tran);

        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::UpdateSequenceData>&)
        {
            NX_ASSERT(false, "This is a non persistent transaction!");
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<nx::vms::api::TranSyncDoneData>&)
        {
            NX_ASSERT(false, "This is a non persistent transaction!");
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<nx::vms::api::DiscoveryDataList>&)
        {
            NX_ASSERT(false, "This is a non persistent transaction!");
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(
            const QnTransaction<nx::vms::api::ReverseConnectionData>&)
        {
            NX_ASSERT(false, "This is a non persistent transaction!");
            return ErrorCode::notImplemented;
        }

        ErrorCode deleteTableRecord(const QnUuid& id, const QString& tableName, const QString& fieldName);
        ErrorCode deleteTableRecord(const qint32& internalId, const QString& tableName, const QString& fieldName);

        ErrorCode saveMiscParam(const nx::vms::api::MiscData& params);
        ErrorCode readSettings(nx::vms::api::ResourceParamDataList& settings);

        ErrorCode insertOrReplaceResource(const nx::vms::api::ResourceData& data,
            qint32* internalId);
        ErrorCode deleteRecordFromResourceTable(const qint32 id);
        ErrorCode removeObject(const ApiObjectInfo& apiObject);

        ErrorCode insertAddParam(const nx::vms::api::ResourceParamWithRefData& param);
        ErrorCode fetchResourceParams(
            const QnQueryFilter& filter,
            nx::vms::api::ResourceParamWithRefDataList& params);

        ErrorCode saveCamera(const nx::vms::api::CameraData& params);
        ErrorCode insertOrReplaceCamera(const nx::vms::api::CameraData& data, qint32 internalId);
        ErrorCode saveCameraUserAttributes(const nx::vms::api::CameraAttributesData& attrs);
        ErrorCode insertOrReplaceCameraAttributes(
            const nx::vms::api::CameraAttributesData& data,
            qint32* const internalId);
        ErrorCode removeCameraAttributes(const QnUuid& id);
        ErrorCode removeResourceStatus(const QnUuid& id);
        ErrorCode updateCameraSchedule(
            const std::vector<nx::vms::api::ScheduleTaskData>& scheduleTasks,
            qint32 internalId);
        ErrorCode removeCameraSchedule(qint32 internalId);
        ErrorCode removeCamera(const QnUuid& guid);
        ErrorCode removeStorage(const QnUuid& guid);
        ErrorCode removeParam(const nx::vms::api::ResourceParamWithRefData& data);
        ErrorCode deleteCameraServerItemTable(qint32 id);

        ErrorCode insertOrReplaceMediaServer(
            const nx::vms::api::MediaServerData& data, qint32 internalId);
        ErrorCode insertOrReplaceMediaServerUserAttributes(
            const nx::vms::api::MediaServerUserAttributesData& attrs);
        ErrorCode removeServer(const QnUuid& guid);
        ErrorCode removeMediaServerUserAttributes(const QnUuid& guid);

        ErrorCode removeLayout(const QnUuid& id);
        ErrorCode saveLayout(const nx::vms::api::LayoutData& params);

        ErrorCode removeLayoutTour(const QnUuid& id);
        ErrorCode saveLayoutTour(const nx::vms::api::LayoutTourData& params);

        ErrorCode deleteUserProfileTable(const qint32 id);
        ErrorCode removeUser( const QnUuid& guid );
        ErrorCode insertOrReplaceUser(const nx::vms::api::UserData& data, qint32 internalId);
        ErrorCode insertOrReplaceUserRole(const nx::vms::api::UserRoleData& data);
        ErrorCode removeUserRole( const QnUuid& guid );
        ErrorCode setAccessRights(const nx::vms::api::AccessRightsData& data);
        ErrorCode cleanAccessRights(const QnUuid& userOrRoleId);

        ErrorCode saveVideowall(const nx::vms::api::VideowallData& params);
        ErrorCode removeVideowall(const QnUuid& id);
        ErrorCode insertOrReplaceVideowall(
            const nx::vms::api::VideowallData& data,
            qint32 internalId);
        ErrorCode deleteVideowallPcs(const QnUuid& videowall_guid);
        ErrorCode deleteVideowallItems(const QnUuid& videowall_guid);
        ErrorCode updateVideowallItems(const nx::vms::api::VideowallData& data);
        ErrorCode updateVideowallScreens(const nx::vms::api::VideowallData& data);
        ErrorCode deleteVideowallMatrices(const QnUuid& videowall_guid);
        ErrorCode updateVideowallMatrices(const nx::vms::api::VideowallData& data);

        ErrorCode saveWebPage(const nx::vms::api::WebPageData& params);
        ErrorCode removeWebPage(const QnUuid &id);
        ErrorCode insertOrReplaceWebPage(const nx::vms::api::WebPageData& data, qint32 internalId);

        ErrorCode saveAnalyticsPlugin(const nx::vms::api::AnalyticsPluginData& params);
        ErrorCode removeAnalyticsPlugin(const QnUuid &id);

        ErrorCode saveAnalyticsEngine(const nx::vms::api::AnalyticsEngineData& params);
        ErrorCode removeAnalyticsEngine(const QnUuid &id);

        ErrorCode insertOrReplaceBusinessRuleTable( const nx::vms::api::EventRuleData& businessRule);
        ErrorCode insertBRuleResource(const QString& tableName, const QnUuid& ruleGuid, const QnUuid& resourceGuid);
        ErrorCode removeBusinessRule( const QnUuid& id );
        ErrorCode updateBusinessRule(const nx::vms::api::EventRuleData& rule);

        ErrorCode saveLicense(const nx::vms::api::LicenseData& license);
        ErrorCode saveLicense(const nx::vms::api::LicenseData& license, QSqlDatabase& database);
        ErrorCode removeLicense(const nx::vms::api::LicenseData& license);
        ErrorCode removeLicense(const nx::vms::api::LicenseData& license, QSqlDatabase& database);

        ErrorCode insertOrReplaceStoredFile(const QString &fileName, const QByteArray &fileContents);

        bool createDatabase();

        qint32 getResourceInternalId( const QnUuid& guid );
        QnUuid getResourceGuid(const qint32 &internalId);

        bool isReadOnly() const { return m_dbReadOnly; }
    public:
        class QnDbTransactionExt: public QnDbTransaction
        {
        public:
            QnDbTransactionExt(
                QSqlDatabase& database,
                QnTransactionLog* tranLog,
                QnReadWriteLock& mutex)
                :
                QnDbTransaction(database, mutex),
                m_tranLog(tranLog),
                m_lazyTranInProgress(false)
            {
            }

            virtual bool beginTran() override;
            virtual void rollback() override;
            virtual bool commit() override;

            bool beginLazyTran();
            bool commitLazyTran();
        private:
            void physicalCommitLazyData();
        private:
            friend class QnLazyTransactionLocker;
            friend class QnDbManager; //< Owner

            QnTransactionLog* m_tranLog;
            bool m_lazyTranInProgress;
        };

        class QnLazyTransactionLocker: public QnAbstractTransactionLocker
        {
        public:
            QnLazyTransactionLocker(QnDbTransactionExt* tran);
            virtual ~QnLazyTransactionLocker();
            virtual bool commit() override;

        private:
            bool m_committed;
            QnDbTransactionExt* m_tran;
        };

        virtual QnDbTransactionExt* getTransaction() override;
    private:

        enum GuidConversionMethod {CM_Default, CM_Binary, CM_MakeHash, CM_String, CM_INT};

        enum ResyncFlag
        {
            None                    =      0,
            ClearLog                =    0x1,
            ResyncLog               =    0x2,
            ResyncLicences          =    0x4,
            ResyncFiles             =    0x8,
            ResyncCameraAttributes  =   0x10,
            ResyncServerAttributes  =   0x20,
            ResyncServers           =   0x40,
            ResyncLayouts           =   0x80,
            ResyncRules             =  0x100,
            ResyncUsers             =  0x200,
            ResyncStorages          =  0x400,
            ResyncClientInfo        =  0x800, //< removed since 3.1
            ResyncVideoWalls        = 0x1000,
            ResyncWebPages          = 0x2000,
            ResyncUserAccessRights  = 0x4000,
            ResyncGlobalSettings    = 0x8000,
            ResyncResourceProperties = 0x10000,
            ResyncAnalyticsPlugins = 0x20000,
            ResyncAnalyticsEngines = 0x40000,
       };
        Q_DECLARE_FLAGS(ResyncFlags, ResyncFlag)

        QMap<int, QnUuid> getGuidList(const QString& request, GuidConversionMethod method, const QByteArray& intHashPostfix = QByteArray());

        bool updateTableGuids(const QString& tableName, const QString& fieldName, const QMap<int, QnUuid>& guids);
        bool updateResourceTypeGuids();
        bool updateGuids();
        bool updateBusinessRulesGuids();
        bool updateBusinessActionParameters();
        QnUuid getType(const QString& typeName);
        bool resyncTransactionLog();
        bool addStoredFiles(const QString& baseDirectoryName, int* count = 0);

        template <class FilterDataType, class ObjectType, class ObjectListType>
        bool fillTransactionLogInternal(
            ApiCommand::Value command,
            std::function<bool (ObjectType& data)> updater = nullptr,
            FilterDataType filter = FilterDataType());

        template <class ObjectListType>
        bool queryObjects(ObjectListType& objects);

        virtual bool beforeInstallUpdate(const QString& updateName) override;
        virtual bool afterInstallUpdate(const QString& updateName) override;

        ErrorCode addCameraHistory(const nx::vms::api::ServerFootageData& params);
        ErrorCode removeCameraHistory(const QnUuid& serverId);
        ErrorCode getScheduleTasks(std::vector<nx::vms::api::ScheduleTaskWithRefData>& scheduleTaskList);
        void addResourceTypesFromXML(nx::vms::api::ResourceTypeDataList& data);
        void loadResourceTypeXML(const QString& fileName, nx::vms::api::ResourceTypeDataList& data);
        bool removeServerStatusFromTransactionLog();
        bool migrateTimeManagerData();
        bool removeEmptyLayoutsFromTransactionLog();
        bool removeOldCameraHistory();
        bool migrateServerGUID(const QString& table, const QString& field);
        bool removeWrongSupportedMotionTypeForONVIF();
        bool cleanupDanglingDbObjects();
        bool fixBusinessRules();
        bool syncLicensesBetweenDB();
        bool encryptKvPairs();
        bool fixDefaultBusinessRuleGuids();
        bool updateBusinessRulesTransactions();

        ErrorCode getLicenses(nx::vms::api::LicenseDataList& data, QSqlDatabase& database);

        /** Raise flags if db is not just created. Always returns true. */
        bool resyncIfNeeded(ResyncFlags flags);

        QString getDatabaseName(const QString& baseName);
        bool rebuildUserAccessRightsTransactions();
        bool setMediaServersStatus(Qn::ResourceStatus status);
        bool updateDefaultRules(const nx::vms::event::RuleList& rules);

    private:
        QnUuid m_storageTypeId;
        QnUuid m_serverTypeId;
        QnUuid m_cameraTypeId;
        QnUuid m_adminUserID;
        int m_adminUserInternalID;
        nx::vms::api::ResourceTypeDataList m_cachedResTypes;
        bool m_licenseOverflowMarked;
        QnUuid m_dbInstanceId;
        bool m_initialized;
        /*
        * Database for static or very rare modified data. Be careful! It's not supported DB transactions for static DB
        * So, only atomic SQL updates are allowed. m_mutexStatic is used for createDB only. Common mutex/transaction is sharing for both DB
        */
        QSqlDatabase m_sdbStatic;
        std::unique_ptr<QnDbTransactionExt> m_tran;
        QnDbTransaction m_tranStatic;
        mutable QnReadWriteLock m_mutexStatic;

        bool m_dbJustCreated;
        bool m_isBackupRestore;
        bool m_dbReadOnly;
        bool m_needReparentLayouts;
        ResyncFlags m_resyncFlags;
        QnTransactionLog* m_tranLog;

        ec2::database::api::QueryCache::Pool m_queryCachePool;
        ec2::database::api::QueryCache m_insertCameraQuery;
        ec2::database::api::QueryCache m_insertCameraUserAttrQuery;
        ec2::database::api::QueryCache m_insertCameraScheduleQuery;
        ec2::database::api::QueryCache m_insertKvPairQuery;
        ec2::database::api::QueryContext m_resourceQueries;
        ec2::database::api::QueryCache m_changeStatusQuery;
    };

    class PersistentStorage : public AbstractPersistentStorage
    {
    public:
        PersistentStorage(QnDbManager* db) : m_db(db) {}

        virtual nx::vms::api::MediaServerData getServer(const QnUuid& id) override
        {
            nx::vms::api::MediaServerDataList result;
            m_db->doQueryNoLock(id, result);
            return result.empty() ? nx::vms::api::MediaServerData() : result[0];
        }

        virtual nx::vms::api::UserData getUser(const QnUuid& id) override
        {
            nx::vms::api::UserDataList result;
            m_db->doQueryNoLock(id, result);
            return result.empty() ? nx::vms::api::UserData() : result[0];
        }

    private:
        QnDbManager* m_db;
    };

} // namespace detail

class QnDbManagerAccess
{
public:
    QnDbManagerAccess(detail::QnDbManager* dbManager, const Qn::UserAccessData &userAccessData);

    Qn::UserAccessData userAccessData() const { return m_userAccessData; }
    void setUserAccessData(Qn::UserAccessData value) { m_userAccessData = value; }
    detail::QnDbManager* db() const { return m_dbManager; }

    ApiObjectType getObjectType(const QnUuid& objectId);

    ErrorCode doQuery(ApiCommand::Value command, std::nullptr_t /*dummy*/, nx::vms::api::FullInfoData& data)
    {
        NX_ASSERT(command == ApiCommand::getFullInfo);
        return readFullInfoDataComplete(&data);
    }

    ErrorCode readFullInfoDataComplete(nx::vms::api::FullInfoData* data)
    {
        const ErrorCode errorCode =
            m_dbManager->readFullInfoDataComplete(data);
        if (errorCode != ErrorCode::ok)
            return errorCode;

        filterData(ApiCommand::getResourceTypes, data->resourceTypes);
        filterData(ApiCommand::getMediaServers, data->servers);
        filterData(ApiCommand::getMediaServerUserAttributesList, data->serversUserAttributesList);
        filterData(ApiCommand::getCameras, data->cameras);
        filterData(ApiCommand::getCameraUserAttributesList, data->cameraUserAttributesList);
        filterData(ApiCommand::getUsers, data->users);
        filterData(ApiCommand::getUserRoles, data->userRoles);
        filterData(ApiCommand::getAccessRights, data->accessRights);
        filterData(ApiCommand::getLayouts, data->layouts);
        filterData(ApiCommand::getLayoutTours, data->layoutTours);
        filterData(ApiCommand::getVideowalls, data->videowalls);
        filterData(ApiCommand::getEventRules, data->rules);
        filterData(ApiCommand::getCameraHistoryItems, data->cameraHistory);
        filterData(ApiCommand::getLicenses, data->licenses);
        filterData(ApiCommand::getDiscoveryData, data->discoveryData);
        filterData(ApiCommand::getResourceParams, data->allProperties);
        filterData(ApiCommand::getStorages, data->storages);
        filterData(ApiCommand::getStatusList, data->resStatusList);
        filterData(ApiCommand::getWebPages, data->webPages);
        filterData(ApiCommand::getAnalyticsPlugins, data->analyticsPlugins);
        filterData(ApiCommand::getAnalyticsEngines, data->analyticsEngines);

        return ErrorCode::ok;
    }

    ErrorCode readFullInfoDataForMobileClient(
        nx::vms::api::FullInfoData* data, const QnUuid& userId);

    QnDbHelper::QnDbTransaction* getTransaction();
    ApiObjectType getObjectTypeNoLock(const QnUuid& objectId);
    ApiObjectInfoList getNestedObjectsNoLock(const ApiObjectInfo& parentObject);
    ApiObjectInfoList getObjectsNoLock(const ApiObjectType& objectType);
    nx::vms::api::IdDataList getLayoutToursNoLock(const QnUuid& parentId);

    void getResourceParamsNoLock(
        const QnUuid& resourceId,
        nx::vms::api::ResourceParamWithRefDataList& resourceParams);

    template <typename T1, typename T2>
    ErrorCode doQuery(ApiCommand::Value command, const T1 &t1, T2 &t2)
    {
        ErrorCode errorCode = m_dbManager->doQuery(t1, t2);
        if (errorCode != ErrorCode::ok)
            return errorCode;

        if (m_userAccessData != Qn::kSystemAccess)
        {
            const auto descriptor = getActualTransactionDescriptorByValue<T2>(command);
            if (!descriptor->checkReadPermissionFunc(m_dbManager->commonModule(), m_userAccessData, t2))
            {
                errorCode = ErrorCode::forbidden;
                t2 = T2();
            }
        }

        return errorCode;
    }

    template<typename T1, template<typename, typename> class Cont, typename T2, typename A>
    ErrorCode doQuery(ApiCommand::Value command, const T1 &inParam, Cont<T2,A>& outParam)
    {
        ErrorCode errorCode = m_dbManager->doQuery(inParam, outParam);
        if (errorCode != ErrorCode::ok)
            return errorCode;

        if (m_userAccessData != Qn::kSystemAccess)
        {
            getActualTransactionDescriptorByValue<Cont<T2, A>>(command)
                ->filterByReadPermissionFunc(m_dbManager->commonModule(), m_userAccessData, outParam);
        }
        return errorCode;
    }

    bool isTranAllowed(const QnAbstractTransaction& tran) const;

    template <typename Param, typename SerializedTransaction>
    ErrorCode executeTransactionNoLock(const QnTransaction<Param> &tran, SerializedTransaction &&serializedTran)
    {
        if (!isTranAllowed(tran))
            return ErrorCode::forbidden;
        if (!getTransactionDescriptorByTransaction(tran)->checkSavePermissionFunc(m_dbManager->commonModule(), m_userAccessData, tran.params))
        {
            NX_WARNING(this, lit("User %1 has not permission to execute transaction %2")
                .arg(m_userAccessData.userId.toString())
                .arg(toString(tran.command)));
            return ErrorCode::forbidden;
        }
        return m_dbManager->executeTransactionNoLock(tran, std::forward<SerializedTransaction>(serializedTran));
    }

    template <template<typename, typename> class Cont, typename Param, typename A, typename SerializedTransaction>
    ErrorCode executeTransactionNoLock(const QnTransaction<Cont<Param,A>> &tran, SerializedTransaction &&serializedTran)
    {
        if (!isTranAllowed(tran))
            return ErrorCode::forbidden;
        auto outParamContainer = tran.params;
        getTransactionDescriptorByTransaction(tran)->filterBySavePermissionFunc(m_dbManager->commonModule(), m_userAccessData, outParamContainer);
        if (outParamContainer.size() != tran.params.size())
            return ErrorCode::forbidden;

        return m_dbManager->executeTransactionNoLock(tran, std::forward<SerializedTransaction>(serializedTran));
    }

    template <class Param, class SerializedTransaction>
    ErrorCode executeTransaction(const QnTransaction<Param> &tran, SerializedTransaction &&serializedTran)
    {
        if (!isTranAllowed(tran))
            return ErrorCode::forbidden;
        if (!getTransactionDescriptorByTransaction(tran)->checkSavePermissionFunc(m_dbManager->commonModule(), m_userAccessData, tran.params))
            return ErrorCode::forbidden;
        return m_dbManager->executeTransaction(tran, std::forward<SerializedTransaction>(serializedTran));
    }

    template <template<typename, typename> class Cont, typename Param, typename A, typename SerializedTransaction>
    ErrorCode executeTransaction(const QnTransaction<Cont<Param,A>> &tran, SerializedTransaction &&serializedTran)
    {
        if (!isTranAllowed(tran))
            return ErrorCode::forbidden;
        Cont<Param,A> paramCopy = tran.params;
        getTransactionDescriptorByTransaction(tran)->filterBySavePermissionFunc(m_dbManager->commonModule(), m_userAccessData, paramCopy);
        if (paramCopy.size() != tran.params.size())
            return ErrorCode::forbidden;

        return m_dbManager->executeTransaction(tran, std::forward<SerializedTransaction>(serializedTran));
    }
private:
    template<typename T>
    void filterData(ApiCommand::Value command, T& target)
    {
        getActualTransactionDescriptorByValue<T>(command)->filterByReadPermissionFunc(m_dbManager->commonModule(), m_userAccessData, target);
    }

    detail::QnDbManager* m_dbManager;
    Qn::UserAccessData m_userAccessData;
};

} // namespace ec2

#define dbManager(db, userAccessData) QnDbManagerAccess(db, userAccessData)
