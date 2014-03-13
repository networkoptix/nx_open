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

    // todo: connect to transaction transport here
}

bool QnDistributedMutex::isAllPeersReady() const
{
    foreach(const QnId& peer, qnTransactionBus->alivePeers().keys())
    {
        if (!m_peerLockInfo.contains(peer))
            return false;
    }
    return true;
}

void QnDistributedMutex::sendTransaction()
{
    QnTransaction<ApiLockInfo> tran(ApiCommand::lockRequest, false);
    tran.params.timestamp = m_selfLock.timestamp;
    tran.params.name = m_name;
    qnTransactionBus->sendTransaction(tran);
}

void QnDistributedMutex::at_newPeerFound()
{
    QMutexLocker lock(&m_mutex);

    if (!m_selfLock.isEmpty())
        sendTransaction();
}

void QnDistributedMutex::lockAsync(const QByteArray& name, int timeoutMs)
{
    QMutexLocker lock(&m_mutex);

    m_name = name;
    m_selfLock = LockRuntimeInfo(qnCommon->moduleGUID(), qnSyncTime->currentMSecsSinceEpoch());
    sendTransaction();
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

void QnDistributedMutex::at_gotLockInfo(LockRuntimeInfo lockInfo)
{
    QMutexLocker lock(&m_mutex);

    m_peerLockInfo.insert(lockInfo, 0);
    checkForLocked();
}

void QnDistributedMutex::at_gotUnlockInfo(LockRuntimeInfo lockInfo)
{
    QMutexLocker lock(&m_mutex);

    LockedMap::iterator itr = m_peerLockInfo.find(lockInfo);
    if (itr != m_peerLockInfo.end()) {
        itr.value() = INT64_MAX;
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
