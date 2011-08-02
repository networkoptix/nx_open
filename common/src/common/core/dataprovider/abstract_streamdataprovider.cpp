#include "resource/resource.h"
#include "abstract_streamdataprovider.h"
#include "common/sleep.h"
#include "dataconsumer/dataconsumer.h"


QnAbstractStreamDataProvider::QnAbstractStreamDataProvider(QnResourcePtr res):
QnResourceConsumer(res),
m_dataRate(999), // very big value, like max fps 
m_mtx(QMutex::Recursive)

{

}

QnAbstractStreamDataProvider::~QnAbstractStreamDataProvider()
{
	stop();
}

bool QnAbstractStreamDataProvider::dataCanBeAccepted() const
{
    // need to read only if all queues has more space and at least one queue is exist
    bool result = false;
    QMutexLocker mtx(&m_mtx);
    foreach(QnAbstractDataConsumer* dp, m_dataprocessors)
    {
        if (dp->canAcceptData())
            result = true;
        else 
            return false;
    }

    return result;
}


void QnAbstractStreamDataProvider::addDataProcessor(QnAbstractDataConsumer* dp)
{
	QMutexLocker mtx(&m_mtx);

	if (!m_dataprocessors.contains(dp))
		m_dataprocessors.push_back(dp);
}

void QnAbstractStreamDataProvider::removeDataProcessor(QnAbstractDataConsumer* dp)
{
	QMutexLocker mtx(&m_mtx);
	m_dataprocessors.removeOne(dp);
}


void QnAbstractStreamDataProvider::pauseDataProcessors()
{
    QMutexLocker mtx(&m_mtx);
    foreach(QnAbstractDataConsumer* dataProcessor, m_dataprocessors) 
    {
        dataProcessor->pause();
    }
}

void QnAbstractStreamDataProvider::resumeDataProcessors()
{
    QMutexLocker mtx(&m_mtx);
    foreach(QnAbstractDataConsumer* dataProcessor, m_dataprocessors) 
    {
        dataProcessor->resume();
    }
}


void QnAbstractStreamDataProvider::putData(QnAbstractDataPacketPtr data)
{
    if (!data)
        return;

	QMutexLocker mtx(&m_mtx);
    foreach(QnAbstractDataConsumer* dataProcessor, m_dataprocessors) 
	{
		dataProcessor->putData(data);
	}
}

void QnAbstractStreamDataProvider::beforeDisconnectFromResource()
{
    pleaseStop();
}

void QnAbstractStreamDataProvider::disconnectFromResource()
{
    stop();
    QnResourceConsumer::disconnectFromResource();
}

void QnAbstractStreamDataProvider::run()
{
    beforeRun();

    while(!needToStop())
    {
        pauseIfNeeded(); // pause if needed;

        if (needToStop()) // extra check after pause
            break;

        sleepIfNeeded();

        if (needToStop()) // extra check after pause
            break;


        if (!dataCanBeAccepted())
        {
            QnSleep::msleep(5);
            continue;
        }

        if (QnResource::commandProcHasSuchResourceInQueue(getResource())) // if command processor has something in the queue for this device let it go first
        {
            QnSleep::msleep(5);
            continue;
        }


        if (!beforeGetData())
            continue;

        QnAbstractDataPacketPtr data (getNextData());

        if (!afterGetData(data))
            continue;

        if (data)
            putData(data); // data can be accepted anyway

    }

    afterRun();
}
