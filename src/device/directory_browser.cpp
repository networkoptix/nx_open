#include "directory_browser.h"
#include "file_device.h"
#include "device_plugins/archive/avi_files/avi_device.h"
#include "util.h"
#include "device_plugins/archive/archive/archive_device.h"

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
	return "CLDirectoryBrowserDeviceServer";
}

// returns all available devices 
CLDeviceList CLDirectoryBrowserDeviceServer::findDevices()
{
    return findDevices(m_directory);

}
//=============================================================================================
CLDeviceList CLDirectoryBrowserDeviceServer::findDevices(const QString& directory)
{
    CLDeviceList result;
    QDir dir(directory);
    if (!dir.exists())
        return result;

    {
        QStringList filter;
        filter << "*.jpg" << "*.jpeg";
        QStringList list = dir.entryList(filter);

        for (int i = 0; i < list.size(); ++i)
        {
            QString file = list.at(i);
            QString abs_file_name = directory + file;
            CLDevice* dev = new CLFileDevice(abs_file_name);
            result[abs_file_name] = dev;
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
        filter << "*.3gp";
        filter << "*.mpeg";
        filter << "*.mpg";
        filter << "*.mpe";

        /*
        filter << "*.";
        filter << "*.";
        filter << "*.";
        filter << "*.";
        /**/
        


        QStringList list = dir.entryList(filter);

        for (int i = 0; i < list.size(); ++i)
        {
            QString file = list.at(i);
            QString abs_file_name = directory + file;
            CLDevice* dev = new CLAviDevice(abs_file_name);
            result[abs_file_name] = dev;
        }

    }



    //=============================================
    QStringList sub_dirs = subDirList(directory);
    //if (directory==getTempRecordingDir() || directory==getRecordingDir())
    if (directory==getRecordingDir()) // ignore temp dir
    {
        // if this is recorded clip dir 
        foreach(QString str, sub_dirs)
        {
            CLDevice* dev = new CLArchiveDevice(directory + str + QString("/"));
            result[dev->getUniqueId()] = dev;
        }

        
    }
    else
    {
        foreach(QString str, sub_dirs)
        {
            CLDeviceList sub_result = findDevices(directory + str + QString("/"));

            foreach(CLDevice* dev, sub_result)
            {
                result[dev->getUniqueId()] = dev;
            }
        }

    }

    return result;

}
//=============================================================================================
QStringList CLDirectoryBrowserDeviceServer::subDirList(const QString& abspath)
{
    QStringList result;

    QDir dir(abspath);
    if (!dir.exists())
        return result;

    QFileInfoList list = dir.entryInfoList();

    foreach(QFileInfo info, list)
    {
        if (info.isDir() && info.fileName()!="." && info.fileName()!="..")
            result.push_back(info.fileName());
    }

    return result;
}