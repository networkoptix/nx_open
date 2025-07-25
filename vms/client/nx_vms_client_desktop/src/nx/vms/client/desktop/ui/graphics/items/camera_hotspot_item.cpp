// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_hotspot_item.h"

#include <optional>

#include <QtCore/QPointer>
#include <QtGui/QGuiApplication>
#include <QtWidgets/QGraphicsPixmapItem>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsSceneMouseEvent>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QMenu>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/api/mediaserver/image_request.h>
#include <nx/vms/client/core/image_providers/camera_thumbnail_provider.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/camera_hotspots/camera_hotspots_display_utils.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_parameter_types.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/widgets/thumbnail_tooltip.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>

namespace nx::vms::client::desktop {

using namespace camera_hotspots;

namespace {

static constexpr auto kTooltipOffset = kHotspotRadius + 8;
static constexpr auto kInvalidPixmapRole = Qt::UserRole;

} // namespace

//-------------------------------------------------------------------------------------------------
// CameraHotspotItem::Private declaration.

struct CameraHotspotItem::Private
{
    CameraHotspotItem* const q;

    const nx::vms::common::CameraHotspotData hotspotData;

    QPointer<QMenu> contextMenu;
    QPointer<BubbleToolTip> tooltip;

    Private(
        CameraHotspotItem* const q,
        nx::vms::common::CameraHotspotData hotspotData);

    QnMediaResourceWidget* mediaResourceWidget() const;

    QnResourcePtr hotspotResource() const;
    QnVirtualCameraResourcePtr hotspotCamera() const;
    QnLayoutResourcePtr hotspotLayout() const;

    // Preview tooltip.
    nx::api::CameraImageRequest tooltipImageRequest() const;
    QString tooltipText() const;
    QPoint tooltipGlobalPos() const;
    BubbleToolTip* createTooltip() const;

    // Interactive actions.
    menu::Parameters::ArgumentHash itemPlaybackParameters() const;
    menu::Parameters::ArgumentHash layoutPlaybackParameters() const;

    void openItem();
    void openInNewTab();
    void openItemInPlace();

    // Optimized hotspot painting routines.
    void setDevicePixelRatio(qreal value);
    void setHovered(bool value);
    void setDecorationIconKey(QnResourceIconCache::Key value);

    CameraHotspotDisplayOption hotspotDisplayOption() const;
    void updateHotspotPixmapIfNeeded() const;
    void updateHotspotDecorationPixmapIfNeeded() const;

private:
    QGraphicsPixmapItem* const hotspotPixmapItem;
    QGraphicsPixmapItem* hotspotDecorationPixmapItem = nullptr;

