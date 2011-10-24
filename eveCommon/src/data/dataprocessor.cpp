#include "dataprocessor.h"
#include "../base/sleep.h"
#include "../base/log.h"

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
	CL_LOG(cl_logINFO) cl_log.log(QLatin1String("data processor started."), cl_logINFO);

//	const int timeoutMs = 100;
	while(!needToStop())
	{
		pauseDelay();

		QnAbstractDataPacketPtr data;
		bool get = m_dataQueue.pop(data, 200);

		if (!get)
		{
			CL_LOG(cl_logDEBUG2) cl_log.log(QLatin1String("queue is empty "), (int)(long)(&m_dataQueue),cl_logDEBUG2);
			CLSleep::msleep(10);
			continue;
		}
		while(!needToStop())
		{
			if (processData(data))
				break;
			else
				CLSleep::msleep(1);
		}
		//cl_log.log("queue size = ", m_dataQueue.size(),cl_logALWAYS);
	}

	endOfRun();

	CL_LOG(cl_logINFO) cl_log.log(QLatin1String("data processor stopped."), cl_logINFO);
}

int QnAbstractDataConsumer::queueSize() const
{
	return m_dataQueue.size();
}
