#ifndef __CLUSTER_MANAGER_H_
#define __CLUSTER_MANAGER_H_

#include "nx_ec/ec_api.h"
#include "transaction/transaction.h"
#include "transaction/transaction_message_bus.h"

namespace ec2
{
    class QnClusterManager
    {
    public:
        static QnClusterManager* instance();
        void initStaticInstance(QnClusterManager* value);

        template <class T>
        ErrorCode distributeAsync(const QnTransaction<T>& tran) {
            QnTransactionMessageBus::instance()->sendTransaction(tran);
            return ErrorCode::ok;
        }

    private:
        template<class T> void transactionReceived( const QnTransaction<T>& tran );
        template<class T> void processRemoteTransaction( const QnTransaction<T>& tran );
    };
};

#define clusterManager QnClusterManager::instance()

#endif // __CLUSTER_MANAGER_H_
