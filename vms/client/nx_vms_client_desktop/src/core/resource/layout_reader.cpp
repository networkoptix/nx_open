// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_reader.h"

#include <QtCore/QTextCodec>
#include <QtCore/QTextStream>
#include <QtCore/QThread>

#include <client/client_globals.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/file_layout_resource.h>
#include <core/storage/file_storage/layout_storage_resource.h>
#include <managers/resource_manager.h>
#include <nx/core/watermark/watermark.h>
#include <nx/fusion/serialization/json.h>
#include <nx/reflect/json/deserializer.h>
#include <nx/vms/client/desktop/export/data/nov_metadata.h>
#include <nx/vms/client/desktop/layout/layout_data_helper.h>
#include <nx/vms/client/desktop/resource/layout_password_management.h>
#include <nx/vms/client/desktop/utils/local_file_cache.h>
#include <nx/vms/common/system_context.h>

#include "layout_proto.h"

using namespace nx::vms::client::desktop;

QnFileLayoutResourcePtr layout::layoutFromFile(
    const QString& layoutUrl, const QString& password)
{
    // Create storage handler and read layout info.
    QnLayoutFileStorageResource layoutStorage(layoutUrl);

    using QIODevicePtr = QScopedPointer<QIODevice>;

    NovMetadata metadata;
    {
        QIODevicePtr metadataFile(layoutStorage.open("metadata.json", QIODevice::ReadOnly));
        if (metadataFile)
            nx::reflect::json::deserialize(metadataFile->readAll().toStdString(), &metadata);
    }

    QIODevicePtr layoutFile(layoutStorage.open("layout.pb", QIODevice::ReadOnly));
    if (!layoutFile)
        return QnFileLayoutResourcePtr();

    QByteArray layoutData = layoutFile->readAll();
    layoutFile.reset();

    QnFileLayoutResourcePtr layout(new QnFileLayoutResource());

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

    // Layout is not accessible if it is Encrypted and we do not know the password.
    bool layoutIsAccessible =
        !fileInfo.isCrypted || (!password.isEmpty() && checkPassword(password, fileInfo));
    if (!layoutIsAccessible && !password.isEmpty())
        NX_WARNING(NX_SCOPE_TAG, "Loading encrypted layout file with wrong password.");

    // We would like to remove items if the layout is not accessible.
    if (!layoutIsAccessible)
        apiLayout.items = {};

    QnUuid layoutId = guidFromArbitraryData(layoutUrl);

    auto layoutBase = layout.staticCast<QnLayoutResource>();
    ec2::fromApiToResource(apiLayout, layoutBase);

    layout->setIdUnsafe(layoutId);
    layout->setParentId(QnUuid());
    layout->setName(QFileInfo(layoutUrl).fileName());
    layout->setUrl(layoutUrl);

    if (fileInfo.isCrypted)
        layout->setIsEncrypted(true);
    if (fileInfo.isCrypted && layoutIsAccessible)
        layout->usePasswordToRead(password);

    // At this point we have enough info for layout node itself.
    // We should do nothing more if the layout is not accessible.
    if (!layoutIsAccessible)
        return layout;

    QnLayoutItemDataList orderedItems;
    foreach(const auto& item, apiLayout.items)
    {
        orderedItems << QnLayoutItemData();
        ec2::fromApiToResource(item, orderedItems.last());
    }

    QIODevicePtr rangeFile(layoutStorage.open("range.bin", QIODevice::ReadOnly));
    if (rangeFile)
    {
        QByteArray data = rangeFile->readAll();
        layout->setLocalRange(QnTimePeriod().deserialize(data));
        rangeFile.reset();
    }

    QIODevicePtr miscFile(layoutStorage.open("misc.bin", QIODevice::ReadOnly));
    bool layoutWithCameras = false;
    if (miscFile)
    {
        QByteArray data = miscFile->readAll();
        if (data.size() >= (int)sizeof(quint32))
        {
            quint32 flags = *((quint32*)data.data());
            // Currently QnFileLayoutResource::readOnly() lives together with LayoutPermissionsRole.
            layout->setReadOnly(flags & QnLayoutFileStorageResource::ReadOnly);
            if (flags & QnLayoutFileStorageResource::ContainsCameras)
                layoutWithCameras = true;
        }

        miscFile.reset();
    }

    if (!layout->backgroundImageFilename().isEmpty())
    {
        QIODevicePtr backgroundFile(layoutStorage.open(layout->backgroundImageFilename(), QIODevice::ReadOnly));
        if (backgroundFile)
        {
            QByteArray data = backgroundFile->readAll();
            LocalFileCache cache;
            cache.storeImageData(layout->backgroundImageFilename(), data);

            backgroundFile.reset();
        }
    }

    QIODevicePtr watermarkFile(layoutStorage.open("watermark.txt", QIODevice::ReadOnly));
    if (watermarkFile)
    {
        QByteArray data = watermarkFile->readAll();
        nx::core::Watermark watermark;

        if (QJson::deserialize(data, &watermark))
            layout->setData(Qn::LayoutWatermarkRole, QVariant::fromValue(watermark));

        watermarkFile.reset();
    }

    QnLayoutItemDataMap updatedItems;

    QIODevicePtr itemNamesIO(layoutStorage.open("item_names.txt", QIODevice::ReadOnly));
    QTextStream itemNames(itemNamesIO.data());
    if (metadata.version >= NovMetadata::kVersion51)
        itemNames.setCodec("UTF-8");
    else
        itemNames.setCodec(QTextCodec::codecForLocale());

    QIODevicePtr itemTimeZonesIO(layoutStorage.open("item_timezones.txt", QIODevice::ReadOnly));
    QTextStream itemTimeZones(itemTimeZonesIO.data());

    // TODO: #sivanov Here is bad place to add resources to the pool. Need refactor.
    // All AVI resources are added to pool here. Layout itself is added later outside this module.
    QnLayoutItemDataList& items = orderedItems;
    for (int i = 0; i < items.size(); ++i)
    {
        QnLayoutItemData& item = items[i];
        QString path = item.resource.path;
        NX_ASSERT(!path.isEmpty(), "Resource path should not be empty. Exported file is not valid.");
        if (!path.endsWith(".mkv"))
            path += ".mkv";
        item.resource.path = QnLayoutFileStorageResource::itemUniqueId(layoutUrl, path);

        QnLayoutFileStorageResourcePtr fileStorage(new QnLayoutFileStorageResource(layoutUrl));
        fileStorage->usePasswordToRead(password); //< Set the password or do nothing.
        auto storage = fileStorage.dynamicCast<QnStorageResource>();

        QnAviResourcePtr aviResource(new QnAviResource(item.resource.path));
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

        auto resourcePool = qnClientCoreModule->resourcePool();
        resourcePool->addResource(aviResource, QnResourcePool::SkipAddingTransaction);

        // Check if we have updated an existing resource.
        auto existingResource = resourcePool->getResourceByUrl(
            aviResource->getUrl()).dynamicCast<QnAviResource>();

        NX_ASSERT(existingResource);
        if (existingResource)
            aviResource = existingResource;

        item.resource.id = aviResource->getId();

        for (int channel = 0; channel < CL_MAX_CHANNELS; ++channel)
        {
            QString normMotionName = path.mid(path.lastIndexOf('?') + 1);
            QIODevicePtr motionIO(layoutStorage.open(QString("motion%1_%2.bin")
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

    if (!QThread::currentThread()->isInterruptionRequested())
        layout->setItems(updatedItems);

    return layout;
}

bool layout::reloadFromFile(
    QnFileLayoutResourcePtr layout, const QString& password)
{
    if (!NX_ASSERT(layout))
        return false;

    // Remove all layout AVI streams from resource pool.
    const auto resources = layoutResources(layout);
    layout->setItems(QnLayoutItemDataList());
    for (const auto& resource: resources)
    {
        if (resource.dynamicCast<QnAviResource>())
            layout->resourcePool()->removeResource(resource);
    }

    QnFileLayoutResourcePtr updatedLayout = layoutFromFile(layout->getUrl(), password);
    if (!updatedLayout)
        return false;

    updatedLayout->cloneItems(updatedLayout); //< This will alter item.id for items to prevent errors.
    layout->update(updatedLayout);

    return true;
}
