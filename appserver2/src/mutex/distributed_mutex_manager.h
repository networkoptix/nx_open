#ifndef __DISTRIBUTED_MUTEX_MANAGER_H_
#define __DISTRIBUTED_MUTEX_MANAGER_H_

#include "nx_ec/data/api_lock_data.h"
#include "transaction/transaction.h"

namespace ec2
{

    class QnMutexUserDataHandler;
    class QnDistributedMutex;

    class QnDistributedMutexManager: public QObject
    {
        Q_OBJECT
    public:
        static const int DEFAULT_TIMEOUT = 1000 * 30;

        QnDistributedMutexManager();
        QnDistributedMutex* createMutex(const QString& name);

        static void initStaticInstance(QnDistributedMutexManager*);
        static QnDistributedMutexManager* instance();

        void setUserDataHandler(QnMutexUserDataHandler* userDataHandler);
    signals:
        void peerFound(ec2::ApiPeerAliveData data);
        void peerLost(ec2::ApiPeerAliveData data);
    private:
        qint64 newTimestamp();
    private:
        friend class QnDistributedMutex;

        void at_gotLockRequest(ApiLockData lockInfo);
        void at_gotLockResponse(ApiLockData lockInfo);
        //void at_gotUnlockRequest(ApiLockData lockInfo);
        void at_newPeerFound(QnUuid peer);
        void at_peerLost(QnUuid peer);
        void releaseMutex(const QString& name);
    private:
        QMap<QString, QnDistributedMutex*> m_mutexList;
        mutable QMutex m_mutex;
        qint64 m_timestamp;
        QnMutexUserDataHandler* m_userDataHandler;
    };
}

#endif // __DISTRIBUTED_MUTEX_MANAGER_H_
