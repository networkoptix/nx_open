#include "device_command_processor.h"
#include "../device/qnresource.h"

QnDeviceCommand::QnDeviceCommand(QnResourcePtr device):
m_device(device)
{
}

QnDeviceCommand::~QnDeviceCommand()
{
};

QnResourcePtr QnDeviceCommand::getDevice() const
{
	return m_device;
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

	QnResourcePtr dev = command->getDevice();
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
	QnResourcePtr dev = command->getDevice();

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

void CLDeviceCommandProcessor::clearUnprocessedData()
{
	QnAbstractDataConsumer::clearUnprocessedData();
	QMutexLocker mutex(&m_cs);
	mDevicesQue.clear();
}

