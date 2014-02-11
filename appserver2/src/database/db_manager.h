#ifndef __DB_MANAGER_H_
#define __DB_MANAGER_H_

#include "nx_ec/ec_api.h"
#include "transaction/transaction.h"
#include "nx_ec/data/camera_data.h"
#include "nx_ec/data/ec2_resource_type_data.h"
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
		QnDbManager(QnResourceFactory* factory);
		virtual ~QnDbManager();

        static QnDbManager* instance();


		// ------------ transactions --------------------------------------

        template<class QueryDataType>
        ErrorCode executeTransaction( const QnTransaction<QueryDataType>& /*tran*/ )
        {
            Q_ASSERT_X(1, "Not implemented", Q_FUNC_INFO);
            return ErrorCode::ok;
        }

        ErrorCode executeTransaction(const QnTransaction<ApiCameraData>& tran);
        ErrorCode executeTransaction(const QnTransaction<ApiMediaServerData>& tran);
        ErrorCode executeTransaction(const QnTransaction<ApiLayoutData>& tran);
        ErrorCode executeTransaction(const QnTransaction<ApiSetResourceStatusData>& tran);
        ErrorCode executeTransaction(const QnTransaction<ApiResourceParams>& tran);
        ErrorCode executeTransaction(const QnTransaction<ApiCameraServerItemData>& tran);
        ErrorCode executeTransaction(const QnTransaction<ApiPanicModeData>& tran);

        // delete camera, server, layout t.e.c
        ErrorCode executeTransaction(const QnTransaction<ApiIdData>& tran);
		
		// --------- get methods ---------------------
        //getResourceTypes
		ErrorCode doQuery(nullptr_t /*dummy*/, ApiResourceTypeList& resourceTypeList);

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
        ErrorCode doQuery(nullptr_t /*dummy*/, ApiLayoutDataList& userList);

        //getCurrentTime
        ErrorCode doQuery(nullptr_t /*dummy*/, qint64& userList);

        //getResourceParams
        ErrorCode doQuery(const QnId& resourceId, ApiResourceParams& params);

        // ApiFullData
        ErrorCode doQuery(nullptr_t /*dummy*/, ApiFullData& data);

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

        ErrorCode updateResource(const ApiResourceData& data);
		ErrorCode insertResource(const ApiResourceData& data);
        ErrorCode deleteResourceTable(const qint32 id);

        ErrorCode insertAddParam(const ApiResourceParam& param);
        ErrorCode removeAddParam(const ApiResourceParam& param);
        ErrorCode deleteAddParams(qint32 resourceId);

		ErrorCode updateCamera(const ApiCameraData& data);
		ErrorCode insertCamera(const ApiCameraData& data);
        ErrorCode updateCameraSchedule(const ApiCameraData& data);
        ErrorCode removeCamera(const qint32 id);
        ErrorCode deleteCameraTable(const qint32 id);
        ErrorCode deleteCameraServerItemTable(qint32 id);
        ErrorCode deleteBusinessRuleResourceTable(qint32 id, const QString& tableName);

        ErrorCode updateMediaServer(const ApiMediaServerData& data);
        ErrorCode insertMediaServer(const ApiMediaServerData& data);
        ErrorCode updateStorages(const ApiMediaServerData&);
        ErrorCode removeServer(const qint32 id);
        ErrorCode removeLayout(const qint32 id);
        ErrorCode deleteServerTable(const qint32 id);
        ErrorCode removeStoragesByServer(qint32 id);

        ErrorCode deleteLayoutItems(const qint32 id);
        ErrorCode deleteLayoutTable(const qint32 id);
        ErrorCode insertLayout(const ApiLayoutData& data);
        ErrorCode updateLayout(const ApiLayoutData& data);
        ErrorCode updateLayoutItems(const ApiLayoutData& data);
        ErrorCode removeLayoutItems(qint32 id);

		bool createDatabase();
        
        void mergeRuleResource(QSqlQuery& query, ApiBusinessRuleDataList& data, std::vector<qint32> ApiBusinessRuleData::*resList);
    private:
        QSqlDatabase m_sdb;
        QReadWriteLock m_mutex;
		QnResourceFactory* m_resourceFactory;
        qint32 m_storageTypeId;
        QnDbTransaction m_tran;
    };
};

#define dbManager QnDbManager::instance()

#endif // __DB_MANAGER_H_
