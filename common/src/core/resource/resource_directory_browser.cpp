#include "resource_directory_browser.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QThread>

#include "core/resource/local_file_resource.h"
#include "core/resourcemanagment/resource_pool.h"
#include "plugins/resources/archive/avi_files/avi_dvd_device.h"
#include "plugins/resources/archive/avi_files/avi_bluray_device.h"
#include "plugins/resources/archive/filetypesupport.h"

namespace {
    class QnResourceDirectoryBrowserInstance: public QnResourceDirectoryBrowser {};

    Q_GLOBAL_STATIC(QnResourceDirectoryBrowserInstance, qnResourceDirectoryBrowserInstance);
}



QnResourceDirectoryBrowser::QnResourceDirectoryBrowser()
{
}

QnResourcePtr QnResourceDirectoryBrowser::createResource(QnId resourceTypeId, const QnResourceParameters &parameters)
{
    QnResourcePtr result;

    if (!isResourceTypeSupported(resourceTypeId))
    {
        return result;
    }

    if (parameters.contains("file"))
    {
        result = createArchiveResource(parameters["file"]);
        result->setTypeId(resourceTypeId);
        result->deserialize(parameters);
    }

    return result;
}

QString QnResourceDirectoryBrowser::manufacture() const
{
    return QLatin1String("DirectoryBrowser");
}

QnResourceDirectoryBrowser &QnResourceDirectoryBrowser::instance()
{
    return *qnResourceDirectoryBrowserInstance();
}

QnResourceList QnResourceDirectoryBrowser::findResources()
{

    setShouldBeUsed(false);

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

QnResourcePtr QnResourceDirectoryBrowser::checkFile(const QString &filename) const
{
    QFile file(filename);
    if (!file.exists())
        return QnResourcePtr(0);

    return createArchiveResource(filename);
}

//=============================================================================================
QnResourceList QnResourceDirectoryBrowser::findResources(const QString& directory)
{
    cl_log.log("Checking ", directory, cl_logALWAYS);

    QnResourceList result;

    if (shouldStop())
        return result;

    QDir dir(directory);
    const QList<QFileInfo> flist = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Files | QDir::NoSymLinks);
    foreach (const QFileInfo &fi, flist)
    {
        if (shouldStop())
            return result;

        QString absoluteFilePath = fi.absoluteFilePath();
        if (fi.isDir())
        {
            if (absoluteFilePath != directory)
                result.append(findResources(absoluteFilePath));
        }
        else
        {
            QnResourcePtr res = createArchiveResource(absoluteFilePath);
            if (res)
            {
                cl_log.log("created local resource: ", absoluteFilePath, cl_logALWAYS);
                result.append(res);
            }
        }
    }

    return result;
}

QnResourcePtr QnResourceDirectoryBrowser::createArchiveResource(const QString& xfile)
{
    //if (FileTypeSupport::isImageFileExt(xfile))
    //    return QnResourcePtr(new QnLocalFileResource(xfile));
    if (CLAviDvdDevice::isAcceptedUrl(xfile))
        return QnResourcePtr(new CLAviDvdDevice(xfile));
    if (CLAviBluRayDevice::isAcceptedUrl(xfile))
        return QnResourcePtr(new CLAviBluRayDevice(xfile));
    if (FileTypeSupport::isMovieFileExt(xfile))
        return QnResourcePtr(new QnAviResource(xfile));

    return QnResourcePtr(0);
}
