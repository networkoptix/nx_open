#include "distributed_mutex_manager.h"
#include "distributed_mutex.h"
#include "transaction/transaction_message_bus.h"

namespace ec2
{

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
    connect(qnTransactionBus, &QnTransactionMessageBus::gotLockRequest,    this, &QnDistributedMutexManager::at_gotLockRequest);
    connect(qnTransactionBus, &QnTransactionMessageBus::gotLockResponse,   this, &QnDistributedMutexManager::at_gotLockResponse);
    connect(qnTransactionBus, &QnTransactionMessageBus::peerFound,         this, &QnDistributedMutexManager::peerFound);
    connect(qnTransactionBus, &QnTransactionMessageBus::peerLost,          this, &QnDistributedMutexManager::peerLost);

    //connect(qnTransactionBus, &QnTransactionMessageBus::gotUnlockRequest,  this, &QnDistributedMutexManager::at_gotUnlockRequest, Qt::DirectConnection);
}

void QnDistributedMutexManager::setUserDataHandler(QnMutexUserDataHandler* userDataHandler)
{
    m_userDataHandler = userDataHandler;
}

QnDistributedMutex* QnDistributedMutexManager::createMutex(const QString& name)
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);

    Q_ASSERT(!m_mutexList.value(name));

    QnDistributedMutex* netMutex = new QnDistributedMutex(this, name);
    m_mutexList.insert(name, netMutex);

    return netMutex;
}

void QnDistributedMutexManager::releaseMutex(const QString& name)
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);
    m_mutexList.remove(name);
}

void QnDistributedMutexManager::at_gotLockRequest(ApiLockData lockData)
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);

    m_timestamp = qMax(m_timestamp, lockData.timestamp);

    QnDistributedMutex* netMutex = m_mutexList.value(lockData.name);
    if (netMutex)
        netMutex->at_gotLockRequest(lockData);
    else {
        QnTransaction<ApiLockData> tran(ApiCommand::lockResponse);
        tran.params.name = lockData.name;
        tran.params.timestamp = lockData.timestamp;
        tran.params.peer = qnCommon->moduleGUID();
        if (m_userDataHandler)
            tran.params.userData = m_userDataHandler->getUserData(lockData.name);
        qnTransactionBus->sendTransaction(tran, lockData.peer);
    }
}


void QnDistributedMutexManager::at_gotLockResponse(ApiLockData lockData)
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);

    QnDistributedMutex* netMutex = m_mutexList.value(lockData.name);
    if (netMutex)
        netMutex->at_gotLockResponse(lockData);
}

/*
void QnDistributedMutexManager::at_gotUnlockRequest(ApiLockData lockData)
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);

    QnDistributedMutexPtr netMutex = m_mutexList.value(lockData.name);
    if (netMutex)
        netMutex->at_gotUnlockRequest(lockData);
}
*/

qint64 QnDistributedMutexManager::newTimestamp()
{
    return ++m_timestamp;
}

}
