#include "fake_recorder_device.h"
#include "base\rand.h"
#include <QTextStream>
#include "ui\ui_common.h"


CLFakeRecorderDevice::CLFakeRecorderDevice()
{

}

CLFakeRecorderDevice::~CLFakeRecorderDevice()
{

}

QString CLFakeRecorderDevice::toString() const
{

	int cpu_usage = cl_get_random_val(20, 25);
	int cpu_temp = cl_get_random_val(50, 53);

	QString result;
	QTextStream(&result) << getFullName() << ":\r\n  " <<  UIDisplayName(getUniqueId())<< "\r\n(127.0.0.1)\r\n" << "CPU USAGE: " \
		<< cpu_usage << "%\r\n CPU temperature: " << cpu_temp << "C \r\nOverAll Status: OK";
	return result;
}


QString CLFakeRecorderDevice::getName() const
{
	return "Network Optix Archiver";
}