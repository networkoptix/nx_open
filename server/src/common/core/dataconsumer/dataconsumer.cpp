#include "common/log.h"
#include "dataconsumer.h"
#include "common/sleep.h"

QnAbstractDataConsumer::QnAbstractDataConsumer(int maxQueueSize)
    : m_dataQueue(maxQueueSize)
{
}

bool QnAbstractDataConsumer::canAcceptData() const
{
	return (m_dataQueue.size() < m_dataQueue.maxSize());
}

void QnAbstractDataConsumer::putData(QnAbstractDataPacketPtr data)
{
	m_dataQueue.push(data);
}

void QnAbstractDataConsumer::clearUnprocessedData()
{
	m_dataQueue.clear();
}

void QnAbstractDataConsumer::endOfRun()
{
}

void QnAbstractDataConsumer::run()
{
	CL_LOG(cl_logINFO) cl_log.log("data processor started.", cl_logINFO);

	const int timeoutMs = 100;
	while(!needToStop())
	{
		pauseIfNeeded();

		QnAbstractDataPacketPtr data;
		bool get = m_dataQueue.pop(data, 200);

		if (!get)
		{
			CL_LOG(cl_logDEBUG2) cl_log.log("queue is empty ", (int)(long)(&m_dataQueue),cl_logDEBUG2);
			QnSleep::msleep(10);
			continue;
		}

		processData(data);

		//cl_log.log("queue size = ", m_dataQueue.size(),cl_logALWAYS);
	}

	endOfRun();

	CL_LOG(cl_logINFO) cl_log.log("data processor stopped.", cl_logINFO);
}

int QnAbstractDataConsumer::queueSize() const
{
	return m_dataQueue.size();
}
