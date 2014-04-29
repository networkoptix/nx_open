#ifndef __DB_MANAGER_H_
#define __DB_MANAGER_H_

#include "nx_ec/ec_api.h"
#include "transaction/transaction.h"
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
#include "nx_ec/data/api_media_server_data.h"
#include "nx_ec/data/api_update_data.h"
#include "utils/db/db_helper.h"
#include "transaction/transaction_log.h"


namespace ec2
{
    class LicenseManagerImpl;

    class QnDbManager: public QnDbHelper
    {
    public:
        QnDbManager(
            QnResourceFactory* factory,
            LicenseManagerImpl* const licenseManagerImpl,
            const QString& dbFileName );
        virtual ~QnDbManager();

        bool init();

        static QnDbManager* instance();
        
        template <class T>
        ErrorCode executeNestedTransaction(const QnTransaction<T>& tran, const QByteArray& serializedTran)
        {
            ErrorCode result = executeTransactionNoLock(tran);
            if (result != ErrorCode::ok)
                return result;
            return transactionLog->saveTransaction( tran, serializedTran);
        }

        void beginTran();
        void commit();
        void rollback();


        template <class T>
        ErrorCode executeTransaction(const QnTransaction<T>& tran, const QByteArray& serializedTran)
        {
            QnDbTransactionLocker lock(&m_tran);
            ErrorCode result = executeTransactionNoLock(tran);
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
        ErrorCode doQuery(const nullptr_t& /*dummy*/, qint64& currentTime);

        //getStoragePath
        ErrorCode doQueryNoLock(const ApiStoredFilePath& path, ApiStoredDirContents& data);
        //getStorageData
        ErrorCode doQueryNoLock(const ApiStoredFilePath& path, ApiStoredFileData& data);

        //getResourceTypes
        ErrorCode doQueryNoLock(const nullptr_t& /*dummy*/, ApiResourceTypeDataList& resourceTypeList);

        //getCameras
        ErrorCode doQueryNoLock(const QnId& mServerId, ApiCameraDataList& cameraList);

        //getServers
        ErrorCode doQueryNoLock(const nullptr_t& /*dummy*/, ApiMediaServerDataList& serverList);

        //getCameraServerItems
        ErrorCode doQueryNoLock(const nullptr_t& /*dummy*/, ApiCameraServerItemDataList& historyList);

        //getUserList
        ErrorCode doQueryNoLock(const nullptr_t& /*dummy*/, ApiUserDataList& userList);

        //getVideowallList
        ErrorCode doQueryNoLock(const nullptr_t& /*dummy*/, ApiVideowallDataList& videowallList);

        //getBusinessRuleList
        ErrorCode doQueryNoLock(const nullptr_t& /*dummy*/, ApiBusinessRuleDataList& userList);

        //getBusinessRuleList
        ErrorCode doQueryNoLock(const nullptr_t& /*dummy*/, ApiLayoutDataList& layoutList);

        //getResourceParams
        ErrorCode doQueryNoLock(const QnId& resourceId, ApiResourceParamsData& params);

        // ApiFullInfo
        ErrorCode doQueryNoLock(const nullptr_t& /*dummy*/, ApiFullInfoData& data);

        //getLicenses
        ErrorCode doQueryNoLock(const nullptr_t& /*dummy*/, ec2::ApiLicenseDataList& data);

        //getParams
        ErrorCode doQueryNoLock(const nullptr_t& /*dummy*/, ec2::ApiResourceParamDataList& data);

		// --------- misc -----------------------------

    private:
        friend class QnTransactionLog;
        QSqlDatabase& getDB() { return m_sdb; }
        QReadWriteLock& getMutex() { return m_mutex; }

        // ------------ transactions --------------------------------------

        ErrorCode executeTransactionNoLock(const QnTransaction<ApiCameraData>& tran);
        ErrorCode executeTransactionNoLock(const QnTransaction<ApiCameraDataList>& tran);
        ErrorCode executeTransactionNoLock(const QnTransaction<ApiMediaServerData>& tran);
        ErrorCode executeTransactionNoLock(const QnTransaction<ApiLayoutData>& tran);
        ErrorCode executeTransactionNoLock(const QnTransaction<ApiLayoutDataList>& tran);
        ErrorCode executeTransactionNoLock(const QnTransaction<ApiSetResourceStatusData>& tran);
        ErrorCode executeTransactionNoLock(const QnTransaction<ApiSetResourceDisabledData>& tran);
        ErrorCode executeTransactionNoLock(const QnTransaction<ApiResourceParamsData>& tran);
        ErrorCode executeTransactionNoLock(const QnTransaction<ApiCameraServerItemData>& tran);
        ErrorCode executeTransactionNoLock(const QnTransaction<ApiPanicModeData>& tran);
        ErrorCode executeTransactionNoLock(const QnTransaction<ApiStoredFileData>& tran);
        ErrorCode executeTransactionNoLock(const QnTransaction<QString> &tran);
        ErrorCode executeTransactionNoLock(const QnTransaction<ApiResourceData>& tran);
        ErrorCode executeTransactionNoLock(const QnTransaction<ApiBusinessRuleData>& tran);
        ErrorCode executeTransactionNoLock(const QnTransaction<ApiUserData>& tran);
        ErrorCode executeTransactionNoLock(const QnTransaction<ApiResetBusinessRuleData>& tran); //reset business rules
        ErrorCode executeTransactionNoLock(const QnTransaction<ApiResourceParamDataList>& tran); // save settings
        ErrorCode executeTransactionNoLock(const QnTransaction<ApiVideowallData>& tran);
        ErrorCode executeTransactionNoLock(const QnTransaction<ApiUpdateUploadResponceData>& tran);
        ErrorCode executeTransactionNoLock(const QnTransaction<ApiVideowallDataList>& tran);

        // delete camera, server, layout, any resource, etc.
        ErrorCode executeTransactionNoLock(const QnTransaction<ApiIdData>& tran);

        ErrorCode executeTransactionNoLock(const QnTransaction<ApiLicenseDataList>& tran);
        ErrorCode executeTransactionNoLock(const QnTransaction<ApiLicenseData>& tran);


        ErrorCode executeTransactionNoLock(const QnTransaction<ApiEmailSettingsData>&) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }
        ErrorCode executeTransactionNoLock(const QnTransaction<ApiEmailData>&) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }
        ErrorCode executeTransactionNoLock(const QnTransaction<ApiFullInfoData>&) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }
        ErrorCode executeTransactionNoLock(const QnTransaction<ApiBusinessActionData>&) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionNoLock(const QnTransaction<ApiVideowallControlMessageData> &) {
            Q_ASSERT_X(0, Q_FUNC_INFO, "This is a non persistent transaction!"); // we MUSTN'T be here
            return ErrorCode::notImplemented;
        }

        ErrorCode executeTransactionNoLock(const QnTransaction<ApiUpdateUploadData> &) {
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
        ErrorCode removeResource(const QnId& id);

        ErrorCode insertAddParams(const std::vector<ApiResourceParamData>& params, qint32 internalId);
        ErrorCode deleteAddParams(qint32 resourceId);

        ErrorCode saveCamera(const ApiCameraData& params);
        ErrorCode insertOrReplaceCamera(const ApiCameraData& data, qint32 internalId);
        ErrorCode updateCameraSchedule(const ApiCameraData& data, qint32 internalId);
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

        ErrorCode insertOrReplaceBusinessRuleTable( const ApiBusinessRuleData& businessRule);
        ErrorCode insertBRuleResource(const QString& tableName, const QnId& ruleGuid, const QnId& resourceGuid);
        ErrorCode removeBusinessRule( const QnId& id );
        ErrorCode updateBusinessRule(const ApiBusinessRuleData& rule);

        ErrorCode saveLicense(const ApiLicenseData& license);

        bool createDatabase();
        
        qint32 getResourceInternalId( const QnId& guid );
        QnId getResourceGuid(const qint32 &internalId);
        qint32 getBusinessRuleInternalId( const QnId& guid );
    private:
        QMap<int, QnId> getGuidList(const QString& request);
        bool updateTableGuids(const QString& tableName, const QString& fieldName, const QMap<int, QnId>& guids);
        bool updateGuids();
        QnId getType(const QString& typeName);
    private:
        QnResourceFactory* m_resourceFactory;
        LicenseManagerImpl* const m_licenseManagerImpl;
        QnId m_storageTypeId;
        QnId m_serverTypeId;
        QnId m_cameraTypeId;
        QnId m_adminUserID;
        int m_adminUserInternalID;
        ApiResourceTypeDataList m_cachedResTypes;

        void fillServerInfo( ApiServerInfoData* const serverInfo );
    };
};

#define dbManager QnDbManager::instance()

#endif // __DB_MANAGER_H_
