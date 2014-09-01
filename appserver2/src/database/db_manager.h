#ifndef __DB_MANAGER_H_
#define __DB_MANAGER_H_

#include "nx_ec/ec_api.h"
#include "transaction/transaction.h"
#include <nx_ec/data/api_lock_data.h>
#include "nx_ec/data/api_fwd.h"
#include "utils/db/db_helper.h"
#include "transaction/transaction_log.h"
#include "nx_ec/data/api_runtime_data.h"

namespace ec2
{
    class LicenseManagerImpl;

    enum ApiOjectType
    {
        ApiObject_NotDefined,
        ApiObject_Resource,
        ApiObject_Server,
        ApiObject_Camera,
        ApiObject_User,
        ApiObject_Layout,
        ApiObject_Videowall,
        ApiObject_BusinessRule,
        ApiObject_Dummy
    };
    struct ApiObjectInfo
    {
        ApiObjectInfo() {}
        ApiObjectInfo(const ApiOjectType& type, const QUuid& id): type(type), id(id) {}

        ApiOjectType type;
        QUuid id;
    };
    class ApiObjectInfoList: public std::vector<ApiObjectInfo>
    {
    public:
        std::vector<ApiIdData> toIdList() 
        {
            std::vector<ApiIdData> result;
            for (size_t i = 0; i < size(); ++i) {
                ApiIdData data;
                data.id = at(i).id;
                result.push_back(data);
            }
            return result;
        }
    };

