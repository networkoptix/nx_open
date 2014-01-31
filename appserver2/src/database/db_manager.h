#ifndef __DB_MANAGER_H_
#define __DB_MANAGER_H_

#include "nx_ec/ec_api.h"
#include "transaction/transaction.h"
#include "nx_ec/data/camera_data.h"
#include "nx_ec/data/ec2_resource_type_data.h"
#include "nx_ec/data/mserver_data.h"
#include "nx_ec/data/camera_server_item_data.h"


class QSqlDatabase;

namespace ec2
{
    class QnDbManager
    {
    public:
		QnDbManager(QnResourceFactoryPtr factory);
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
		
		// --------- get methods ---------------------
        //getResourceTypes
		ErrorCode doQuery(nullptr_t /*dummy*/, ApiResourceTypeList& resourceTypeList);

		//getCameras
        ErrorCode doQuery(const QnId& mServerId, ApiCameraDataList& cameraList);

        //getServers
        ErrorCode doQuery(nullptr_t /*dummy*/, ApiMediaServerDataList& serverList);

        //getCameraServerItems
        ErrorCode doQuery(nullptr_t /*dummy*/, ApiCameraServerItemDataList& historyList);

		// --------- misc -----------------------------

		int getNextSequence();
    private:
        ErrorCode updateResource(const ApiResourceData& data);
		ErrorCode insertResource(const ApiResourceData& data);

		ErrorCode updateCamera(const ApiCameraData& data);
		ErrorCode insertCamera(const ApiCameraData& data);

		ErrorCode updateCameraSchedule(const ApiCameraData& data);

		bool createDatabase();
    private:
        QSqlDatabase m_sdb;
		QMutex m_mutex;
		QnResourceFactoryPtr  m_resourceFactory;
    };
};

#define dbManager QnDbManager::instance()

#endif // __DB_MANAGER_H_
