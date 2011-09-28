#include "directory_browser.h"
#include "file_device.h"
#include "device_plugins/archive/avi_files/avi_device.h"
#include "util.h"
#include "device_plugins/archive/filetypesupport.h"
#include "device/device_managmen/device_manager.h"

CLDeviceDirectoryBrowser::CLDeviceDirectoryBrowser():
    mNeedStop(false),
    m_resultMutex(QMutex::Recursive)
{
    start();
    QObject::moveToThread(this);

    connect(&m_fsWatcher, SIGNAL(directoryChanged(const QString&)), this, SLOT(directoryChanged(const QString&)));
    connect(this, SIGNAL(reload()), this, SLOT(reloadDirs()));
}

CLDeviceDirectoryBrowser::~CLDeviceDirectoryBrowser()
{
    mNeedStop = true;
    wait();
}

void CLDeviceDirectoryBrowser::setDirList(QStringList& dirs)
{
    {
        QMutexLocker _lock(&m_resultMutex);
        mDirsToCheck = dirs;
    }

    emit reload();
}

CLDeviceList CLDeviceDirectoryBrowser::result() const
{
    QMutexLocker _lock(&m_resultMutex);
    return mResult;
}

void CLDeviceDirectoryBrowser::reloadDirs()
{
    cl_log.log(QLatin1String("Browsing directories...."), cl_logALWAYS);

    QTime time;
    time.restart();

    mNeedStop = false;
    m_tmpResult.clear();

    foreach (const QString &dir, mDirsToCheck)
    {
        CLDeviceList dev_lst = findDevices(dir);

        cl_log.log(QLatin1String("found "), dev_lst.count(), QLatin1String(" devices"), cl_logALWAYS);

        foreach (CLDevice *dev, dev_lst)
            m_tmpResult[dev->getUniqueId()] = dev;
    }

    {
        QMutexLocker _lock(&m_resultMutex);
        mResult = m_tmpResult;
    }

    m_tmpResult.clear();
    emit dataReloaded();

    cl_log.log(QLatin1String("Done(Browsing directories). Time elapsed = "), time.elapsed(), cl_logALWAYS);
}

void CLDeviceDirectoryBrowser::run()
{
    QThread::currentThread()->setPriority(QThread::IdlePriority);

    reloadDirs();

    exec();
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

    m_fsWatcher.addPath(directory);

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
        //CLDevice* dev = new CLAviDevice(abs_file_name);
        CLDevice* dev = CLDeviceManager::instance().getArchiveDevice(abs_file_name);
        result[abs_file_name] = dev;
    }

    // dirs
    QFileInfoList sub_dirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    foreach (const QFileInfo &fi, sub_dirs)
    {
        foreach (CLDevice *dev, findDevices(fi.absoluteFilePath()))
            result[dev->getUniqueId()] = dev;
    }

    return result;
}

void CLDeviceDirectoryBrowser::directoryChanged(const QString & path)
{
    QStringList checkLst(path);
    CLDeviceManager::instance().pleaseCheckDirs(checkLst, true);
}
