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
        ApiObjectInfo(const ApiOjectType& type, const QnId& id): type(type), id(id) {}

        ApiOjectType type;
        QnId id;
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

    class QnDbManager;

    class QnDbManager: public QnDbHelper
    {
    public:

        QnDbManager(
            QnResourceFactory* factory,
            LicenseManagerImpl* const licenseManagerImpl,
            const QString& dbFileName );
        virtual ~QnDbManager();


        class Locker
        {
        public:
            Locker(QnDbManager* db);
            ~Locker();
            void beginTran();
            void commit();
        private:
            bool m_inTran;
            QnDbManager* m_db;
        };

        bool init();

        static QnDbManager* instance();
        
        template <class T>
        ErrorCode executeTransactionNoLock(const QnTransaction<T>& tran, const QByteArray& serializedTran)
        {
            ErrorCode result = executeTransactionInternal(tran);
            if (result != ErrorCode::ok)
                return result;
            return transactionLog->saveTransaction( tran, serializedTran);
        }


        template <class T>
        ErrorCode executeTransaction(const QnTransaction<T>& tran, const QByteArray& serializedTran)
        {
            QnDbTransactionLocker lock(&m_tran);
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
        ErrorCode doQuery(const std::nullptr_t& /*dummy*/, qint64& currentTime);

        //getStoragePath
        ErrorCode doQueryNoLock(const ApiStoredFilePath& path, ApiStoredDirContents& data);
        //getStorageData
        ErrorCode doQueryNoLock(const ApiStoredFilePath& path, ApiStoredFileData& data);

        //getResourceTypes
        ErrorCode doQueryNoLock(const std::nullptr_t& /*dummy*/, ApiResourceTypeDataList& resourceTypeList);

        //getCameras
        ErrorCode doQueryNoLock(const QnId& mServerId, ApiCameraDataList& cameraList);

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
        ErrorCode doQueryNoLock(const QnId& resourceId, ApiResourceParamsData& params);

        // ApiFullInfo
        ErrorCode doQueryNoLock(const std::nullptr_t& /*dummy*/, ApiFullInfoData& data);

        //getLicenses
        ErrorCode doQueryNoLock(const std::nullptr_t& /*dummy*/, ec2::ApiLicenseDataList& data);

        //getParams
        ErrorCode doQueryNoLock(const std::nullptr_t& /*dummy*/, ec2::ApiResourceParamDataList& data);

        //getHelp
        ErrorCode doQueryNoLock(const QString& group, ec2::ApiHelpGroupDataList& data);

		// --------- misc -----------------------------
        bool markLicenseOverflow(bool value, qint64 time);
        QUuid getID() const;

        ApiOjectType getObjectType(const QnId& objectId);
        ApiObjectInfoList getNestedObjects(const ApiObjectInfo& parentObject);
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
        //ErrorCode executeTransactionInternal(const QnTransaction<ApiSetResourceDisabledData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiResourceParamsData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiCameraServerItemData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiPanicModeData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiStoredFileData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<QString> &tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiResourceData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiBusinessRuleData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiUserData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiResetBusinessRuleData>& tran); //reset business rules
        ErrorCode executeTransactionInternal(const QnTransaction<ApiResourceParamDataList>& tran); // save settings
        ErrorCode executeTransactionInternal(const QnTransaction<ApiVideowallData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiUpdateUploadResponceData>& tran);
        ErrorCode executeTransactionInternal(const QnTransaction<ApiVideowallDataList>& tran);

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

        ErrorCode executeTransactionInternal(const QnTransaction<ApiVideowallInstanceStatusData> &) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiUpdateUploadData> &) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiLockData> &) {
           Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<ApiPeerAliveData> &) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<QnTranState> &) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionInternal(const QnTransaction<int> &) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode deleteTableRecord(const QnId& id, const QString& tableName, const QString& fieldName);
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
        ErrorCode removeCamera(const QnId& guid);
        ErrorCode deleteCameraServerItemTable(qint32 id);

        ErrorCode insertOrReplaceMediaServer(const ApiMediaServerData& data, qint32 internalId);
        ErrorCode updateStorages(const ApiMediaServerData&);
        ErrorCode removeServer(const QnId& guid);
        ErrorCode removeStoragesByServer(const QnId& serverGUID);

        ErrorCode removeLayout(const QnId& id);
        ErrorCode removeLayoutInternal(const QnId& id, const qint32 &internalId);
        ErrorCode saveLayout(const ApiLayoutData& params);
        ErrorCode insertOrReplaceLayout(const ApiLayoutData& data, qint32 internalId);
        ErrorCode updateLayoutItems(const ApiLayoutData& data, qint32 internalLayoutId);
        ErrorCode removeLayoutItems(qint32 id);

        ErrorCode deleteUserProfileTable(const qint32 id);
        ErrorCode removeUser( const QnId& guid );
        ErrorCode insertOrReplaceUser(const ApiUserData& data, qint32 internalId);

        ErrorCode saveVideowall(const ApiVideowallData& params);
        ErrorCode removeVideowall(const QnId& id);
        ErrorCode insertOrReplaceVideowall(const ApiVideowallData& data, qint32 internalId);
        ErrorCode deleteVideowallItems(const QnId &videowall_guid);
        ErrorCode updateVideowallItems(const ApiVideowallData& data);
        ErrorCode updateVideowallScreens(const ApiVideowallData& data);
        ErrorCode removeLayoutFromVideowallItems(const QnId &layout_id);
        ErrorCode deleteVideowallMatrices(const QnId &videowall_guid);
        ErrorCode updateVideowallMatrices(const ApiVideowallData &data);

        ErrorCode insertOrReplaceBusinessRuleTable( const ApiBusinessRuleData& businessRule);
        ErrorCode insertBRuleResource(const QString& tableName, const QnId& ruleGuid, const QnId& resourceGuid);
        ErrorCode removeBusinessRule( const QnId& id );
        ErrorCode updateBusinessRule(const ApiBusinessRuleData& rule);

        ErrorCode saveLicense(const ApiLicenseData& license);

        ErrorCode addCameraBookmarkTag(const ApiCameraBookmarkTagData &tag);
        ErrorCode removeCameraBookmarkTag(const ApiCameraBookmarkTagData &tag);

        bool createDatabase(bool *dbJustCreated);
        bool migrateBusinessEvents();
        bool doRemap(int id, int newVal, const QString& fieldName);
        
        qint32 getResourceInternalId( const QnId& guid );
        QnId getResourceGuid(const qint32 &internalId);
        qint32 getBusinessRuleInternalId( const QnId& guid );

        void beginTran();
        void commit();
        void rollback();
    private:
        QMap<int, QnId> getGuidList(const QString& request);
        bool updateTableGuids(const QString& tableName, const QString& fieldName, const QMap<int, QnId>& guids);
        bool updateGuids();
        QnId getType(const QString& typeName);
        bool loadHelpData(const QString& fileName);
    private:
        QnResourceFactory* m_resourceFactory;
        LicenseManagerImpl* const m_licenseManagerImpl;
        QnId m_storageTypeId;
        QnId m_serverTypeId;
        QnId m_cameraTypeId;
        QnId m_adminUserID;
        int m_adminUserInternalID;
        ApiResourceTypeDataList m_cachedResTypes;
        bool m_licenseOverflowMarked;
        qint64 m_licenseOverflowTime;
        QUuid m_dbInstanceId;
        ApiHelpGroupDataList* m_helpData;
    };
};

#define dbManager QnDbManager::instance()

#endif // __DB_MANAGER_H_
