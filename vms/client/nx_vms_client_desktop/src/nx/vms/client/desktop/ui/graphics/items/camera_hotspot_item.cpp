// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_hotspot_item.h"

#include <optional>

#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QStyleOptionGraphicsItem>

#include <core/resource/camera_resource.h>
#include <nx/api/mediaserver/image_request.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/camera_hotspots/camera_hotspots_display_utils.h>
#include <nx/vms/client/desktop/image_providers/camera_thumbnail_provider.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/workbench/widgets/thumbnail_tooltip.h>
#include <ui/workbench/workbench_context.h>

namespace {

static constexpr auto kTooltipOffset = 8;

} // namespace

namespace nx::vms::client::desktop {

//-------------------------------------------------------------------------------------------------
// CameraHotspotItem::Private declaration.

struct CameraHotspotItem::Private
{
    CameraHotspotItem* const q;

    nx::vms::common::CameraHotspotData hotspotData;

    QString tooltipText;
    std::unique_ptr<ThumbnailTooltip> tooltip;
    std::unique_ptr<CameraThumbnailProvider> thumbnailProvider;
};

//-------------------------------------------------------------------------------------------------
// CameraHotspotItem definition.

CameraHotspotItem::CameraHotspotItem(
    const nx::vms::common::CameraHotspotData& hotspotData,
    QGraphicsItem* parent)
    :
    base_type(parent),
    d(new Private({this}))
{
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    d->hotspotData = hotspotData;
}

CameraHotspotItem::~CameraHotspotItem()
{
}

QRectF CameraHotspotItem::boundingRect() const
{
    return camera_hotspots::makeHotspotOutline().boundingRect();
}

nx::vms::common::CameraHotspotData CameraHotspotItem::hotspotData() const
{
    return d->hotspotData;
}

void CameraHotspotItem::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget*)
{
    using namespace nx::vms::client::core;
    using namespace camera_hotspots;

    painter->setRenderHint(QPainter::Antialiasing);

    CameraHotspotDisplayOption hotspotOption;
    hotspotOption.rect = parentItem()->boundingRect().translated(-pos());

    if (option->state.testFlag(QStyle::State_MouseOver))
        hotspotOption.state = camera_hotspots::CameraHotspotDisplayOption::State::hovered;

    hotspotOption.cameraState = CameraHotspotDisplayOption::CameraState::valid;

    auto paintedHotspotData = d->hotspotData;
    if (!qFuzzyIsNull(nx::vms::client::core::Geometry::length(paintedHotspotData.direction)))
    {
        const auto directionLine = QLineF(QPointF(), d->hotspotData.direction);
        const auto rotatedDirectionLine = painter->transform().map(directionLine).unitVector();
        paintedHotspotData.direction =
            rotatedDirectionLine.translated(-rotatedDirectionLine.p1()).p2();
    }

    camera_hotspots::paintHotspot(painter, paintedHotspotData, hotspotOption);
}

void CameraHotspotItem::initTooltip(
    QnWorkbenchContext* context,
    const QnVirtualCameraResourcePtr& camera)
{
    if (d->tooltip)
        return;

    d->tooltip = std::make_unique<ThumbnailTooltip>(context);
    d->tooltipText = camera->getName();

    nx::api::CameraImageRequest request;
    request.camera = camera;
    request.format = nx::api::ImageRequest::ThumbnailFormat::jpg;
    request.aspectRatio = nx::api::ImageRequest::AspectRatio::auto_;
    request.timestampUs = nx::api::ImageRequest::kLatestThumbnail;
    request.roundMethod = nx::api::ImageRequest::RoundMethod::precise;

    d->thumbnailProvider = std::make_unique<CameraThumbnailProvider>(request);
    d->tooltip->setImageProvider(d->thumbnailProvider.get());
}

void CameraHotspotItem::hoverEnterEvent(QGraphicsSceneHoverEvent*)
{
    if (!d->tooltip)
        return;

    const QGraphicsView* view = scene()->views().first();
    const auto tooltipScenePos =
        mapToScene(boundingRect().center() + QPointF(16 + kTooltipOffset, 0));
    const auto tooltipGlobalPos = view->mapToGlobal(view->mapFromScene(tooltipScenePos));

    d->thumbnailProvider->loadAsync();
    d->tooltip->setTarget(tooltipGlobalPos);
    d->tooltip->setText(d->tooltipText);
    d->tooltip->show();
}

void CameraHotspotItem::hoverLeaveEvent(QGraphicsSceneHoverEvent*)
{
    if (d->tooltip)
        d->tooltip->hide();
}

} // namespace nx::vms::client::desktop
