#include "resource_directory_browser.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QThread>

#include "core/resource_management/resource_pool.h"
#include "plugins/resources/archive/avi_files/avi_dvd_resource.h"
#include "plugins/resources/archive/avi_files/avi_bluray_resource.h"
#include "plugins/resources/archive/filetypesupport.h"
#include "core/resource/layout_resource.h"
#include "plugins/storage/file_storage/layout_storage_resource.h"
#include "client/client_globals.h"

#include <utils/common/warnings.h>
#include <utils/local_file_cache.h>
#include "nx_ec/data/api_layout_data.h"
#include "utils/serialization/binary_functions.h"

namespace {
    const int maxResourceCount = 1024;
}

QnResourceDirectoryBrowser::QnResourceDirectoryBrowser() {
    m_resourceReady = false;
}

QnResourcePtr QnResourceDirectoryBrowser::createResource(QnId resourceTypeId, const QnResourceParams& params) {
    QnResourcePtr result;

    if (!isResourceTypeSupported(resourceTypeId)) {
        return result;
    }

    result = createArchiveResource(params.url);
    result->setTypeId(resourceTypeId);

    return result;
}

QString QnResourceDirectoryBrowser::manufacture() const {
    return QLatin1String("DirectoryBrowser");
}

QnResourceDirectoryBrowser &QnResourceDirectoryBrowser::instance() {
    static QnResourceDirectoryBrowser inst;
    return inst;
}

void QnResourceDirectoryBrowser::cleanup() {
    m_resourceReady = false;
}

QnResourceList QnResourceDirectoryBrowser::findResources()
{
    if (m_resourceReady)
        return QnResourceList();

    setShouldBeUsed(false);

    QThread::Priority oldPriority = QThread::currentThread()->priority();
    QThread::currentThread()->setPriority(QThread::IdlePriority);

    NX_LOG(lit("Browsing directories..."), cl_logDEBUG1);

    QTime time;
    time.restart();

    QnResourceList result;

    QStringList dirs = getPathCheckList();

    foreach (const QString &dir, dirs) {
        findResources(dir, &result);
        if (shouldStop())
            break;
    }

    NX_LOG(lit("Done(Browsing directories). Time elapsed = %1.").arg(time.elapsed()), cl_logDEBUG1);

    QThread::currentThread()->setPriority(oldPriority);

    m_resourceReady = true;

    while(result.size() > maxResourceCount)
        result.pop_back();

    return result;
}

QnResourcePtr QnResourceDirectoryBrowser::checkFile(const QString &filename) const {
    QFile file(filename);
    if (!file.exists())
        return QnResourcePtr(0);

    return createArchiveResource(filename);
}

//=============================================================================================
void QnResourceDirectoryBrowser::findResources(const QString& directory, QnResourceList *result) {
    NX_LOG(lit("Checking %1").arg(directory), cl_logDEBUG1);

    if (shouldStop())
        return;

    const QList<QFileInfo> files = QDir(directory).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Files | QDir::NoSymLinks);
    foreach (const QFileInfo &file, files) {
        if (shouldStop())
            return;

        QString absoluteFilePath = file.absoluteFilePath();
        if (file.isDir()) {
            if (absoluteFilePath != directory)
                findResources(absoluteFilePath, result);
        } else {
            QnResourcePtr res = createArchiveResource(absoluteFilePath);
            if (res)
                result->append(res);
        }
    }
}

