#ifndef __CLUSTER_MANAGER_H_
#define __CLUSTER_MANAGER_H_

#include "transaction/transaction.h"

namespace ec2
{
    class QnClusterManager
    {
    public:
        static QnClusterManager* instance();
        void initStaticInstance(QnClusterManager* value);

        enum ErrorCode {
            No_Error,
            General_Error
        };

        template <class T>
        ErrorCode distributeAsync(QnTransaction<T> tran) {
            // todo: implement me
            return No_Error;
        }

    };
};

#define clusterManager QnClusterManager::instance()

#endif // __CLUSTER_MANAGER_H_
