#include "resource_command_processor.h"
#include "resource.h"

QnResourceCommand::QnResourceCommand(QnResourcePtr res):
QnResourceConsumer(res)
{
    disconnectFromResource();
}

QnResourceCommand::~QnResourceCommand()
{

};

void QnResourceCommand::beforeDisconnectFromResource()
{
    disconnectFromResource();
}

/////////////////////


QnResourceCommandProcessor::QnResourceCommandProcessor():
    QnAbstractDataConsumer(1000)
{
	//start(); This is static singleton and run() uses log (another singleton). Jusst in case I will start it with first device created.
}

QnResourceCommandProcessor::~QnResourceCommandProcessor()
{
	stop();
}

void QnResourceCommandProcessor::putData(QnAbstractDataPacketPtr data)
{
    QnResourceCommandPtr command = data.staticCast<QnResourceCommand>();
    QnId id = command->getResource()->getId();

    QMutexLocker mutex(&m_cs);
    if (!mResourceQueue.contains(id))
    {
        mResourceQueue[id] = 1;
    }
    else
    {
        mResourceQueue[id]++;
    }

    QnAbstractDataConsumer::putData(data);

}


bool QnResourceCommandProcessor::processData(QnAbstractDataPacketPtr data)
{
	QnResourceCommandPtr command = data.staticCast<QnResourceCommand>();

    //if (command->isConnectedToTheResource())
    command->execute();

	QnId id = command->getResource()->getId();
	QMutexLocker mutex(&m_cs);
	Q_ASSERT(mResourceQueue.contains(id));
	Q_ASSERT(mResourceQueue[id]>0);

	mResourceQueue[id]--;
    return true;
}

bool QnResourceCommandProcessor::hasSuchResourceInQueue(QnResourcePtr res) const
{
    QnId id = res->getId();

	QMutexLocker mutex(&m_cs);
	if (!mResourceQueue.contains(id))
		return false;

	return (mResourceQueue[id]>0);
}


void QnResourceCommandProcessor::clearUnprocessedData()
{
	QnAbstractDataConsumer::clearUnprocessedData();
	QMutexLocker mutex(&m_cs);
	mResourceQueue.clear();
}

