#ifndef __DB_MANAGER_H_
#define __DB_MANAGER_H_

#include <memory>

#include <QtSql/QSqlError>

#include "nx_ec/ec_api.h"
#include "transaction/transaction.h"
#include <nx_ec/data/api_lock_data.h>
#include "nx_ec/data/api_fwd.h"
#include "nx_ec/data/api_misc_data.h"
#include "utils/db/db_helper.h"
#include "transaction/transaction_log.h"
#include "nx_ec/data/api_runtime_data.h"
#include <nx/utils/log/log.h>
#include <utils/common/unused.h>
#include <nx/utils/singleton.h>
#include "nx/utils/type_utils.h"
#include "core/resource_management/user_access_data.h"
#include "core/resource_management/resource_access_manager.h"
#include "core/resource/user_resource.h"


namespace ec2
{

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
    std::vector<ApiIdData> toIdList() const
    {
        std::vector<ApiIdData> result;
        result.reserve(size());
        for (size_t i = 0; i < size(); ++i)
            result.push_back(ApiIdData(at(i).id));
        return result;
    }
};

class QnDbManagerAccess;

namespace detail
{
    class QnDbManager
    :
        public QObject,
        public QnDbHelper,
        public Singleton<QnDbManager>
    {
        Q_OBJECT

        friend class ::ec2::QnDbManagerAccess;
    public:
        QnDbManager();
        virtual ~QnDbManager();

        bool init(const QUrl& dbUrl);
        bool isInitialized() const;

        template <class T>
        ErrorCode executeTransactionNoLock(const QnTransaction<T>& tran, const QByteArray& serializedTran)
        {
            NX_ASSERT(!tran.persistentInfo.isNull(), Q_FUNC_INFO, "You must register transaction command in persistent command list!");
            if (!tran.isLocal()) {
                QnTransactionLog::ContainsReason isContains = transactionLog->contains(tran);
                if (isContains == QnTransactionLog::Reason_Timestamp)
                    return ErrorCode::containsBecauseTimestamp;
                else if (isContains == QnTransactionLog::Reason_Sequence)
                    return ErrorCode::containsBecauseSequence;
            }
            ErrorCode result = executeTransactionInternal(tran);
            if (result != ErrorCode::ok)
                return result;
            if (tran.isLocal())
                return ErrorCode::ok;
            return transactionLog->saveTransaction( tran, serializedTran);
        }

        ErrorCode executeTransactionNoLock(const QnTransaction<ApiDatabaseDumpData>& tran, const QByteArray& /*serializedTran*/)
        {
            return executeTransactionInternal(tran);
        }

        template <class T>
        ErrorCode executeTransaction(const QnTransaction<T>& tran, const QByteArray& serializedTran)
        {
            NX_ASSERT(!tran.persistentInfo.isNull(), Q_FUNC_INFO, "You must register transaction command in persistent command list!");
            QnDbTransactionLocker lock(getTransaction());
            ErrorCode result = executeTransactionNoLock(tran, serializedTran);
            if (result == ErrorCode::ok) {
                if (!lock.commit())
                {
                    NX_LOG( QnLog::EC2_TRAN_LOG, lit("Commit error while executing transaction %1: %2").
                        arg(toString(result)).arg(m_sdb.lastError().text()), cl_logWARNING );
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

        //getCurrentTime
        ErrorCode doQuery(const nullptr_t& /*dummy*/, ApiTimeData& currentTime);

        //dumpDatabase
        ErrorCode doQuery(const nullptr_t& /*dummy*/, ApiDatabaseDumpData& data);
        ErrorCode doQuery(const ApiStoredFilePath& path, ApiDatabaseDumpToFileData& dumpFileSize);

        // --------- misc -----------------------------
        QnUuid getID() const;

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

        bool readMiscParam( const QByteArray& name, QByteArray* value );

        //!Reads settings (properties of user 'admin')

        virtual QnDbTransaction* getTransaction() override;

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
        ErrorCode doQueryNoLock(nullptr_t /*dummy*/, ApiResourceParamDataList& data);

        //listDirectory
        ErrorCode doQueryNoLock(const ApiStoredFilePath& path, ApiStoredDirContents& data);
        //getStorageData
        ErrorCode doQueryNoLock(const ApiStoredFilePath& path, ApiStoredFileData& data);

        //getStoredFiles
        ErrorCode doQueryNoLock(const ApiStoredFilePath& path, ApiStoredFileDataList& data);
        ErrorCode doQueryNoLock(const nullptr_t&, ApiStoredFileDataList& data) { return doQueryNoLock(ApiStoredFilePath(), data); }

        ErrorCode doQueryNoLock(const QByteArray &paramName, ApiMiscData& miscData);
        //getResourceTypes
        ErrorCode doQueryNoLock(const nullptr_t& /*dummy*/, ApiResourceTypeDataList& resourceTypeList);

        //getCameras
        ErrorCode doQueryNoLock(const nullptr_t& /*dummy*/, ApiCameraDataList& cameraList);

        //getStorages
        ErrorCode doQueryNoLock(const QnUuid& mServerId, ApiStorageDataList& cameraList);

        //get resource status
        ErrorCode doQueryNoLock(const QnUuid& resId, ApiResourceStatusDataList& statusList);

        //getCameraUserAttributesList
        ErrorCode doQueryNoLock(const QnUuid& cameraId, ApiCameraAttributesDataList& cameraUserAttributesList);

        //getCamerasEx
        ErrorCode doQueryNoLock(const nullptr_t& /*dummy*/, ApiCameraDataExList& cameraList);

        //getServers
        ErrorCode doQueryNoLock(const nullptr_t& /*dummy*/, ApiMediaServerDataList& serverList);

        //getServersEx
        ErrorCode doQueryNoLock(const nullptr_t& /*dummy*/, ApiMediaServerDataExList& serverList);

        //getCameraServerItems
        ErrorCode doQueryNoLock(const nullptr_t& /*dummy*/, ApiServerFootageDataList& historyList);

        //getUserList
        ErrorCode doQueryNoLock(const nullptr_t& /*dummy*/, ApiUserDataList& userList);

        //getUserGroupList
        ErrorCode doQueryNoLock(const QnUuid& id, ApiUserGroupDataList& groupList);

        //getPredefinedRoles
        ErrorCode doQueryNoLock(const nullptr_t& /*dummy*/, ApiPredefinedRoleDataList& rolesList);

        //getAccessRights
        ErrorCode doQueryNoLock(const nullptr_t& /*dummy*/, ApiAccessRightsDataList& accessRightsList);

        //getVideowallList
        ErrorCode doQueryNoLock(const nullptr_t& /*dummy*/, ApiVideowallDataList& videowallList);

        //getWebPageList
        ErrorCode doQueryNoLock(const nullptr_t& /*dummy*/, ApiWebPageDataList& webPageList);

        //getBusinessRuleList
        ErrorCode doQueryNoLock(const nullptr_t& /*dummy*/, ApiBusinessRuleDataList& userList);

        //getBusinessRuleList
        ErrorCode doQueryNoLock(const nullptr_t& /*dummy*/, ApiLayoutDataList& layoutList);

        //getResourceParams
        ErrorCode doQueryNoLock(const QnUuid& resourceId, ApiResourceParamWithRefDataList& params);

        // ApiFullInfo
        ErrorCode doQueryNoLock(const nullptr_t& /*dummy*/, ApiFullInfoData& data);

        //getLicenses
        ErrorCode doQueryNoLock(const nullptr_t& /*dummy*/, ApiLicenseDataList& data);

        // ApiDiscoveryDataList
        ErrorCode doQueryNoLock(const QnUuid& id, ApiDiscoveryDataList& data);

        //getMediaServerUserAttributesList
        ErrorCode doQueryNoLock(const QnUuid& mServerId, ApiMediaServerUserAttributesDataList& serverAttrsList);

        //getTransactionLog
        ErrorCode doQueryNoLock(const ApiTranLogFilter&, ApiTransactionDataList& tranList);

        //getClientInfoList
        ErrorCode doQueryNoLock(const QnUuid& clientId, ApiClientInfoDataList& data);

        // Stub - acts as if nothing is found in the database. Needed for merge algorithm.
        ErrorCode doQueryNoLock(const QnUuid& /*id*/, ApiUpdateUploadResponceDataList& data)
        {
            data.clear();
            return ErrorCode::ok;
        }

        // ------------ transactions --------------------------------------

        template<typename T>
        ErrorCode executeTransactionInternal(const QnTransaction<T>&)
        {
            NX_ASSERT(0, "This function should be explicitely specialized");
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiCameraData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiCameraAttributesData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiCameraAttributesDataList>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiMediaServerData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiStorageData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiMediaServerUserAttributesData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiLayoutData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiLayoutDataList>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiResourceStatusData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiResourceParamWithRefData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiServerFootageData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiStoredFileData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiStoredFilePath> &tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiBusinessRuleData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiUserData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiUserGroupData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiAccessRightsData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiResetBusinessRuleData>& /*tran*/) {
            NX_ASSERT(0, Q_FUNC_INFO, "This transaction can't be executed directly!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }
        ErrorCode executeTransactionInternal(const QnTransaction<ApiVideowallData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiUpdateUploadResponceData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiVideowallDataList>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiWebPageData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiWebPageDataList>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiDiscoveryData> &tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiDatabaseDumpData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiClientInfoData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiMiscData>& tran);

        // delete camera, server, layout, any resource, etc.
        ErrorCode executeTransactionInternal(const QnTransaction<ApiIdData>& tran);

        ErrorCode executeTransactionInternal(const QnTransaction<ApiLicenseDataList>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiLicenseData>& tran);

        ErrorCode executeTransactionInternal(const QnTransaction<ApiIdDataList>& /*tran*/)
        {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiMediaServerUserAttributesDataList>& /*tran*/)
        {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiCameraDataList>& /*tran*/)
        {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiStorageDataList>& /*tran*/)
        {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiResourceParamDataList>& /*tran*/)
        {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiResourceParamWithRefDataList>& /*tran*/)
        {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiEmailSettingsData>&) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }
        ErrorCode executeTransactionInternal(const QnTransaction<ApiEmailData>&) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }
        ErrorCode executeTransactionInternal(const QnTransaction<ApiFullInfoData>&) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }
        ErrorCode executeTransactionInternal(const QnTransaction<ApiBusinessActionData>&) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiVideowallControlMessageData> &) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiUpdateUploadData> &) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiDiscoveredServerData> &) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiDiscoveredServerDataList> &) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiDiscoverPeerData> &) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiSystemNameData> &) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiLockData> &) {
           NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiRuntimeData> &) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiPeerAliveData> &) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiUpdateInstallData> &) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiSyncRequestData> &) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<QnTranStateResponse> &) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiPeerSystemTimeData> &) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiPeerSystemTimeDataList> &) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiLicenseOverflowData> &);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiRebuildTransactionLogData>& tran);

        ErrorCode executeTransactionInternal(const QnTransaction<ApiUpdateSequenceData> &) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiTranSyncDoneData> &) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiDiscoveryDataList> &) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiReverseConnectionData> &) {
            NX_ASSERT(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode deleteTableRecord(const QnUuid& id, const QString& tableName, const QString& fieldName);
        ErrorCode deleteTableRecord(const qint32& internalId, const QString& tableName, const QString& fieldName);

        ErrorCode saveMiscParam(const ApiMiscData &params);
        ErrorCode readSettings(ApiResourceParamDataList& settings);

        ErrorCode insertOrReplaceResource(const ApiResourceData& data, qint32* internalId);
        ErrorCode deleteRecordFromResourceTable(const qint32 id);
        ErrorCode removeObject(const ApiObjectInfo& apiObject);

        ErrorCode insertAddParam(const ApiResourceParamWithRefData& param);
        ErrorCode fetchResourceParams( const QnQueryFilter& filter, ApiResourceParamWithRefDataList& params );

        ErrorCode saveCamera(const ApiCameraData& params);
        ErrorCode insertOrReplaceCamera(const ApiCameraData& data, qint32 internalId);
        ErrorCode saveCameraUserAttributes( const ApiCameraAttributesData& attrs );
        ErrorCode insertOrReplaceCameraAttributes(const ApiCameraAttributesData& data, qint32* const internalId);
        ErrorCode removeCameraAttributes(const QnUuid& id);
        ErrorCode removeResourceAccessRights(const QnUuid& id);
        ErrorCode removeResourceStatus(const QnUuid& id);
        ErrorCode updateCameraSchedule(const std::vector<ApiScheduleTaskData>& scheduleTasks, qint32 internalId);
        ErrorCode removeCameraSchedule(qint32 internalId);
        ErrorCode removeCamera(const QnUuid& guid);
        ErrorCode removeStorage(const QnUuid& guid);
        ErrorCode removeParam(const ApiResourceParamWithRefData& data);
        ErrorCode deleteCameraServerItemTable(qint32 id);

        ErrorCode insertOrReplaceMediaServer(const ApiMediaServerData& data, qint32 internalId);
        ErrorCode insertOrReplaceMediaServerUserAttributes(const ApiMediaServerUserAttributesData& attrs);
        ErrorCode removeServer(const QnUuid& guid);
        ErrorCode removeMediaServerUserAttributes(const QnUuid& guid);

        ErrorCode removeLayout(const QnUuid& id);
        ErrorCode removeLayoutInternal(const QnUuid& id, const qint32 &internalId);
        ErrorCode saveLayout(const ApiLayoutData& params);
        ErrorCode insertOrReplaceLayout(const ApiLayoutData& data, qint32 internalId);
        ErrorCode updateLayoutItems(const ApiLayoutData& data, qint32 internalLayoutId);
        ErrorCode removeLayoutItems(qint32 id);

        ErrorCode deleteUserProfileTable(const qint32 id);
        ErrorCode removeUser( const QnUuid& guid );
        ErrorCode insertOrReplaceUser(const ApiUserData& data, qint32 internalId);
        ErrorCode checkExistingUser(const QString &name, qint32 internalId);
        ErrorCode insertOrReplaceUserGroup(const ApiUserGroupData& data);
        ErrorCode removeUserGroup( const QnUuid& guid );
        ErrorCode setAccessRights(const ApiAccessRightsData& data);
        ErrorCode cleanAccessRights(const QnUuid& userOrGroupId);

        ErrorCode saveVideowall(const ApiVideowallData& params);
        ErrorCode removeVideowall(const QnUuid& id);
        ErrorCode insertOrReplaceVideowall(const ApiVideowallData& data, qint32 internalId);
        ErrorCode deleteVideowallPcs(const QnUuid &videowall_guid);
        ErrorCode deleteVideowallItems(const QnUuid &videowall_guid);
        ErrorCode updateVideowallItems(const ApiVideowallData& data);
        ErrorCode updateVideowallScreens(const ApiVideowallData& data);
        ErrorCode removeLayoutFromVideowallItems(const QnUuid &layout_id);
        ErrorCode deleteVideowallMatrices(const QnUuid &videowall_guid);
        ErrorCode updateVideowallMatrices(const ApiVideowallData &data);

        ErrorCode saveWebPage(const ApiWebPageData &params);
        ErrorCode removeWebPage(const QnUuid &id);
        ErrorCode insertOrReplaceWebPage(const ApiWebPageData &data, qint32 internalId);

        ErrorCode insertOrReplaceBusinessRuleTable( const ApiBusinessRuleData& businessRule);
        ErrorCode insertBRuleResource(const QString& tableName, const QnUuid& ruleGuid, const QnUuid& resourceGuid);
        ErrorCode removeBusinessRule( const QnUuid& id );
        ErrorCode updateBusinessRule(const ApiBusinessRuleData& rule);

        ErrorCode saveLicense(const ApiLicenseData& license);
        ErrorCode saveLicense(const ApiLicenseData& license, QSqlDatabase& database);
        ErrorCode removeLicense(const ApiLicenseData& license);
        ErrorCode removeLicense(const ApiLicenseData& license, QSqlDatabase& database);

        ErrorCode insertOrReplaceStoredFile(const QString &fileName, const QByteArray &fileContents);

        bool createDatabase();

        qint32 getResourceInternalId( const QnUuid& guid );
        QnUuid getResourceGuid(const qint32 &internalId);
        qint32 getBusinessRuleInternalId( const QnUuid& guid );

        bool isReadOnly() const { return m_dbReadOnly; }

    private:
        class QnDbTransactionExt: public QnDbTransaction
        {
        public:
            QnDbTransactionExt(QSqlDatabase& database, QnReadWriteLock& mutex): QnDbTransaction(database, mutex) {}

            virtual bool beginTran() override;
            virtual void rollback() override;
            virtual bool commit() override;
        };

        enum GuidConversionMethod {CM_Default, CM_Binary, CM_MakeHash, CM_String, CM_INT};

        QMap<int, QnUuid> getGuidList(const QString& request, GuidConversionMethod method, const QByteArray& intHashPostfix = QByteArray());

        bool updateTableGuids(const QString& tableName, const QString& fieldName, const QMap<int, QnUuid>& guids);
        bool updateResourceTypeGuids();
        bool updateGuids();
        bool updateBusinessActionParameters();
        QnUuid getType(const QString& typeName);
        bool resyncTransactionLog();
        bool addStoredFiles(const QString& baseDirectoryName, int* count = 0);

        template <class ObjectType, class ObjectListType>
        bool fillTransactionLogInternal(ApiCommand::Value command, std::function<bool (ObjectType& data)> updater = nullptr);

        template <class ObjectListType>
        bool queryObjects(ObjectListType& objects);

        virtual bool beforeInstallUpdate(const QString& updateName) override;
        virtual bool afterInstallUpdate(const QString& updateName) override;
        virtual bool afterAllUpdateActions() override;

        ErrorCode addCameraHistory(const ApiServerFootageData& params);
        ErrorCode removeCameraHistory(const QnUuid& serverId);
        ErrorCode getScheduleTasks(std::vector<ApiScheduleTaskWithRefData>& scheduleTaskList);
        void addResourceTypesFromXML(ApiResourceTypeDataList& data);
        void loadResourceTypeXML(const QString& fileName, ApiResourceTypeDataList& data);
        bool removeServerStatusFromTransactionLog();
        bool removeEmptyLayoutsFromTransactionLog();
        bool tuneDBAfterOpen();
        bool removeOldCameraHistory();
        bool migrateServerGUID(const QString& table, const QString& field);
        bool removeWrongSupportedMotionTypeForONVIF();
        bool fixBusinessRules();
        bool syncLicensesBetweenDB();
        bool upgradeSerializedTransactions();
        bool encryptKvPairs();
        ErrorCode getLicenses(ApiLicenseDataList& data, QSqlDatabase& database);
    private:
        QnUuid m_storageTypeId;
        QnUuid m_serverTypeId;
        QnUuid m_cameraTypeId;
        QnUuid m_adminUserID;
        int m_adminUserInternalID;
        ApiResourceTypeDataList m_cachedResTypes;
        bool m_licenseOverflowMarked;
        QnUuid m_dbInstanceId;
        bool m_initialized;
        /*
        * Database for static or very rare modified data. Be careful! It's not supported DB transactions for static DB
        * So, only atomic SQL updates are allowed. m_mutexStatic is used for createDB only. Common mutex/transaction is sharing for both DB
        */
        QSqlDatabase m_sdbStatic;
        QnDbTransactionExt m_tran;
        QnDbTransaction m_tranStatic;
        mutable QnReadWriteLock m_mutexStatic;
        bool m_needResyncLog;
        bool m_needResyncLicenses;
        bool m_needResyncFiles;
        bool m_needResyncCameraUserAttributes;
        bool m_needResyncServerUserAttributes;
        bool m_dbJustCreated;
        bool m_isBackupRestore;
        bool m_needResyncLayout;
        bool m_needResyncbRules;
        bool m_needResyncUsers;
        bool m_needResyncStorages;
        bool m_needResyncClientInfoData;
        bool m_dbReadOnly;
    };
} // namespace detail

