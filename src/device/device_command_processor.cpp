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


CLDeviceCommandProcessor::CLDeviceCommandProcessor():
CLAbstractDataProcessor(1000)
{
	//start(); This is static singleton and run() uses log (another singleton). Jusst in case I will start it with first device created.
}

CLDeviceCommandProcessor::~CLDeviceCommandProcessor()
{
	stop();
}


void CLDeviceCommandProcessor::processData(CLAbstractData* data)
{
	CLDeviceCommand* command = static_cast<CLDeviceCommand*>(data);
	command->execute();
}