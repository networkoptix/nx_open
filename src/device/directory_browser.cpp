#include "directory_browser.h"
#include "file_device.h"
#include "device_plugins\archive\avi_files\avi_device.h"


CLDirectoryBrowserDeviceServer::CLDirectoryBrowserDeviceServer(const QString dir)
{
	m_directory = dir;
}

CLDirectoryBrowserDeviceServer::~CLDirectoryBrowserDeviceServer()
{

}


bool CLDirectoryBrowserDeviceServer::isProxy() const
{
	return false;
}

// return the name of the server 
QString CLDirectoryBrowserDeviceServer::name() const
{
	return m_directory + " browser";
}

// returns all available devices 
CLDeviceList CLDirectoryBrowserDeviceServer::findDevices()
{
	CLDeviceList result;
	QDir dir(m_directory);
	if (!dir.exists())
		return result;

	{
		QStringList filter;
		filter << "*.jpg" << "*.jpeg";
		QStringList list = dir.entryList(filter);

		for (int i = 0; i < list.size(); ++i)
		{
			QString file = list.at(i);
			QString abs_file_name = m_directory + QString("/") + file;
			CLDevice* dev = new CLFileDevice(abs_file_name);
			result[file] = dev;
		}

	}


	{
		QStringList filter;
		filter << "*.avi";
		filter << "*.mp4";
		filter << "*.mkv";
		filter << "*.wmv";
		filter << "*.mov";
		filter << "*.vob";
		filter << "*.m4v";

		QStringList list = dir.entryList(filter);

		for (int i = 0; i < list.size(); ++i)
		{
			QString file = list.at(i);
			QString abs_file_name = m_directory + QString("/") + file;
			CLDevice* dev = new CLAviDevice(abs_file_name);
			result[file] = dev;
		}

	}



	return result;

}