    class QnDbManager
    :
        public QObject,
        public QnDbHelper
    {
        Q_OBJECT

    public:
        QnDbManager();
        virtual ~QnDbManager();


        class Locker
        {
        public:
            Locker(QnDbManager* db);
            ~Locker();
            void commit();

        private:
            QnDbManager* m_db;
            QnDbHelper::QnDbTransactionLocker m_scopedTran;
        };

        bool init(
            QnResourceFactory* factory,
            const QString& dbFilePath,
            const QString& dbFilePathStatic );
        bool isInitialized() const;

        static QnDbManager* instance();
        
        template <class T>
        ErrorCode executeTransactionNoLock(const QnTransaction<T>& tran, const QByteArray& serializedTran)
        {
            Q_ASSERT_X(!tran.persistentInfo.isNull(), Q_FUNC_INFO, "You must register transaction command in persistent command list!");
            ErrorCode result = executeTransactionInternal(tran);
            if (result != ErrorCode::ok)
                return result;
            return transactionLog->saveTransaction( tran, serializedTran);
        }

        ErrorCode executeTransactionNoLock(const QnTransaction<ApiDatabaseDumpData>& tran, const QByteArray& /*serializedTran*/)
        {
            return executeTransactionInternal(tran);
        }

        template <class T>
        ErrorCode executeTransaction(const QnTransaction<T>& tran, const QByteArray& serializedTran)
        {
            Q_ASSERT_X(!tran.persistentInfo.isNull(), Q_FUNC_INFO, "You must register transaction command in persistent command list!");
            Locker lock(this);
            ErrorCode result = executeTransactionInternal(tran);
            if (result != ErrorCode::ok)
                return result;

            result = transactionLog->saveTransaction( tran, serializedTran);
            if (result == ErrorCode::ok)
                lock.commit();
            return result;
        }


        // --------- get methods ---------------------

        template <class T1, class T2>
        ErrorCode doQuery(const T1& t1, T2& t2)
        {
            QReadLocker lock(&m_mutex);
            return doQueryNoLock(t1, t2);
        }

        //getCurrentTime
        ErrorCode doQuery(const std::nullptr_t& /*dummy*/, ApiTimeData& currentTime);

        //dumpDatabase
        ErrorCode doQuery(const std::nullptr_t& /*dummy*/, ApiDatabaseDumpData& data);

        //listDirectory
        ErrorCode doQueryNoLock(const ApiStoredFilePath& path, ApiStoredDirContents& data);
        //getStorageData
        ErrorCode doQueryNoLock(const ApiStoredFilePath& path, ApiStoredFileData& data);

        //getResourceTypes
        ErrorCode doQueryNoLock(const std::nullptr_t& /*dummy*/, ApiResourceTypeDataList& resourceTypeList);

        //getCameras
        ErrorCode doQueryNoLock(const QUuid& mServerId, ApiCameraDataList& cameraList);

        //getServers
        ErrorCode doQueryNoLock(const std::nullptr_t& /*dummy*/, ApiMediaServerDataList& serverList);

        //getCameraServerItems
        ErrorCode doQueryNoLock(const std::nullptr_t& /*dummy*/, ApiCameraServerItemDataList& historyList);

        //getCameraBookmarkTags
        ErrorCode doQueryNoLock(const std::nullptr_t& /*dummy*/, ApiCameraBookmarkTagDataList& tags);

        //getUserList
        ErrorCode doQueryNoLock(const std::nullptr_t& /*dummy*/, ApiUserDataList& userList);

        //getVideowallList
        ErrorCode doQueryNoLock(const std::nullptr_t& /*dummy*/, ApiVideowallDataList& videowallList);

        //getBusinessRuleList
        ErrorCode doQueryNoLock(const std::nullptr_t& /*dummy*/, ApiBusinessRuleDataList& userList);

        //getBusinessRuleList
        ErrorCode doQueryNoLock(const std::nullptr_t& /*dummy*/, ApiLayoutDataList& layoutList);

        //getResourceParams
        ErrorCode doQueryNoLock(const QUuid& resourceId, ApiResourceParamsData& params);

        // ApiFullInfo
        ErrorCode doQueryNoLock(const std::nullptr_t& /*dummy*/, ApiFullInfoData& data);

        //getLicenses
        ErrorCode doQueryNoLock(const std::nullptr_t& /*dummy*/, ec2::ApiLicenseDataList& data);

        //getParams
        ErrorCode doQueryNoLock(const std::nullptr_t& /*dummy*/, ec2::ApiResourceParamDataList& data);

        // ApiDiscoveryDataList
        ErrorCode doQueryNoLock(const std::nullptr_t& /*dummy*/, ec2::ApiDiscoveryDataList& data);

		// --------- misc -----------------------------
        bool markLicenseOverflow(bool value, qint64 time);
        QUuid getID() const;

        ApiOjectType getObjectType(const QUuid& objectId);
        ApiObjectInfoList getNestedObjects(const ApiObjectInfo& parentObject);

        bool saveMiscParam( const QByteArray& name, const QByteArray& value );
        bool readMiscParam( const QByteArray& name, QByteArray* value );

    signals:
        //!Emitted after \a QnDbManager::init was successfully executed
        void initialized();

    private:
        friend class QnTransactionLog;
        QSqlDatabase& getDB() { return m_sdb; }
        QReadWriteLock& getMutex() { return m_mutex; }

        // ------------ transactions --------------------------------------

        ErrorCode executeTransactionInternal(const QnTransaction<ApiCameraData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiCameraDataList>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiMediaServerData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiLayoutData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiLayoutDataList>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiSetResourceStatusData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiResourceParamsData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiCameraServerItemData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiPanicModeData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiStoredFileData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiStoredFilePath> &tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiResourceData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiBusinessRuleData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiUserData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiResetBusinessRuleData>& tran); //reset business rules
        ErrorCode executeTransactionInternal(const QnTransaction<ApiResourceParamDataList>& tran); // save settings
        ErrorCode executeTransactionInternal(const QnTransaction<ApiVideowallData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiUpdateUploadResponceData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiVideowallDataList>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiDiscoveryDataList> &tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiDatabaseDumpData>& tran);

        // delete camera, server, layout, any resource, etc.
        ErrorCode executeTransactionInternal(const QnTransaction<ApiIdData>& tran);

        ErrorCode executeTransactionInternal(const QnTransaction<ApiLicenseDataList>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiLicenseData>& tran);

        /* Add or remove camera bookmark tags */
        ErrorCode executeTransactionInternal(const QnTransaction<ApiCameraBookmarkTagDataList>& tran);

        ErrorCode executeTransactionInternal(const QnTransaction<ApiEmailSettingsData>&) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }
        ErrorCode executeTransactionInternal(const QnTransaction<ApiEmailData>&) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }
        ErrorCode executeTransactionInternal(const QnTransaction<ApiFullInfoData>&) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }
        ErrorCode executeTransactionInternal(const QnTransaction<ApiBusinessActionData>&) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiVideowallControlMessageData> &) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiUpdateUploadData> &) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiModuleData> &) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiModuleDataList> &) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiDiscoverPeerData> &) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiConnectionData> &) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiConnectionDataList> &) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiSystemNameData> &) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiLockData> &) {
           Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiRuntimeData> &) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiPeerAliveData> &) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiUpdateInstallData> &) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<QnTranState> &) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<QnTranStateResponse> &) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiPeerSystemTimeData> &) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiFillerData> &) {
            return ErrorCode::ok; // nothing to do
        }

        ErrorCode deleteTableRecord(const QUuid& id, const QString& tableName, const QString& fieldName);
        ErrorCode deleteTableRecord(const qint32& internalId, const QString& tableName, const QString& fieldName);

        ErrorCode updateResource(const ApiResourceData& data, qint32 internalId);
        ErrorCode insertResource(const ApiResourceData& data, qint32* internalId);
        ErrorCode insertOrReplaceResource(const ApiResourceData& data, qint32* internalId);
        //ErrorCode insertOrReplaceResource(const ApiResourceData& data);
        ErrorCode deleteRecordFromResourceTable(const qint32 id);
        ErrorCode removeObject(const ApiObjectInfo& apiObject);

        ErrorCode insertAddParams(const std::vector<ApiResourceParamData>& params, qint32 internalId);
        ErrorCode deleteAddParams(qint32 resourceId);

        ErrorCode saveCamera(const ApiCameraData& params);
        ErrorCode insertOrReplaceCamera(const ApiCameraData& data, qint32 internalId);
        ErrorCode updateCameraSchedule(const ApiCameraData& data, qint32 internalId);
        ErrorCode removeCameraSchedule(qint32 internalId);
        ErrorCode removeCamera(const QUuid& guid);
        ErrorCode deleteCameraServerItemTable(qint32 id);

        ErrorCode insertOrReplaceMediaServer(const ApiMediaServerData& data, qint32 internalId);
        ErrorCode updateStorages(const ApiMediaServerData&);
        ErrorCode removeServer(const QUuid& guid);
        ErrorCode removeStoragesByServer(const QUuid& serverGUID);

        ErrorCode removeLayout(const QUuid& id);
        ErrorCode removeLayoutInternal(const QUuid& id, const qint32 &internalId);
        ErrorCode saveLayout(const ApiLayoutData& params);
        ErrorCode insertOrReplaceLayout(const ApiLayoutData& data, qint32 internalId);
        ErrorCode updateLayoutItems(const ApiLayoutData& data, qint32 internalLayoutId);
        ErrorCode removeLayoutItems(qint32 id);

        ErrorCode deleteUserProfileTable(const qint32 id);
        ErrorCode removeUser( const QUuid& guid );
        ErrorCode insertOrReplaceUser(const ApiUserData& data, qint32 internalId);
        ErrorCode checkExistingUser(const QString &name, qint32 internalId);

        ErrorCode saveVideowall(const ApiVideowallData& params);
        ErrorCode removeVideowall(const QUuid& id);
        ErrorCode insertOrReplaceVideowall(const ApiVideowallData& data, qint32 internalId);
        ErrorCode deleteVideowallPcs(const QUuid &videowall_guid);
        ErrorCode deleteVideowallItems(const QUuid &videowall_guid);
        ErrorCode updateVideowallItems(const ApiVideowallData& data);
        ErrorCode updateVideowallScreens(const ApiVideowallData& data);
        ErrorCode removeLayoutFromVideowallItems(const QUuid &layout_id);
        ErrorCode deleteVideowallMatrices(const QUuid &videowall_guid);
        ErrorCode updateVideowallMatrices(const ApiVideowallData &data);

        ErrorCode insertOrReplaceBusinessRuleTable( const ApiBusinessRuleData& businessRule);
        ErrorCode insertBRuleResource(const QString& tableName, const QUuid& ruleGuid, const QUuid& resourceGuid);
        ErrorCode removeBusinessRule( const QUuid& id );
        ErrorCode updateBusinessRule(const ApiBusinessRuleData& rule);

        ErrorCode saveLicense(const ApiLicenseData& license);
        ErrorCode removeLicense(const ApiLicenseData& license);

        ErrorCode addCameraBookmarkTag(const ApiCameraBookmarkTagData &tag);
        ErrorCode removeCameraBookmarkTag(const ApiCameraBookmarkTagData &tag);

        bool createDatabase(bool *dbJustCreated, bool *isMigrationFrom2_2);
        bool migrateBusinessEvents();
        bool doRemap(int id, int newVal, const QString& fieldName);
        
        qint32 getResourceInternalId( const QUuid& guid );
        QUuid getResourceGuid(const qint32 &internalId);
        qint32 getBusinessRuleInternalId( const QUuid& guid );

        //void beginTran();
        //void commit();
        //void rollback();
    private:
        enum GuidConversionMethod {CM_Default, CM_Binary, CM_MakeHash, CM_INT};

        QMap<int, QUuid> getGuidList(const QString& request, GuidConversionMethod method, const QByteArray& intHashPostfix = QByteArray());

        bool updateTableGuids(const QString& tableName, const QString& fieldName, const QMap<int, QUuid>& guids);
        bool updateGuids();
        QUuid getType(const QString& typeName);
        bool resyncTransactionLog();

        template <class ObjectType, class ObjectListType> 
        bool fillTransactionLogInternal(ApiCommand::Value command);
        bool addTransactionForGeneralSettings();
    private:
        QnResourceFactory* m_resourceFactory;
        QUuid m_storageTypeId;
        QUuid m_serverTypeId;
        QUuid m_cameraTypeId;
        QUuid m_adminUserID;
        int m_adminUserInternalID;
        ApiResourceTypeDataList m_cachedResTypes;
        bool m_licenseOverflowMarked;
        qint64 m_licenseOverflowTime;
        QUuid m_dbInstanceId;
        bool m_initialized;
        
        /*
        * Database for static or very rare modified data. Be carefull! It's not supported DB transactions for static DB
        * So, only atomic SQL updates are allowed. m_mutexStatic is used for createDB only. Common mutex/transaction is sharing for both DB
        */
        QSqlDatabase m_sdbStatic;
        QnDbTransaction m_tranStatic;
        mutable QReadWriteLock m_mutexStatic;
        bool m_needResyncLog;
    };
};

#define dbManager QnDbManager::instance()

#endif // __DB_MANAGER_H_
