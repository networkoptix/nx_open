#pragma once

#include <nx/vms/api/data/lock_data.h>
#include "transaction/transaction.h"

namespace ec2
{

    class QnMutexUserDataHandler;
    class QnDistributedMutex;
    class QnTransactionMessageBus;

    class QnDistributedMutexManager: public QObject
    {
        Q_OBJECT
    public:
        static const int DEFAULT_TIMEOUT = 1000 * 30;

        QnDistributedMutexManager(QnTransactionMessageBus* messageBus);
        QnDistributedMutex* createMutex(const QString& name);

        void setUserDataHandler(QnMutexUserDataHandler* userDataHandler);

        QnTransactionMessageBus* messageBus() const;

    signals:
        void peerFound(QnUuid data, nx::vms::api::PeerType peerType);
        void peerLost(QnUuid data, nx::vms::api::PeerType peerType);

    private:
        qint64 newTimestamp();
    private:
        friend class QnDistributedMutex;

        void at_gotLockRequest(nx::vms::api::LockData lockInfo);
        void at_gotLockResponse(nx::vms::api::LockData lockInfo);
        //void at_gotUnlockRequest(nx::vms::api::LockData lockInfo);
        void at_newPeerFound(QnUuid peer);
        void at_peerLost(QnUuid peer);
        void releaseMutex(const QString& name);
    private:
        QMap<QString, QnDistributedMutex*> m_mutexList;
        mutable QnMutex m_mutex;
        qint64 m_timestamp{1};
        QnMutexUserDataHandler* m_userDataHandler{nullptr};
        QnTransactionMessageBus* m_messageBus{nullptr};
    };
}
