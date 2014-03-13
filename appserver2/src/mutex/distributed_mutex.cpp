#include "distributed_mutex.h"

#include "transaction/transaction_message_bus.h"
#include "utils/common/synctime.h"
#include "common/common_module.h"

namespace ec2
{

static const qint64 NO_MUTEX_LOCK = INT64_MAX;

QnDistributedMutexManager::QnDistributedMutexManager()
{
    connect(qnTransactionBus, &QnTransactionMessageBus::gotLockRequest,    this, &QnDistributedMutexManager::at_gotLockRequest, Qt::DirectConnection);
    connect(qnTransactionBus, &QnTransactionMessageBus::gotLockResponse,   this, &QnDistributedMutexManager::at_gotLockResponse, Qt::DirectConnection);
    connect(qnTransactionBus, &QnTransactionMessageBus::gotUnlockRequest,  this, &QnDistributedMutexManager::at_gotUnlockRequest, Qt::DirectConnection);
}

QnDistributedMutexPtr QnDistributedMutexManager::getLock(const QByteArray& name, int timeoutMs)
{
    Q_ASSERT(!m_mutexList.value(name));

    QnDistributedMutexPtr netMutex(new QnDistributedMutex(this));
    m_mutexList.insert(name, netMutex);

    connect(this, &QnDistributedMutexManager::locked,      netMutex.data(), &QnDistributedMutex::locked, Qt::DirectConnection);
    connect(this, &QnDistributedMutexManager::lockTimeout, netMutex.data(), &QnDistributedMutex::lockTimeout, Qt::DirectConnection);

    netMutex->lockAsync(name, timeoutMs);

    return netMutex;
}

void QnDistributedMutexManager::releaseMutex(const QByteArray& name)
{
    m_mutexList.remove(name);
}

void QnDistributedMutexManager::at_gotLockRequest(ApiLockData lockData)
{
    QMutexLocker lock(&m_mutex);

    QnDistributedMutexPtr netMutex = m_mutexList.value(lockData.name);
    if (netMutex)
        netMutex->at_gotLockRequest(lockData);
    else {
        QnTransaction<ApiLockData> tran(ApiCommand::lockResponse, false);
        tran.params.name = lockData.name;
        tran.params.peer = qnCommon->moduleGUID();
        tran.params.timestamp = NO_MUTEX_LOCK;
        qnTransactionBus->sendTransaction(tran);
    }
}


void QnDistributedMutexManager::at_gotLockResponse(ApiLockData lockData)
{
    QMutexLocker lock(&m_mutex);

    QnDistributedMutexPtr netMutex = m_mutexList.value(lockData.name);
    if (netMutex)
        netMutex->at_gotLockResponse(lockData);
}

void QnDistributedMutexManager::at_gotUnlockRequest(ApiLockData lockData)
{
    QMutexLocker lock(&m_mutex);

    QnDistributedMutexPtr netMutex = m_mutexList.value(lockData.name);
    if (netMutex)
        netMutex->at_gotUnlockRequest(lockData);
}

// ----------------------------- QnDistributedMutex ----------------------------

QnDistributedMutex::QnDistributedMutex(QnDistributedMutexManager* owner): 
    QObject(),
    m_owner(owner),
    m_locked(false)
{
    connect(qnTransactionBus, &QnTransactionMessageBus::peerFound,         this, &QnDistributedMutex::at_newPeerFound, Qt::DirectConnection);
    connect(qnTransactionBus, &QnTransactionMessageBus::peerLost,          this, &QnDistributedMutex::at_peerLost, Qt::DirectConnection);

    connect(&timer, &QTimer::timeout, this, &QnDistributedMutex::at_timeout);
    timer.setSingleShot(true);
}

QnDistributedMutex::~QnDistributedMutex()
{
    unlock();
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

void QnDistributedMutex::sendTransaction(const LockRuntimeInfo& lockInfo, ApiCommand::Value command)
{
    QnTransaction<ApiLockData> tran(command, false);
    tran.params.name = m_name;
    tran.params.peer = lockInfo.peer;
    tran.params.timestamp = lockInfo.timestamp;
    qnTransactionBus->sendTransaction(tran);
}

void QnDistributedMutex::at_newPeerFound(QnId peer)
{
    QMutexLocker lock(&m_mutex);

    if (!m_selfLock.isEmpty())
        sendTransaction(m_selfLock, ApiCommand::lockRequest);
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

void QnDistributedMutex::at_timeout()
{
    unlock();
    emit lockTimeout(m_name);
}

void QnDistributedMutex::lockAsync(const QByteArray& name, int timeoutMs)
{
    //QMutexLocker lock(&m_mutex);

    m_name = name;
    m_selfLock = LockRuntimeInfo(qnCommon->moduleGUID(), qnSyncTime->currentMSecsSinceEpoch());
    if (!m_peerLockInfo.isEmpty())
        m_selfLock.timestamp = qMax(m_selfLock.timestamp, m_peerLockInfo.lastKey().timestamp+1);
    sendTransaction(m_selfLock, ApiCommand::lockRequest);
    timer.start(timeoutMs);
    m_peerLockInfo.insert(m_selfLock, 0);
    checkForLocked();
}

void QnDistributedMutex::unlock()
{
    QMutexLocker lock(&m_mutex);

    if (m_locked) {
        foreach(const ApiLockData& lockData, m_delayedResponse)
        {
            m_peerLockInfo.insert(lockData, 0);
            sendTransaction(m_peerLockInfo.begin().key(), ApiCommand::lockResponse);
        }
        m_delayedResponse.clear();
    }

    if (!m_selfLock.isEmpty()) {
        sendTransaction(m_selfLock, ApiCommand::unlockRequest);
        m_peerLockInfo.remove(m_selfLock);
        m_selfLock.clear();
    }
    m_locked = false;
    timer.stop();
    m_proccesedPeers.clear();
    m_peerLockInfo.clear();
    m_owner->releaseMutex(m_name);
}

void QnDistributedMutex::at_gotLockResponse(ApiLockData lockData)
{
    QMutexLocker lock(&m_mutex);
    if (lockData.timestamp != NO_MUTEX_LOCK)
        m_peerLockInfo.insert(lockData, 0);
    m_proccesedPeers << lockData.peer;
    checkForLocked();
}

void QnDistributedMutex::at_gotLockRequest(ApiLockData lockData)
{
    QMutexLocker lock(&m_mutex);

    if (m_locked && LockRuntimeInfo(lockData) < m_peerLockInfo.firstKey())
    {
        m_delayedResponse << lockData; // it is possible if new peer just appeared. block originator just do not sending response
    }
    else {
        m_peerLockInfo.insert(lockData, 0);
        sendTransaction(m_selfLock, ApiCommand::lockResponse);
    }
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
        m_locked = true;
        emit locked(m_name);
    }
}

bool QnDistributedMutex::isLocking() const
{
    QMutexLocker lock(&m_mutex);
    return !m_peerLockInfo.isEmpty();
}

bool QnDistributedMutex::isLocked() const
{
    QMutexLocker lock(&m_mutex);
    return m_locked;
}

}
