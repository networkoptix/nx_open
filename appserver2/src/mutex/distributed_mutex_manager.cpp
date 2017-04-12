#include "distributed_mutex_manager.h"
#include "distributed_mutex.h"
#include "transaction/transaction_message_bus.h"

namespace ec2
{

QnDistributedMutexManager::QnDistributedMutexManager(QnTransactionMessageBus* messageBus):
    m_messageBus(messageBus)
{
    connect(m_messageBus, &QnTransactionMessageBus::gotLockRequest,    this, &QnDistributedMutexManager::at_gotLockRequest);
    connect(m_messageBus, &QnTransactionMessageBus::gotLockResponse,   this, &QnDistributedMutexManager::at_gotLockResponse);
    connect(m_messageBus, &QnTransactionMessageBus::peerFound,         this, &QnDistributedMutexManager::peerFound);
    connect(m_messageBus, &QnTransactionMessageBus::peerLost,          this, &QnDistributedMutexManager::peerLost);

    //connect(qnTransactionBus, &QnTransactionMessageBus::gotUnlockRequest,  this, &QnDistributedMutexManager::at_gotUnlockRequest, Qt::DirectConnection);
}

void QnDistributedMutexManager::setUserDataHandler(QnMutexUserDataHandler* userDataHandler)
{
    m_userDataHandler = userDataHandler;
}

QnDistributedMutex* QnDistributedMutexManager::createMutex(const QString& name)
{
    QnMutexLocker lock( &m_mutex );

    NX_ASSERT(!m_mutexList.value(name));

    QnDistributedMutex* netMutex = new QnDistributedMutex(this, name);
    m_mutexList.insert(name, netMutex);

    return netMutex;
}

void QnDistributedMutexManager::releaseMutex(const QString& name)
{
    QnMutexLocker lock( &m_mutex );
    m_mutexList.remove(name);
}

void QnDistributedMutexManager::at_gotLockRequest(ApiLockData lockData)
{
    QnMutexLocker lock( &m_mutex );

    m_timestamp = qMax(m_timestamp, lockData.timestamp);

    QnDistributedMutex* netMutex = m_mutexList.value(lockData.name);
    if (netMutex)
        netMutex->at_gotLockRequest(lockData);
    else {
        QnTransaction<ApiLockData> tran(
            ApiCommand::lockResponse,
            m_messageBus->commonModule()->moduleGUID());
        tran.params.name = lockData.name;
        tran.params.timestamp = lockData.timestamp;
        tran.params.peer = m_messageBus->commonModule()->moduleGUID();
        if (m_userDataHandler)
            tran.params.userData = m_userDataHandler->getUserData(lockData.name);
        m_messageBus->sendTransaction(tran, lockData.peer);
    }
}


void QnDistributedMutexManager::at_gotLockResponse(ApiLockData lockData)
{
    QnMutexLocker lock( &m_mutex );

    QnDistributedMutex* netMutex = m_mutexList.value(lockData.name);
    if (netMutex)
        netMutex->at_gotLockResponse(lockData);
}

/*
void QnDistributedMutexManager::at_gotUnlockRequest(ApiLockData lockData)
{
    QnMutexLocker lock( &m_mutex );

    QnDistributedMutexPtr netMutex = m_mutexList.value(lockData.name);
    if (netMutex)
        netMutex->at_gotUnlockRequest(lockData);
}
*/

qint64 QnDistributedMutexManager::newTimestamp()
{
    return ++m_timestamp;
}

QnTransactionMessageBus* QnDistributedMutexManager::messageBus() const
{
    return m_messageBus;
}

}