class QnDbManagerAccess
{
public:
    QnDbManagerAccess(const Qn::UserAccessData &userAccessData);
    ApiObjectType getObjectType(const QnUuid& objectId);

    template <typename T1>
    ErrorCode doQuery(const T1& t1, ApiFullInfoData& data)
    {
        ErrorCode errorCode = detail::QnDbManager::instance()->doQuery(t1, data);
        if (errorCode != ErrorCode::ok)
            return errorCode;

        readData(data.resourceTypes);
        readData(data.servers);
        readData(data.serversUserAttributesList);
        readData(data.cameras);
        readData(data.cameraUserAttributesList);
        readData(data.users);
        readData(data.userGroups);
        readData(data.userGroups);
        readData(data.accessRights);
        readData(data.layouts);
        readData(data.videowalls);
        readData(data.rules);
        readData(data.cameraHistory);
        readData(data.licenses);
        readData(data.discoveryData);
        readData(data.allProperties);
        readData(data.storages);
        readData(data.resStatusList);
        readData(data.webPages);

        return errorCode;
    }

    QnDbHelper::QnDbTransaction* getTransaction();
    ApiObjectType getObjectTypeNoLock(const QnUuid& objectId);
    ApiObjectInfoList getNestedObjectsNoLock(const ApiObjectInfo& parentObject);
    ApiObjectInfoList getObjectsNoLock(const ApiObjectType& objectType);
    void getResourceParamsNoLock(const QnUuid& resourceId, ApiResourceParamWithRefDataList& resourceParams);

