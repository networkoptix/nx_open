#include "ptz_instrument.h"
#include "ptz_instrument_p.h"

#include <QtCore/QVariant>

#include <QtWidgets/QGraphicsSceneMouseEvent>
#include <QtWidgets/QApplication>

#include <utils/common/checked_cast.h>
#include <utils/common/scoped_painter_rollback.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/client/core/utils/geometry.h>
#include <utils/math/math.h>
#include <utils/math/color_transformations.h>

#include <core/resource/client_camera.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/vms/api/data/dewarping_data.h>
#include <core/ptz/media_dewarping_params.h>

#include <ui/animation/opacity_animator.h>
#include <ui/animation/animation_event.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/graphics_message_box.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_context.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <ui/workbench/workbench_layout.h>

#include <nx/vms/client/desktop/ini.h>

using nx::vms::client::core::Geometry;

using namespace nx::core;
using namespace nx::vms::client::desktop;

namespace {

static const QVector3D kKeyboardPtzSensitivity(/*pan*/ 0.1, /*tilt*/ 0.1, /*zoom*/ 1.0);

const qreal instantSpeedUpdateThreshold = 0.1;
const qreal speedUpdateThreshold = 0.001;
const int speedUpdateIntervalMSec = 500;

const double minPtzZoomRectSize = 0.08;

const qreal itemUnzoomThreshold = 0.975; /* In sync with hardcoded constant in workbench_controller */ // TODO: #Elric

// Key to store if we already displayed ptz redirect warning on the current layout.
static const char* kPtzRedirectLockedLayoutWarningKey = "__ptz_redirect_locked_warning";

nx::core::ptz::Vector truncate(const nx::core::ptz::Vector& value)
{
    return nx::core::ptz::Vector(
        trunc(value.pan * 100) / 100.0,
        trunc(value.tilt * 100) / 100.0,
        trunc(value.rotation * 100) / 100.0,
        trunc(value.zoom * 100) / 100.0);
}

Qt::Orientations capabilitiesToMode(Ptz::Capabilities capabilities)
{
    Qt::Orientations result = 0;
    bool isFisheye = capabilities.testFlag(Ptz::VirtualPtzCapability);
    if (isFisheye)
        return result;

    if (capabilities.testFlag(Ptz::ContinuousPanCapability))
        result |= Qt::Horizontal;

    if (capabilities.testFlag(Ptz::ContinuousTiltCapability))
        result |= Qt::Vertical;

    return result;
}

PtzInstrument::DirectionFlag oppositeDirection(PtzInstrument::DirectionFlag direction)
{
    switch (direction)
    {
        case PtzInstrument::DirectionFlag::panLeft:
            return PtzInstrument::DirectionFlag::panRight;
        case PtzInstrument::DirectionFlag::panRight:
            return PtzInstrument::DirectionFlag::panLeft;
        case PtzInstrument::DirectionFlag::tiltUp:
            return PtzInstrument::DirectionFlag::tiltDown;
        case PtzInstrument::DirectionFlag::tiltDown:
            return PtzInstrument::DirectionFlag::tiltUp;
        case PtzInstrument::DirectionFlag::zoomIn:
            return PtzInstrument::DirectionFlag::zoomOut;
        case PtzInstrument::DirectionFlag::zoomOut:
            return PtzInstrument::DirectionFlag::zoomIn;
    };

    NX_ASSERT(false);
    return {};
};

// TODO: Remove code duplication with QnWorkbenchPtzHandler.
QVector3D applyRotation(const QVector3D& speed, qreal rotation)
{
    QVector3D transformedSpeed = speed;

    rotation = static_cast<qint64>(rotation >= 0 ? rotation + 0.5 : rotation - 0.5) % 360;
    if (rotation < 0)
        rotation += 360;

    if (rotation >= 45 && rotation < 135)
    {
        transformedSpeed.setX(-speed.y());
        transformedSpeed.setY(speed.x());
    }
    else if (rotation >= 135 && rotation < 225)
    {
        transformedSpeed.setX(-speed.x());
        transformedSpeed.setY(-speed.y());
    }
    else if (rotation >= 225 && rotation < 315)
    {
        transformedSpeed.setX(speed.y());
        transformedSpeed.setY(-speed.x());
    }

    return transformedSpeed;
}

} // namespace

class PtzInstrument::MovementFilter: public QObject
{
    using base_type = QObject;

public:
    MovementFilter(
        QnMediaResourceWidget* widget,
        PtzInstrument* parent);

    virtual ~MovementFilter();

    void updateFilteringSpeed(const nx::core::ptz::Vector& speed);

    void stopMovement();

private:
    void setMovementSpeed(const nx::core::ptz::Vector& speed);

    void onTimeout();

private:
    QPointer<PtzInstrument> const m_parent;
    QPointer<QnMediaResourceWidget> const m_widget;
    QTimer m_filteringTimer;
    nx::core::ptz::Vector m_targetSpeed;
    nx::core::ptz::Vector m_filteringSpeed;
};

PtzInstrument::MovementFilter::MovementFilter(
    QnMediaResourceWidget* widget,
    PtzInstrument* parent)
    :
    m_parent(parent),
    m_widget(widget)
{
    static constexpr int kFilteringTimerIntervalMs = 150;
    m_filteringTimer.setInterval(kFilteringTimerIntervalMs);
    connect(&m_filteringTimer, &QTimer::timeout, this, &MovementFilter::onTimeout);
}

