// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_data_helper.h"

#include <QtCore/QFileInfo>

#include <core/resource/camera_resource.h>
#include <core/resource/resource_media_layout.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/log.h>
#include <nx/vms/api/data/layout_data.h>
#include <nx/vms/client/core/cross_system/cloud_layouts_manager.h>
#include <nx/vms/client/core/file_cache/file_cache.h>
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/client/core/resource/resource_descriptor_helpers.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/file_cache/file_cache_utils.h>
#include <utils/common/delayed.h>

namespace nx::vms::client::desktop {

nx::vms::common::LayoutItemData layoutItemFromResource(
    const QnResourcePtr& resource, bool forceCloud)
{
    nx::vms::common::LayoutItemData data;

    data.uuid = nx::Uuid::createUuid();
    data.resource = core::descriptor(resource, forceCloud);

    if (auto mediaResource = resource.dynamicCast<QnMediaResource>())
        data.rotation = mediaResource->forcedRotation().value_or(0);

    return data;
}

core::LayoutResourcePtr layoutFromResource(const QnResourcePtr& resource)
{
    NX_ASSERT(QnResourceAccessFilter::isOpenableInLayout(resource));
    if (!resource)
        return core::LayoutResourcePtr();

    core::LayoutResourcePtr layout(new core::LayoutResource());
    layout->setCellSpacing(0);
    layout->setName(resource->getName());
    if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        const auto ar = camera->aspectRatioRotated();
        if (ar.isValid())
            layout->setCellAspectRatio(QnAspectRatio::closestStandardRatio(ar.toFloat()).toFloat());
    }

    QRect cellGeometry = QRect(0, 0, 1, 1);
    qreal rotation = 0;
    if (const auto media = resource.dynamicCast<QnMediaResource>())
    {
        // If video occupies several cells, remember this.
        if (media->getVideoLayout() && media->getVideoLayout()->size().isValid())
            cellGeometry.setSize(media->getVideoLayout()->size());
        // Set rotation.
        rotation = media->forcedRotation().value_or(0);
        // Transpose cell configuration if 90 degree rotated.
        if (QnAspectRatio::isRotated90(rotation))
            cellGeometry = cellGeometry.transposed();
    }

    nx::vms::common::LayoutItemData item = layoutItemFromResource(resource);
    // TODO: #sivanov Move to api.
    item.flags = /*pinned*/ 0x1; // Layout data item flags are declared in the client module.
    item.combinedGeometry = cellGeometry;
    item.rotation = rotation;

    layout->addItem(item);

    return layout;
}

QSet<QnResourcePtr> layoutResources(const QnLayoutResourcePtr& layout)
{
    QSet<QnResourcePtr> result;
    for (const auto& item: layout->getItems())
    {
        if (item.uuid.isNull())
            continue;

        if (auto resource = core::getResourceByDescriptor(item.resource))
            result.insert(resource);
    }
    return result;
}

bool resourceBelongsToLayout(const QnResourcePtr& resource, const QnLayoutResourcePtr& layout)
{
    for (const auto& item: layout->getItems())
    {
        if (item.resource.id == resource->getId())
            return true;
    }
    return false;
}

namespace {

void ensureBackgroundImageInCloudCache(
    const core::LayoutResourcePtr& layout,
    const core::LayoutResourcePtr& cloudLayout,
    std::function<void(bool success)> callback)
{
    const QString filename = layout->backgroundImageFilename();
    if (filename.isEmpty())
    {
        executeLater([callback] { callback(true); }, cloudLayout.get());
        return;
    }

    const auto sourceCache = file_cache::backgroundImageCache(layout);
    const QString sourcePath = sourceCache ? sourceCache->absoluteFilePath(filename) : QString();
    if (sourcePath.isEmpty())
    {
        NX_WARNING(NX_SCOPE_TAG, "Rejecting unsafe background image filename: %1", filename);
        executeLater([callback] { callback(false); }, cloudLayout.get());
        return;
    }

    // Layouts saved by the previous versions refer to the background image by a name derived from
    // its source path, while the cloud cache is content-addressed: re-key the image by content.
    file_cache::cachedImageFilenameAsync(sourcePath,
        [cloudLayout, sourcePath, callback](const QString& targetFilename)
        {
            if (targetFilename.isEmpty())
            {
                // The layout was never displayed on this machine, so the image is not cached. The
                // user can set the background anew via the layout settings dialog.
                NX_WARNING(NX_SCOPE_TAG,
                    "Background image is not available locally: %1", sourcePath);
                callback(false);
                return;
            }

            const auto targetCache = file_cache::backgroundImageCache(cloudLayout);
            const QString targetPath = targetCache
                ? targetCache->absoluteFilePath(targetFilename)
                : QString();

            if (!NX_ASSERT(!targetPath.isEmpty()))
            {
                callback(false);
                return;
            }

            if (QFileInfo::exists(targetPath))
            {
                cloudLayout->setBackgroundImageFilename(targetFilename);
                callback(true);
                return;
            }

            file_cache::copyFileAsync(sourcePath, targetPath,
                [cloudLayout, targetFilename, callback](bool success)
                {
                    if (!success)
                    {
                        NX_WARNING(NX_SCOPE_TAG,
                            "Failed to copy background image to the cloud image cache: %1",
                            targetFilename);
                        callback(false);
                        return;
                    }

                    cloudLayout->setBackgroundImageFilename(targetFilename);
                    callback(true);
                },
                cloudLayout.get());
        },
        cloudLayout.get());
}

} // namespace

core::LayoutResourcePtr convertLayoutToCloud(
    const core::LayoutResourcePtr& layout,
    core::LayoutResource::ItemsRemapHash* itemsRemapHash,
    std::function<void(const core::LayoutResourcePtr& cloudLayout)> onCompleted)
{
    const auto cloudLayout =
        appContext()->cloudLayoutsManager()->convertLocalLayout(layout, itemsRemapHash);
    if (!cloudLayout)
        return cloudLayout;

    // The cloned background image filename refers to an image that is not yet available in the
    // cloud cache. Reset the filename so the cloud layout restores its background only after the
    // image becomes available there.
    cloudLayout->setBackgroundImageFilename({});

    ensureBackgroundImageInCloudCache(layout, cloudLayout,
        [cloudLayout, onCompleted = std::move(onCompleted)](bool success)
        {
            if (!success)
            {
                cloudLayout->setBackgroundImageFilename({});
                cloudLayout->setBackgroundOpacity(
                    nx::vms::api::LayoutData::kDefaultBackgroundOpacity);
                cloudLayout->setBackgroundSize({});
            }

            if (onCompleted)
                onCompleted(cloudLayout);
        });

    return cloudLayout;
}

} // namespace nx::vms::client::desktop
