#include "directory_browser.h"
#include "resource/file_resource.h"
#include "resources/archive/avi_files/avi_resource.h"
#include "resources/archive/filetypesupport.h"
#include "resources/archive/avi_files/avi_dvd_resource.h"
#include "resources/archive/avi_files/avi_bluray_resource.h"



QnResourceDirectoryBrowser::QnResourceDirectoryBrowser()
{

}

QnResourceDirectoryBrowser::~QnResourceDirectoryBrowser()
{
}


QString QnResourceDirectoryBrowser::name() const
{
    return "DirectoryBrowser";
}

QnResourceList QnResourceDirectoryBrowser::findDevices()
{

    QThread::Priority old_priority = QThread::currentThread()->priority();
    QThread::currentThread()->setPriority(QThread::IdlePriority);

    cl_log.log("Browsing directories....", cl_logALWAYS);
    
    QTime time;
    time.restart();


    QnResourceList result;


    foreach(QString dir, m_PathListToCheck)
    {
        dir += "/";

        QnResourceList dev_lst = findDevices(dir);

        cl_log.log("found ", dev_lst.count(), " devices",  cl_logALWAYS);

        foreach(QnResource* dev, dev_lst)
        {
            result[dev->getUniqueId()] = dev;
        }

        if (shouldStop())
            return result;


    }

    cl_log.log("Done(Browsing directories). Time elapsed =  ", time.elapsed(), cl_logALWAYS);

    QThread::currentThread()->setPriority(old_priority);

    return result;    
}

QnResource* QnResourceDirectoryBrowser::checkFile(const QString& filename)
{
    FileTypeSupport fileTypeSupport;


    if (fileTypeSupport.isImageFileExt(filename))
        return new CLFileDevice(filename);

    if (CLAviDvdDevice::isAcceptedUrl(filename))
        return new CLAviDvdDevice(filename);
    
    if (CLAviBluRayDevice::isAcceptedUrl(filename))
        return new CLAviBluRayDevice(filename);

    if (fileTypeSupport.isMovieFileExt(filename))
        return new CLAviDevice(filename);

    return 0;
}

//=============================================================================================
QnResourceList QnResourceDirectoryBrowser::findDevices(const QString& directory)
{

    cl_log.log("Checking ", directory,   cl_logALWAYS);

    FileTypeSupport fileTypeSupport;

    QnResourceList result;

    if (shouldStop())
        return result;

    QDir dir(directory);
    if (!dir.exists())
        return result;

    {
        QStringList list = dir.entryList(fileTypeSupport.imagesFilter());

        for (int i = 0; i < list.size(); ++i)
        {
            QString file = list.at(i);
            QString abs_file_name = directory + file;
            QnResource* dev = new CLFileDevice(abs_file_name);
            result[abs_file_name] = dev;
        }
    }

    {
        QStringList list = dir.entryList(fileTypeSupport.moviesFilter());

        for (int i = 0; i < list.size(); ++i)
        {
            QString file = list.at(i);
            QString abs_file_name = directory + file;
            QnResource* dev = new CLAviDevice(abs_file_name);
            result[abs_file_name] = dev;
        }
    }

    //=============================================
    QStringList sub_dirs = subDirList(directory);

    foreach(QString str, sub_dirs)
    {
        QnResourceList sub_result = findDevices(directory + str + QString("/"));

        foreach(QnResource* dev, sub_result)
        {
            result[dev->getUniqueId()] = dev;
        }
    }


    return result;
}

//=============================================================================================
QStringList QnResourceDirectoryBrowser::subDirList(const QString& abspath)
{
    QStringList result;

    QDir dir(abspath);
    if (!dir.exists())
        return result;

    QFileInfoList list = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);

    foreach(QFileInfo info, list)
    {
        result.push_back(info.fileName());
    }

    return result;
}