    qreal devicePixelRatio = qGuiApp->devicePixelRatio();
    bool hovered = false;
    QnResourceIconCache::Key decorationIconKey;
};

//-------------------------------------------------------------------------------------------------
// CameraHotspotItem::Private definition.

CameraHotspotItem::Private::Private(
    CameraHotspotItem* const q,
    common::CameraHotspotData hotspotData)
    :
    q(q),
    hotspotData(hotspotData),
    hotspotPixmapItem(new QGraphicsPixmapItem(q))
{
    if (hotspotData.hasDirection())
    {
        hotspotPixmapItem->setTransformationMode(Qt::SmoothTransformation);
        hotspotDecorationPixmapItem = new QGraphicsPixmapItem(hotspotPixmapItem);
        hotspotDecorationPixmapItem->setFlag(QGraphicsItem::ItemIgnoresTransformations);
    }
    else
    {
        hotspotPixmapItem->setFlag(QGraphicsItem::ItemIgnoresTransformations);
    }
}

QnMediaResourceWidget* CameraHotspotItem::Private::mediaResourceWidget() const
{
    const auto hotspotsOverlayWidget = q->parentItem();
    return dynamic_cast<QnMediaResourceWidget*>(hotspotsOverlayWidget->parentItem());
}

QnResourcePtr CameraHotspotItem::Private::hotspotResource() const
{
    const auto resourcePool = q->resourcePool();
    const auto resource = resourcePool->getResourceById(hotspotData.targetResourceId);
    NX_ASSERT(resource, "Hotspot refers to a nonexistent resource");
    return resource;
}

QnVirtualCameraResourcePtr CameraHotspotItem::Private::hotspotCamera() const
{
    return hotspotResource().dynamicCast<QnVirtualCameraResource>();
}

QnLayoutResourcePtr CameraHotspotItem::Private::hotspotLayout() const
{
    return hotspotResource().dynamicCast<QnLayoutResource>();
}

nx::api::CameraImageRequest CameraHotspotItem::Private::tooltipImageRequest() const
{
    const auto resourceWidget = mediaResourceWidget();

    nx::api::CameraImageRequest request;
    request.camera = hotspotCamera();
    request.format = nx::api::ImageRequest::ThumbnailFormat::jpg;
    request.aspectRatio = nx::api::ImageRequest::AspectRatio::auto_;

    request.timestampMs = resourceWidget->isPlayingLive()
        ? nx::api::ImageRequest::kLatestThumbnail
        : resourceWidget->position();

    request.roundMethod = nx::api::ImageRequest::RoundMethod::precise;
    request.tolerant = true;

    return request;
}

QString CameraHotspotItem::Private::tooltipText() const
{
    const auto resource = hotspotResource();
    return resource ? resource->getName() : QString();
}

QPoint CameraHotspotItem::Private::tooltipGlobalPos() const
{
    const auto* graphicsView = q->scene()->views().first();
    const auto tooltipScenePos = q->mapToScene(q->boundingRect().center());
    const auto tooltipViewPos = graphicsView->mapFromScene(tooltipScenePos);
    const auto tooltipGlobalPos = graphicsView->mapToGlobal(tooltipViewPos);
    return tooltipGlobalPos;
}

BubbleToolTip* CameraHotspotItem::Private::createTooltip() const
{
    if (auto layout = hotspotLayout())
    {
        const auto tooltip = new BubbleToolTip(q->windowContext());
        tooltip->setTooltipOffset(kTooltipOffset);
        tooltip->setEnclosingRect(q->scene()->views().first()->window()->geometry());
        tooltip->setText(tooltipText());
        tooltip->setTarget(tooltipGlobalPos());
        return tooltip;
    };

    const auto imageRequest = tooltipImageRequest();
    if (!imageRequest.camera)
        return nullptr;

    const auto tooltip = new ThumbnailTooltip(q->windowContext());
    tooltip->setTooltipOffset(kTooltipOffset);
    tooltip->setEnclosingRect(q->scene()->views().first()->window()->geometry());
    tooltip->setText(tooltipText());
    tooltip->setTarget(tooltipGlobalPos());

    const auto thumbnailProvider = new core::CameraThumbnailProvider(imageRequest, tooltip);
    tooltip->setImageProvider(thumbnailProvider);
    thumbnailProvider->loadAsync();

    return tooltip;
}

menu::Parameters::ArgumentHash CameraHotspotItem::Private::itemPlaybackParameters() const
{
    using namespace std::chrono;

    menu::Parameters::ArgumentHash result;

    const auto playbackSpeed = mediaResourceWidget()->speed();
    const auto positionMs = mediaResourceWidget()->isPlayingLive()
        ? DATETIME_NOW
        : duration_cast<milliseconds>(mediaResourceWidget()->position()).count();

    result.insert(Qn::ItemTimeRole, positionMs);
    result.insert(Qn::ItemSpeedRole, playbackSpeed);
    result.insert(Qn::ItemPausedRole, qFuzzyIsNull(playbackSpeed));

    return result;
}

menu::Parameters::ArgumentHash CameraHotspotItem::Private::layoutPlaybackParameters() const
{
    using namespace std::chrono;

    menu::Parameters::ArgumentHash result;
    StreamSynchronizationState state{
        .isSyncOn = q->navigator()->syncEnabled(),
        .timeUs = mediaResourceWidget()->isPlayingLive()
            ? DATETIME_NOW
            : duration_cast<microseconds>(mediaResourceWidget()->position()).count(),
        .speed = mediaResourceWidget()->speed()};

    result.insert(Qn::LayoutSyncStateRole, QVariant::fromValue(state));

    return result;
}

void CameraHotspotItem::Private::openItem()
{
    const auto actionManager = q->menu();

    const auto workbenchItems = q->workbench()->currentLayout()->items();
    const bool isCurrentItemZoomed = q->workbench()->item(Qn::ZoomedRole) != nullptr;

    for (const auto item: workbenchItems)
    {
        if (item->resource()->getId() == hotspotData.targetResourceId)
        {
            actionManager->trigger(menu::GoToLayoutItemAction, menu::Parameters()
                .withArgument(Qn::ItemUuidRole, item->uuid())
                .withArgument(Qn::ItemAddInZoomedStateRole, isCurrentItemZoomed));

            if (!q->navigator()->syncEnabled())
            {
                if (mediaResourceWidget()->isPlayingLive())
                {
                    actionManager->trigger(menu::JumpToLiveAction, q->navigator()->currentWidget());
                }
                else
                {
                    const auto parameters = itemPlaybackParameters();
                    for (const auto& role: parameters.keys())
                        item->setData(static_cast<Qn::ItemDataRole>(role), parameters.value(role));
                }
            }

            const auto display = q->display();
            if (!display->boundedViewportGeometry().contains(display->itemEnclosingGeometry(item)))
                display->fitInView(true);

            return;
        }
    }

    auto parameters = menu::Parameters(hotspotCamera())
        .withArgument(Qn::ItemAddInZoomedStateRole, isCurrentItemZoomed);
    if (!q->navigator()->syncEnabled())
        parameters.setArguments(itemPlaybackParameters());

    actionManager->trigger(menu::OpenInCurrentLayoutAction, parameters);
}

void CameraHotspotItem::Private::openInNewTab()
{
    auto parameters = menu::Parameters(hotspotResource());
    parameters.setArguments(layoutPlaybackParameters());
    q->menu()->trigger(menu::OpenInNewTabAction, parameters);
}

void CameraHotspotItem::Private::openItemInPlace()
{
    const bool isCurrentItemZoomed = q->workbench()->item(Qn::ZoomedRole) != nullptr;

    auto parameters = menu::Parameters(mediaResourceWidget())
        .withArgument(core::ResourceRole, hotspotCamera())
        .withArgument(Qn::ItemAddInZoomedStateRole, isCurrentItemZoomed);

    if (!q->navigator()->syncEnabled()
        || q->workbench()->currentLayout()->items().size() == 1)
    {
        parameters.setArguments(itemPlaybackParameters());
    }

    q->menu()->trigger(menu::ReplaceLayoutItemAction, parameters);
}

void CameraHotspotItem::Private::setDevicePixelRatio(qreal value)
{
    if (qFuzzyEquals(devicePixelRatio, value))
        return;

    devicePixelRatio = value;

    hotspotPixmapItem->setData(kInvalidPixmapRole, true);
    if (hotspotDecorationPixmapItem)
        hotspotDecorationPixmapItem->setData(kInvalidPixmapRole, true);
}

void CameraHotspotItem::Private::setHovered(bool value)
{
    if (hovered == value)
        return;

    hovered = value;

    hotspotPixmapItem->setData(kInvalidPixmapRole, true);
}

void CameraHotspotItem::Private::setDecorationIconKey(QnResourceIconCache::Key value)
{
    if (decorationIconKey == value)
        return;

    decorationIconKey = value;

    if (hotspotDecorationPixmapItem)
        hotspotDecorationPixmapItem->setData(kInvalidPixmapRole, true);
    else
        hotspotPixmapItem->setData(kInvalidPixmapRole, true);
}

CameraHotspotDisplayOption CameraHotspotItem::Private::hotspotDisplayOption() const
{
    CameraHotspotDisplayOption hotspotOption;

    hotspotOption.state = q->isUnderMouse()
        ? CameraHotspotDisplayOption::State::hovered
        : CameraHotspotDisplayOption::State::none;
    hotspotOption.decoration = qnResIconCache->icon(decorationIconKey);

    return hotspotOption;
}

void CameraHotspotItem::Private::updateHotspotPixmapIfNeeded() const
{
    const auto invalidVar = hotspotPixmapItem->data(kInvalidPixmapRole);
    if (!invalidVar.isNull() && !invalidVar.toBool())
        return;

    auto displayOption = hotspotDisplayOption();
    if (hotspotDecorationPixmapItem)
        displayOption.displayedComponents = CameraHotspotDisplayOption::Component::body;

    const auto pixmap = paintHotspotPixmap(hotspotData, displayOption, devicePixelRatio);
    hotspotPixmapItem->setPixmap(pixmap);
    hotspotPixmapItem->setOffset(-QRectF({}, pixmap.deviceIndependentSize()).center());
    hotspotPixmapItem->setData(kInvalidPixmapRole, false);
}

void CameraHotspotItem::Private::updateHotspotDecorationPixmapIfNeeded() const
{
    if (!hotspotDecorationPixmapItem)
        return;

    const auto invalidVar = hotspotDecorationPixmapItem->data(kInvalidPixmapRole);
    if (!invalidVar.isNull() && !invalidVar.toBool())
        return;

    auto displayOption = hotspotDisplayOption();
    displayOption.displayedComponents = CameraHotspotDisplayOption::Component::decoration;

    const auto pixmap = paintHotspotPixmap(hotspotData, displayOption, devicePixelRatio);
    hotspotDecorationPixmapItem->setPixmap(pixmap);
    hotspotDecorationPixmapItem->setOffset(-QRectF({}, pixmap.deviceIndependentSize()).center());
    hotspotDecorationPixmapItem->setData(kInvalidPixmapRole, false);
}

//-------------------------------------------------------------------------------------------------
// CameraHotspotItem definition.

CameraHotspotItem::CameraHotspotItem(
    const nx::vms::common::CameraHotspotData& hotspotData,
    SystemContext* systemContext,
    WindowContext* windowContext,
    QGraphicsItem* parent)
    :
    base_type(parent),
    SystemContextAware(systemContext),
    WindowContextAware(windowContext),
    d(new Private({this, hotspotData}))
{
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons({Qt::LeftButton, Qt::RightButton});

    setProperty(Qn::BlockMotionSelection, true);
    setProperty(Qn::NoHandScrollOver, true);
}

CameraHotspotItem::~CameraHotspotItem()
{
    if (d->tooltip)
        d->tooltip->deleteLater();
}

QRectF CameraHotspotItem::boundingRect() const
{
    return makeHotspotOutline().boundingRect();
}

nx::vms::common::CameraHotspotData CameraHotspotItem::hotspotData() const
{
    return d->hotspotData;
}

void CameraHotspotItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    d->setDevicePixelRatio(painter->device()->devicePixelRatioF());
    d->setHovered(isUnderMouse());
    d->setDecorationIconKey(qnResIconCache->key(d->hotspotResource()));

