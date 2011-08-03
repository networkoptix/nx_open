#include "directory_browser.h"
#include "file_device.h"
#include "device_plugins/archive/avi_files/avi_device.h"
#include "util.h"
#include "device_plugins/archive/archive/archive_device.h"
#include "device_plugins/archive/filetypesupport.h"

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

    cl_log.log(QLatin1String("Browsing directories...."), cl_logALWAYS);

    QTime time;
    time.restart();

    mNeedStop = false;
    mResult.clear();

    foreach (const QString &dir, mDirsToCheck)
    {
        CLDeviceList dev_lst = findDevices(dir);

        cl_log.log(QLatin1String("found "), dev_lst.count(), QLatin1String(" devices"), cl_logALWAYS);

        foreach (CLDevice *dev, dev_lst)
            mResult[dev->getUniqueId()] = dev;
    }

    cl_log.log(QLatin1String("Done(Browsing directories). Time elapsed = "), time.elapsed(), cl_logALWAYS);

}

//=============================================================================================
CLDeviceList CLDeviceDirectoryBrowser::findDevices(const QString& directory)
{
    cl_log.log(QLatin1String("Checking "), directory, cl_logALWAYS);

    CLDeviceList result;

    if (mNeedStop)
        return result;

    QDir dir(directory);
    if (!dir.exists())
        return result;

    FileTypeSupport fileTypeSupport;

    // images
    foreach (const QFileInfo &fi, dir.entryInfoList(fileTypeSupport.imagesFilter()))
    {
        QString abs_file_name = fi.absoluteFilePath();
        CLDevice* dev = new CLFileDevice(abs_file_name);
        result[abs_file_name] = dev;
    }

    // movies
    foreach (const QFileInfo &fi, dir.entryInfoList(fileTypeSupport.moviesFilter()))
    {
        QString abs_file_name = fi.absoluteFilePath();
        CLDevice* dev = new CLAviDevice(abs_file_name);
        result[abs_file_name] = dev;
    }

    // dirs
    QFileInfoList sub_dirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    if (dir.absolutePath() + QLatin1Char('/') == getRecordingDir()
        // || dir.absolutePath() + QLatin1Char('/') == getTempRecordingDir() // ignore temp dir
       )
    {
        // if this is recorded clip dir
        foreach (const QFileInfo &fi, sub_dirs)
        {
            CLDevice *dev = new CLArchiveDevice(fi.absoluteFilePath());
            result[dev->getUniqueId()] = dev;
        }
    }
    else
    {
        foreach (const QFileInfo &fi, sub_dirs)
        {
            foreach (CLDevice *dev, findDevices(fi.absoluteFilePath()))
                result[dev->getUniqueId()] = dev;
        }
    }

    return result;
}
