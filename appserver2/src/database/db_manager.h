#ifndef __DB_MANAGER_H_
#define __DB_MANAGER_H_

#include "nx_ec/ec_api.h"
#include "transaction/transaction.h"


namespace ec2
{
    class QnDbManager
    {
    public:
        static QnDbManager* instance();
        void initStaticInstance(QnDbManager* value);

        template<class QueryDataType>
        ErrorCode executeTransaction( const QnTransaction<QueryDataType>& /*tran*/ )
        {
            // todo: implement me
            return ErrorCode::ok;
        }
    };
};

#define dbManager QnDbManager::instance()

#endif // __DB_MANAGER_H_
