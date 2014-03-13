#include "distributed_mutex.h"

#include "transaction/transaction_message_bus.h"
#include "utils/common/synctime.h"
#include "common/common_module.h"

namespace ec2
{

QnDistributedMutex::QnDistributedMutex(): 
    QObject()
{
    connect(&timer, &QTimer::timeout, this, &QnDistributedMutex::lockTimeout);
    timer.setSingleShot(true);

    connect(qnTransactionBus, &QnTransactionMessageBus::peerFound,         this, &QnDistributedMutex::at_newPeerFound);
    connect(qnTransactionBus, &QnTransactionMessageBus::peerLost,          this, &QnDistributedMutex::at_peerLost);
    connect(qnTransactionBus, &QnTransactionMessageBus::gotLockRequest,    this, &QnDistributedMutex::at_gotLockRequest);
    connect(qnTransactionBus, &QnTransactionMessageBus::gotLockResponse,   this, &QnDistributedMutex::at_gotLockResponse);
    connect(qnTransactionBus, &QnTransactionMessageBus::gotUnlockRequest,  this, &QnDistributedMutex::at_gotUnlockRequest);
}

bool QnDistributedMutex::isAllPeersReady() const
{
    foreach(const QnId& peer, qnTransactionBus->alivePeers().keys())
    {
        if (!m_proccesedPeers.contains(peer))
            return false;
    }
    return true;
}

void QnDistributedMutex::sendTransaction(const LockRuntimeInfo& lockInfo)
{
    QnTransaction<ApiLockData> tran(ApiCommand::lockRequest, false);
    tran.params.name = m_name;
    tran.params.peer = lockInfo.peer;
    tran.params.timestamp = lockInfo.timestamp;
    qnTransactionBus->sendTransaction(tran);
}

void QnDistributedMutex::at_newPeerFound(QnId peer)
{
    QMutexLocker lock(&m_mutex);

    if (!m_selfLock.isEmpty())
        sendTransaction(m_selfLock);
}

void QnDistributedMutex::at_peerLost(QnId peer)
{
    QMutexLocker lock(&m_mutex);

    m_proccesedPeers.remove(peer);
    for (LockedMap::iterator itr = m_peerLockInfo.begin(); itr != m_peerLockInfo.end();)
    {
        if (itr.key().peer == peer)
            itr = m_peerLockInfo.erase(itr);
        else
            ++itr;
    }
    checkForLocked();
}

void QnDistributedMutex::lockAsync(const QByteArray& name, int timeoutMs)
{
    QMutexLocker lock(&m_mutex);

    m_name = name;
    m_selfLock = LockRuntimeInfo(qnCommon->moduleGUID(), qnSyncTime->currentMSecsSinceEpoch());
    sendTransaction(m_selfLock);
    timer.start(timeoutMs);
    m_peerLockInfo.insert(m_selfLock, 0);
    checkForLocked();
}

void QnDistributedMutex::unlock()
{
    QMutexLocker lock(&m_mutex);

    m_peerLockInfo.remove(m_selfLock);
    m_selfLock.clear();
    timer.stop();
}

void QnDistributedMutex::at_gotLockResponse(ApiLockData lockData)
{
    QMutexLocker lock(&m_mutex);
    m_peerLockInfo.insert(lockData, 0);
    m_proccesedPeers << lockData.peer;
    checkForLocked();
}

void QnDistributedMutex::at_gotLockRequest(ApiLockData lockData)
{
    QMutexLocker lock(&m_mutex);
    m_peerLockInfo.insert(lockData, 0);
    sendTransaction(m_peerLockInfo.begin().key());
}

void QnDistributedMutex::at_gotUnlockRequest(ApiLockData lockData)
{
    QMutexLocker lock(&m_mutex);

    LockedMap::iterator itr = m_peerLockInfo.find(lockData);
    if (itr != m_peerLockInfo.end()) {
        m_peerLockInfo.erase(itr);
        checkForLocked();
    }
}

void QnDistributedMutex::checkForLocked()
{
    if (!m_selfLock.isEmpty() && isAllPeersReady() && m_peerLockInfo.begin().key() == m_selfLock) {
        timer.stop();
        emit locked();
    }
}

}
