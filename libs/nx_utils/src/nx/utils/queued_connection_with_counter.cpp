// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "queued_connection_with_counter.h"

#include <nx/utils/log/log.h>

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
    NX_MUTEX_LOCKER lock(&m_pendingOpMutex);
    auto itr = m_expectants.insert(m_expectants.end(), m_pendingOpCounter);
    NX_VERBOSE(this, "waitForPendingSlotsToBeFinished: Pending counter: %1", m_pendingOpCounter);
    while (*itr > 0)
    {
        NX_VERBOSE(
            this, "waitForPendingSlotsToBeFinished: Woken up. Pending counter: %1, expectant "
            "counter %2", m_pendingOpCounter, *itr);

        m_pendingOpWaitCondition.wait(&m_pendingOpMutex);
    }

    NX_VERBOSE(
        this, "waitForPendingSlotsToBeFinished: Wait is over up. Pending counter: %1",
        m_pendingOpCounter);

    m_expectants.erase(itr);
}

void QueuedConnectionWithCounter::addPendingOp()
{
    NX_MUTEX_LOCKER lock(&m_pendingOpMutex);
    m_pendingOpCounter++;
    NX_VERBOSE(this, "addPendingOp: Counter: %1", m_pendingOpCounter);
}

void QueuedConnectionWithCounter::finishPendingOp()
{
    NX_MUTEX_LOCKER lock(&m_pendingOpMutex);
    --m_pendingOpCounter;

    NX_VERBOSE(this, "finishPendingOp: Counter: %1", m_pendingOpCounter);
    for (auto& value: m_expectants)
        --value;

    m_pendingOpWaitCondition.wakeAll();
}

} // namespace nx::utils
