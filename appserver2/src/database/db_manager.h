#ifndef __DB_MANAGER_H_
#define __DB_MANAGER_H_

#include "managers/impl/stored_file_manager_impl.h"
#include "nx_ec/ec_api.h"
#include "transaction/transaction.h"
#include "nx_ec/data/camera_data.h"
#include "nx_ec/data/ec2_resource_type_data.h"
#include "nx_ec/data/ec2_stored_file_data.h"
#include "nx_ec/data/mserver_data.h"
#include "nx_ec/data/camera_server_item_data.h"
#include "nx_ec/data/ec2_user_data.h"
#include "nx_ec/data/ec2_layout_data.h"
#include "nx_ec/data/ec2_business_rule_data.h"
#include "nx_ec/data/ec2_full_data.h"


class QSqlDatabase;

namespace ec2
{
    class QnDbManager
    {
    public:
		QnDbManager(QnResourceFactory* factory, StoredFileManagerImpl* const storedFileManagerImpl, const QString& dbFileName);
		virtual ~QnDbManager();

        static QnDbManager* instance();


		// ------------ transactions --------------------------------------

        template<class QueryDataType>
        ErrorCode executeTransaction( const QnTransaction<QueryDataType>& /*tran*/ )
        {
            //static_assert( false, "You have to add QnDbManager::executeTransaction specification" );
            Q_ASSERT_X( 0, Q_FUNC_INFO, "You have to add QnDbManager::executeTransaction specification" );
            return ErrorCode::ok;
        }

        ErrorCode executeTransaction(const QnTransaction<ApiCameraData>& tran);
        ErrorCode executeTransaction(const QnTransaction<ApiCameraDataList>& tran);
        ErrorCode executeTransaction(const QnTransaction<ApiMediaServerData>& tran);
        ErrorCode executeTransaction(const QnTransaction<ApiLayoutData>& tran);
        ErrorCode executeTransaction(const QnTransaction<ApiLayoutDataList>& tran);
        ErrorCode executeTransaction(const QnTransaction<ApiSetResourceStatusData>& tran);
        ErrorCode executeTransaction(const QnTransaction<ApiSetResourceDisabledData>& tran);
        ErrorCode executeTransaction(const QnTransaction<ApiResourceParams>& tran);
        ErrorCode executeTransaction(const QnTransaction<ApiCameraServerItemData>& tran);
        ErrorCode executeTransaction(const QnTransaction<ApiPanicModeData>& tran);
        ErrorCode executeTransaction(const QnTransaction<ApiStoredFileData>& tran);
        ErrorCode executeTransaction(const QnTransaction<ApiStoredFilePath>& tran);
        ErrorCode executeTransaction(const QnTransaction<ApiResourceData>& tran);
        ErrorCode executeTransaction(const QnTransaction<ApiBusinessRuleData>& tran);
        ErrorCode executeTransaction(const QnTransaction<ApiUserData>& tran);

        // delete camera, server, layout, any resource t.e.c
        ErrorCode executeTransaction(const QnTransaction<ApiIdData>& tran);
		
		// --------- get methods ---------------------

        template <class T1, class T2>
        ErrorCode doQuery(const T1& t1, T2& t2)
        {
            QReadLocker lock(&m_mutex);
            return doQueryNoLock(t1, t2);
        }

        template <class T1, class T2>
        ErrorCode doQueryNoLock(const T1& t1, T2& t2)
        {
            static_assert( false, "You have to add QnDbManager::doQueryNoLock specification" );
            return ErrorCode::ok;
        }

        //getCurrentTime
        ErrorCode doQuery(const nullptr_t& /*dummy*/, qint64& currentTime);
        //getStoragePath
        ErrorCode doQuery(const ApiStoredFilePath& path, ApiStoredDirContents& data);
        ErrorCode doQuery(const ApiStoredFilePath& path, ApiStoredFileData& data);


        //getResourceTypes
		ErrorCode doQueryNoLock(const nullptr_t& /*dummy*/, ApiResourceTypeList& resourceTypeList);

        //getCameras
        ErrorCode doQueryNoLock(const QnId& mServerId, ApiCameraDataList& cameraList);

        //getServers
        ErrorCode doQueryNoLock(const nullptr_t& /*dummy*/, ApiMediaServerDataList& serverList);

        //getCameraServerItems
        ErrorCode doQueryNoLock(const nullptr_t& /*dummy*/, ApiCameraServerItemDataList& historyList);

        //getUserList
        ErrorCode doQueryNoLock(const nullptr_t& /*dummy*/, ApiUserDataList& userList);

        //getBusinessRuleList
        ErrorCode doQueryNoLock(const nullptr_t& /*dummy*/, ApiBusinessRuleDataList& userList);

