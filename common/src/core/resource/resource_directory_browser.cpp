#include "resource_directory_browser.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QThread>

#include "core/resourcemanagment/resource_pool.h"
#include "plugins/resources/archive/avi_files/avi_dvd_resource.h"
#include "plugins/resources/archive/avi_files/avi_bluray_resource.h"
#include "plugins/resources/archive/filetypesupport.h"
#include "layout_resource.h"
#include "plugins/storage/file_storage/layout_storage_resource.h"
#include "api/serializer/pb_serializer.h"

namespace {
    class QnResourceDirectoryBrowserInstance: public QnResourceDirectoryBrowser {};

   // defined but never used
   // Q_GLOBAL_STATIC(QnResourceDirectoryBrowserInstance, qnResourceDirectoryBrowserInstance);
}



QnResourceDirectoryBrowser::QnResourceDirectoryBrowser()
{
    m_resourceReady = false;
}

QnResourcePtr QnResourceDirectoryBrowser::createResource(QnId resourceTypeId, const QnResourceParameters &parameters)
{
    QnResourcePtr result;

    if (!isResourceTypeSupported(resourceTypeId))
    {
        return result;
    }

    if (parameters.contains(QLatin1String("file")))
    {
        result = createArchiveResource(parameters[QLatin1String("file")]);
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
    // TODO: this causes heap corruption, investigate
    //return *qnResourceDirectoryBrowserInstance();
    static QnResourceDirectoryBrowser inst;
    return inst;
}

void QnResourceDirectoryBrowser::cleanup()
{
    m_resourceReady = false;
}

QnResourceList QnResourceDirectoryBrowser::findResources()
{
    if (m_resourceReady) // TODO: if path check list is changed, this check will prevent us from re-updating the resource list.
    {
        return QnResourceList();
    }

    setShouldBeUsed(false);

    QThread::Priority old_priority = QThread::currentThread()->priority();
    QThread::currentThread()->setPriority(QThread::IdlePriority);

    qDebug() << "Browsing directories....";

    QTime time;
    time.restart();

    QnResourceList result;

    QStringList dirs = getPathCheckList();

    foreach (const QString &dir, dirs)
    {
        QnResourceList dev_lst = findResources(dir);

        qDebug() << "found " << dev_lst.count() << " devices";

        result.append(dev_lst);

        if (shouldStop())
            return result;
    }

    qDebug() << "Done(Browsing directories). Time elapsed = " << time.elapsed();

    QThread::currentThread()->setPriority(old_priority);

    m_resourceReady = true;

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
    qDebug() << "Checking " << directory;

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

    if (QnAviDvdResource::isAcceptedUrl(xfile))
        return QnResourcePtr(new QnAviDvdResource(xfile));
    if (QnAviBlurayResource::isAcceptedUrl(xfile))
        return QnResourcePtr(new QnAviBlurayResource(xfile));
    if (FileTypeSupport::isMovieFileExt(xfile))
        return QnResourcePtr(new QnAviResource(xfile));

    if (FileTypeSupport::isImageFileExt(xfile)) 
    {
        QnResourcePtr rez = QnResourcePtr(new QnAviResource(xfile));
        rez->addFlags(QnResource::still_image);
        rez->removeFlags(QnResource::video | QnResource::audio);
        return rez;
    }

    /*if (FileTypeSupport::isLayoutFileExt(xfile))
    {
        QnLayoutResourcePtr layout = QnLayoutResource::fromFile(xfile);
        return layout;
    }*/ // TODO

    return QnResourcePtr(0);
}
