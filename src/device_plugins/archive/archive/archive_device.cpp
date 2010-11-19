#include "archive_device.h"


CLArchiveDevice::CLArchiveDevice(const QString arch_path)
{
	m_uniqueid = arch_path;
	m_name = arch_path;
}

CLArchiveDevice::~CLArchiveDevice()
{

}

void CLArchiveDevice::readdescrfile()
{

}