PtzInstrument::MovementFilter::~MovementFilter()
{
    m_filteringTimer.stop();
    setMovementSpeed(nx::core::ptz::Vector());
}

void PtzInstrument::MovementFilter::onTimeout()
{
    setMovementSpeed(m_filteringSpeed);
}

void PtzInstrument::MovementFilter::stopMovement()
{
    updateFilteringSpeed(nx::core::ptz::Vector());
}

void PtzInstrument::MovementFilter::updateFilteringSpeed(const nx::core::ptz::Vector& speed)
{
    if (speed.isNull())
        setMovementSpeed(m_filteringSpeed);

    const auto truncatedSpeed = truncate(speed);
    if (m_filteringSpeed == truncatedSpeed)
        return;

    m_filteringSpeed = truncatedSpeed;
    m_filteringTimer.start(); //< Restarts if it's started already.
}

void PtzInstrument::MovementFilter::setMovementSpeed(const nx::core::ptz::Vector& speed)
{
    if (speed == m_targetSpeed)
        return;

    if (!m_widget || !m_parent)
        return;

    m_targetSpeed = speed;
    m_parent->ptzMove(m_widget, m_targetSpeed);
}

// -------------------------------------------------------------------------- //
// PtzInstrument
// -------------------------------------------------------------------------- //
PtzInstrument::PtzInstrument(QObject *parent):
    base_type(
        makeSet(QEvent::MouseButtonPress, AnimationEvent::Animation),
        makeSet(),
        makeSet(),
        makeSet(
            QEvent::GraphicsSceneMousePress,
            QEvent::GraphicsSceneMouseMove,
            QEvent::GraphicsSceneMouseDoubleClick,
            QEvent::GraphicsSceneMouseRelease),
        parent
    ),
    QnWorkbenchContextAware(parent),
    m_clickDelayMSec(QApplication::doubleClickInterval()),
    m_expansionSpeed(qnGlobals->workbenchUnitSize() / 5.0),
    m_movement(NoMovement)
{
    connect(resourcePool(), &QnResourcePool::statusChanged, this,
        [this](const QnResourcePtr& resource, Qn::StatusChangeReason /*reason*/)
        {
            if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
            {
                const bool wasInaccessible =
                    resource->getPreviousStatus() == Qn::Offline ||
                    resource->getPreviousStatus() == Qn::Unauthorized;

                const bool isAccessible =
                        resource->getStatus() == Qn::Online ||
                        resource->getStatus() == Qn::Recording;

                if (wasInaccessible && isAccessible)
                {
                    for (const auto& widget: display()->widgets(camera))
                    {
                        // In the showreel preview mode we can have non-media widgets with a camera.
                        if (auto mediaWidget = qobject_cast<QnMediaResourceWidget*>(widget))
                            updateTraits(mediaWidget);
                    }
                }
            }
        });

    connect(display(), &QnWorkbenchDisplay::widgetAboutToBeChanged,
        this, &PtzInstrument::at_display_widgetAboutToBeChanged);
}

PtzInstrument::~PtzInstrument()
{
    ensureUninstalled();
}

QnMediaResourceWidget* PtzInstrument::target() const
{
    return m_target.data();
}

void PtzInstrument::setTarget(QnMediaResourceWidget* target)
{
    if (m_target)
        disconnect(m_target, &QGraphicsWidget::visibleChanged, this, &PtzInstrument::resetIfTargetIsInvisible);

    m_target = target;

    if (m_target)
        connect(m_target, &QGraphicsWidget::visibleChanged, this, &PtzInstrument::resetIfTargetIsInvisible);
}

void PtzInstrument::resetIfTargetIsInvisible()
{
    if (m_target && m_target->isVisible())
        return;

    reset();
    setTarget(nullptr);
}

PtzManipulatorWidget* PtzInstrument::targetManipulator() const
{
    return overlayWidget(target())->manipulatorWidget();
}

QnSplashItem* PtzInstrument::newSplashItem(QGraphicsItem* parentItem)
{
    QnSplashItem *result;
    if (!m_freeAnimations.empty())
    {
        result = m_freeAnimations.back().item;
        m_freeAnimations.pop_back();
        result->setParentItem(parentItem);
    }
    else
    {
        result = new PtzSplashItem(parentItem);
        connect(result, &QObject::destroyed, this, &PtzInstrument::at_splashItem_destroyed);
    }

    result->setOpacity(0.0);
    result->show();
    return result;
}

PtzOverlayWidget* PtzInstrument::overlayWidget(QnMediaResourceWidget* widget) const
{
    return m_dataByWidget[widget].overlayWidget;
}