    template <typename T1, typename T2>
    ErrorCode doQuery(const T1 &t1, T2 &t2)
    {
        ErrorCode errorCode = detail::QnDbManager::instance()->doQuery(t1, t2);
        if (errorCode != ErrorCode::ok)
            return errorCode;

        if (!getTransactionDescriptorByParam<T2>()->checkReadPermissionFunc(m_userAccessData, t2))
        {
            errorCode = ErrorCode::forbidden;
            t2 = T2();
        }
        return errorCode;
    }

    template<typename T1, template<typename, typename> class Cont, typename T2, typename A>
    ErrorCode doQuery(const T1 &inParam, Cont<T2,A>& outParam)
    {
        ErrorCode errorCode = detail::QnDbManager::instance()->doQuery(inParam, outParam);
        if (errorCode != ErrorCode::ok)
            return errorCode;

        getTransactionDescriptorByParam<Cont<T2,A>>()->filterByReadPermissionFunc(m_userAccessData, outParam);
        return errorCode;
    }

    bool isTranAllowed(const QnAbstractTransaction& tran) const;

    template <typename Param, typename SerializedTransaction>
    ErrorCode executeTransactionNoLock(const QnTransaction<Param> &tran, SerializedTransaction &&serializedTran)
    {
        if (!isTranAllowed(tran))
            return ErrorCode::forbidden;
        if (!getTransactionDescriptorByTransaction(tran)->checkSavePermissionFunc(m_userAccessData, tran.params))
            return ErrorCode::forbidden;
        return detail::QnDbManager::instance()->executeTransactionNoLock(tran, std::forward<SerializedTransaction>(serializedTran));
    }