    d->updateHotspotPixmapIfNeeded();
    d->updateHotspotDecorationPixmapIfNeeded();
}

void CameraHotspotItem::hoverEnterEvent(QGraphicsSceneHoverEvent*)
{
    if (!NX_ASSERT(!d->tooltip))
        return;

    d->tooltip = d->createTooltip();
    if (!d->tooltip)
        return;

    connect(d->tooltip, &ThumbnailTooltip::stateChanged,
        [tooltip = d->tooltip](ThumbnailTooltip::State state)
        {
            if (state == ThumbnailTooltip::State::hidden)
                tooltip->deleteLater();
        });

    d->tooltip->show();
}

void CameraHotspotItem::hoverLeaveEvent(QGraphicsSceneHoverEvent*)
{
    if (d->tooltip)
        d->tooltip->hide();
}

void CameraHotspotItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        event->accept();
        return;
    }
    else if (event->button() == Qt::RightButton && event->modifiers() == Qt::NoModifier)
    {
        event->accept();

        d->contextMenu = new QMenu();
        connect(d->contextMenu, &QMenu::aboutToHide, d->contextMenu, &QMenu::deleteLater);

        if (d->hotspotCamera())
        {
            const auto openAction = d->contextMenu->addAction(tr("Open Camera"));
            connect(openAction, &QAction::triggered, this, [this] { d->openItem(); });

            const auto openInNewTabAction =
                d->contextMenu->addAction(tr("Open Camera in new Tab"));
            connect(openInNewTabAction, &QAction::triggered, this, [this] { d->openInNewTab(); });

            const auto openInPlaceAction = d->contextMenu->addAction(tr("Open Camera in place"));
            connect(openInPlaceAction, &QAction::triggered,
                this, [this] { d->openItemInPlace(); });
        }
        else if (d->hotspotLayout())
        {
            const auto openInNewTabAction =
                d->contextMenu->addAction(tr("Open Layout in new Tab"));
            connect(openInNewTabAction, &QAction::triggered, this, [this] { d->openInNewTab(); });
        }

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
        if (d->hotspotCamera())
        {
            switch (event->modifiers())
            {
                case Qt::KeyboardModifier::ControlModifier:
                    d->openInNewTab();
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

                default:
                    break;
            }
        }
        else if (d->hotspotLayout())
        {
            if (event->modifiers() == Qt::KeyboardModifier::NoModifier)
            {
                d->openInNewTab();
                event->accept();
                return;
            }
        }
    }

    base_type::mouseReleaseEvent(event);
}

QVariant CameraHotspotItem::itemChange(
    QGraphicsItem::GraphicsItemChange change,
    const QVariant& value)
{
    if (change == QGraphicsItem::ItemVisibleChange && !value.toBool() && d->tooltip)
        d->tooltip->hide();

    return base_type::itemChange(change, value);
}

} // namespace nx::vms::client::desktop
