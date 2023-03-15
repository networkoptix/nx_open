// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_hotspot_item.h"

#include <optional>

#include <QtCore/QPointer>
#include <QtGui/QTransform>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsSceneMouseEvent>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QMenu>
#include <QtWidgets/QStyleOptionGraphicsItem>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/api/mediaserver/image_request.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/camera_hotspots/camera_hotspots_display_utils.h>
#include <nx/vms/client/desktop/image_providers/camera_thumbnail_provider.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_parameter_types.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/workbench/widgets/thumbnail_tooltip.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>

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
    QnWorkbenchContext* context;

    QString tooltipText;
    QPointer<QMenu> contextMenu;
    std::unique_ptr<ThumbnailTooltip> tooltip;
    std::unique_ptr<CameraThumbnailProvider> thumbnailProvider;

    void initTooltip();
    QnVirtualCameraResourcePtr cameraResource() const;

    void openItem();
    void opemItemInNewLayout();
    void openItemInPlace();
};

void CameraHotspotItem::Private::initTooltip()
{
    if (tooltip)
        return;

    const auto camera = cameraResource();

    tooltip = std::make_unique<ThumbnailTooltip>(context);
    tooltipText = camera->getName();

    nx::api::CameraImageRequest request;
    request.camera = camera;
    request.format = nx::api::ImageRequest::ThumbnailFormat::jpg;
    request.aspectRatio = nx::api::ImageRequest::AspectRatio::auto_;
    request.timestampUs = nx::api::ImageRequest::kLatestThumbnail;
    request.roundMethod = nx::api::ImageRequest::RoundMethod::precise;

    thumbnailProvider = std::make_unique<CameraThumbnailProvider>(request);
    tooltip->setImageProvider(thumbnailProvider.get());
}

QnVirtualCameraResourcePtr CameraHotspotItem::Private::cameraResource() const
{
    return context->resourcePool()->getResourceById<QnVirtualCameraResource>(hotspotData.cameraId);
}

void CameraHotspotItem::Private::openItem()
{
    const auto actionManager = context->menu();

    const auto workbenchItems = context->workbench()->currentLayout()->items();
    for (const auto item: workbenchItems)
    {
        if (item->resource()->getId() == hotspotData.cameraId)
        {
            if (context->workbench()->item(Qn::ZoomedRole))
                context->workbench()->setItem(Qn::ZoomedRole, nullptr);

            actionManager->trigger(ui::action::GoToLayoutItemAction, ui::action::Parameters()
                .withArgument(Qn::ItemUuidRole, item->uuid()));

            const auto display = context->display();
            if (!display->boundedViewportGeometry().contains(display->itemEnclosingGeometry(item)))
                display->fitInView(true);

            return;
        }
    }

    actionManager->trigger(ui::action::OpenInCurrentLayoutAction, cameraResource());
}

void CameraHotspotItem::Private::opemItemInNewLayout()
{
    context->menu()->trigger(ui::action::OpenInNewTabAction, cameraResource());
}

void CameraHotspotItem::Private::openItemInPlace()
{
    // TODO: #vbreus VMS-38379, implementation is pending.
}

//-------------------------------------------------------------------------------------------------
// CameraHotspotItem definition.

CameraHotspotItem::CameraHotspotItem(
    const nx::vms::common::CameraHotspotData& hotspotData,
    QnWorkbenchContext* context,
    QGraphicsItem* parent)
    :
    base_type(parent),
    d(new Private({this, hotspotData, context}))
{
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons({Qt::LeftButton, Qt::RightButton});

    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIgnoresTransformations, true);

    d->initTooltip();
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

    if (option->state.testFlag(QStyle::State_MouseOver))
        hotspotOption.state = camera_hotspots::CameraHotspotDisplayOption::State::hovered;

    hotspotOption.cameraState = CameraHotspotDisplayOption::CameraState::valid;
    hotspotOption.decoration = qnResIconCache->icon(d->cameraResource());

    auto paintedHotspotData = d->hotspotData;
    if (paintedHotspotData.hasDirection())
    {
        const auto hotspotsOverlayWidget = parentItem();
        const auto mediaResourceWidget = hotspotsOverlayWidget->parentItem();

        QTransform rotationMatrix;
        rotationMatrix.rotate(mediaResourceWidget->rotation());

        paintedHotspotData.direction = rotationMatrix.map(d->hotspotData.direction);
    }

    camera_hotspots::paintHotspot(painter, paintedHotspotData, option->rect.center(), hotspotOption);
}

void CameraHotspotItem::hoverEnterEvent(QGraphicsSceneHoverEvent*)
{
    if (!d->tooltip)
        return;

    const auto hotspotsOverlayWidget = parentItem();
    const auto mediaResourceWidget = hotspotsOverlayWidget->parentItem();

    QTransform rotationMatrix;
    rotationMatrix.rotate(-mediaResourceWidget->rotation());

    const auto tooltipScenePos = mapToScene(boundingRect().center()
        + rotationMatrix.map(QPointF(boundingRect().width() / 2 + kTooltipOffset, 0)));

    const QGraphicsView* view = scene()->views().first();
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

void CameraHotspotItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::RightButton && event->modifiers() == Qt::NoModifier)
    {
        event->accept();

        d->contextMenu = new QMenu();
        QObject::connect(d->contextMenu, &QMenu::aboutToHide, d->contextMenu, &QMenu::deleteLater);

        const auto openAction = d->contextMenu->addAction(tr("Open Camera"));
        QObject::connect(openAction, &QAction::triggered,
            [this] { d->openItem(); });

        const auto openInNewTabAction = d->contextMenu->addAction(tr("Open Camera in new Tab"));
        QObject::connect(openInNewTabAction, &QAction::triggered,
            [this] { d->opemItemInNewLayout(); });

        const auto openInPlaceAction = d->contextMenu->addAction(tr("Open Camera in place"));
        QObject::connect(openInPlaceAction, &QAction::triggered,
            [this] { d->openItemInPlace(); });

        // TODO: #vbreus VMS-38379, implementation is pending, action is unavailable to user for now.
        openInPlaceAction->setEnabled(false);

        QnHiDpiWorkarounds::showMenu(d->contextMenu, QCursor::pos());

        return;
    }

    base_type::mousePressEvent(event);
}

void CameraHotspotItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (!isUnderMouse())
        return;

    if (event->button() == Qt::LeftButton)
    {
        switch (event->modifiers())
        {
            case Qt::KeyboardModifier::ControlModifier:
                d->opemItemInNewLayout();
                event->accept();
                return;

            case Qt::KeyboardModifier::ShiftModifier:
                d->openItemInPlace();
                event->accept();
                return;

            case Qt::KeyboardModifier::NoModifier:
                d->openItem();
                event->accept();
                return;
        }
    }

    base_type::mouseReleaseEvent(event);
}

} // namespace nx::vms::client::desktop
