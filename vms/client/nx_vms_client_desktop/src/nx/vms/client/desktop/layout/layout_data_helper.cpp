// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_data_helper.h"

#include <core/resource/camera_resource.h>
#include <core/resource/resource_media_layout.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/resource_descriptor.h>

namespace nx::vms::client::desktop {

nx::vms::common::LayoutItemData layoutItemFromResource(
    const QnResourcePtr& resource, bool forceCloud)
{
    nx::vms::common::LayoutItemData data;

    data.uuid = nx::Uuid::createUuid();
    data.resource = descriptor(resource, forceCloud);

    if (auto mediaResource = resource.dynamicCast<QnMediaResource>())
        data.rotation = mediaResource->forcedRotation().value_or(0);

    return data;
}

LayoutResourcePtr layoutFromResource(const QnResourcePtr& resource)
{
    NX_ASSERT(QnResourceAccessFilter::isOpenableInLayout(resource));
    if (!resource)
        return LayoutResourcePtr();

    LayoutResourcePtr layout(new LayoutResource());
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

        if (auto resource = getResourceByDescriptor(item.resource))
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

} // namespace nx::vms::client::desktop
