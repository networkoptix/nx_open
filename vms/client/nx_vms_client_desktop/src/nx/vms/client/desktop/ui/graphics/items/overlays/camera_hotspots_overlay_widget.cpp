// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_hotspots_overlay_widget.h"

#include <core/resource/camera_resource.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/camera_hotspots/camera_hotspots_display_utils.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/ui/graphics/items/camera_hotspot_item.h>
#include <nx/vms/client/desktop/utils/dewarping_transform.h>
#include <nx/vms/client/desktop/window_context.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>

namespace nx::vms::client::desktop {

using nx::vms::client::core::AccessController;
using nx::vms::client::core::Geometry;

namespace {

using namespace nx::vms::client::desktop;

static constexpr auto kHotspotsDisplaySizeThreshold = QSizeF(
    camera_hotspots::kHotspotRadius * 8,
    camera_hotspots::kHotspotRadius * 8);

} // namespace

//-------------------------------------------------------------------------------------------------
// CameraHotspotsOverlayWidget::Private declaration.

struct CameraHotspotsOverlayWidget::Private
{
    CameraHotspotsOverlayWidget* const q;
    QnMediaResourceWidget* parentMediaResourceWidget;

    std::vector<CameraHotspotItem*> hotspotItems;

    QHash<QnResourcePtr, AccessController::NotifierPtr> notifiers;

    QnResourcePool* resourcePool() const;
    QnWorkbenchContext* workbenchContext() const;

    QnVirtualCameraResourcePtr resourceWidgetCamera() const;
    QnUuidSet resourceWidgetCameraHotspotsIds() const;

