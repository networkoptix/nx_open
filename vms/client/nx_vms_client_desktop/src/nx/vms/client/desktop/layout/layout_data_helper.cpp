// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_data_helper.h"

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_access/resource_access_filter.h>

namespace nx::vms::client::desktop::layout {

QString resourcePath(const QnResourcePtr& resource)
{
    if (!NX_ASSERT(resource))
        return {};

    if (resource->hasFlags(Qn::exported_layout))
        return resource->getUrl();

    if (resource->hasFlags(Qn::local_video))
        return resource->getUrl();

    return {};
}

QnLayoutItemData itemFromResource(const QnResourcePtr& resource)
{
    QnLayoutItemData data;

    data.uuid = QnUuid::createUuid();
    data.resource.id = resource->getId();
    data.resource.path = resourcePath(resource);

    if (auto mediaResource = resource.dynamicCast<QnMediaResource>())
        data.rotation = mediaResource->forcedRotation().value_or(0);

    return data;
}

QnLayoutResourcePtr layoutFromResource(const QnResourcePtr& resource)
{
    NX_ASSERT(QnResourceAccessFilter::isOpenableInLayout(resource));
    if (!resource)
        return QnLayoutResourcePtr();

    QnLayoutResourcePtr layout(new QnLayoutResource());
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

    QnLayoutItemData item = itemFromResource(resource);
    // TODO: #sivanov Move to api.
    item.flags = /*pinned*/ 0x1; // Layout data item flags are declared in the client module.
    item.combinedGeometry = cellGeometry;
    item.rotation = rotation;

    layout->addItem(item);

    return layout;
}

} // namespace nx::vms::client::desktop::layout
