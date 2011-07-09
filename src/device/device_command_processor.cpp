#include "device_command_processor.h"
#include "../device/device.h"

CLDeviceCommand::CLDeviceCommand(CLDevice* device):
m_device(device)
{
	m_device->addRef();
}

CLDeviceCommand::~CLDeviceCommand()
{
	m_device->releaseRef();
};

CLDevice* CLDeviceCommand::getDevice() const
{
	return m_device;
}

CLDeviceCommandProcessor::CLDeviceCommandProcessor():
CLAbstractDataProcessor(1000)
{
	//start(); This is static singleton and run() uses log (another singleton). Jusst in case I will start it with first device created.
}

CLDeviceCommandProcessor::~CLDeviceCommandProcessor()
{
	stop();
}

bool CLDeviceCommandProcessor::processData(CLAbstractData* data)
{
	CLDeviceCommand* command = static_cast<CLDeviceCommand*>(data);
	command->execute();

	CLDevice* dev = command->getDevice();
	QMutexLocker mutex(&m_cs);
	Q_ASSERT(mDevicesQue.contains(dev));
	Q_ASSERT(mDevicesQue[dev]>0);

	mDevicesQue[dev]--;
    return true;
}

bool CLDeviceCommandProcessor::hasSuchDeviceInQueue(CLDevice* dev) const
{
	QMutexLocker mutex(&m_cs);
	if (!mDevicesQue.contains(dev))
		return false;

	return (mDevicesQue[dev]>0);
}

void CLDeviceCommandProcessor::putData(CLAbstractData* data)
{
	CLDeviceCommand* command = static_cast<CLDeviceCommand*>(data);
	CLDevice* dev = command->getDevice();

	QMutexLocker mutex(&m_cs);
	if (!mDevicesQue.contains(dev))
	{
		mDevicesQue[dev] = 1;
	}
	else
	{
		mDevicesQue[dev]++;
	}

	CLAbstractDataProcessor::putData(data);

}

void CLDeviceCommandProcessor::clearUnprocessedData()
{
	CLAbstractDataProcessor::clearUnprocessedData();
	QMutexLocker mutex(&m_cs);
	mDevicesQue.clear();
}

