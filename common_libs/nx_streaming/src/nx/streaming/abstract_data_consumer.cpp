#include "abstract_data_consumer.h"

#include <utils/common/sleep.h>
#include <nx/utils/log/log.h>

namespace {
    static const int kWaitTimeoutMs = 200;
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
    QnMutexLocker lock(&m_pleaseStopMutex);
    QnLongRunnable::pleaseStop();
    m_dataQueue.setTerminated(true);
}

void QnAbstractDataConsumer::resumeDataQueue()
{
    QnMutexLocker lock(&m_pleaseStopMutex);
    if (!needToStop())
        m_dataQueue.setTerminated(false);
}

void QnAbstractDataConsumer::run()
{
//    const int timeoutMs = 100;

    initSystemThreadId();
    beforeRun();
    resumeDataQueue();

    while(!needToStop())
    {
        pauseDelay();

        QnAbstractDataPacketPtr data;
        bool get = m_dataQueue.pop(data, kWaitTimeoutMs);

        if (!get)
        {
            //NX_LOG( lit("QnAbstractDataConsumer::run. queue is empty %1").arg((int)(long)(&m_dataQueue)), cl_logDEBUG2 );
            QnSleep::msleep(10);
            continue;
        }
        while(!needToStop())
        {
            if (processData(data))
                break;
            else
                QnSleep::msleep(10);
        }
    }

    endOfRun();
}

int QnAbstractDataConsumer::queueSize() const
{
    return m_dataQueue.size();
}

int QnAbstractDataConsumer::maxQueueSize() const
{
    return m_dataQueue.maxSize();
}
