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

QnResourceDirectoryBrowser& QnResourceDirectoryBrowser::instance()
{
    static QnResourceDirectoryBrowser inst;
    return inst;
}

QnResourceList QnResourceDirectoryBrowser::findResources()
{

    QThread::Priority old_priority = QThread::currentThread()->priority();
    QThread::currentThread()->setPriority(QThread::IdlePriority);

    cl_log.log("Browsing directories....", cl_logALWAYS);
    
    QTime time;
    time.restart();


    QnResourceList result;


    foreach(QString dir, m_pathListToCheck)
    {
        dir += "/";

        QnResourceList dev_lst = findResources(dir);

        cl_log.log("found ", dev_lst.count(), " devices",  cl_logALWAYS);

        result.append(dev_lst);
        
        if (shouldStop())
            return result;

    }

    cl_log.log("Done(Browsing directories). Time elapsed =  ", time.elapsed(), cl_logALWAYS);

    QThread::currentThread()->setPriority(old_priority);

    return result;    
}

QnResourcePtr QnResourceDirectoryBrowser::checkFile(const QString& filename)
{
    FileTypeSupport fileTypeSupport;


    if (fileTypeSupport.isImageFileExt(filename))
        return QnResourcePtr(new QnFileResource(filename));

    if (CLAviDvdDevice::isAcceptedUrl(filename))
        return QnResourcePtr( new CLAviDvdDevice(filename));
    
    if (CLAviBluRayDevice::isAcceptedUrl(filename))
        return QnResourcePtr( new CLAviBluRayDevice(filename) );

    if (fileTypeSupport.isMovieFileExt(filename))
        return QnResourcePtr( new QnAviResource(filename) );

    return QnResourcePtr(0);
}

//=============================================================================================
QnResourceList QnResourceDirectoryBrowser::findResources(const QString& directory)
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
            result.push_back( QnResourcePtr( new QnFileResource(abs_file_name) ) );
        }
    }

    {
        QStringList list = dir.entryList(fileTypeSupport.moviesFilter());

        for (int i = 0; i < list.size(); ++i)
        {
            QString file = list.at(i);
            QString abs_file_name = directory + file;
            result.push_back( QnResourcePtr (new QnAviResource(abs_file_name)) );
        }
    }

    //=============================================
    QStringList sub_dirs = subDirList(directory);

    foreach(QString str, sub_dirs)
    {
        QnResourceList sub_result = findResources(directory + str + QString("/"));
        result.append(sub_result);
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