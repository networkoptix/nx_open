#ifndef __DISTRIBUTED_MUTEX_H_
#define __DISTRIBUTED_MUTEX_H_

#include <QObject>
#include <QUuid>
#include <QSet>
#include <QSharedPointer>
#include "nx_ec/data/ec2_lock_data.h"
#include "transaction/transaction.h"

namespace ec2
{
    class QnDistributedMutexManager;

    struct LockRuntimeInfo: public ApiLockData
    {
        LockRuntimeInfo(const QUuid& _peer = QUuid(), qint64 _timestamp = 0) {
            peer = _peer;
            timestamp = _timestamp;
        }
        LockRuntimeInfo(const ApiLockData& data): ApiLockData(data) {}

        bool operator<(const LockRuntimeInfo& other) const  { return timestamp != other.timestamp ? timestamp < other.timestamp : peer < other.peer; }
        bool operator==(const LockRuntimeInfo& other) const { return timestamp == other.timestamp && timestamp < other.timestamp; }
        bool isEmpty() const { return peer.isNull(); }
        void clear()         { peer = QUuid(); }
    };

    /*
    * Ricart–Agrawala algorithm
    */

    class QnDistributedMutex: public QObject
    {
        Q_OBJECT
    public:
        void unlock();
        bool checkUserData() const;
        virtual ~QnDistributedMutex();
    private:
        friend class QnDistributedMutexManager;

        static const int DEFAULT_LOCK_TIMEOUT = 1000 * 30;

        QnDistributedMutex(QnDistributedMutexManager* owner);
        
        /* Try to lock within timeout */
        void lockAsync(const QByteArray& name, int timeoutMs = DEFAULT_LOCK_TIMEOUT);
        bool isLocking() const;
        bool isLocked() const;
    signals:
        void locked(QByteArray name);
        void lockTimeout(QByteArray name);
    private slots:
        void at_gotLockRequest(ApiLockData lockInfo);
        void at_gotLockResponse(ApiLockData lockInfo);
        void at_gotUnlockRequest(ApiLockData lockInfo);
        void at_newPeerFound(QnId peer);
        void at_peerLost(QnId peer);
        void at_timeout();
    private:
        bool isAllPeersReady() const;
        void checkForLocked();
        void sendTransaction(const LockRuntimeInfo& lockInfo, ApiCommand::Value command, const QnId& dstPeer = QnId());
    private:
        QByteArray m_name;
        LockRuntimeInfo m_selfLock;
        typedef QMap<LockRuntimeInfo, int> LockedMap;
        LockedMap m_peerLockInfo;
        QSet<QnId> m_proccesedPeers;
        QTimer timer;
        mutable QMutex m_mutex;
        bool m_locked;
        QQueue<ApiLockData> m_delayedResponse;
        QnDistributedMutexManager* m_owner;
        QByteArray m_userData;
    };
    typedef QSharedPointer<QnDistributedMutex> QnDistributedMutexPtr;
    
    class QnMutexUserDataHandler
    {
    public:
        virtual ~QnMutexUserDataHandler() {}
        virtual QByteArray getUserData(const QString& name) = 0;
        virtual bool checkUserData(const QString& name, const QByteArray& data) = 0;
    };

    class QnDistributedMutexManager: public QObject
    {
        Q_OBJECT
    public:
        QnDistributedMutexManager();
        QnDistributedMutexManager* instance();
        QnDistributedMutexPtr getLock(const QByteArray& name, int timeoutMs);
    signals:
        void locked(QByteArray name);
        void lockTimeout(QByteArray name);
    private:
        qint64 newTimestamp();
    private:
        friend class QnDistributedMutex;

        void at_gotLockRequest(ApiLockData lockInfo);
        void at_gotLockResponse(ApiLockData lockInfo);
        void at_gotUnlockRequest(ApiLockData lockInfo);
        void at_newPeerFound(QnId peer);
        void at_peerLost(QnId peer);
        void releaseMutex(const QByteArray& name);
    private:
        QMap<QByteArray, QnDistributedMutexPtr> m_mutexList;
        mutable QMutex m_mutex;
        qint64 m_timestamp;
        QnMutexUserDataHandler* m_userDataHandler;
    };

}

#endif // __DISTRIBUTED_MUTEX_H_
