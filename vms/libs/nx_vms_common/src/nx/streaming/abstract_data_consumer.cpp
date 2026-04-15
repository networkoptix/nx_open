// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_data_consumer.h"

#include <utils/common/sleep.h>
#include <nx/utils/log/log.h>

#if defined(Q_OS_APPLE)
    #include <utils/apple_utils.h>
#endif

namespace {
    static constexpr std::chrono::milliseconds kWaitTimeoutMs(200);
} // namespace


QnAbstractDataConsumer::QnAbstractDataConsumer(int maxQueueSize)
    : m_dataQueue(maxQueueSize)
{
}

bool QnAbstractDataConsumer::canAcceptData() const
{
    return (m_dataQueue.size() < m_dataQueue.maxSize());
}

void QnAbstractDataConsumer::putData( const QnAbstractDataPacketPtr& data )
{
    if (!needToStop())
        m_dataQueue.push(data);
}

void QnAbstractDataConsumer::clearUnprocessedData()
{
    m_dataQueue.clear();
}

void QnAbstractDataConsumer::beforeRun()
{
}

void QnAbstractDataConsumer::endOfRun()
{
    clearUnprocessedData();
}

void QnAbstractDataConsumer::pleaseStop()
{
    NX_MUTEX_LOCKER lock(&m_pleaseStopMutex);
    QnLongRunnable::pleaseStop();
    m_dataQueue.setTerminated(true);
}

void QnAbstractDataConsumer::resumeDataQueue()
{
    NX_MUTEX_LOCKER lock(&m_pleaseStopMutex);
    if (!needToStop())
        m_dataQueue.setTerminated(false);
}

void QnAbstractDataConsumer::run()
{
    initSystemThreadId();
    beforeRun();
    resumeDataQueue();

    while(!needToStop())
    {
        pauseDelay();
        #if defined(Q_OS_APPLE)
            drainAutoreleasePoolAndCall([this]() { runCycle(); });
        #else
            runCycle();
        #endif
    }

    endOfRun();
}

void QnAbstractDataConsumer::runCycle()
{
    QnAbstractDataPacketPtr data;
    bool get = m_dataQueue.pop(data, kWaitTimeoutMs);

    if (!get)
    {
        QnSleep::msleep(kNoDataDelayIntervalMs);
        return;
    }
    while(!needToStop())
    {
        if (processData(data))
            break;
        else
            QnSleep::msleep(kNoDataDelayIntervalMs);
    }
}

int QnAbstractDataConsumer::queueSize() const
{
    return m_dataQueue.size();
}

int QnAbstractDataConsumer::maxQueueSize() const
{
    return m_dataQueue.maxSize();
}
