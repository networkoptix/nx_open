// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_hotspots_overlay_widget.h"

#include <core/resource/camera_resource.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/ui/graphics/items/camera_hotspot_item.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {

using Geometry = nx::vms::client::core::Geometry;

//-------------------------------------------------------------------------------------------------
// CameraHotspotsOverlayWidget::Private declaration.

struct CameraHotspotsOverlayWidget::Private
{
    CameraHotspotsOverlayWidget* const q;

    std::vector<CameraHotspotItem*> hotspotItems;
};

//-------------------------------------------------------------------------------------------------
// CameraHotspotsOverlayWidget definition.

CameraHotspotsOverlayWidget::CameraHotspotsOverlayWidget(QGraphicsItem* parent):
    base_type(parent),
    d(new Private{this})
{
}

CameraHotspotsOverlayWidget::~CameraHotspotsOverlayWidget()
{
}

void CameraHotspotsOverlayWidget::initHotspots(
    QnWorkbenchContext* context,
    const QnVirtualCameraResourcePtr& camera,
    const nx::vms::common::CameraHotspotDataList& hotspotsData)
{
    qDeleteAll(d->hotspotItems);
    d->hotspotItems.clear();

    if (!context || !camera)
        return;

    const auto acccessProvider = context->resourceAccessProvider();
    const auto subject = context->user();
    const auto resourcePool = context->resourcePool();
    for (const auto& hotspotData: hotspotsData)
    {
        const auto hotspotCamera =
            resourcePool->getResourceById<QnVirtualCameraResource>(hotspotData.cameraId);
        if (!hotspotCamera || !acccessProvider->hasAccess(subject, hotspotCamera))
            continue;

        CameraHotspotItem* item(new CameraHotspotItem(hotspotData, this));
        item->setPos(Geometry::subPoint(rect(), hotspotData.pos));
        d->hotspotItems.push_back(item);
        item->initTooltip(context, hotspotCamera);
    }
}

void CameraHotspotsOverlayWidget::resizeEvent(QGraphicsSceneResizeEvent* event)
{
    for (auto hotspotItem: d->hotspotItems)
        hotspotItem->setPos(Geometry::subPoint(rect(), hotspotItem->hotspotData().pos));
}

} // namespace nx::vms::client::desktop
