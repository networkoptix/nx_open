#include "dataprocessor.h"
#include "../base/sleep.h"
#include "../base/log.h"

CLAbstractDataProcessor::CLAbstractDataProcessor(int maxQueueSize)
    : m_dataQueue(maxQueueSize)
{
}

bool CLAbstractDataProcessor::canAcceptData() const
{
	return (m_dataQueue.size() < m_dataQueue.maxSize());
}

void CLAbstractDataProcessor::putData(CLAbstractData* data)
{
	data->addRef();
	m_dataQueue.push(data);
}

void CLAbstractDataProcessor::clearUnprocessedData()
{
	m_dataQueue.clear();
}

void CLAbstractDataProcessor::endOfRun()
{
}

void CLAbstractDataProcessor::run()
{
	CL_LOG(cl_logINFO) cl_log.log("data processor started.", cl_logINFO);

//	const int timeoutMs = 100;
	while(!needToStop())
	{
		pauseDelay();

		CLAbstractData* data;
		bool get = m_dataQueue.pop(data, 200);

		if (!get)
		{
			CL_LOG(cl_logDEBUG2) cl_log.log("queue is empty ", (int)(long)(&m_dataQueue),cl_logDEBUG2);
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

		data->releaseRef();
	}

	endOfRun();

	CL_LOG(cl_logINFO) cl_log.log("data processor stopped.", cl_logINFO);
}

int CLAbstractDataProcessor::queueSize() const
{
	return m_dataQueue.size();
}
