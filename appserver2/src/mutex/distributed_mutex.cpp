#include "distributed_mutex.h"

#include "transaction/transaction_message_bus.h"
#include "utils/common/synctime.h"
#include "common/common_module.h"

namespace ec2
{

static const qint64 NO_MUTEX_LOCK = INT64_MAX;
static QnDistributedMutexManager* m_staticInstance = 0;

QnDistributedMutexManager* QnDistributedMutexManager::instance()
{
    return m_staticInstance;
}

void QnDistributedMutexManager::initStaticInstance(QnDistributedMutexManager* value)
{
    if (m_staticInstance)
        delete m_staticInstance;
    m_staticInstance = value;
}

QnDistributedMutexManager::QnDistributedMutexManager():
    m_timestamp(1),
    m_userDataHandler(0)
{
    connect(qnTransactionBus, &QnTransactionMessageBus::gotLockRequest,    this, &QnDistributedMutexManager::at_gotLockRequest, Qt::DirectConnection);
    connect(qnTransactionBus, &QnTransactionMessageBus::gotLockResponse,   this, &QnDistributedMutexManager::at_gotLockResponse, Qt::DirectConnection);
    //connect(qnTransactionBus, &QnTransactionMessageBus::gotUnlockRequest,  this, &QnDistributedMutexManager::at_gotUnlockRequest, Qt::DirectConnection);
}

void QnDistributedMutexManager::setUserDataHandler(QnMutexUserDataHandler* userDataHandler)
{
    m_userDataHandler = userDataHandler;
}

QnDistributedMutexPtr QnDistributedMutexManager::getLock(const QString& name, int timeoutMs)
{
    QMutexLocker lock(&m_mutex);

    Q_ASSERT(!m_mutexList.value(name));


    QnDistributedMutexPtr netMutex(new QnDistributedMutex(this));
    m_mutexList.insert(name, netMutex);

    connect(netMutex.data(), &QnDistributedMutex::locked,      this, &QnDistributedMutexManager::locked, Qt::DirectConnection);
    connect(netMutex.data(), &QnDistributedMutex::lockTimeout, this, &QnDistributedMutexManager::lockTimeout, Qt::DirectConnection);

    netMutex->lockAsync(name, timeoutMs);

    return netMutex;
}

void QnDistributedMutexManager::releaseMutex(const QString& name)
{
    m_mutexList.remove(name);
}

void QnDistributedMutexManager::at_gotLockRequest(ApiLockData lockData)
{
    QMutexLocker lock(&m_mutex);

    m_timestamp = qMax(m_timestamp, lockData.timestamp);

    QnDistributedMutexPtr netMutex = m_mutexList.value(lockData.name);
    if (netMutex)
        netMutex->at_gotLockRequest(lockData);
    else {
        QnTransaction<ApiLockData> tran(ApiCommand::lockResponse, false);
        tran.params.name = lockData.name;
        tran.params.timestamp = lockData.timestamp;
        tran.params.peer = qnCommon->moduleGUID();
        if (m_userDataHandler)
            tran.params.userData = m_userDataHandler->getUserData(lockData.name);
        tran.fillSequence();
        qnTransactionBus->sendTransaction(tran, lockData.peer);
    }
}


void QnDistributedMutexManager::at_gotLockResponse(ApiLockData lockData)
{
    QMutexLocker lock(&m_mutex);

    QnDistributedMutexPtr netMutex = m_mutexList.value(lockData.name);
    if (netMutex)
        netMutex->at_gotLockResponse(lockData);
}

/*
void QnDistributedMutexManager::at_gotUnlockRequest(ApiLockData lockData)
{
    QMutexLocker lock(&m_mutex);

    QnDistributedMutexPtr netMutex = m_mutexList.value(lockData.name);
    if (netMutex)
        netMutex->at_gotUnlockRequest(lockData);
}
*/

qint64 QnDistributedMutexManager::newTimestamp()
{
    return ++m_timestamp;
}

// ----------------------------- QnDistributedMutex ----------------------------

QnDistributedMutex::QnDistributedMutex(QnDistributedMutexManager* owner): 
    QObject(),
    m_locked(false),
    m_owner(owner)
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
    foreach(const QnId& peer, qnTransactionBus->aliveServerPeers().keys())
    {
        if (!m_proccesedPeers.contains(peer))
            return false;
    }
    return true;
}

void QnDistributedMutex::sendTransaction(const LockRuntimeInfo& lockInfo, ApiCommand::Value command, const QnId& dstPeer)
{
    QnTransaction<ApiLockData> tran(command, false);
    tran.params.name = m_name;
    tran.params.peer = lockInfo.peer;
    tran.params.timestamp = lockInfo.timestamp;
    if (m_owner->m_userDataHandler)
        tran.params.userData = m_owner->m_userDataHandler->getUserData(lockInfo.name);
    tran.fillSequence();
    qnTransactionBus->sendTransaction(tran, dstPeer);
}

void QnDistributedMutex::at_newPeerFound(ec2::ApiServerAliveData data)
{
    if (data.isClient)
        return;

    QMutexLocker lock(&m_mutex);
    Q_ASSERT(data.serverId != qnCommon->moduleGUID());
    if (!m_selfLock.isEmpty())
        sendTransaction(m_selfLock, ApiCommand::lockRequest, data.serverId);
}

void QnDistributedMutex::at_peerLost(ec2::ApiServerAliveData data)
{
    if (data.isClient)
        return;

    QMutexLocker lock(&m_mutex);

    m_proccesedPeers.remove(data.serverId);
    for (LockedMap::iterator itr = m_peerLockInfo.begin(); itr != m_peerLockInfo.end();)
    {
        if (itr.key().peer == data.serverId)
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

void QnDistributedMutex::lockAsync(const QString& name, int timeoutMs)
{
    QMutexLocker lock(&m_mutex);
    
    m_name = name;
    m_selfLock = LockRuntimeInfo(qnCommon->moduleGUID(), m_owner->newTimestamp(), name);
    if (m_owner->m_userDataHandler)
        m_selfLock.userData = m_owner->m_userDataHandler->getUserData(name);
    sendTransaction(m_selfLock, ApiCommand::lockRequest, QnId()); // send broadcast
    timer.start(timeoutMs);
    m_peerLockInfo.insert(m_selfLock, 0);
    checkForLocked();
}

void QnDistributedMutex::unlock()
{
    QMutexLocker lock(&m_mutex);

    /*
    ApiLockData data;
    data.name = m_name;
    data.peer = qnCommon->moduleGUID();
    data.timestamp = NO_MUTEX_LOCK;
    */

    foreach(ApiLockData lockData, m_delayedResponse) {
        QnId srcPeer = lockData.peer;
        lockData.peer = qnCommon->moduleGUID();
        sendTransaction(lockData, ApiCommand::lockResponse, srcPeer);
    }
    m_delayedResponse.clear();

    if (!m_selfLock.isEmpty()) {
        //sendTransaction(m_selfLock, ApiCommand::unlockRequest);
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
    if (!m_selfLock.isEmpty() && isAllPeersReady()) {
        timer.stop();
        if (!m_locked) {
            m_locked = true;
            emit locked(m_name);
        }
    }
}

bool QnDistributedMutex::checkUserData() const
{
    QMutexLocker lock(&m_mutex);
    if (!m_owner->m_userDataHandler)
        return true;

    foreach(const LockRuntimeInfo& info, m_peerLockInfo.keys())
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