    void initHotspotItems();
    void updateHotspotItem(CameraHotspotItem* hotspotItem) const;
    void removeNotifiers(const QnResourceList& resources);
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

void CameraHotspotsOverlayWidget::Private::initHotspotItems()
{
    qDeleteAll(hotspotItems);
    hotspotItems.clear();

    if (!workbenchContext() || !resourceWidgetCamera())
        return;

    if (!parentMediaResourceWidget->hotspotsVisible())
        return;

    const auto hotspotsData = resourceWidgetCamera()->cameraHotspots();
    for (const auto& hotspotData: hotspotsData)
    {
        if (const auto hotspotCamera =
            resourcePool()->getResourceById<QnVirtualCameraResource>(hotspotData.cameraId))
        {
            const auto accessController = ResourceAccessManager::accessController(hotspotCamera);
            if (!accessController)
                continue;

            if (!notifiers.contains(hotspotCamera))
            {
                const auto notifier = accessController->createNotifier(hotspotCamera);
                notifiers.insert(hotspotCamera, notifier);

                connect(notifier.get(), &AccessController::Notifier::permissionsChanged, q,
                    [this]() { initHotspotItems(); });
            }

            if (!accessController->hasPermissions(hotspotCamera, Qn::ReadPermission))
                continue;

            CameraHotspotItem* item(new CameraHotspotItem(
                hotspotData,
                parentMediaResourceWidget->systemContext(),
                parentMediaResourceWidget->windowContext(),
                q));
            updateHotspotItem(item);
            hotspotItems.push_back(item);
        }
    }
}

void CameraHotspotsOverlayWidget::Private::updateHotspotItem(CameraHotspotItem* hotspotItem) const
{
    using namespace nx::vms::api;

    if (!Geometry::contains(q->rect().size(), kHotspotsDisplaySizeThreshold))
    {
        hotspotItem->setVisible(false);
        return;
    }

    const auto unitRect = QRectF(0.0, 0.0, 1.0, 1.0);
    auto relativePos = hotspotItem->hotspotData().pos;

    if (parentMediaResourceWidget->dewarpingApplied())
    {
        const auto mediaData = parentMediaResourceWidget->dewarpingParams();
        const auto viewData = parentMediaResourceWidget->item()->dewarpingParams();
        const auto isCeilingFisheye = mediaData.isFisheye()
            && mediaData.viewMode == dewarping::FisheyeCameraMount::ceiling;

        DewarpingTransform transform(mediaData, viewData, resourceWidgetCamera()->aspectRatio());
        if (const auto dewarpedPos = transform.mapToView(relativePos))
        {
            if (hotspotItem->hotspotData().hasDirection())
            {
                const auto directionVector = hotspotItem->hotspotData().direction;
                const auto directionPoint = relativePos + directionVector * 0.001;

                // Angular difference in radians between dewarped directional hotspot and
                // untransformed one.
                double dewarpedHotspotRotation = 0.0;

                if (const auto dewarpedDirectionPoint = transform.mapToView(directionPoint))
                {
                    const auto dewarpedDirectionVector =
                        dewarpedDirectionPoint.value() - dewarpedPos.value();
                    dewarpedHotspotRotation += Geometry::atan2(dewarpedDirectionVector);
                    dewarpedHotspotRotation -= Geometry::atan2(directionVector);
                }
                if (isCeilingFisheye)
                    dewarpedHotspotRotation += M_PI;

                hotspotItem->setRotation(qRadiansToDegrees(dewarpedHotspotRotation));
            }

            relativePos = dewarpedPos.value();
            if (isCeilingFisheye)
                relativePos = Geometry::rotated(relativePos, unitRect.center(), 180.0);
        }
        else
        {
            hotspotItem->setVisible(false);
            hotspotItem->setPos(q->rect().center());
            return;
        }
    }
    else
    {
        if (hotspotItem->hotspotData().hasDirection())
            hotspotItem->setRotation(0.0);

        if (parentMediaResourceWidget->isZoomWindow())
        {
            relativePos = Geometry::subPoint(
                Geometry::unsubRect(unitRect, parentMediaResourceWidget->zoomRect()), relativePos);
        }
    }

    hotspotItem->setVisible(unitRect.contains(relativePos));
    hotspotItem->setPos(camera_hotspots::hotspotOrigin(relativePos, q->rect()));
}

void CameraHotspotsOverlayWidget::Private::removeNotifiers(const QnResourceList& resources)
{
    for (const auto& resource: resources)
    {
        if (const auto it = notifiers.find(resource); it != notifiers.end())
        {
            it.value()->disconnect(q);
            notifiers.erase(it);
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

    connect(d->parentMediaResourceWidget, &QnMediaResourceWidget::optionsChanged, this,
        [this](QnMediaResourceWidget::Options changedOptions)
        {
            if (changedOptions.testFlag(QnMediaResourceWidget::DisplayHotspots))
                d->initHotspotItems();
        });

    connect(camera.get(), &QnVirtualCameraResource::cameraHotspotsChanged, this,
        [this] { d->initHotspotItems(); });

    const auto initHotspotsOnResourcePoolChange =
        [this](const QnResourceList& resources)
        {
            const auto resourcesIds = resources.ids();
            QnUuidSet resourcesIdsSet(resourcesIds.cbegin(), resourcesIds.cend());
            if (resourcesIdsSet.intersects(d->resourceWidgetCameraHotspotsIds()))
                d->initHotspotItems();
        };

    connect(camera->resourcePool(), &QnResourcePool::resourcesAdded,
        this, initHotspotsOnResourcePoolChange);

    connect(camera->resourcePool(), &QnResourcePool::resourcesRemoved, this,
        [this, initHotspotsOnResourcePoolChange](const QnResourceList& resources)
        {
            d->removeNotifiers(resources);
            initHotspotsOnResourcePoolChange(resources);
        });

    connect(d->parentMediaResourceWidget, &QnMediaResourceWidget::zoomRectChanged, this,
        [this]
        {
            for (auto hotspotItem: d->hotspotItems)
                d->updateHotspotItem(hotspotItem);
        });

    connect(d->parentMediaResourceWidget, &QnMediaResourceWidget::dewarpingParamsChanged, this,
        [this]
        {
            for (auto hotspotItem: d->hotspotItems)
                d->updateHotspotItem(hotspotItem);
        });

    d->initHotspotItems();
}

CameraHotspotsOverlayWidget::~CameraHotspotsOverlayWidget()
{
    // Required here for forward-declared scoped pointer destruction.
}

void CameraHotspotsOverlayWidget::resizeEvent(QGraphicsSceneResizeEvent*)
{
    for (auto hotspotItem: d->hotspotItems)
        d->updateHotspotItem(hotspotItem);
}

} // namespace nx::vms::client::desktop
