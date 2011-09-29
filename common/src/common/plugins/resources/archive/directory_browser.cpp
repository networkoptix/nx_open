#include "directory_browser.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QThread>

#include "resource/url_resource.h"
//#include "resources/archive/avi_files/avi_bluray_resource.h"
//#include "resources/archive/avi_files/avi_dvd_resource.h"
//#include "resources/archive/avi_files/avi_resource.h"
#include "resources/archive/filetypesupport.h"

QnResourceDirectoryBrowser::QnResourceDirectoryBrowser()
{
}

QnResourceDirectoryBrowser::~QnResourceDirectoryBrowser()
{
}

QString QnResourceDirectoryBrowser::manufacture() const
{
    return QLatin1String("DirectoryBrowser");
}

QnResourceDirectoryBrowser &QnResourceDirectoryBrowser::instance()
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

    foreach (const QString &dir, m_pathListToCheck)
    {
        QnResourceList dev_lst = findResources(dir);

        cl_log.log("found ", dev_lst.count(), " devices", cl_logALWAYS);

        result.append(dev_lst);

        if (shouldStop())
            return result;
    }

    cl_log.log("Done(Browsing directories). Time elapsed = ", time.elapsed(), cl_logALWAYS);

    QThread::currentThread()->setPriority(old_priority);

    return result;
}

QnResourcePtr QnResourceDirectoryBrowser::checkFile(const QString& filename)
{
    QnResource *res = 0;
    /*//todo
    FileTypeSupport fileTypeSupport;
    if (fileTypeSupport.isImageFileExt(filename))
        res = new QnURLResource(filename);
    else if (CLAviDvdDevice::isAcceptedUrl(filename))
        res = new CLAviDvdDevice(filename);
    else if (CLAviBluRayDevice::isAcceptedUrl(filename))
        res = new CLAviBluRayDevice(filename);
    else if (fileTypeSupport.isMovieFileExt(filename))
        res = new QnAviResource(filename);
    */
    return QnResourcePtr(res);
}

//=============================================================================================
QnResourceList QnResourceDirectoryBrowser::findResources(const QString& directory)
{
    cl_log.log("Checking ", directory, cl_logALWAYS);

    QnResourceList result;

    if (shouldStop())
        return result;

    QDir dir(directory);
    if (!dir.exists())
        return result;

    FileTypeSupport fileTypeSupport;
    /*//todo
    // images
    foreach (const QFileInfo &fi, dir.entryInfoList(fileTypeSupport.imagesFilter()))
        result.append(QnResourcePtr(new QnURLResource(fi.absoluteFilePath())));

    // movies
    foreach (const QFileInfo &fi, dir.entryInfoList(fileTypeSupport.moviesFilter()))
        result.append(QnResourcePtr(new QnAviResource(fi.absoluteFilePath())));
    */
    // dirs
    foreach (const QFileInfo &fi, dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks))
        result.append(findResources(fi.absoluteFilePath()));

    return result;
}
