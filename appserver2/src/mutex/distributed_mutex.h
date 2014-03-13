#ifndef __DISTRIBUTED_MUTEX_H_
#define __DISTRIBUTED_MUTEX_H_

#include <QObject>
#include <QUuid>
#include <QSet>
#include "nx_ec/data/ec2_lock_data.h"

namespace ec2
{
    struct LockRuntimeInfo
    {
        LockRuntimeInfo(const QUuid& peer = QUuid(), qint64 timestamp = 0): peer(peer), timestamp(timestamp) {}
        LockRuntimeInfo(const ApiLockData& data): peer(data.peer), timestamp(data.timestamp) {}

        bool operator<(const LockRuntimeInfo& other) const  { return timestamp != other.timestamp ? timestamp < other.timestamp : peer < other.peer; }
        bool operator==(const LockRuntimeInfo& other) const { return timestamp == other.timestamp && timestamp < other.timestamp; }
        bool isEmpty() const { return peer.isNull(); }
        void clear()         { peer = QUuid(); }

        QUuid peer;
        qint64 timestamp;
    };


    class QnDistributedMutex: public QObject
    {
        Q_OBJECT
    public:
        static const int DEFAULT_LOCK_TIMEOUT = 1000 * 30;

        QnDistributedMutex();
        
        /* Try to lock within timeout */
        void lockAsync(const QByteArray& name, int timeoutMs = DEFAULT_LOCK_TIMEOUT);
        void unlock();
        //bool isLocking() const;
        //bool isLocked() const;
    signals:
        void locked();
        void lockTimeout();
    private slots:
        void at_gotLockRequest(ApiLockData lockInfo);
        void at_gotLockResponse(ApiLockData lockInfo);
        void at_gotUnlockRequest(ApiLockData lockInfo);
        void at_newPeerFound(QnId peer);
        void at_peerLost(QnId peer);
    private:
        bool isAllPeersReady() const;
        void checkForLocked();
        void sendTransaction(const LockRuntimeInfo& lockInfo);
    private:
        QByteArray m_name;
        LockRuntimeInfo m_selfLock;
        typedef QMap<LockRuntimeInfo, int> LockedMap;
        LockedMap m_peerLockInfo;
        QSet<QnId> m_proccesedPeers;
        QTimer timer;
        QMutex m_mutex;
    };
}

#endif // __DISTRIBUTED_MUTEX_H_