PtzOverlayWidget* PtzInstrument::ensureOverlayWidget(QnMediaResourceWidget* widget)
{
    PtzData& data = m_dataByWidget[widget];

    if (data.overlayWidget)
        return data.overlayWidget;

    bool isFisheye = data.hasCapabilities(Ptz::VirtualPtzCapability);
    bool isFisheyeEnabled = widget->dewarpingParams().enabled;

    PtzOverlayWidget *overlay = new PtzOverlayWidget();
    overlay->setOpacity(0.0);
    overlay->manipulatorWidget()->setCursor(Qt::SizeAllCursor);
    overlay->zoomInButton()->setTarget(widget);
    overlay->zoomOutButton()->setTarget(widget);
    overlay->focusInButton()->setTarget(widget);
    overlay->focusOutButton()->setTarget(widget);
    overlay->focusAutoButton()->setTarget(widget);
    overlay->modeButton()->setTarget(widget);
    overlay->modeButton()->setVisible(isFisheye && isFisheyeEnabled);

    overlay->setMarkersMode(ini().oldPtzAimOverlay
        ? capabilitiesToMode(data.capabilities)
        : Qt::Orientations());

    data.overlayWidget = overlay;

    connect(overlay->zoomInButton(), &QnImageButtonWidget::pressed, this,
        &PtzInstrument::at_zoomInButton_pressed);
    connect(overlay->zoomInButton(), &QnImageButtonWidget::released, this,
        &PtzInstrument::at_zoomInButton_released);
    connect(overlay->zoomOutButton(), &QnImageButtonWidget::pressed, this,
        &PtzInstrument::at_zoomOutButton_pressed);
    connect(overlay->zoomOutButton(), &QnImageButtonWidget::released, this,
        &PtzInstrument::at_zoomOutButton_released);
    connect(overlay->focusInButton(), &QnImageButtonWidget::pressed, this,
        &PtzInstrument::at_focusInButton_pressed);
    connect(overlay->focusInButton(), &QnImageButtonWidget::released, this,
        &PtzInstrument::at_focusInButton_released);
    connect(overlay->focusOutButton(), &QnImageButtonWidget::pressed, this,
        &PtzInstrument::at_focusOutButton_pressed);
    connect(overlay->focusOutButton(), &QnImageButtonWidget::released, this,
        &PtzInstrument::at_focusOutButton_released);
    connect(overlay->focusAutoButton(), &QnImageButtonWidget::clicked, this,
        &PtzInstrument::at_focusAutoButton_clicked);
    connect(overlay->modeButton(), &QnImageButtonWidget::clicked, this,
        &PtzInstrument::at_modeButton_clicked);

    widget->addOverlayWidget(overlay, {QnResourceWidget::Invisible,
        QnResourceWidget::OverlayFlag::autoRotate, QnResourceWidget::TopControlsLayer});

    return overlay;
}

void PtzInstrument::handlePtzRedirect(QnMediaResourceWidget* widget)
{
    const auto camera = widget->resource().dynamicCast<QnClientCameraResource>();
    if (!camera || !camera->isPtzRedirected())
        return;

    const auto dynamicSensor = camera->ptzRedirectedTo();
    if (!dynamicSensor)
        return;

    // Add dynamic sensor on the current layout if possible (near the static one).
    if (!ensurePtzRedirectedCameraIsOnLayout(widget, dynamicSensor))
        return;

    // Switch actual sensor widget to the PTZ mode.
    for (const auto ptzTargetWidget: display()->widgets(dynamicSensor))
	{
        auto mediaWidget = qobject_cast<QnMediaResourceWidget*>(ptzTargetWidget);
        NX_ASSERT(mediaWidget);
        if (mediaWidget)
            mediaWidget->setPtzMode(true);
	}

    // If current widget is zoomed, zoom target widget instead.
    if (display()->widget(Qn::ZoomedRole) == widget)
    {
        const auto ptzTargetItems = workbench()->currentLayout()->items(dynamicSensor);
        if (!ptzTargetItems.empty())
            workbench()->setItem(Qn::ZoomedRole, *ptzTargetItems.cbegin());
    }
}

bool PtzInstrument::ensurePtzRedirectedCameraIsOnLayout(
    QnMediaResourceWidget* staticSensor,
	const QnVirtualCameraResourcePtr& dynamicSensor)
{
    const auto currentLayout = workbench()->currentLayout();
    if (!currentLayout->items(dynamicSensor).empty())
        return true;

    if (!currentLayout->resource())
        return false;

    // Check if layout is locked and show warning if needed.
    if (currentLayout->locked())
    {
        const bool warned = currentLayout->property(kPtzRedirectLockedLayoutWarningKey).toBool();
        if (!warned)
        {
            QnGraphicsMessageBox::information(tr("Layout is locked"));
            currentLayout->setProperty(kPtzRedirectLockedLayoutWarningKey, true);
        }
        return false;
    }

    // Drop it as near to source camera as possible.
    ui::action::Parameters parameters(dynamicSensor);
    parameters.setArgument(Qn::ItemPositionRole,
        qVariantFromValue(QRectF(staticSensor->item()->geometry()).center()));

    return menu()->triggerIfPossible(ui::action::OpenInCurrentLayoutAction, parameters);
}

bool PtzInstrument::processMousePress(QGraphicsItem* item, QGraphicsSceneMouseEvent* event)
{
    m_clickTimer.stop();

    if (event->button() != Qt::LeftButton)
    {
        m_skipNextAction = true; /* Interrupted by RMB? Do nothing. */
        reset();
        return false;
    }

    const auto target = checked_cast<QnMediaResourceWidget*>(item);
    if (!target->options().testFlag(QnResourceWidget::ControlPtz))
        return false;

    if (!target->rect().contains(event->pos()))
        return false; /* Ignore clicks on widget frame. */

    PtzManipulatorWidget* manipulator = nullptr;
    if (const auto overlay = overlayWidget(target))
    {
        manipulator = overlay->manipulatorWidget();
        if (!manipulator->isVisible()
            || !manipulator->rect().contains(manipulator->mapFromItem(item, event->pos())))
        {
            manipulator = nullptr;
        }
    }

    const PtzData& data = m_dataByWidget[target];
    if (manipulator)
    {
        m_movement = ContinuousMovement;
    }
    else
    {
        if (data.hasCapabilities(Ptz::VirtualPtzCapability
            | Ptz::AbsolutePtzCapabilities
            | Ptz::LogicalPositioningPtzCapability))
        {
            m_movement = VirtualMovement;
        }
        else if (data.hasCapabilities(Ptz::ViewportPtzCapability))
        {
            m_movement = ViewportMovement;
        }
        else if (!ini().oldPtzAimOverlay && (data.hasCapabilities(Ptz::ContinuousPanCapability)
            || data.hasCapabilities(Ptz::ContinuousTiltCapability)))
        {
            m_movement = ContinuousMovement;
        }
        else
        {
            m_movement = NoMovement;
            return false;
        }
    }

    if (m_movement == ContinuousMovement)
    {
        m_movementOrientations = 0;
        if (data.hasCapabilities(Ptz::ContinuousPanCapability))
            m_movementOrientations |= Qt::Horizontal;
        if (data.hasCapabilities(Ptz::ContinuousTiltCapability))
            m_movementOrientations |= Qt::Vertical;
    }

    m_skipNextAction = false;
    setTarget(target);

    return true;
}

