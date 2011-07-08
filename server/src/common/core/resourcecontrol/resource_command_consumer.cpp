#include "resource_command_consumer.h"
#include "resource/resource.h"



QnResourceCommand::QnResourceCommand(QnResource* device):
m_resource(device)
{

}

QnResourceCommand::~QnResourceCommand()
{

};

QnResource* QnResourceCommand::getDevice() const
{
	return m_resource;
}

QnResourceCommandProcessor::QnResourceCommandProcessor():
QnAbstractDataConsumer(1000)
{
	//start(); This is static singleton and run() uses log (another singleton). Jusst in case I will start it with first device created.
}

QnResourceCommandProcessor::~QnResourceCommandProcessor()
{
	stop();
}

void QnResourceCommandProcessor::processData(QnAbstractDataPacketPtr data)
{
	QnResourceCommandPtr command = data.staticCast<QnResourceCommand>();
	command->execute();

	QnResource* dev = command->getDevice();
	QMutexLocker mutex(&m_cs);
	Q_ASSERT(mDevicesQue.contains(dev));
	Q_ASSERT(mDevicesQue[dev]>0);

	mDevicesQue[dev]--;
}

bool QnResourceCommandProcessor::hasSuchDeviceInQueue(QnResource* dev) const
{
	QMutexLocker mutex(&m_cs);
	if (!mDevicesQue.contains(dev))
		return false;

	return (mDevicesQue[dev]>0);
}

void QnResourceCommandProcessor::putData(QnAbstractDataPacketPtr data)
{
	QnResourceCommandPtr command = data.staticCast<QnResourceCommand>();
	QnResource* dev = command->getDevice();

	QMutexLocker mutex(&m_cs);
	if (!mDevicesQue.contains(dev))
	{
		mDevicesQue[dev] = 1;
	}
	else
	{
		mDevicesQue[dev]++;
	}

	QnAbstractDataConsumer::putData(data);

}

void QnResourceCommandProcessor::clearUnprocessedData()
{
	QnAbstractDataConsumer::clearUnprocessedData();
	QMutexLocker mutex(&m_cs);
	mDevicesQue.clear();
}

