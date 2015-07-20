/**********************************************************
* 28 may 2014
* a.kolesnikov
***********************************************************/

#ifndef EC_CONNECTION_AUDIT_MANAGER_MANAGER_H
#define EC_CONNECTION_AUDIT_MANAGER_MANAGER_H

#include "nx_ec/ec_api.h"

#include "transaction/transaction.h"
#include <transaction/transaction_log.h>
#include "api/model/audit/auth_session.h"

namespace ec2
{
    class AbstractECConnection;

    class ECConnectionAuditManager
    {
    public:
        ECConnectionAuditManager(AbstractECConnection* ecConnection);
        template <class T>
        void addAuditRecord( const QnTransaction<T>& tran, const QnAuthSession& authInfo)
        {
            // nothing to do by default
        }

        void addAuditRecord( const QnTransaction<ApiCameraAttributesData>& tran, const QnAuthSession& authInfo);
    };

}

#endif  //EC_CONNECTION_NOTIFICATION_MANAGER_H
