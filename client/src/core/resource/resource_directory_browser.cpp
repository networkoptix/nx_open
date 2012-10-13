#include "resource_directory_browser.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QThread>

#include "core/resource_managment/resource_pool.h"
#include "plugins/resources/archive/avi_files/avi_dvd_resource.h"
#include "plugins/resources/archive/avi_files/avi_bluray_resource.h"
#include "plugins/resources/archive/filetypesupport.h"
#include "core/resource/layout_resource.h"
#include "plugins/storage/file_storage/layout_storage_resource.h"
#include "api/serializer/pb_serializer.h"
#include "ui/workbench/workbench_globals.h"

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

QnLayoutResourcePtr QnResourceDirectoryBrowser::layoutFromFile(const QString& xfile)
{
    QnLayoutResourcePtr layout;
    QnLayoutFileStorageResource layoutStorage;
    layoutStorage.setUrl(xfile);
    QIODevice* layoutFile = layoutStorage.open(QLatin1String("layout.pb"), QIODevice::ReadOnly);
    if (layoutFile == 0)
        return layout;
    QByteArray layoutData = layoutFile->readAll();
    delete layoutFile;
    QnApiPbSerializer serializer;
    try {
        serializer.deserializeLayout(layout, layoutData);
        if (layout == 0)
            return layout;
    } catch(...) {
        return layout;
    }

    QIODevice* uuidFile = layoutStorage.open(QLatin1String("uuid.bin"), QIODevice::ReadOnly);
    if (uuidFile)
    {
        QByteArray data = uuidFile->readAll();
        delete uuidFile;
        layout->setGuid(QUuid(data.data()));
        QnLayoutResourcePtr existingLayout = qnResPool->getResourceByGuid(layout->getGuid()).dynamicCast<QnLayoutResource>();
        if (existingLayout)
            return existingLayout;
    }
    else {
        layout->setGuid(QUuid::createUuid());
    }

    QIODevice* rangeFile = layoutStorage.open(QLatin1String("range.bin"), QIODevice::ReadOnly);
    if (rangeFile)
    {
        QByteArray data = rangeFile->readAll();
        delete rangeFile;
        layout->setLocalRange(QnTimePeriod().deserialize(data));
    }

    QIODevice* miscFile = layoutStorage.open(QLatin1String("misc.bin"), QIODevice::ReadOnly);
    if (miscFile)
    {
        QByteArray data = miscFile->readAll();
        delete miscFile;
        if (data.size() >= sizeof(quint32))
        {
            quint32 flags = *((quint32*) data.data());
            if (flags & 1) {
                Qn::Permissions permissions = Qn::RemovePermission | Qn::AddRemoveItemsPermission;
                layout->setData(Qn::LayoutPermissionsRole, (int) permissions);
            }
        }
        //layout->setLocalRange(QnTimePeriod().deserialize(data));
    }

    layout->setParentId(0);
    layout->setId(QnId::generateSpecialId());
    layout->setName(QFileInfo(xfile).fileName());

    layout->addFlags(QnResource::url);
    layout->setUrl(xfile);

    QnLayoutItemDataMap items = layout->getItems();
    QnLayoutItemDataMap updatedItems;

    QIODevice* itemNamesIO = layoutStorage.open(QLatin1String("item_names.txt"), QIODevice::ReadOnly);
    QTextStream itemNames(itemNamesIO);

    // todo: here is bad place to add resources to pool. need rafactor
    for(QnLayoutItemDataMap::iterator itr = items.begin(); itr != items.end(); ++itr)
    {
        QnLayoutItemData& item = itr.value();
        QString path = item.resource.path;
        item.uuid = QUuid::createUuid();
        item.resource.id = QnId::generateSpecialId();
        if (!path.endsWith(QLatin1String(".mkv")))
            item.resource.path += QLatin1String(".mkv");
        item.resource.path = QnLayoutResource::updateNovParent(xfile,item.resource.path);
        updatedItems.insert(item.uuid, item);

        QnStorageResourcePtr storage(new QnLayoutFileStorageResource());
        storage->setUrl(xfile);

        QnAviResourcePtr aviResource(new QnAviResource(item.resource.path));
        aviResource->setStorage(storage);
        aviResource->setId(item.resource.id);
        aviResource->removeFlags(QnResource::local); // do not display in tree root and disable 'open in container folder' menu item
        QString itemName(itemNames.readLine());
        if (!itemName.isEmpty())
            aviResource->setName(itemName);

        int numberOfChannels = aviResource->getVideoLayout()->numberOfChannels();
        for (int channel = 0; channel < numberOfChannels; ++channel)
        {
            QString normMotionName = path.mid(path.lastIndexOf(L'?')+1);
            QIODevice* motionIO = layoutStorage.open(QString(QLatin1String("motion%1_%2.bin")).arg(channel).arg(QFileInfo(normMotionName).baseName()), QIODevice::ReadOnly);
            if (motionIO) {
                Q_ASSERT(motionIO->size() % sizeof(QnMetaDataV1Light) == 0);
                QnMetaDataLightVector motionData;
                int motionDataSize = motionIO->size() / sizeof(QnMetaDataV1Light);
                if (motionDataSize > 0) {
                    motionData.resize(motionDataSize);
                    motionIO->read((char*) &motionData[0], motionIO->size());
                }
                delete motionIO;
                for (uint i = 0; i < motionData.size(); ++i)
                    motionData[i].doMarshalling();
                if (!motionData.empty())
                    aviResource->setMotionBuffer(motionData, channel);
            }
        }

        qnResPool->addResource(aviResource);
    }
    delete itemNamesIO;
    layout->setItems(updatedItems);
    //layout->addFlags(QnResource::local_media);
    layout->addFlags(QnResource::local);
    return layout;
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

    if (FileTypeSupport::isLayoutFileExt(xfile))
    {
        QnLayoutResourcePtr layout = layoutFromFile(xfile);
        return layout;
    }

    return QnResourcePtr(0);
}
