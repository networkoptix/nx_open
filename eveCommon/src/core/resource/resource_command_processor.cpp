#include "resource_command_processor.h"
#include "resource.h"

QnDeviceCommand::QnDeviceCommand(QnResourcePtr device):
m_resource(device)
{
}

QnDeviceCommand::~QnDeviceCommand()
{
};

QnResourcePtr QnDeviceCommand::getResource() const
{
	return m_resource;
}

CLDeviceCommandProcessor::CLDeviceCommandProcessor():
QnAbstractDataConsumer(1000)
{
	//start(); This is static singleton and run() uses log (another singleton). Jusst in case I will start it with first device created.
}

CLDeviceCommandProcessor::~CLDeviceCommandProcessor()
{
	stop();
}

bool CLDeviceCommandProcessor::processData(QnAbstractDataPacketPtr data)
{
	QnDeviceCommandPtr command = qSharedPointerDynamicCast<QnDeviceCommand>(data);
	command->execute();

	QnResourcePtr dev = command->getResource();
	QMutexLocker mutex(&m_cs);
	Q_ASSERT(mDevicesQue.contains(dev));
	Q_ASSERT(mDevicesQue[dev]>0);

	mDevicesQue[dev]--;
    return true;
}

bool CLDeviceCommandProcessor::hasSuchDeviceInQueue(const QString& uniqId) const
{
	QMutexLocker mutex(&m_cs);
    foreach(QnResourcePtr res, mDevicesQue.keys()) 
    {
        if (res->getUniqueId() == uniqId) {
            return mDevicesQue[res] > 0;
        }
    }
    return false;
}

void CLDeviceCommandProcessor::putData(QnAbstractDataPacketPtr data)
{
	QnDeviceCommandPtr command = qSharedPointerDynamicCast<QnDeviceCommand>(data);
	QnResourcePtr res = command->getResource();

	QMutexLocker mutex(&m_cs);
	if (!mDevicesQue.contains(res))
	{
		mDevicesQue[res] = 1;
	}
	else
	{
		mDevicesQue[res]++;
	}

	QnAbstractDataConsumer::putData(data);

}

void CLDeviceCommandProcessor::clearUnprocessedData()
{
	QnAbstractDataConsumer::clearUnprocessedData();
	QMutexLocker mutex(&m_cs);
	mDevicesQue.clear();
}