FixedArSelectionItem* PtzInstrument::selectionItem() const
{
    return m_selectionItem.data();
}

void PtzInstrument::ensureSelectionItem()
{
    if (selectionItem())
        return;

    m_selectionItem = new PtzSelectionItem();
    selectionItem()->setOpacity(0.0);
    selectionItem()->setElementSize(qnGlobals->workbenchUnitSize() / 64.0);
    selectionItem()->setOptions(
        FixedArSelectionItem::DrawCentralElement | FixedArSelectionItem::DrawSideElements);

    if (scene())
        scene()->addItem(selectionItem());
}

PtzElementsWidget* PtzInstrument::elementsWidget() const
{
    return m_elementsWidget.data();
}

void PtzInstrument::ensureElementsWidget()
{
    if (elementsWidget())
        return;

    m_elementsWidget = new PtzElementsWidget();
    display()->setLayer(elementsWidget(), QnWorkbenchDisplay::EffectsLayer);

    if (scene())
        scene()->addItem(elementsWidget());
}

void PtzInstrument::updateOverlayWidget()
{
    updateOverlayWidgetInternal(checked_cast<QnMediaResourceWidget*>(sender()));
}

void PtzInstrument::updateWidgetPtzController(QnMediaResourceWidget* widget)
{
    PtzData& data = m_dataByWidget[widget];
    data.capabilitiesConnection = connect(widget->ptzController(),
        &QnAbstractPtzController::changed,
        this,
        [this, widget](Qn::PtzDataFields fields)
        {
            if (fields.testFlag(Qn::CapabilitiesPtzField))
                updateCapabilities(widget);
            if (fields.testFlag(Qn::AuxiliaryTraitsPtzField))
                updateTraits(widget);
        });

    updateCapabilities(widget);
    updateTraits(widget);
    updateOverlayWidgetInternal(widget);
}

void PtzInstrument::updateOverlayWidgetInternal(QnMediaResourceWidget* widget)
{
    const bool ptzModeEnabled = widget->options().testFlag(QnResourceWidget::ControlPtz);
    const bool animate = display()->animationAllowed();

    if (ptzModeEnabled)
        ensureOverlayWidget(widget);

    const QnResourceWidget::OverlayVisibility visibility = ptzModeEnabled
        ? QnResourceWidget::AutoVisible
        : QnResourceWidget::Invisible;

    if (auto overlayWidget = this->overlayWidget(widget))
    {
        widget->setOverlayWidgetVisibility(overlayWidget, visibility, animate);

        const PtzData& data = m_dataByWidget[widget];

        const bool isFisheye = data.hasCapabilities(Ptz::VirtualPtzCapability);
        const bool isFisheyeEnabled = widget->dewarpingParams().enabled;

        const bool canMove = data.hasCapabilities(Ptz::ContinuousPanCapability)
            || data.hasCapabilities(Ptz::ContinuousTiltCapability);
        const bool hasZoom = data.hasCapabilities(Ptz::ContinuousZoomCapability);
        const bool hasFocus = data.hasCapabilities(Ptz::ContinuousFocusCapability);
        const bool hasAutoFocus = data.traits.contains(Ptz::ManualAutoFocusPtzTrait);
        const bool hasViewportMode = data.hasCapabilities(Ptz::ViewportPtzCapability);
        const bool showManipulator = canMove && ini().oldPtzAimOverlay;

        overlayWidget->manipulatorWidget()->setVisible(showManipulator);
        overlayWidget->zoomInButton()->setVisible(hasZoom);
        overlayWidget->zoomOutButton()->setVisible(hasZoom);
        overlayWidget->focusInButton()->setVisible(hasFocus);
        overlayWidget->focusOutButton()->setVisible(hasFocus);

        if (hasViewportMode)
            overlayWidget->setCursor(Qt::CrossCursor);
        else if (canMove && !showManipulator )
            overlayWidget->setCursor(Qt::SizeAllCursor);
        else
            overlayWidget->unsetCursor();

        const bool autoFocusWasVisible = overlayWidget->focusAutoButton()->isVisible();
        overlayWidget->focusAutoButton()->setVisible(hasAutoFocus);

        if (isFisheye)
        {
            int panoAngle = widget->item()
                ? 90 * widget->item()->dewarpingParams().panoFactor
                : 90;
            overlayWidget->modeButton()->setText(QString::number(panoAngle));
        }

        overlayWidget->modeButton()->setVisible(isFisheye && isFisheyeEnabled);
        overlayWidget->setMarkersMode(ini().oldPtzAimOverlay
            ? capabilitiesToMode(data.capabilities)
            : Qt::Orientations());

        if (autoFocusWasVisible != hasAutoFocus)
            overlayWidget->forceUpdateLayout();
    }
}

