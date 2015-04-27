#include "distributed_mutex.h"

#include "transaction/transaction_message_bus.h"
#include "common/common_module.h"
#include "utils/common/synctime.h"
#include "utils/common/delete_later.h"
#include "distributed_mutex_manager.h"


namespace ec2
{

static const qint64 NO_MUTEX_LOCK = INT64_MAX;

// ----------------------------- QnDistributedMutex ----------------------------

QnDistributedMutex::QnDistributedMutex(QnDistributedMutexManager* owner, const QString& name):
    QObject(),
    m_name(name),
    m_timer(nullptr),
    m_locked(false),
    m_owner(owner)
{
    connect(m_owner, &QnDistributedMutexManager::peerFound,         this, &QnDistributedMutex::at_newPeerFound, Qt::DirectConnection);
    connect(m_owner, &QnDistributedMutexManager::peerLost,          this, &QnDistributedMutex::at_peerLost, Qt::DirectConnection);
    m_timer = new QTimer();
    connect(m_timer, &QTimer::timeout, this, &QnDistributedMutex::at_timeout);
    m_timer->setSingleShot(true);
}

QnDistributedMutex::~QnDistributedMutex()
{
    unlock();
}

bool QnDistributedMutex::isAllPeersReady() const
{
    for(const QnUuid& peer: qnTransactionBus->aliveServerPeers().keys())
    {
        if (!m_proccesedPeers.contains(peer))
            return false;
    }
    return true;
}

void QnDistributedMutex::sendTransaction(const LockRuntimeInfo& lockInfo, ApiCommand::Value command, const QnUuid& dstPeer)
{
    QnTransaction<ApiLockData> tran(command);
    tran.params.name = m_name;
    tran.params.peer = lockInfo.peer;
    tran.params.timestamp = lockInfo.timestamp;
    if (m_owner->m_userDataHandler)
        tran.params.userData = m_owner->m_userDataHandler->getUserData(lockInfo.name);
    qnTransactionBus->sendTransaction(tran, dstPeer);
}

void QnDistributedMutex::at_newPeerFound(ec2::ApiPeerAliveData data)
{
    if (data.peer.peerType != Qn::PT_Server)
        return;

    QMutexLocker lock(&m_mutex);
    Q_ASSERT(data.peer.id != qnCommon->moduleGUID());
    if (!m_selfLock.isEmpty())
        sendTransaction(m_selfLock, ApiCommand::lockRequest, data.peer.id);
}

void QnDistributedMutex::at_peerLost(ec2::ApiPeerAliveData data)
{
    if (data.peer.peerType != Qn::PT_Server)
        return;

    QMutexLocker lock(&m_mutex);

    m_proccesedPeers.remove(data.peer.id);
    for (LockedMap::iterator itr = m_peerLockInfo.begin(); itr != m_peerLockInfo.end();)
    {
        if (itr.key().peer == data.peer.id)
            itr = m_peerLockInfo.erase(itr);
        else
            ++itr;
    }
    checkForLocked();
}

void QnDistributedMutex::at_timeout()
{
    QSet<QnUuid> failedPeers;
    {
        QMutexLocker lock(&m_mutex);
        if (m_locked)
            return;
        failedPeers = QSet<QnUuid>::fromList(qnTransactionBus->alivePeers().keys()) - m_proccesedPeers;
    }
    unlock();
    emit lockTimeout(failedPeers);
}

void QnDistributedMutex::lockAsync(int timeoutMs)
{
    QMutexLocker lock(&m_mutex);
    m_selfLock = LockRuntimeInfo(qnCommon->moduleGUID(), m_owner->newTimestamp(), m_name);
    if (m_owner->m_userDataHandler)
        m_selfLock.userData = m_owner->m_userDataHandler->getUserData(m_name);
    sendTransaction(m_selfLock, ApiCommand::lockRequest, QnUuid()); // send broadcast
    m_timer->start(timeoutMs);
    m_peerLockInfo.insert(m_selfLock, 0);
    checkForLocked();
}

void QnDistributedMutex::unlock()
{
    m_owner->releaseMutex(m_name);
    QMutexLocker lock(&m_mutex);
    unlockInternal();
}

void QnDistributedMutex::unlockInternal()
{
    if (m_timer) {
        m_timer->deleteLater();
        m_timer = 0;
    }

    /*
    ApiLockData data;
    data.name = m_name;
    data.peer = qnCommon->moduleGUID();
    data.timestamp = NO_MUTEX_LOCK;
    */

    for(ApiLockData lockData: m_delayedResponse) {
        QnUuid srcPeer = lockData.peer;
        lockData.peer = qnCommon->moduleGUID();
        sendTransaction(lockData, ApiCommand::lockResponse, srcPeer);
    }
    m_delayedResponse.clear();

    if (!m_selfLock.isEmpty()) {
        //sendTransaction(m_selfLock, ApiCommand::unlockRequest);
        m_selfLock.clear();
    }
    m_locked = false;
    m_proccesedPeers.clear();
    m_peerLockInfo.clear();
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

    if (!m_locked && LockRuntimeInfo(lockData) < m_selfLock)
    {
        m_peerLockInfo.insert(lockData, 0);
        sendTransaction(m_selfLock, ApiCommand::lockResponse, lockData.peer);
    }
    else {
        m_delayedResponse << lockData;
    }
}

/*
void QnDistributedMutex::at_gotUnlockRequest(ApiLockData lockData)
{
    QMutexLocker lock(&m_mutex);

    LockedMap::iterator itr = m_peerLockInfo.find(lockData);
    if (itr != m_peerLockInfo.end()) {
        m_peerLockInfo.erase(itr);
        checkForLocked();
    }
}
*/

void QnDistributedMutex::checkForLocked()
{
    if (!m_selfLock.isEmpty() && isAllPeersReady()) 
    {
        if (m_timer) {
            m_timer->deleteLater();
            m_timer = 0;
        }
        if (!m_locked) {
            m_locked = true;
            emit locked();
        }
    }
}

bool QnDistributedMutex::checkUserData() const
{
    QMutexLocker lock(&m_mutex);
    if (!m_owner->m_userDataHandler)
        return true;

    for(const LockRuntimeInfo& info: m_peerLockInfo.keys())
    {
        if (!m_owner->m_userDataHandler->checkUserData(info.name, info.userData))
            return false;
    }
    return true;
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
