#ifndef __DB_MANAGER_H_
#define __DB_MANAGER_H_

#include "nx_ec/ec_api.h"
#include "transaction/transaction.h"
#include "nx_ec/data/camera_data.h"


class QSqlDatabase;

namespace ec2
{
    class QnDbManager
    {
    public:
		QnDbManager();
		virtual ~QnDbManager();

        static QnDbManager* instance();


		// todo: const QnTransaction<QueryDataType> MUST be used. Const absent as temporary solution because we are still using ID instead of GUID
        template<class QueryDataType>
        ErrorCode executeTransaction( const QnTransaction<QueryDataType>& /*tran*/ )
        {
            Q_ASSERT_X(1, "Not implemented", Q_FUNC_INFO);
            return ErrorCode::ok;
        }

        ErrorCode executeTransaction(const QnTransaction<ApiCameraData>& tran);

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
    };
};

#define dbManager QnDbManager::instance()

#endif // __DB_MANAGER_H_
