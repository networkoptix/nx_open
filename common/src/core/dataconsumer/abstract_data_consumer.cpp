#include "abstract_data_consumer.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <utils/common/sleep.h>
#include <utils/common/log.h>


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

void QnAbstractDataConsumer::endOfRun()
{
    clearUnprocessedData();
}

void QnAbstractDataConsumer::run()
{
//    const int timeoutMs = 100;

    initSystemThreadId();

    while(!needToStop())
    {
        pauseDelay();

        QnAbstractDataPacketPtr data;
        bool get = m_dataQueue.pop(data, 200);

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
                QnSleep::msleep(1);
        }
        //NX_LOG("queue size = ", m_dataQueue.size(),cl_logALWAYS);
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

#endif // ENABLE_DATA_PROVIDERS