    template <template<typename, typename> class Cont, typename Param, typename A, typename SerializedTransaction>
    ErrorCode executeTransactionNoLock(const QnTransaction<Cont<Param,A>> &tran, SerializedTransaction &&serializedTran)
    {
        if (!isTranAllowed(tran))
            return ErrorCode::forbidden;
        auto outParamContainer = tran.params;
        getTransactionDescriptorByTransaction(tran)->filterBySavePermissionFunc(m_userAccessData, outParamContainer);
        if (outParamContainer.size() != tran.params.size())
            return ErrorCode::forbidden;

        return detail::QnDbManager::instance()->executeTransactionNoLock(tran, std::forward<SerializedTransaction>(serializedTran));
    }

    template <class Param, class SerializedTransaction>
    ErrorCode executeTransaction(const QnTransaction<Param> &tran, SerializedTransaction &&serializedTran)
    {
        if (!isTranAllowed(tran))
            return ErrorCode::forbidden;
        if (!getTransactionDescriptorByTransaction(tran)->checkSavePermissionFunc(m_userAccessData, tran.params))
            return ErrorCode::forbidden;
        return detail::QnDbManager::instance()->executeTransaction(tran, std::forward<SerializedTransaction>(serializedTran));
    }

    template <template<typename, typename> class Cont, typename Param, typename A, typename SerializedTransaction>
    ErrorCode executeTransaction(const QnTransaction<Cont<Param,A>> &tran, SerializedTransaction &&serializedTran)
    {
        if (!isTranAllowed(tran))
            return ErrorCode::forbidden;
        Cont<Param,A> paramCopy = tran.params;
        getTransactionDescriptorByTransaction(tran)->filterBySavePermissionFunc(m_userAccessData, paramCopy);
        if (paramCopy.size() != tran.params.size())
            return ErrorCode::forbidden;

        return detail::QnDbManager::instance()->executeTransaction(tran, std::forward<SerializedTransaction>(serializedTran));
    }

private:
    template<typename T>
    void readData(T& target)
    {
        getTransactionDescriptorByParam<T>()->filterByReadPermissionFunc(m_userAccessData, target);
    }

    Qn::UserAccessData m_userAccessData;
};

} // namespace ec2

#define dbManager(userAccessData) QnDbManagerAccess(userAccessData)

#endif // __DB_MANAGER_H_
