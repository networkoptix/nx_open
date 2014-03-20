#ifndef __AUX_MANAGER_H_
#define __AUX_MANAGER_H_

#include "nx_ec/ec_api.h"
#include "transaction/transaction.h"
#include "nx_ec/data/camera_data.h"
#include "nx_ec/data/ec2_resource_type_data.h"
#include "nx_ec/data/ec2_stored_file_data.h"
#include "utils/db/db_helper.h"
#include "transaction/transaction_log.h"


namespace ec2
{
    class EmailManagerImpl;

    class QnAuxManager
    {
    public:
        QnAuxManager(EmailManagerImpl* const emailManagerImpl);
        virtual ~QnAuxManager();

        static QnAuxManager* instance();
        
        template <class T>
        ErrorCode executeTransaction(const QnTransaction<T>&)
        {
            return ErrorCode::ok;
        }

        ErrorCode executeTransaction(const QnTransaction<ApiEmailSettingsData>& tran);
        ErrorCode executeTransaction(const QnTransaction<ApiEmailData>& tran);
    private:
        EmailManagerImpl* const m_emailManagerImpl;
    };
};

#define auxManager QnAuxManager::instance()

#endif // __AUX_MANAGER_H_
