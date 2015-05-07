#ifndef __DB_MANAGER_H_
#define __DB_MANAGER_H_

#include <QSqlError>

#include "nx_ec/ec_api.h"
#include "transaction/transaction.h"
#include <nx_ec/data/api_lock_data.h>
#include "nx_ec/data/api_fwd.h"
#include "utils/db/db_helper.h"
#include "transaction/transaction_log.h"
#include "nx_ec/data/api_runtime_data.h"
#include <utils/common/log.h>


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
        ApiObject_Storage,
        ApiObject_Dummy
    };
    struct ApiObjectInfo
    {
        ApiObjectInfo() {}
        ApiObjectInfo(const ApiOjectType& type, const QnUuid& id): type(type), id(id) {}

        ApiOjectType type;
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

    class QnDbManager
    :
        public QObject,
        public QnDbHelper
    {
        Q_OBJECT

    public:
        QnDbManager();
        virtual ~QnDbManager();

        bool init(QnResourceFactory* factory, const QUrl& dbUrl);
        bool isInitialized() const;

        static QnDbManager* instance();
        
        template <class T>
        ErrorCode executeTransactionNoLock(const QnTransaction<T>& tran, const QByteArray& serializedTran)
        {
            Q_ASSERT_X(!tran.persistentInfo.isNull(), Q_FUNC_INFO, "You must register transaction command in persistent command list!");
            if (!tran.isLocal) {
                QnTransactionLog::ContainsReason isContains = transactionLog->contains(tran);
                if (isContains == QnTransactionLog::Reason_Timestamp)
                    return ErrorCode::containsBecauseTimestamp;
                else if (isContains == QnTransactionLog::Reason_Sequence)
                    return ErrorCode::containsBecauseSequence;
            }
            ErrorCode result = executeTransactionInternal(tran);
            if (result != ErrorCode::ok)
                return result;
            if (tran.isLocal)
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
            Q_ASSERT_X(!tran.persistentInfo.isNull(), Q_FUNC_INFO, "You must register transaction command in persistent command list!");
            QnDbTransactionLocker lock(getTransaction());
            ErrorCode result = executeTransactionNoLock(tran, serializedTran);
            if (result == ErrorCode::ok) {
                if (!lock.commit())
                {
                    NX_LOG( QnLog::EC2_TRAN_LOG, lit("Commit error while executing transaction %1: %2").
                        arg(ec2::toString(result)).arg(m_sdb.lastError().text()), cl_logWARNING );
                    return ErrorCode::dbError;
                }
            }
            return result;
        }


        // --------- get methods ---------------------

        template <class T1, class T2>
        ErrorCode doQuery(const T1& t1, T2& t2)
        {
            QWriteLocker lock(&m_mutex);
            return doQueryNoLock(t1, t2);
        }

        //getCurrentTime
        ErrorCode doQuery(const std::nullptr_t& /*dummy*/, ApiTimeData& currentTime);

        //dumpDatabase
        ErrorCode doQuery(const std::nullptr_t& /*dummy*/, ApiDatabaseDumpData& data);
        ErrorCode doQuery(const ApiStoredFilePath& path, qint64& dumpFileSize);

		// --------- misc -----------------------------
        QnUuid getID() const;

        ApiOjectType getObjectType(const QnUuid& objectId)
        {
            QWriteLocker lock( &m_mutex );
            return getObjectTypeNoLock( objectId );
        }
        /*!
            \note This overload should be called within transaction
        */
        ApiOjectType getObjectTypeNoLock(const QnUuid& objectId);
        ApiObjectInfoList getNestedObjectsNoLock(const ApiObjectInfo& parentObject);
        ApiObjectInfoList getObjectsNoLock(const ApiOjectType& objectType);

        bool saveMiscParam( const QByteArray& name, const QByteArray& value );
        bool readMiscParam( const QByteArray& name, QByteArray* value );

        //!Reads settings (properties of user 'admin')
        ErrorCode readSettings(ApiResourceParamDataList& settings);

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


        friend class QnTransactionLog;
        QSqlDatabase& getDB() { return m_sdb; }
        QReadWriteLock& getMutex() { return m_mutex; }

        // ------------ data retrieval --------------------------------------

        //listDirectory
        ErrorCode doQueryNoLock(const ApiStoredFilePath& path, ApiStoredDirContents& data);
        //getStorageData
        ErrorCode doQueryNoLock(const ApiStoredFilePath& path, ApiStoredFileData& data);

        //getStoredFiles
        ErrorCode doQueryNoLock(const ApiStoredFilePath& path, ApiStoredFileDataList& data);
        ErrorCode doQueryNoLock(const std::nullptr_t&, ApiStoredFileDataList& data) { return doQueryNoLock(ApiStoredFilePath(), data); }

        //getResourceTypes
        ErrorCode doQueryNoLock(const std::nullptr_t& /*dummy*/, ApiResourceTypeDataList& resourceTypeList);

        //getCameras
        ErrorCode doQueryNoLock(const QnUuid& mServerId, ApiCameraDataList& cameraList);

        //getStorages
        ErrorCode doQueryNoLock(const QnUuid& mServerId, ApiStorageDataList& cameraList);

        //get resource status
        ErrorCode doQueryNoLock(const QnUuid& resId, ApiResourceStatusDataList& statusList);

        //getCameraUserAttributes
        ErrorCode doQueryNoLock(const QnUuid& serverId, ApiCameraAttributesDataList& cameraUserAttributesList);

        //getCamerasEx
        ErrorCode doQueryNoLock(const QnUuid& serverId, ApiCameraDataExList& cameraList);

        //getServers
        ErrorCode doQueryNoLock(const QnUuid& mServerId, ApiMediaServerDataList& serverList);

        //getServersEx
        ErrorCode doQueryNoLock(const QnUuid& mServerId, ApiMediaServerDataExList& cameraList);

        //getCameraServerItems
        ErrorCode doQueryNoLock(const std::nullptr_t& /*dummy*/, ApiServerFootageDataList& historyList);

        //getCameraBookmarkTags
        ErrorCode doQueryNoLock(const std::nullptr_t& /*dummy*/, ApiCameraBookmarkTagDataList& tags);

        //getUserList
        ErrorCode doQueryNoLock(const QnUuid& userId, ApiUserDataList& userList);

        //getVideowallList
        ErrorCode doQueryNoLock(const std::nullptr_t& /*dummy*/, ApiVideowallDataList& videowallList);

        //getBusinessRuleList
        ErrorCode doQueryNoLock(const std::nullptr_t& /*dummy*/, ApiBusinessRuleDataList& userList);

        //getBusinessRuleList
        ErrorCode doQueryNoLock(const std::nullptr_t& /*dummy*/, ApiLayoutDataList& layoutList);

        //getResourceParams
        ErrorCode doQueryNoLock(const QnUuid& resourceId, ApiResourceParamWithRefDataList& params);

        // ApiFullInfo
        ErrorCode doQueryNoLock(const std::nullptr_t& /*dummy*/, ApiFullInfoData& data);

        //getLicenses
        ErrorCode doQueryNoLock(const std::nullptr_t& /*dummy*/, ec2::ApiLicenseDataList& data);

        // ApiDiscoveryDataList
        ErrorCode doQueryNoLock(const std::nullptr_t& /*dummy*/, ec2::ApiDiscoveryDataList& data);

        //getServerUserAttributes
        ErrorCode doQueryNoLock(const QnUuid& mServerId, ApiMediaServerUserAttributesDataList& serverAttrsList);

        //getTransactionLog
        ErrorCode doQueryNoLock(const std::nullptr_t&, ApiTransactionDataList& tranList);


        // ------------ transactions --------------------------------------

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
        ErrorCode executeTransactionInternal(const QnTransaction<ApiResourceData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiBusinessRuleData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiUserData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiResetBusinessRuleData>& /*tran*/) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This transaction can't be executed directly!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }
        ErrorCode executeTransactionInternal(const QnTransaction<ApiVideowallData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiUpdateUploadResponceData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiVideowallDataList>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiDiscoveryData> &tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiDatabaseDumpData>& tran);

        // delete camera, server, layout, any resource, etc.
        ErrorCode executeTransactionInternal(const QnTransaction<ApiIdData>& tran);

        ErrorCode executeTransactionInternal(const QnTransaction<ApiLicenseDataList>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiLicenseData>& tran);

        /* Add or remove camera bookmark tags */
        ErrorCode executeTransactionInternal(const QnTransaction<ApiCameraBookmarkTagDataList>& tran);

        ErrorCode executeTransactionInternal(const QnTransaction<ApiIdDataList>& /*tran*/)
        {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiMediaServerUserAttributesDataList>& /*tran*/)
        {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiCameraDataList>& /*tran*/)
        {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiStorageDataList>& /*tran*/)
        {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiResourceParamDataList>& /*tran*/)
        {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiResourceParamWithRefDataList>& /*tran*/)
        {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

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

        ErrorCode executeTransactionInternal(const QnTransaction<ApiSyncRequestData> &) {
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

        ErrorCode executeTransactionInternal(const QnTransaction<ApiPeerSystemTimeDataList> &) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiLicenseOverflowData> &);

        ErrorCode executeTransactionInternal(const QnTransaction<ApiUpdateSequenceData> &) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }
        
        ErrorCode executeTransactionInternal(const QnTransaction<ApiTranSyncDoneData> &) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiDiscoveryDataList> &) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode deleteTableRecord(const QnUuid& id, const QString& tableName, const QString& fieldName);
        ErrorCode deleteTableRecord(const qint32& internalId, const QString& tableName, const QString& fieldName);

        ErrorCode insertOrReplaceResource(const ApiResourceData& data, qint32* internalId);
        //ErrorCode insertOrReplaceResource(const ApiResourceData& data);
        ErrorCode deleteRecordFromResourceTable(const qint32 id);
        ErrorCode removeObject(const ApiObjectInfo& apiObject);

        ErrorCode insertAddParam(const ApiResourceParamWithRefData& param);
        ErrorCode fetchResourceParams( const QnQueryFilter& filter, ApiResourceParamWithRefDataList& params );
        //ErrorCode deleteAddParams(qint32 resourceId);

        ErrorCode saveCamera(const ApiCameraData& params);
        ErrorCode insertOrReplaceCamera(const ApiCameraData& data, qint32 internalId);
        ErrorCode saveCameraUserAttributes( const ApiCameraAttributesData& attrs );
        ErrorCode insertOrReplaceCameraAttributes(const ApiCameraAttributesData& data, qint32* const internalId);
        ErrorCode updateCameraSchedule(const std::vector<ApiScheduleTaskData>& scheduleTasks, qint32 internalId);
        ErrorCode removeCameraSchedule(qint32 internalId);
        ErrorCode removeCamera(const QnUuid& guid);
        ErrorCode removeStorage(const QnUuid& guid);
        ErrorCode deleteCameraServerItemTable(qint32 id);

        ErrorCode insertOrReplaceMediaServer(const ApiMediaServerData& data, qint32 internalId);
        ErrorCode insertOrReplaceMediaServerUserAttributes(const ApiMediaServerUserAttributesData& attrs);
        //ErrorCode updateStorages(const ApiMediaServerData&);
        ErrorCode removeServer(const QnUuid& guid);
        ErrorCode removeMediaServerUserAttributes(const QnUuid& guid);
        //ErrorCode removeStoragesByServer(const QnUuid& serverGUID);

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

        ErrorCode insertOrReplaceBusinessRuleTable( const ApiBusinessRuleData& businessRule);
        ErrorCode insertBRuleResource(const QString& tableName, const QnUuid& ruleGuid, const QnUuid& resourceGuid);
        ErrorCode removeBusinessRule( const QnUuid& id );
        ErrorCode updateBusinessRule(const ApiBusinessRuleData& rule);

        ErrorCode saveLicense(const ApiLicenseData& license);
        ErrorCode removeLicense(const ApiLicenseData& license);

        ErrorCode addCameraBookmarkTag(const ApiCameraBookmarkTagData &tag);
        ErrorCode removeCameraBookmarkTag(const ApiCameraBookmarkTagData &tag);

        ErrorCode insertOrReplaceStoredFile(const QString &fileName, const QByteArray &fileContents);

        bool createDatabase();
        bool migrateBusinessEvents();
        bool doRemap(int id, int newVal, const QString& fieldName);
        
        qint32 getResourceInternalId( const QnUuid& guid );
        QnUuid getResourceGuid(const qint32 &internalId);
        qint32 getBusinessRuleInternalId( const QnUuid& guid );
    private:
        class QnDbTransactionExt: public QnDbTransaction
        {
        public:
            QnDbTransactionExt(QSqlDatabase& database, QReadWriteLock& mutex): QnDbTransaction(database, mutex) {}

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
        bool fillTransactionLogInternal(ApiCommand::Value command);

        bool beforeInstallUpdate(const QString& updateName);
        bool afterInstallUpdate(const QString& updateName);

        ErrorCode addCameraHistory(const ApiServerFootageData& params);
        ErrorCode removeCameraHistory(const QnUuid& serverId);
        ErrorCode getScheduleTasks(const QnUuid& serverId, std::vector<ApiScheduleTaskWithRefData>& scheduleTaskList);
        void addResourceTypesFromXML(ApiResourceTypeDataList& data);
        void loadResourceTypeXML(const QString& fileName, ApiResourceTypeDataList& data);
        bool removeServerStatusFromTransactionLog();
        bool tuneDBAfterOpen();
        bool removeOldCameraHistory();
        bool migrateServerGUID(const QString& table, const QString& field);
        bool removeWrongSupportedMotionTypeForONVIF();
        bool fixBusinessRules();
    private:
        QnResourceFactory* m_resourceFactory;
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
        * Database for static or very rare modified data. Be carefull! It's not supported DB transactions for static DB
        * So, only atomic SQL updates are allowed. m_mutexStatic is used for createDB only. Common mutex/transaction is sharing for both DB
        */
        QSqlDatabase m_sdbStatic;
        QnDbTransactionExt m_tran;
        QnDbTransaction m_tranStatic;
        mutable QReadWriteLock m_mutexStatic;
        bool m_needResyncLog;
        bool m_needResyncLicenses;
        bool m_needResyncFiles;
        bool m_needResyncCameraUserAttributes;
        bool m_dbJustCreated;
        bool m_isBackupRestore;
        bool m_needResyncLayout;
        bool m_needResyncbRules;
    };
};

#define dbManager QnDbManager::instance()

#endif // __DB_MANAGER_H_
