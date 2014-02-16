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
		QnDbManager(QnResourceFactory* factory, StoredFileManagerImpl* const storedFileManagerImpl);
		virtual ~QnDbManager();

        static QnDbManager* instance();


		// ------------ transactions --------------------------------------

        template<class QueryDataType>
        ErrorCode executeTransaction( const QnTransaction<QueryDataType>& /*tran*/ )
        {
            static_assert( false, "You have to add QnDbManager::executeTransaction specification" );
            return ErrorCode::ok;
        }

        ErrorCode executeTransaction(const QnTransaction<ApiCameraData>& tran);
        ErrorCode executeTransaction(const QnTransaction<ApiCameraDataList>& tran);
        ErrorCode executeTransaction(const QnTransaction<ApiMediaServerData>& tran);
        ErrorCode executeTransaction(const QnTransaction<ApiLayoutData>& tran);
        ErrorCode executeTransaction(const QnTransaction<ApiLayoutDataList>& tran);
        ErrorCode executeTransaction(const QnTransaction<ApiSetResourceStatusData>& tran);
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
        //getResourceTypes
		ErrorCode doQuery(nullptr_t /*dummy*/, ApiResourceTypeList& resourceTypeList);

        //getResources
		ErrorCode doQuery(nullptr_t /*dummy*/, ApiResourceDataList& resourceList);

        //getCameras
        ErrorCode doQuery(const QnId& mServerId, ApiCameraDataList& cameraList);

        //getServers
        ErrorCode doQuery(nullptr_t /*dummy*/, ApiMediaServerDataList& serverList);

        //getCameraServerItems
        ErrorCode doQuery(nullptr_t /*dummy*/, ApiCameraServerItemDataList& historyList);

        //getUserList
        ErrorCode doQuery(nullptr_t /*dummy*/, ApiUserDataList& userList);

        //getBusinessRuleList
        ErrorCode doQuery(nullptr_t /*dummy*/, ApiBusinessRuleDataList& userList);

        //getBusinessRuleList
        ErrorCode doQuery(nullptr_t /*dummy*/, ApiLayoutDataList& layoutList);

        //getCurrentTime
        ErrorCode doQuery(nullptr_t /*dummy*/, qint64& userList);

        //getResourceParams
        ErrorCode doQuery(const QnId& resourceId, ApiResourceParams& params);

        // ApiFullData
        ErrorCode doQuery(nullptr_t /*dummy*/, ApiFullData& data);

        ErrorCode doQuery(const ApiStoredFilePath& path, ApiStoredDirContents& data);
        ErrorCode doQuery(const ApiStoredFilePath& path, ApiStoredFileData& data);

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

        ErrorCode deleteTableRecord(qint32 id, const QString& tableName, const QString& fieldName);

        ErrorCode updateResource(const ApiResourceData& data);
		ErrorCode insertResource(const ApiResourceData& data);
        ErrorCode insertOrReplaceResource(const ApiResourceData& data);
        ErrorCode deleteResourceTable(const qint32 id);
        ErrorCode removeResource(const qint32 id);

        ErrorCode insertAddParam(const ApiResourceParam& param);
        ErrorCode removeAddParam(const ApiResourceParam& param);
        ErrorCode deleteAddParams(qint32 resourceId);

		ErrorCode updateCamera(const ApiCameraData& data);
		ErrorCode insertCamera(const ApiCameraData& data);
        ErrorCode updateCameraSchedule(const ApiCameraData& data);
        ErrorCode removeCamera(const qint32 id);
        ErrorCode deleteCameraServerItemTable(qint32 id);

        ErrorCode updateMediaServer(const ApiMediaServerData& data);
        ErrorCode insertMediaServer(const ApiMediaServerData& data);
        ErrorCode updateStorages(const ApiMediaServerData&);
        ErrorCode removeServer(const qint32 id);
        ErrorCode removeLayout(const qint32 id);
        ErrorCode removeLayoutNoLock(const qint32 id);
        ErrorCode removeStoragesByServer(qint32 id);

        ErrorCode deleteLayoutItems(const qint32 id);
        ErrorCode insertLayout(const ApiLayoutData& data);
        ErrorCode insertOrReplaceLayout(const ApiLayoutData& data);
        ErrorCode updateLayout(const ApiLayoutData& data);
        ErrorCode updateLayoutItems(const ApiLayoutData& data);
        ErrorCode removeLayoutItems(qint32 id);

        ErrorCode insertUser( const ApiUserData& data );
        ErrorCode updateUser( const ApiUserData& data );
        ErrorCode deleteUserProfileTable(const qint32 id);
        ErrorCode removeUser( qint32 id );

        ErrorCode insertBusinessRule( const ApiBusinessRuleData& businessRule );
        ErrorCode updateBusinessRule( const ApiBusinessRuleData& businessRule );
        ErrorCode removeBusinessRule( qint32 id );

		bool createDatabase();
        
        void mergeRuleResource(QSqlQuery& query, ApiBusinessRuleDataList& data, std::vector<qint32> ApiBusinessRuleData::*resList);
    private:
        QSqlDatabase m_sdb;
        QReadWriteLock m_mutex;
		QnResourceFactory* m_resourceFactory;
        StoredFileManagerImpl* const m_storedFileManagerImpl;
        qint32 m_storageTypeId;
        QnDbTransaction m_tran;
    };
};

#define dbManager QnDbManager::instance()

#endif // __DB_MANAGER_H_
