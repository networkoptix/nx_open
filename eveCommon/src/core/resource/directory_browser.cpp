#include "directory_browser.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QThread>
#include "device_plugins/archive/filetypesupport.h"
#include "device_plugins/archive/avi_files/avi_device.h"
#include "device_plugins/archive/avi_files/avi_bluray_device.h"
#include "device_plugins/archive/avi_files/avi_dvd_device.h"
#include "device/file_resource.h"


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

    QStringList dirs = getPathCheckList();

    foreach (const QString &dir, dirs)
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
    QFile file(filename);
    if (!file.exists())
        return QnResourcePtr(0);

    return createArchiveResource(filename);
}

QnResourceList QnResourceDirectoryBrowser::checkFiles(const QStringList files)
{
    QnResourceList result;
    foreach(QString file, files)
    {
        QnResourcePtr res = checkFile(file);
        if (res)
            result.push_back(res);
    }

    return result;
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

    foreach (const QFileInfo &fi, dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks))
    {
        if (shouldStop())

            return result;

        QnResourcePtr res = createArchiveResource(fi.absoluteFilePath());

        if (res!=0)
            result.append(res);
    }

    return result;
}

QnResourcePtr QnResourceDirectoryBrowser::createArchiveResource(const QString& xfile)
{
    static FileTypeSupport m_fileTypeSupport;

    if (m_fileTypeSupport.isImageFileExt(xfile))
        return QnResourcePtr(new QnLocalFileResource(xfile));
    else if (CLAviDvdDevice::isAcceptedUrl(xfile))
        return QnResourcePtr(new CLAviDvdDevice(xfile));
    else if (CLAviBluRayDevice::isAcceptedUrl(xfile))
        return QnResourcePtr(new CLAviBluRayDevice(xfile));
    else if (m_fileTypeSupport.isMovieFileExt(xfile))
        return QnResourcePtr(new QnAviResource(xfile));
    else
        return QnResourcePtr();
}