QnLayoutResourcePtr QnResourceDirectoryBrowser::layoutFromFile(const QString& xfile) {
    QnLayoutFileStorageResource layoutStorage;
    layoutStorage.setUrl(xfile);
    QIODevice* layoutFile = layoutStorage.open(QLatin1String("layout.pb"), QIODevice::ReadOnly);
    if (layoutFile == 0)
        return QnLayoutResourcePtr();
    QByteArray layoutData = layoutFile->readAll();
    delete layoutFile;
    
    QnLayoutResourcePtr layout(new QnLayoutResource());
    ec2::ApiLayoutData apiLayout;
    QnInputBinaryStream<QByteArray> stream(layoutData);
    if (QnBinary::deserialize(&stream, &apiLayout))
        fromApiToResource(apiLayout, layout);
    else
        return QnLayoutResourcePtr();
    QnLayoutItemDataList orderedItems;
    foreach(const ec2::ApiLayoutItemData& item, apiLayout.items) {
        orderedItems << QnLayoutItemData();
        fromApiToResource(item, orderedItems.last());
    }

    QIODevice *uuidFile = layoutStorage.open(QLatin1String("uuid.bin"), QIODevice::ReadOnly);
    if (uuidFile) {
        QByteArray data = uuidFile->readAll();
        delete uuidFile;
        layout->setId(QUuid(data.data()));
        QnLayoutResourcePtr existingLayout = qnResPool->getResourceById(layout->getId()).dynamicCast<QnLayoutResource>();
        if (existingLayout)
            return existingLayout;
    } else {
        layout->setId(QUuid::createUuid());
    }

    QIODevice* rangeFile = layoutStorage.open(QLatin1String("range.bin"), QIODevice::ReadOnly);
    if (rangeFile) {
        QByteArray data = rangeFile->readAll();
        delete rangeFile;
        layout->setLocalRange(QnTimePeriod().deserialize(data));
    }

    QIODevice* miscFile = layoutStorage.open(QLatin1String("misc.bin"), QIODevice::ReadOnly);
    bool layoutWithCameras = false;
    if (miscFile) {
        QByteArray data = miscFile->readAll();
        delete miscFile;
        if (data.size() >= (int)sizeof(quint32))
        {
            quint32 flags = *((quint32*) data.data());
            if (flags & QnLayoutFileStorageResource::ReadOnly) {
                Qn::Permissions permissions = Qn::ReadPermission | Qn::RemovePermission;
                layout->setData(Qn::LayoutPermissionsRole, (int) permissions);
            }
            if (flags & QnLayoutFileStorageResource::ContainsCameras)
                layoutWithCameras = true;
        }
    }

    if (!layout->backgroundImageFilename().isEmpty()) {
        QIODevice* backgroundFile = layoutStorage.open(layout->backgroundImageFilename(), QIODevice::ReadOnly);
        if (backgroundFile) {
            QByteArray data = backgroundFile->readAll();
            delete backgroundFile;

            QnLocalFileCache cache;
            cache.storeImage(layout->backgroundImageFilename(), data);
        }
    }


    layout->setParentId(0);
    layout->setId(QnId::createUuid());
    layout->setName(QFileInfo(xfile).fileName());
    layout->addFlags(QnResource::local);

    layout->addFlags(QnResource::url);
    layout->setUrl(xfile);

    //QnLayoutItemDataMap items = layout->getItems();
    QnLayoutItemDataMap updatedItems;

    QIODevice *itemNamesIO = layoutStorage.open(QLatin1String("item_names.txt"), QIODevice::ReadOnly);
    QTextStream itemNames(itemNamesIO);
    QIODevice *itemTimeZonesIO = layoutStorage.open(QLatin1String("item_timezones.txt"), QIODevice::ReadOnly);
    QTextStream itemTimeZones(itemTimeZonesIO);

    // TODO: #Elric here is bad place to add resources to pool. need refactor
    QnLayoutItemDataList& items = orderedItems;
    for (int i = 0; i < items.size(); ++i)
    {
        QnLayoutItemData& item = items[i];
        QString path = item.resource.path;
        item.uuid = QUuid::createUuid();
        if (!path.endsWith(QLatin1String(".mkv")))
            item.resource.path += QLatin1String(".mkv");
        item.resource.path = QnLayoutFileStorageResource::updateNovParent(xfile,item.resource.path);

        QnStorageResourcePtr storage(new QnLayoutFileStorageResource());
        storage->setUrl(xfile);

        QnAviResourcePtr aviResource(new QnAviResource(item.resource.path));
        if (layoutWithCameras)
            aviResource->addFlags(QnResource::utc | QnResource::sync | QnResource::periods | QnResource::motion);
        aviResource->setStorage(storage);
        aviResource->setParentId(layout->getId());
        QString itemName(itemNames.readLine());
        if (!itemName.isEmpty())
            aviResource->setName(itemName);
        qint64 timeZoneOffset = itemTimeZones.readLine().toLongLong();
        if (timeZoneOffset != Qn::InvalidUtcOffset)
            aviResource->setTimeZoneOffset(timeZoneOffset);

        qnResPool->addResource(aviResource);
        aviResource = qnResPool->getResourceByUniqId(aviResource->getUniqueId()).dynamicCast<QnAviResource>(); // It may have already been in the pool!
        if(!aviResource) {
            qnWarning("ACHTUNG! Total mess up in exported layout loading!");
            continue;
        }
        item.resource.id = aviResource->getId();

        for (int channel = 0; channel < CL_MAX_CHANNELS; ++channel) {
            QString normMotionName = path.mid(path.lastIndexOf(L'?')+1);
            QIODevice* motionIO = layoutStorage.open(QString(QLatin1String("motion%1_%2.bin")).arg(channel).arg(QFileInfo(normMotionName).completeBaseName()), QIODevice::ReadOnly);
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
            else
                break;
        }

        updatedItems.insert(item.uuid, item);
    }
    delete itemNamesIO;
    delete itemTimeZonesIO;
    layout->setItems(updatedItems);
    layout->addFlags(QnResource::local);
    return layout;
}

QnResourcePtr QnResourceDirectoryBrowser::createArchiveResource(const QString& xfile) {
    //if (FileTypeSupport::isImageFileExt(xfile))
    //    return QnResourcePtr(new QnLocalFileResource(xfile));

    if (QnAviDvdResource::isAcceptedUrl(xfile))
        return QnResourcePtr(new QnAviDvdResource(xfile));
    if (QnAviBlurayResource::isAcceptedUrl(xfile))
        return QnResourcePtr(new QnAviBlurayResource(xfile));
    if (FileTypeSupport::isMovieFileExt(xfile))
        return QnResourcePtr(new QnAviResource(xfile));

    if (FileTypeSupport::isImageFileExt(xfile)) {
        QnResourcePtr rez = QnResourcePtr(new QnAviResource(xfile));
        rez->addFlags(QnResource::still_image);
        rez->removeFlags(QnResource::video | QnResource::audio);
        return rez;
    }

    if (FileTypeSupport::isLayoutFileExt(xfile)) {
        QnLayoutResourcePtr layout = layoutFromFile(xfile);
        return layout;
    }

    return QnResourcePtr(0);
}