void PtzInstrument::updateCapabilities(QnMediaResourceWidget* widget)
{
    PtzData& data = m_dataByWidget[widget];

    Ptz::Capabilities capabilities = widget->ptzController()->getCapabilities();
    if (data.capabilities == capabilities)
        return;

    if ((data.capabilities ^ capabilities) & Ptz::AuxiliaryPtzCapability)
        updateTraits(widget);

    data.capabilities = capabilities;
    updateOverlayWidgetInternal(widget);
}

void PtzInstrument::updateTraits(QnMediaResourceWidget* widget)
{
    PtzData& data = m_dataByWidget[widget];

    QnPtzAuxiliaryTraitList traits;
    widget->ptzController()->getAuxiliaryTraits(&traits);
    if (data.traits == traits)
        return;

    data.traits = traits;
    updateOverlayWidgetInternal(widget);
}

void PtzInstrument::ptzMoveTo(QnMediaResourceWidget* widget, const QPointF& pos)
{
    ptzMoveTo(widget, QRectF(pos - Geometry::toPoint(widget->size() / 2), widget->size()));
}

void PtzInstrument::ptzMoveTo(QnMediaResourceWidget* widget, const QRectF& rect)
{
    handlePtzRedirect(widget);
    const qreal aspectRatio = Geometry::aspectRatio(widget->size());
    const QRectF viewport = Geometry::cwiseDiv(rect, widget->size());
    widget->ptzController()->viewportMove(aspectRatio, viewport, 1.0);
}

void PtzInstrument::ptzUnzoom(QnMediaResourceWidget* widget)
{
    QSizeF size = widget->size() * 100;
    ptzMoveTo(widget, QRectF(widget->rect().center() - Geometry::toPoint(size) / 2, size));
}

void PtzInstrument::ptzMove(QnMediaResourceWidget* widget, const nx::core::ptz::Vector& speed, bool instant)
{
    PtzData& data = m_dataByWidget[widget];
    data.requestedSpeed = speed;

    if (!instant && (data.currentSpeed - data.requestedSpeed).lengthSquared() < speedUpdateThreshold * speedUpdateThreshold)
        return;

    instant = instant ||
        (qFuzzyIsNull(data.currentSpeed) ^ qFuzzyIsNull(data.requestedSpeed)) ||
        (data.currentSpeed - data.requestedSpeed).lengthSquared() > instantSpeedUpdateThreshold * instantSpeedUpdateThreshold;

    if (instant)
    {
        widget->ptzController()->continuousMove(data.requestedSpeed);
        data.currentSpeed = data.requestedSpeed;

        m_movementTimer.stop();
    }
    else
    {
        if (!m_movementTimer.isActive())
            m_movementTimer.start(speedUpdateIntervalMSec, this);
    }
}

void PtzInstrument::focusMove(QnMediaResourceWidget* widget, qreal speed)
{
    widget->ptzController()->continuousFocus(speed);
}

void PtzInstrument::focusAuto(QnMediaResourceWidget* widget)
{
    widget->ptzController()->runAuxiliaryCommand(
        Ptz::ManualAutoFocusPtzTrait,
        QString());
}

void PtzInstrument::processPtzClick(const QPointF& pos)
{
    if (!target() || m_skipNextAction)
        return;

    /* Don't animate for virtual cameras as it looks bad. */
    if (m_movement != VirtualMovement)
    {
        QnSplashItem *splashItem = newSplashItem(target());
        splashItem->setSplashType(QnSplashItem::Circular);
        splashItem->setRect(QRectF(0.0, 0.0, 0.0, 0.0));
        splashItem->setPos(pos);
        m_activeAnimations.push_back(SplashItemAnimation(splashItem, 1.0, 1.0));
    }

    ptzMoveTo(target(), pos);
}

void PtzInstrument::processPtzDrag(const QRectF& rect)
{
    if (!target() || m_skipNextAction)
        return;

    QnSplashItem *splashItem = newSplashItem(target());
    splashItem->setSplashType(QnSplashItem::Rectangular);
    splashItem->setPos(rect.center());
    splashItem->setRect(QRectF(-Geometry::toPoint(rect.size()) / 2, rect.size()));
    m_activeAnimations.push_back(SplashItemAnimation(splashItem, 1.0, 1.0));

    ptzMoveTo(target(), rect);
}

