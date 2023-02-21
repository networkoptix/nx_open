// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_hotspots_overlay_widget.h"

#include <core/resource/camera_resource.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/ui/graphics/items/camera_hotspot_item.h>
#include <nx/vms/client/desktop/window_context.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {

using Geometry = nx::vms::client::core::Geometry;

//-------------------------------------------------------------------------------------------------
// CameraHotspotsOverlayWidget::Private declaration.

struct CameraHotspotsOverlayWidget::Private
{
    CameraHotspotsOverlayWidget* const q;
    QnMediaResourceWidget* parentMediaResourceWidget;

    std::vector<CameraHotspotItem*> hotspotItems;

    QnResourcePool* resourcePool() const;
    QnWorkbenchContext* workbenchContext() const;

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

QnVirtualCameraResourcePtr CameraHotspotsOverlayWidget::Private::resourceWidgetCamera() const
{
    return parentMediaResourceWidget->resource().dynamicCast<QnVirtualCameraResource>();
}

QnUuidSet CameraHotspotsOverlayWidget::Private::resourceWidgetCameraHotspotsIds() const
{
    const auto camera = resourceWidgetCamera();
    if (!NX_ASSERT(camera))
        return {};

    const auto hotspots = camera->cameraHotspots();
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

    const auto camera = resourceWidgetCamera();
    if (!camera->cameraHotspotsEnabled())
        return;

    const auto hotspotsData = resourceWidgetCamera()->cameraHotspots();
    for (const auto& hotspotData: hotspotsData)
    {
        if (const auto hotspotCamera =
            resourcePool()->getResourceById<QnVirtualCameraResource>(hotspotData.cameraId))
        {
            if (!ResourceAccessManager::hasPermissions(hotspotCamera, Qn::ReadPermission))
                continue;

            CameraHotspotItem* item(new CameraHotspotItem(hotspotData, workbenchContext(), q));
            item->setPos(Geometry::subPoint(q->rect(), hotspotData.pos));
            hotspotItems.push_back(item);
        }
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

    connect(camera.get(), &QnVirtualCameraResource::cameraHotspotsEnabledChanged, this,
        [this] { d->initHotspots(); });

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

    const auto accessController = ResourceAccessManager::accessController(camera);
    NX_ASSERT(accessController);

    connect(accessController, &QnWorkbenchAccessController::permissionsChanged, this,
        [this](const QnResourcePtr& resource)
        {
            if (d->resourceWidgetCameraHotspotsIds().contains(resource->getId()))
                d->initHotspots();
        });

    connect(accessController, &QnWorkbenchAccessController::permissionsReset, this,
        [this] { d->initHotspots(); });

    d->initHotspots();
}

CameraHotspotsOverlayWidget::~CameraHotspotsOverlayWidget()
{
    // Required here for forward-declared scoped pointer destruction.
}

void CameraHotspotsOverlayWidget::resizeEvent(QGraphicsSceneResizeEvent* event)
{
    for (auto hotspotItem: d->hotspotItems)
        hotspotItem->setPos(Geometry::subPoint(rect(), hotspotItem->hotspotData().pos));
}

} // namespace nx::vms::client::desktop
