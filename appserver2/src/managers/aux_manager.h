#ifndef __AUX_MANAGER_H_
#define __AUX_MANAGER_H_

#include "nx_ec/ec_api.h"
#include "transaction/transaction.h"
#include "nx_ec/data/api_resource_type_data.h"
#include "utils/db/db_helper.h"
#include "transaction/transaction_log.h"

namespace ec2
{
    class QnAuxManager
    {
    public:
        QnAuxManager();
        virtual ~QnAuxManager();

        static QnAuxManager* instance();
        
        template <class T>
        ErrorCode executeTransaction(const QnTransaction<T>&)
        {
            return ErrorCode::ok;
        }
    };
};

#define auxManager QnAuxManager::instance()

#endif // __AUX_MANAGER_H_