void PtzInstrument::processPtzDoubleClick()
{
    m_isDoubleClick = false; //do not repeat double-click
    if (!target() || m_skipNextAction)
        return;

    // Ptz unzoom is not supported on redirected ptz.
    const auto camera = target()->resource().dynamicCast<QnClientCameraResource>();
    if (camera && camera->isPtzRedirected())
    {
        emit doubleClicked(target());
        return;
    }

    auto splashItem = newSplashItem(target());
    splashItem->setSplashType(QnSplashItem::Rectangular);
    splashItem->setPos(target()->rect().center());
    QSizeF size = target()->size() * 1.1;
    splashItem->setRect(QRectF(-Geometry::toPoint(size) / 2, size));
    m_activeAnimations.push_back(SplashItemAnimation(splashItem, -1.0, 1.0));

    ptzUnzoom(target());

    /* Also do item unzoom if we're zoomed in. */
    QRectF viewportGeometry = display()->viewportGeometry();
    QRectF zoomedItemGeometry = display()->itemGeometry(target()->item());
    if (viewportGeometry.width() < zoomedItemGeometry.width() * itemUnzoomThreshold
        || viewportGeometry.height() < zoomedItemGeometry.height() * itemUnzoomThreshold)
    {
        emit doubleClicked(target());
    }
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void PtzInstrument::installedNotify()
{
    NX_ASSERT(selectionItem() == nullptr);
    NX_ASSERT(elementsWidget() == nullptr);

    base_type::installedNotify();
}

void PtzInstrument::aboutToBeDisabledNotify()
{
    m_clickTimer.stop();
    m_movementTimer.stop();

    base_type::aboutToBeDisabledNotify();
}

void PtzInstrument::aboutToBeUninstalledNotify()
{
    base_type::aboutToBeUninstalledNotify();

    if (selectionItem())
        delete selectionItem();
    if (elementsWidget())
        delete elementsWidget();
}

bool PtzInstrument::registeredNotify(QGraphicsItem* item)
{
    if (!base_type::registeredNotify(item))
        return false;

    auto widget = dynamic_cast<QnMediaResourceWidget*>(item);
    if (!widget || !widget->resource())
        return false;

    connect(widget, &QnMediaResourceWidget::optionsChanged, this,
        &PtzInstrument::updateOverlayWidget);
    connect(widget, &QnMediaResourceWidget::fisheyeChanged, this,
        &PtzInstrument::updateOverlayWidget);
    connect(widget, &QnMediaResourceWidget::ptzControllerChanged, this,
        [this, widget] { updateWidgetPtzController(widget); });

    updateWidgetPtzController(widget);
    updateOverlayWidgetInternal(widget);
    return true;
}

void PtzInstrument::unregisteredNotify(QGraphicsItem* item)
{
    base_type::unregisteredNotify(item);

    /* We don't want to use RTTI at this point, so we don't cast to QnMediaResourceWidget. */
    QGraphicsObject* object = item->toGraphicsObject();
    object->disconnect(this);

    PtzData& data = m_dataByWidget[object];
    disconnect(data.capabilitiesConnection);

    m_dataByWidget.remove(object);
}

void PtzInstrument::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == m_clickTimer.timerId())
    {
        m_clickTimer.stop();
        processPtzClick(m_clickPos);
    }
    else if (event->timerId() == m_movementTimer.timerId())
    {
        if (!target())
            return;

        ptzMove(target(), m_dataByWidget[target()].requestedSpeed);
        m_movementTimer.stop();
    }
    else
    {
        base_type::timerEvent(event);
    }
}

bool PtzInstrument::animationEvent(AnimationEvent* event)
{
    qreal dt = event->deltaTime() / 1000.0;

    for (int i = m_activeAnimations.size() - 1; i >= 0; i--)
    {
        SplashItemAnimation &animation = m_activeAnimations[i];

        qreal opacity = animation.item->opacity();
        QRectF rect = animation.item->boundingRect();

        qreal opacitySpeed = animation.fadingIn ? 8.0 : -1.0;

        opacity += dt * opacitySpeed * animation.opacityMultiplier;
        if (opacity >= 1.0)
        {
            animation.fadingIn = false;
            opacity = 1.0;
        }
        else if (opacity <= 0.0)
        {
            animation.item->hide();
            m_freeAnimations.push_back(animation);
            m_activeAnimations.removeAt(i);
            continue;
        }
        qreal ds = dt * m_expansionSpeed * animation.expansionMultiplier;
        rect = rect.adjusted(-ds, -ds, ds, ds);

        animation.item->setOpacity(opacity);
        animation.item->setRect(rect);
    }

    return false;
}

bool PtzInstrument::mousePressEvent(QWidget* viewport, QMouseEvent* /*event*/)
{
    m_viewport = viewport;

    return false;
}

bool PtzInstrument::mousePressEvent(QGraphicsItem* item, QGraphicsSceneMouseEvent* event)
{
    if (!processMousePress(item, event))
        return false;

    return base_type::mousePressEvent(item, event);
}

bool PtzInstrument::mouseDoubleClickEvent(QGraphicsItem* item, QGraphicsSceneMouseEvent* event)
{
    if (!processMousePress(item, event))
        return false;

    m_isDoubleClick = true;
    return base_type::mouseDoubleClickEvent(item, event);
}

void PtzInstrument::startDragProcess(DragInfo* /*info*/)
{
    m_isClick = true;
    emit ptzProcessStarted(target());
}

void PtzInstrument::startDrag(DragInfo* info)
{
    m_isClick = false;
    m_isDoubleClick = false;
    m_ptzStartedEmitted = false;

    if (!target())
    {
        /* Whoops, already destroyed. */
        reset();
        return;
    }

    switch (m_movement)
    {
        case ContinuousMovement:
        case VirtualMovement:
            m_movementFilter.reset(target()->dewarpingParams().enabled
                ? nullptr
                : new MovementFilter(target(), this));

            targetManipulator()->setCursor(Qt::BlankCursor);
            target()->setCursor(Qt::BlankCursor);

            ensureElementsWidget();

            if (ini().oldPtzAimOverlay)
            {
                opacityAnimator(elementsWidget()->arrowItem())->animateTo(1.0);
            }
            else
            {
                elementsWidget()->newArrowItem()->setDirection({});
                elementsWidget()->newArrowItem()->setPos(elementsWidget()->mapFromItem(target(),
                    info->mouseItemPos()));

                opacityAnimator(elementsWidget()->newArrowItem())->animateTo(1.0);
            }

            break;

        case ViewportMovement:
            ensureSelectionItem();
            selectionItem()->setParentItem(target());
            selectionItem()->setViewport(m_viewport.data());
            opacityAnimator(selectionItem())->stop();
            selectionItem()->setOpacity(1.0);
            break;

        default:
            break;
    }

    /* Everything else will be initialized in the first call to drag(). */

    emit ptzStarted(target());
    m_ptzStartedEmitted = true;
}

