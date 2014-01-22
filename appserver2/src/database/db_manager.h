#ifndef __DB_MANAGER_H_
#define __DB_MANAGER_H_

#include "transaction/transaction.h"

namespace ec2
{
    class QnDbManager
    {
    public:
        static QnDbManager* instance();
        void initStaticInstance(QnDbManager* value);

        enum ErrorCode {
            No_Error,
            General_Error
        };

        ErrorCode executeTransaction(QnTransaction<ApiCameraData> tran);
    };
};

#define dbManager QnDbManager::instance()

#endif // __DB_MANAGER_H_
