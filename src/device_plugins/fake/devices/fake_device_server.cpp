#include "fake_device_server.h"
#include "../../../base/log.h"
#include "fake_device.h"
#include <QStringList>

FakeDeviceServer ::FakeDeviceServer ()
{

}

bool FakeDeviceServer ::isProxy() const
{
	return false; // this not actualy a server; this is just a cameras
}

QString FakeDeviceServer::name() const
{
	return "Fake device server";
}

// returns all available devices 
CLDeviceList FakeDeviceServer ::findDevices()
{
	return FakeDevice::findDevices();
}

FakeDeviceServer & FakeDeviceServer::instance()
{
	static FakeDeviceServer  inst;
	return inst;
}