void PtzInstrument::dragMove(DragInfo* info)
{
    if (!target())
    {
        reset();
        return;
    }

    switch (m_movement)
    {
        case ContinuousMovement:
        {
            if (m_externalPtzDirections != 0)
                break;

            QPointF mouseItemPos = info->mouseItemPos();
            QPointF itemCenter = target()->rect().center();

            if (!(m_movementOrientations & Qt::Horizontal))
                mouseItemPos.setX(itemCenter.x());
            if (!(m_movementOrientations & Qt::Vertical))
                mouseItemPos.setY(itemCenter.y());

            QPointF delta = mouseItemPos - itemCenter;
            QSizeF size = target()->size();
            qreal scale = qMax(size.width(), size.height()) / 2.0;
            QPointF speed(qBound(-1.0, delta.x() / scale, 1.0), qBound(-1.0, -delta.y() / scale, 1.0));

            qreal speedMagnitude = Geometry::length(speed);
            qreal arrowSize = 12.0 * (1.0 + 3.0 * speedMagnitude);

            ensureElementsWidget();

            if (ini().oldPtzAimOverlay)
            {
                auto arrowItem = elementsWidget()->arrowItem();
                arrowItem->moveTo(elementsWidget()->mapFromItem(target(), target()->rect().center()),
                    elementsWidget()->mapFromItem(target(), mouseItemPos));
                arrowItem->setSize(QSizeF(arrowSize, arrowSize));
            }
            else
            {
                elementsWidget()->newArrowItem()->setDirection((
                    elementsWidget()->mapFromItem(target(), info->mouseItemPos())
                        - elementsWidget()->newArrowItem()->pos()) / 2.0);
            }

            if (m_movementFilter)
                m_movementFilter->updateFilteringSpeed(nx::core::ptz::Vector(speed));
            else
                ptzMove(target(), nx::core::ptz::Vector(speed));

             break;
        }

        case ViewportMovement:
            ensureSelectionItem();
            selectionItem()->setGeometry(info->mousePressItemPos(), info->mouseItemPos(),
                Geometry::aspectRatio(target()->size()), target()->rect());
            break;

        case VirtualMovement:
        {
            const QPointF mouseItemPos = info->mouseItemPos();
            QPointF delta = mouseItemPos - info->lastMouseItemPos();

            qreal scale = target()->size().width() / 2.0;
            QPointF shift(delta.x() / scale, -delta.y() / scale);

            nx::core::ptz::Vector position;
            target()->ptzController()->getPosition(Qn::LogicalPtzCoordinateSpace, &position);

            qreal speed = 0.5 * position.zoom;
            nx::core::ptz::Vector positionDelta(shift.x() * speed, shift.y() * speed, 0.0, 0.0);
            target()->ptzController()->absoluteMove(
                Qn::LogicalPtzCoordinateSpace,
                position + positionDelta,
                2.0); /* 2.0 means instant movement. */

            ensureElementsWidget();
            auto arrowItem = elementsWidget()->arrowItem();
            arrowItem->moveTo(elementsWidget()->mapFromItem(target(), target()->rect().center()),
                elementsWidget()->mapFromItem(target(), mouseItemPos));
            const qreal arrowSize = 24.0;
            arrowItem->setSize(QSizeF(arrowSize, arrowSize));

            break;
        }

        default:
            break;
    }
}

void PtzInstrument::finishDrag(DragInfo* /*info*/)
{
    if (target())
    {
        switch (m_movement)
        {
            case ContinuousMovement:
            case VirtualMovement:
                targetManipulator()->setCursor(Qt::SizeAllCursor);
                target()->unsetCursor();
                ensureElementsWidget();
                opacityAnimator(elementsWidget()->newArrowItem())->animateTo(0.0);
                opacityAnimator(elementsWidget()->arrowItem())->animateTo(0.0);
                break;

            case ViewportMovement:
            {
                ensureSelectionItem();
                opacityAnimator(selectionItem(), 4.0)->animateTo(0.0);

                QRectF selectionRect = selectionItem()->boundingRect();
                QSizeF targetSize = target()->size();

                qreal relativeSize = qMax(selectionRect.width() / targetSize.width(),
                    selectionRect.height() / targetSize.height());

                if (relativeSize >= minPtzZoomRectSize)
                    processPtzDrag(selectionRect);
                break;
            }

            default:
                break;
        }
    }

    if (m_ptzStartedEmitted)
        emit ptzFinished(target());
}

void PtzInstrument::finishDragProcess(DragInfo* info)
{
    if (target())
    {
        switch (m_movement)
        {
            case ContinuousMovement:
                if (m_movementFilter)
                    m_movementFilter->stopMovement();
                else
                    ptzMove(target(), nx::core::ptz::Vector());
                break;

            case ViewportMovement:
            case VirtualMovement:
                if (m_isClick)
                {
                    if (m_isDoubleClick)
                    {
                        processPtzDoubleClick();
                    }
                    else
                    {
                        m_clickTimer.start(m_clickDelayMSec, this);
                        m_clickPos = info->mousePressItemPos();
                    }
                }
                break;

            default:
                break;
        }
    }

    emit ptzProcessFinished(target());
}

