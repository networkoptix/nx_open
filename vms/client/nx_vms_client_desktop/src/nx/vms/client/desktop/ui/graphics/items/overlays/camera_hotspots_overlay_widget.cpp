// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_hotspots_overlay_widget.h"

#include <core/resource/camera_resource.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/ui/graphics/items/camera_hotspot_item.h>
#include <nx/vms/client/desktop/window_context.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {

using Geometry = nx::vms::client::core::Geometry;
using ResourceAccessProvider = nx::core::access::ResourceAccessProvider;

//-------------------------------------------------------------------------------------------------
// CameraHotspotsOverlayWidget::Private declaration.

struct CameraHotspotsOverlayWidget::Private
{
    CameraHotspotsOverlayWidget* const q;
    QnMediaResourceWidget* parentMediaResourceWidget;

    std::vector<CameraHotspotItem*> hotspotItems;

    QnResourcePool* resourcePool() const;
    QnWorkbenchContext* workbenchContext() const;
    ResourceAccessProvider* accessProvider() const;
    QnResourceAccessSubject accessSubject() const;

    QnVirtualCameraResourcePtr resourceWidgetCamera() const;
    QnUuidSet resourceWidgetCameraHotspotsIds() const;

    void initHotspots();
};

//-------------------------------------------------------------------------------------------------
// CameraHotspotsOverlayWidget::Private definition.

QnResourcePool* CameraHotspotsOverlayWidget::Private::resourcePool() const
{
    return parentMediaResourceWidget->resourcePool();
}

QnWorkbenchContext* CameraHotspotsOverlayWidget::Private::workbenchContext() const
{
    return parentMediaResourceWidget->windowContext()->workbenchContext();
}

ResourceAccessProvider* CameraHotspotsOverlayWidget::Private::accessProvider() const
{
    return parentMediaResourceWidget->resourceAccessProvider();
}

QnVirtualCameraResourcePtr CameraHotspotsOverlayWidget::Private::resourceWidgetCamera() const
{
    return parentMediaResourceWidget->resource().dynamicCast<QnVirtualCameraResource>();
}

QnResourceAccessSubject CameraHotspotsOverlayWidget::Private::accessSubject() const
{
    return workbenchContext()->user();
}

QnUuidSet CameraHotspotsOverlayWidget::Private::resourceWidgetCameraHotspotsIds() const
{
    const auto camera = resourceWidgetCamera();
    if (!NX_ASSERT(camera))
        return {};

    const auto hotspots = camera->getCameraHotspots();
    QnUuidSet hospotsCamerasIds;
    for (const auto& hotspot: hotspots)
        hospotsCamerasIds.insert(hotspot.cameraId);

    return hospotsCamerasIds;
}

void CameraHotspotsOverlayWidget::Private::initHotspots()
{
    qDeleteAll(hotspotItems);
    hotspotItems.clear();

    if (!workbenchContext() || !resourceWidgetCamera())
        return;

    const auto hotspotsData = resourceWidgetCamera()->getCameraHotspots();
    for (const auto& hotspotData: hotspotsData)
    {
        const auto hotspotCamera =
            resourcePool()->getResourceById<QnVirtualCameraResource>(hotspotData.cameraId);
        if (!hotspotCamera || !accessProvider()->hasAccess(accessSubject(), hotspotCamera))
            continue;

        CameraHotspotItem* item(new CameraHotspotItem(hotspotData, q));
        item->setPos(Geometry::subPoint(q->rect(), hotspotData.pos));
        hotspotItems.push_back(item);
        item->initTooltip(workbenchContext(), hotspotCamera);
    }
}

//-------------------------------------------------------------------------------------------------
// CameraHotspotsOverlayWidget definition.

CameraHotspotsOverlayWidget::CameraHotspotsOverlayWidget(QnMediaResourceWidget* parent):
    base_type(parent),
    d(new Private{this, parent})
{
    const auto camera = d->resourceWidgetCamera();

    if (!NX_ASSERT(camera))
        return;

    connect(camera.get(), &QnVirtualCameraResource::cameraHotspotsChanged, this,
        [this] { d->initHotspots(); });

    const auto initHotspotsOnResourcePoolChange =
        [this](const QnResourceList& resources)
        {
            const auto resourcesIds = resources.ids();
            QnUuidSet resourcesIdsSet(resourcesIds.cbegin(), resourcesIds.cend());
            if (resourcesIdsSet.intersects(d->resourceWidgetCameraHotspotsIds()))
                d->initHotspots();
        };

    connect(camera->resourcePool(), &QnResourcePool::resourcesAdded,
        this, initHotspotsOnResourcePoolChange);

    connect(camera->resourcePool(), &QnResourcePool::resourcesRemoved,
        this, initHotspotsOnResourcePoolChange);

    connect(d->accessProvider(), &ResourceAccessProvider::accessChanged, this,
        [this](const QnResourceAccessSubject& subject,
            const QnResourcePtr& resource,
            nx::core::access::Source)
        {
            if (d->accessSubject() != subject)
                return;

            if (d->resourceWidgetCameraHotspotsIds().contains(resource->getId()))
                d->initHotspots();
        });

    d->initHotspots();
}

CameraHotspotsOverlayWidget::~CameraHotspotsOverlayWidget()
{
}

void CameraHotspotsOverlayWidget::resizeEvent(QGraphicsSceneResizeEvent* event)
{
    for (auto hotspotItem: d->hotspotItems)
        hotspotItem->setPos(Geometry::subPoint(rect(), hotspotItem->hotspotData().pos));
}

} // namespace nx::vms::client::desktop