        //getBusinessRuleList
        ErrorCode doQueryNoLock(const nullptr_t& /*dummy*/, ApiLayoutDataList& layoutList);

        //getResourceParams
        ErrorCode doQueryNoLock(const QnId& resourceId, ApiResourceParams& params);

        // ApiFullData
        ErrorCode doQueryNoLock(const nullptr_t& /*dummy*/, ApiFullData& data);

		// --------- misc -----------------------------

		int getNextSequence();
    private:

        class QnDbTransactionLocker;

        class QnDbTransaction
        {
        public:
            QnDbTransaction(QSqlDatabase& m_sdb, QReadWriteLock& mutex);
        private:
            friend class QnDbTransactionLocker;

            void beginTran();
            void rollback();
            void commit();
        private:
            QSqlDatabase& m_sdb;
            QReadWriteLock& m_mutex;
        };

        class QnDbTransactionLocker
        {
        public:
            QnDbTransactionLocker(QnDbTransaction* tran);
            ~QnDbTransactionLocker();
            void commit();
        private:
            bool m_committed;
            QnDbTransaction* m_tran;
        };

        ErrorCode deleteTableRecord(const QnId& id, const QString& tableName, const QString& fieldName);
        ErrorCode deleteTableRecord(const qint32& internalId, const QString& tableName, const QString& fieldName);

        ErrorCode updateResource(const ApiResourceData& data, qint32 internalId);
		ErrorCode insertResource(const ApiResourceData& data, qint32* internalId);
        ErrorCode insertOrReplaceResource(const ApiResourceData& data, qint32* internalId);
        //ErrorCode insertOrReplaceResource(const ApiResourceData& data);
        ErrorCode deleteResourceTable(const qint32 id);
        ErrorCode removeResource(const QnId& id);

        ErrorCode insertAddParam(const ApiResourceParam& param, qint32 internalId);
        ErrorCode removeAddParam(const ApiResourceParam& param);
        ErrorCode deleteAddParams(qint32 resourceId);

        ErrorCode saveCamera(const ApiCameraData& params);
        ErrorCode insertOrReplaceCamera(const ApiCameraData& data, qint32 internalId);
        ErrorCode updateCameraSchedule(const ApiCameraData& data, qint32 internalId);
        ErrorCode removeCamera(const QnId& guid);
        ErrorCode deleteCameraServerItemTable(qint32 id);

        ErrorCode insertOrReplaceMediaServer(const ApiMediaServerData& data, qint32 internalId);
        ErrorCode updateStorages(const ApiMediaServerData&);
        ErrorCode removeServer(const QnId& guid);
        ErrorCode removeLayout(const QnId& guid);
        ErrorCode removeLayout(qint32 id);
        ErrorCode removeStoragesByServer(const QnId& serverGUID);

        ErrorCode deleteLayoutItems(const qint32 id);
        ErrorCode saveLayout(const ApiLayoutData& params);
        ErrorCode insertOrReplaceLayout(const ApiLayoutData& data, qint32 internalId);
        ErrorCode updateLayoutItems(const ApiLayoutData& data, qint32 internalLayoutId);
        ErrorCode removeLayoutItems(qint32 id);

        ErrorCode saveUser( const ApiUserData& data );
        ErrorCode deleteUserProfileTable(const qint32 id);
        ErrorCode removeUser( const QnId& guid );

        ErrorCode insertOrReplaceBusinessRuleTable( const ApiBusinessRuleData& businessRule);
        ErrorCode insertBRuleResource(const QString& tableName, const QnId& ruleGuid, const QnId& resourceGuid);
        ErrorCode removeBusinessRule( const QnId& id );

		bool createDatabase();
        bool execSQLFile(const QString& fileName);
        
        void mergeRuleResource(QSqlQuery& query, ApiBusinessRuleDataList& data, std::vector<qint32> ApiBusinessRuleData::*resList);

        bool isObjectExists(const QString& objectType, const QString& objectName);
        qint32 getResourceInternalId( const QnId& guid );
        qint32 getBusinessRuleInternalId( const QnId& guid );
    private:
        QMap<int, QnId> getGuidList(const QString& request);
        bool updateTableGuids(const QString& tableName, const QString& fieldName, const QMap<int, QnId>& guids);
        bool updateGuids();
    private:
        QSqlDatabase m_sdb;
        QReadWriteLock m_mutex;
		QnResourceFactory* m_resourceFactory;
        StoredFileManagerImpl* const m_storedFileManagerImpl;
        QnId m_storageTypeId;
        QnDbTransaction m_tran;
    };
};

#define dbManager QnDbManager::instance()

#endif // __DB_MANAGER_H_
