#include "layout_reader.h"
#include "layout_proto.h"

#include <nx/core/watermark/watermark.h>
#include <nx/fusion/serialization/json.h>

#include <core/resource/file_layout_resource.h>
#include <core/resource/avi/avi_resource.h>
#include <core/storage/file_storage/layout_storage_resource.h>

#include <common/common_module.h>

#include <managers/resource_manager.h>

#include <client_core/client_core_module.h>

#include <nx/vms/client/desktop/resources/layout_password_management.h>
#include <nx/vms/client/desktop/utils/local_file_cache.h>

QnFileLayoutResourcePtr nx::vms::client::desktop::layout::layoutFromFile(const QString& layoutUrl)
{
    // Create storage handler and read layout info.
    QnLayoutFileStorageResource layoutStorage(qnClientCoreModule->commonModule(), layoutUrl);
    QScopedPointer<QIODevice> layoutFile(layoutStorage.open(lit("layout.pb"), QIODevice::ReadOnly));
    if (!layoutFile)
        return QnFileLayoutResourcePtr();

    QByteArray layoutData = layoutFile->readAll();
    layoutFile.reset();

    QnFileLayoutResourcePtr layout(new QnFileLayoutResource());

    // Deal with encrypted layouts.
    const auto fileInfo = nx::core::layout::identifyFile(layoutUrl);
    if (!fileInfo.isValid)
        return QnFileLayoutResourcePtr();

    nx::vms::api::LayoutData apiLayout;
    if (!QJson::deserialize(layoutData, &apiLayout))
    {
        QnProto::Message<nx::vms::api::LayoutData> apiLayoutMessage;
        if (!QnProto::deserialize(layoutData, &apiLayoutMessage))
            return QnFileLayoutResourcePtr();

        apiLayout = apiLayoutMessage.data;
    }

    ec2::fromApiToResource(apiLayout, layout.staticCast<QnLayoutResource>());

    QnLayoutItemDataList orderedItems;
    foreach(const auto& item, apiLayout.items)
    {
        orderedItems << QnLayoutItemData();
        ec2::fromApiToResource(item, orderedItems.last());
    }

    QnUuid layoutId = guidFromArbitraryData(layoutUrl);

    layout->setId(layoutId);
    layout->setParentId(QnUuid());
    layout->setName(QFileInfo(layoutUrl).fileName());
    layout->setUrl(layoutUrl);

    if (fileInfo.isCrypted)
        nx::vms::client::desktop::layout::markAsEncrypted(layout);

    QScopedPointer<QIODevice> rangeFile(layoutStorage.open(lit("range.bin"), QIODevice::ReadOnly));
    if (rangeFile)
    {
        QByteArray data = rangeFile->readAll();
        layout->setLocalRange(QnTimePeriod().deserialize(data));
        rangeFile.reset();
    }

    QScopedPointer<QIODevice> miscFile(layoutStorage.open(lit("misc.bin"), QIODevice::ReadOnly));
    bool layoutWithCameras = false;
    if (miscFile)
    {
        QByteArray data = miscFile->readAll();
        if (data.size() >= (int)sizeof(quint32))
        {
            quint32 flags = *((quint32*)data.data());
            if (flags & QnLayoutFileStorageResource::ReadOnly)
            {
                Qn::Permissions permissions = Qn::ReadPermission | Qn::RemovePermission;
                layout->setData(Qn::LayoutPermissionsRole, (int)permissions);
            }
            if (flags & QnLayoutFileStorageResource::ContainsCameras)
                layoutWithCameras = true;
        }

        miscFile.reset();
    }

    if (!layout->backgroundImageFilename().isEmpty())
    {
        QScopedPointer<QIODevice> backgroundFile(layoutStorage.open(layout->backgroundImageFilename(), QIODevice::ReadOnly));
        if (backgroundFile)
        {
            QByteArray data = backgroundFile->readAll();
            nx::vms::client::desktop::LocalFileCache cache;
            cache.storeImageData(layout->backgroundImageFilename(), data);

            backgroundFile.reset();
        }
    }

    QScopedPointer<QIODevice> watermarkFile(layoutStorage.open(lit("watermark.txt"), QIODevice::ReadOnly));
    if (watermarkFile)
    {
        QByteArray data = watermarkFile->readAll();
        nx::core::Watermark watermark;

        if (QJson::deserialize(data, &watermark))
            layout->setData(Qn::LayoutWatermarkRole, QVariant::fromValue(watermark));

        watermarkFile.reset();
    }

    QnLayoutItemDataMap updatedItems;

    QScopedPointer<QIODevice> itemNamesIO(layoutStorage.open(lit("item_names.txt"), QIODevice::ReadOnly));
    QTextStream itemNames(itemNamesIO.data());

    QScopedPointer<QIODevice> itemTimeZonesIO(layoutStorage.open(lit("item_timezones.txt"), QIODevice::ReadOnly));
    QTextStream itemTimeZones(itemTimeZonesIO.data());

    // TODO: #Elric here is bad place to add resources to pool. need refactor
    // All AVI resources are added to pool here. Layout itself is added later outside this module.
    QnLayoutItemDataList& items = orderedItems;
    for (int i = 0; i < items.size(); ++i)
    {
        QnLayoutItemData& item = items[i];
        QString path = item.resource.uniqueId;
        NX_ASSERT(!path.isEmpty(), "Resource path should not be empty. Exported file is not valid.");
        if (!path.endsWith(lit(".mkv")))
            path += lit(".mkv");
        item.resource.uniqueId = QnLayoutFileStorageResource::itemUniqueId(layoutUrl, path);

        QnStorageResourcePtr storage(
            new QnLayoutFileStorageResource(qnClientCoreModule->commonModule(), layoutUrl));

        QnAviResourcePtr aviResource(new QnAviResource(item.resource.uniqueId));
        aviResource->addFlags(Qn::exported_media);
        if (layoutWithCameras)
            aviResource->addFlags(Qn::utc | Qn::sync | Qn::periods | Qn::motion);
        aviResource->setStorage(storage);
        aviResource->setParentId(layout->getId());
        QString itemName(itemNames.readLine());
        if (!itemName.isEmpty())
            aviResource->setName(itemName);
        qint64 timeZoneOffset = itemTimeZones.readLine().toLongLong();
        if (timeZoneOffset != Qn::InvalidUtcOffset)
            aviResource->setTimeZoneOffset(timeZoneOffset);

        auto resourcePool = qnClientCoreModule->commonModule()->resourcePool();
        resourcePool->addResource(aviResource, QnResourcePool::SkipAddingTransaction);

        // Check if we have updated an existing resource.
        auto existingResource = resourcePool->getResourceByUniqueId<QnAviResource>(
            aviResource->getUniqueId());

        NX_ASSERT(existingResource);
        if (existingResource)
            aviResource = existingResource;

        item.resource.id = aviResource->getId();

        for (int channel = 0; channel < CL_MAX_CHANNELS; ++channel)
        {
            QString normMotionName = path.mid(path.lastIndexOf(L'?') + 1);
            QScopedPointer<QIODevice> motionIO(layoutStorage.open(lit("motion%1_%2.bin")
                .arg(channel)
                .arg(QFileInfo(normMotionName).completeBaseName()), QIODevice::ReadOnly));
            // Doing some additional checks in case data is broken.
            if (motionIO && (motionIO->size() % sizeof(QnMetaDataV1Light) == 0))
            {
                QnMetaDataLightVector motionData;
                size_t motionDataSize = motionIO->size() / sizeof(QnMetaDataV1Light);
                if (motionDataSize > 0)
                {
                    motionData.resize(motionDataSize);
                    motionIO->read((char*)&motionData[0], motionIO->size());
                }
                for (uint i = 0; i < motionData.size(); ++i)
                    motionData[i].doMarshalling();
                if (!motionData.empty())
                    aviResource->setMotionBuffer(motionData, channel);

                motionIO.reset();
            }
            else
            {
                break;
            }
        }

        updatedItems.insert(item.uuid, item);
    }

    layout->setItems(updatedItems);

    return layout;
}