void PtzInstrument::at_splashItem_destroyed()
{
    QnSplashItem* item = static_cast<QnSplashItem*>(sender());

    m_freeAnimations.removeAll(item);
    m_activeAnimations.removeAll(item);
}

void PtzInstrument::at_modeButton_clicked()
{
    context()->statisticsModule()->registerClick(lit("ptz_overlay_mode"));

    PtzImageButtonWidget* button = checked_cast<PtzImageButtonWidget*>(sender());

    if (QnMediaResourceWidget* widget = button->target())
    {
        ensureOverlayWidget(widget);

        nx::vms::api::DewarpingData iparams = widget->item()->dewarpingParams();
        QnMediaDewarpingParams mparams = widget->dewarpingParams();

        const QList<int> allowed = mparams.allowedPanoFactorValues();

        int idx = (allowed.indexOf(iparams.panoFactor) + 1) % allowed.size();
        iparams.panoFactor = allowed[idx];
        widget->item()->setDewarpingParams(iparams);

        updateOverlayWidgetInternal(widget);
    }
}

void PtzInstrument::at_zoomInButton_pressed()
{
    context()->statisticsModule()->registerClick(lit("ptz_overlay_zoom_in"));
    at_zoomButton_activated(1.0);
}

void PtzInstrument::at_zoomInButton_released()
{
    at_zoomButton_activated(0.0);
}

void PtzInstrument::at_zoomOutButton_pressed()
{
    context()->statisticsModule()->registerClick(lit("ptz_overlay_zoom_out"));
    at_zoomButton_activated(-1.0);
}

void PtzInstrument::at_zoomOutButton_released()
{
    at_zoomButton_activated(0.0);
}

void PtzInstrument::at_zoomButton_activated(qreal zoomSpeed)
{
    PtzImageButtonWidget* button = checked_cast<PtzImageButtonWidget*>(sender());

    if (QnMediaResourceWidget* widget = button->target())
        ptzMove(widget, nx::core::ptz::Vector(0.0, 0.0, 0.0, zoomSpeed), true);
}

void PtzInstrument::at_focusInButton_pressed()
{
    context()->statisticsModule()->registerClick(lit("ptz_overlay_focus_in"));
    at_focusButton_activated(1.0);
}

void PtzInstrument::at_focusInButton_released()
{
    at_focusButton_activated(0.0);
}

void PtzInstrument::at_focusOutButton_pressed()
{
    context()->statisticsModule()->registerClick(lit("ptz_overlay_focus_out"));
    at_focusButton_activated(-1.0);
}

void PtzInstrument::at_focusOutButton_released()
{
    at_focusButton_activated(0.0);
}

void PtzInstrument::at_focusButton_activated(qreal speed)
{
    PtzImageButtonWidget* button = checked_cast<PtzImageButtonWidget*>(sender());

    if (QnMediaResourceWidget* widget = button->target())
        focusMove(widget, speed);
}

void PtzInstrument::at_focusAutoButton_clicked()
{
    context()->statisticsModule()->registerClick(lit("ptz_overlay_focus_auto"));

    PtzImageButtonWidget* button = checked_cast<PtzImageButtonWidget*>(sender());

    if (QnMediaResourceWidget* widget = button->target())
        focusAuto(widget);
}

bool PtzInstrument::PtzData::hasCapabilities(Ptz::Capabilities value) const
{
    return (capabilities & value) == value;
}

void PtzInstrument::toggleContinuousPtz(DirectionFlag direction, bool on)
{
    if (dragProcessor()->isRunning())
        return;

    const auto widget = qobject_cast<QnMediaResourceWidget*>(display()->widget(Qn::CentralRole));
    if (!widget || m_externalPtzDirections.testFlag(direction) == on)
        return;

    setTarget(widget);

    // Stop PTZ before setting a new speed. Otherwise it works unexpectedly.
    ptzMove(widget, {}, true);

    m_externalPtzDirections.setFlag(direction, on);

    if (on)
        m_externalPtzDirections.setFlag(oppositeDirection(direction), false);

    const QVector3D directionMultiplier(
        -int(m_externalPtzDirections.testFlag(DirectionFlag::panRight))
            + int(m_externalPtzDirections.testFlag(DirectionFlag::panLeft)),
        -int(m_externalPtzDirections.testFlag(DirectionFlag::tiltUp))
            + int(m_externalPtzDirections.testFlag(DirectionFlag::tiltDown)),
        -int(m_externalPtzDirections.testFlag(DirectionFlag::zoomOut))
            + int(m_externalPtzDirections.testFlag(DirectionFlag::zoomIn)));

    const auto rotation = widget->item()->rotation()
       + (widget->item()->data<bool>(Qn::ItemFlipRole, false) ? 0.0 : 180.0);

    const auto speed = applyRotation(kKeyboardPtzSensitivity * directionMultiplier, rotation);
    ptzMove(widget, {speed.x(), speed.y(), 0, speed.z()}, true);
}

void PtzInstrument::at_display_widgetAboutToBeChanged(Qn::ItemRole role)
{
    if (role != Qn::CentralRole)
        return;

    m_externalPtzDirections = {};

    menu()->triggerIfPossible(ui::action::PtzContinuousMoveAction, ui::action::Parameters()
        .withArgument(Qn::ItemDataRole::PtzSpeedRole, QVariant::fromValue(QVector3D()))
        .withArgument(Qn::ItemDataRole::ForceRole, true));
}
