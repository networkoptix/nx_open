#include "directory_browser.h"
#include "file_device.h"
#include "device_plugins/archive/avi_files/avi_device.h"
#include "util.h"
#include "device_plugins/archive/archive/archive_device.h"

CLDeviceDirectoryBrowser::CLDeviceDirectoryBrowser():
mNeedStop(false)
{

}

CLDeviceDirectoryBrowser::~CLDeviceDirectoryBrowser()
{
    mNeedStop = true;
    wait();
}

void CLDeviceDirectoryBrowser::setDirList(QStringList& dirs)
{
    mDirsToCheck = dirs;
}

CLDeviceList CLDeviceDirectoryBrowser::result()
{
    return mResult;
}

void CLDeviceDirectoryBrowser::run()
{

    QThread::currentThread()->setPriority(QThread::IdlePriority);

    cl_log.log("Browsing directories....", cl_logALWAYS);
    
    QTime time;
    time.restart();




    mNeedStop = false;
    mResult.clear();


    foreach(QString dir, mDirsToCheck)
    {

        QChar c = dir.at(dir.length()-1);

        if (c!='/' && c!='\\')
            dir+="/";


        cl_log.log("Checking ", dir,   cl_logALWAYS);

        CLDeviceList dev_lst = findDevices(dir);

        cl_log.log("found ", dev_lst.count(), " devices",  cl_logALWAYS);

        foreach(CLDevice* dev, dev_lst)
        {
            mResult[dev->getUniqueId()] = dev;
        }

    }

    cl_log.log("Done(Browsing directories). Time elapsed =  ", time.elapsed(), cl_logALWAYS);
    
}

//=============================================================================================
CLDeviceList CLDeviceDirectoryBrowser::findDevices(const QString& directory)
{
    CLDeviceList result;



    if (mNeedStop)
        return result;

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
        filter << "*.m2ts";
        

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
QStringList CLDeviceDirectoryBrowser::subDirList(const QString& abspath)
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