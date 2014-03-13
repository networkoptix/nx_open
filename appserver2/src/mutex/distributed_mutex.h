#ifndef __DISTRIBUTED_MUTEX_H_
#define __DISTRIBUTED_MUTEX_H_

#include <QObject>
#include <QUuid>
#include <QSet>
#include "nx_ec/data/api_data.h"

namespace ec2
{
    struct ApiLockInfo: public ApiData
    {
        qint64 timestamp;
        QByteArray name;
    };
    QN_DEFINE_STRUCT_SERIALIZATORS (ApiLockInfo, (timestamp) (name) )

    struct LockRuntimeInfo
    {
        LockRuntimeInfo(const QUuid& peer = QUuid(), qint64 timestamp = 0): peer(peer), timestamp(timestamp) {}

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
        void at_gotLockInfo(LockRuntimeInfo lockInfo);
        void at_gotUnlockInfo(LockRuntimeInfo lockInfo);
        void at_newPeerFound();
    private:
        bool isAllPeersReady() const;
        void checkForLocked();
        void sendTransaction();
    private:
        QByteArray m_name;
        LockRuntimeInfo m_selfLock;
        typedef QMap<LockRuntimeInfo, int> LockedMap;
        LockedMap m_peerLockInfo;
        QTimer timer;
        QMutex m_mutex;
    };
}

#endif // __DISTRIBUTED_MUTEX_H_
