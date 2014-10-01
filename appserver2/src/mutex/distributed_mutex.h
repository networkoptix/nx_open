#ifndef __DISTRIBUTED_MUTEX_H_
#define __DISTRIBUTED_MUTEX_H_

#include <QtCore/QObject>
#include <utils/common/uuid.h>
#include <QtCore/QSet>
#include <QtCore/QSharedPointer>
#include <QtCore/QTimer>
#include <QtCore/QQueue>

#include "nx_ec/data/api_lock_data.h"
#include "transaction/transaction.h"

namespace ec2
{
    class QnDistributedMutexManager;

    struct LockRuntimeInfo: public ApiLockData
    {
        LockRuntimeInfo(const QnUuid& _peer = QnUuid(), qint64 _timestamp = 0, const QString& _name = QString()) {
            peer = _peer;
            timestamp = _timestamp;
            name = _name;
        }
        LockRuntimeInfo(const ApiLockData& data): ApiLockData(data) {}

        bool operator<(const LockRuntimeInfo& other) const  { return timestamp != other.timestamp ? timestamp < other.timestamp : peer < other.peer; }
        bool operator==(const LockRuntimeInfo& other) const { return timestamp == other.timestamp && peer == other.peer; }
        bool isEmpty() const { return peer.isNull(); }
        void clear()         { peer = QnUuid(); }
    };

    /*
    * Ricart-Agrawala algorithm
    */

    class QnDistributedMutex: public QObject
    {
        Q_OBJECT
    public:
        static const int DEFAULT_LOCK_TIMEOUT = 1000 * 30;
        void lockAsync(int timeoutMs = DEFAULT_LOCK_TIMEOUT);
        void unlock();
        bool checkUserData() const;
        virtual ~QnDistributedMutex();

        QString name() const { return m_name; }
    private:
        friend class QnDistributedMutexManager;

        QnDistributedMutex(QnDistributedMutexManager* owner, const QString& name);
        
        /* Try to lock within timeout */
        bool isLocking() const;
        bool isLocked() const;
    signals:
        void locked();
        void lockTimeout();
    private slots:
        void at_gotLockRequest(ApiLockData lockInfo);
        void at_gotLockResponse(ApiLockData lockInfo);
        //void at_gotUnlockRequest(ApiLockData lockInfo);
        void at_newPeerFound(ec2::ApiPeerAliveData data);
        void at_peerLost(ec2::ApiPeerAliveData data);
        void at_timeout();
    private:
        bool isAllPeersReady() const;
        void checkForLocked();
        void sendTransaction(const LockRuntimeInfo& lockInfo, ApiCommand::Value command, const QnUuid& dstPeer);
        void unlockInternal();
    private:
        QString m_name;
        LockRuntimeInfo m_selfLock;
        typedef QMap<LockRuntimeInfo, int> LockedMap;
        LockedMap m_peerLockInfo;
        QSet<QnUuid> m_proccesedPeers;
        QTimer* m_timer;
        mutable QMutex m_mutex;
        bool m_locked;
        QQueue<ApiLockData> m_delayedResponse;
        QnDistributedMutexManager* m_owner;
        QByteArray m_userData;
    };
    //typedef QSharedPointer<QnDistributedMutex> QnDistributedMutexPtr;
    
    /*
    * This class is using for provide extra information
    * For camera adding scenario under a mutex, this class should provide information
    * if peer already contain camera in the lockResponse data.
    * It requires because of peer can catch mutex, but camera already exists on the other peer,
    * so that peer should tell about it camera in a response message
    */
    class QnMutexUserDataHandler
    {
    public:
        virtual ~QnMutexUserDataHandler() {}
        virtual QByteArray getUserData(const QString& name) = 0;
        virtual bool checkUserData(const QString& name, const QByteArray& data) = 0;
    };

}

#endif // __DISTRIBUTED_MUTEX_H_
