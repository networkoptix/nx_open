#include "queued_connection_with_counter.h"

namespace nx::utils {

QueuedConnectionWithCounter::~QueuedConnectionWithCounter()
{
    queuedDisconnectAll();
}

void QueuedConnectionWithCounter::queuedDisconnectAll()
{
    for (auto& connection: m_connections)
        connection.first->disconnect(connection.second);
    m_connections.clear();
}

void QueuedConnectionWithCounter::waitForPendingSlotsToBeFinished()
{
    QnMutexLocker lock(&m_pendingOpMutex);
    auto itr = m_expectants.insert(m_expectants.end(), m_pendingOpCounter);
    while (*itr > 0)
        m_pendingOpWaitCondition.wait(&m_pendingOpMutex);
    m_expectants.erase(itr);
}

void QueuedConnectionWithCounter::addPendingOp()
{
    QnMutexLocker lock(&m_pendingOpMutex);
    m_pendingOpCounter++;
}

void QueuedConnectionWithCounter::finishPendingOp()
{
    QnMutexLocker lock(&m_pendingOpMutex);
    --m_pendingOpCounter;
    for (auto& value: m_expectants)
        --value;
    m_pendingOpWaitCondition.wakeAll();
}

} // namespace nx::utils
