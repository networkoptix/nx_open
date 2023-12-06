// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ptz_instrument.h"

#include <chrono>

#include <QtCore/QMetaEnum>
#include <QtCore/QTimer>
#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsSceneMouseEvent>

#include <core/resource/client_camera.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/utils/math/math.h>
#include <nx/vms/api/data/dewarping_data.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/settings/show_once_settings.h>
#include <nx/vms/client/desktop/statistics/context_statistics_module.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/common/custom_cursors.h>
#include <nx/vms/client/desktop/ui/common/keyboard_modifiers_watcher.h>
#include <nx/vms/client/desktop/ui/graphics/items/overlays/ptz_promo_overlay.h>
#include <nx/vms/client/desktop/ui/scene/widgets/scene_banners.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/animation/animation_event.h>
#include <ui/animation/opacity_animator.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <utils/common/checked_cast.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/math/color_transformations.h>

#include "ptz_instrument_p.h"
#include "ptz_overlay_widget.h"

using nx::vms::client::core::Geometry;

using namespace std::chrono;
using namespace nx::core;
using namespace nx::vms::client::desktop;
using namespace nx::vms::common::ptz;

namespace {

static const QVector3D kKeyboardPtzSensitivity(/*pan*/ 0.5, /*tilt*/ 0.5, /*zoom*/ 1.0);

static const qreal kWheelFisheyeZoomSensitivity = 4.0;

const qreal instantSpeedUpdateThreshold = 0.1;
const qreal speedUpdateThreshold = 0.001;
const int speedUpdateIntervalMSec = 500;

const double minPtzZoomRectSize = 0.08;

// TODO: #sivanov Remove code sync.
// In sync with hardcoded constant in workbench_controller.
const qreal itemUnzoomThreshold = 0.975;

// Key to store if we already displayed ptz redirect warning on the current layout.
static const char* kPtzRedirectLockedLayoutWarningKey = "__ptz_redirect_locked_warning";

Vector truncate(const Vector& value)
{
    return Vector(
        trunc(value.pan * 100) / 100.0,
        trunc(value.tilt * 100) / 100.0,
        trunc(value.rotation * 100) / 100.0,
        trunc(value.zoom * 100) / 100.0);
}

Qt::Orientations capabilitiesToMode(Ptz::Capabilities capabilities)
{
    Qt::Orientations result;
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

Ptz::Capability requiredCapability(PtzInstrument::DirectionFlag direction)
{
    switch (direction)
    {
        case PtzInstrument::DirectionFlag::panLeft:
        case PtzInstrument::DirectionFlag::panRight:
            return Ptz::ContinuousPanCapability;

        case PtzInstrument::DirectionFlag::tiltUp:
        case PtzInstrument::DirectionFlag::tiltDown:
            return Ptz::ContinuousTiltCapability;

        case PtzInstrument::DirectionFlag::zoomIn:
        case PtzInstrument::DirectionFlag::zoomOut:
            return Ptz::ContinuousZoomCapability;
    }

    NX_ASSERT(false);
    return Ptz::NoPtzCapabilities;
}

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

    void updateFilteringSpeed(const Vector& speed);

    void stopMovement();

private:
    void setMovementSpeed(const Vector& speed);

    void onTimeout();

private:
    QPointer<PtzInstrument> const m_parent;
    QPointer<QnMediaResourceWidget> const m_widget;
    QTimer m_filteringTimer;
    Vector m_targetSpeed;
    Vector m_filteringSpeed;
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
    setMovementSpeed(Vector());
}

void PtzInstrument::MovementFilter::onTimeout()
{
    setMovementSpeed(m_filteringSpeed);
}

void PtzInstrument::MovementFilter::stopMovement()
{
    updateFilteringSpeed(Vector());
}

void PtzInstrument::MovementFilter::updateFilteringSpeed(const Vector& speed)
{
    if (speed.isNull())
        setMovementSpeed(m_filteringSpeed);

    const auto truncatedSpeed = truncate(speed);
    if (m_filteringSpeed == truncatedSpeed)
        return;

    m_filteringSpeed = truncatedSpeed;
    m_filteringTimer.start(); //< Restarts if it's started already.
}

void PtzInstrument::MovementFilter::setMovementSpeed(const Vector& speed)
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
    m_expansionSpeed(Workbench::kUnitSize / 5.0),
    m_movement(NoMovement),
    m_wheelZoomTimer(new QTimer(this))
{
    connect(resourcePool(), &QnResourcePool::statusChanged, this,
        [this](const QnResourcePtr& resource, Qn::StatusChangeReason /*reason*/)
        {
            if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
            {
                const bool wasInaccessible =
                    resource->getPreviousStatus() == nx::vms::api::ResourceStatus::offline
                    || resource->getPreviousStatus() == nx::vms::api::ResourceStatus::unauthorized;

                const bool isAccessible = resource->getStatus() == nx::vms::api::ResourceStatus::online
                    || resource->getStatus() == nx::vms::api::ResourceStatus::recording;

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

    m_wheelZoomTimer->setSingleShot(true);
    connect(m_wheelZoomTimer, &QTimer::timeout, this,
        [this]()
        {
            m_wheelZoomDirection = 0;
            updateExternalPtzSpeed();
        });

    connect(&appContext()->localSettings()->ptzAimOverlayEnabled,
        &nx::utils::property_storage::BaseProperty::changed,
        this,
        [this]()
        {
            for (auto iter = m_dataByWidget.begin(); iter != m_dataByWidget.end(); ++iter)
            {
                if (!iter->overlayWidget)
                    continue;

                iter->overlayWidget->forceUpdateLayout();
                updateOverlayWidgetInternal(qobject_cast<QnMediaResourceWidget*>(iter.key()));
            }
        });

    const auto updatePromoOnWidgets =
        [this](nx::utils::property_storage::BaseProperty* property)
        {
            if (property->variantValue().toBool())
                return;

            for (auto iter = m_dataByWidget.begin(); iter != m_dataByWidget.end(); ++iter)
            {
                if (iter->overlayWidget)
                    updatePromo(qobject_cast<QnMediaResourceWidget*>(iter.key()));
            }
        };

    connect(&showOnceSettings()->newPtzMechanicPromo,
        &nx::utils::property_storage::BaseProperty::changed,
        this,
        updatePromoOnWidgets);
    connect(&showOnceSettings()->autoTrackingPromo,
        &nx::utils::property_storage::BaseProperty::changed,
        this,
        updatePromoOnWidgets);

    connect(KeyboardModifiersWatcher::instance(), &KeyboardModifiersWatcher::modifiersChanged, this,
        [this](Qt::KeyboardModifiers changedModifiers)
        {
            if (!changedModifiers.testFlag(Qt::ShiftModifier))
                return;

            for (auto iter = m_dataByWidget.begin(); iter != m_dataByWidget.end(); ++iter)
            {
                if (iter->overlayWidget)
                    updateOverlayCursor(qobject_cast<QnMediaResourceWidget*>(iter.key()));
            }
        });
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

    bool isFisheyeEnabled = widget->dewarpingParams().enabled;

    PtzOverlayWidget *overlay = new PtzOverlayWidget();
    overlay->setOpacity(0.0);
    overlay->zoomInButton()->setTarget(widget);
    overlay->zoomOutButton()->setTarget(widget);
    overlay->focusInButton()->setTarget(widget);
    overlay->focusOutButton()->setTarget(widget);
    overlay->focusAutoButton()->setTarget(widget);
    overlay->modeButton()->setTarget(widget);
    overlay->modeButton()->setVisible(data.isFisheye() && isFisheyeEnabled);

    overlay->setMarkersMode(appContext()->localSettings()->ptzAimOverlayEnabled()
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

    data.cursorOverlay = new QGraphicsWidget();
    data.cursorOverlay->setAcceptedMouseButtons(Qt::NoButton);

    widget->addOverlayWidget(data.cursorOverlay, {QnResourceWidget::Invisible,
        QnResourceWidget::OverlayFlag::autoRotate, QnResourceWidget::SelectionLayer});

    return overlay;
}

void PtzInstrument::updatePromo(QnMediaResourceWidget* widget)
{
    if (showOnceSettings()->newPtzMechanicPromo() && showOnceSettings()->autoTrackingPromo())
        return;

    PtzData& data = m_dataByWidget[widget];
    if (data.isFisheye())
        return;

    if (!data.promoOverlay)
    {
        if (appContext()->localSettings()->ptzAimOverlayEnabled())
            return;

        data.promoOverlay = new PtzPromoOverlay();

        connect(data.promoOverlay.data(), &PtzPromoOverlay::closeRequested, this,
            [this, widget, overlay = data.promoOverlay.data()]()
            {
                showOnceSettings()->newPtzMechanicPromo = true;
                showOnceSettings()->autoTrackingPromo = true;
                widget->removeOverlayWidget(overlay);
                overlay->setVisible(false);
                overlay->deleteLater();
                updateOverlayWidgetInternal(widget);
            });

        widget->addOverlayWidget(data.promoOverlay, {QnResourceWidget::Invisible,
            QnResourceWidget::OverlayFlag::bindToViewport | QnResourceWidget::OverlayFlag::autoRotate,
            QnResourceWidget::TopControlsLayer});
    }

    data.promoOverlay->setPagesVisibility(
        !showOnceSettings()->newPtzMechanicPromo(),
        !showOnceSettings()->autoTrackingPromo());

    const bool showPromo = widget->options().testFlag(QnResourceWidget::ControlPtz)
        && !appContext()->localSettings()->ptzAimOverlayEnabled()
        && widget->ptzActivationReason() != QnMediaResourceWidget::PtzEnabledBy::joystick;

    const bool animate = display()->animationAllowed();

    const QnResourceWidget::OverlayVisibility visibility = showPromo
        ? QnResourceWidget::AutoVisible
        : QnResourceWidget::Invisible;

    widget->setOverlayWidgetVisibility(data.promoOverlay, visibility, animate);
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
            SceneBanner::show(tr("Layout is locked"));
            currentLayout->setProperty(kPtzRedirectLockedLayoutWarningKey, true);
        }
        return false;
    }

    // Drop it as near to source camera as possible.
    ui::action::Parameters parameters(dynamicSensor);
    parameters.setArgument(Qn::ItemPositionRole,
        QVariant::fromValue(QRectF(staticSensor->item()->geometry()).center()));

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

    const PtzData& data = m_dataByWidget[target];

    PtzManipulatorWidget* manipulator = nullptr;
    if (const auto overlay = overlayWidget(target))
    {
        manipulator = overlay->manipulatorWidget();
        if (!manipulator->isVisible()
            || !manipulator->rect().contains(manipulator->mapFromItem(item, event->pos())))
        {
            manipulator = nullptr;

            if (data.promoOverlay && data.promoOverlay->isVisible())
                return false; //< Ignore clicks on the promo pages background.
        }
    }

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
        else
        {
            if (appContext()->localSettings()->ptzAimOverlayEnabled())
            {
                m_movement = data.hasAdvancedPtz() ? ViewportMovement : NoMovement;
            }
            else
            {
                if (event->modifiers().testFlag(Qt::ShiftModifier))
                {
                    m_movement = data.hasAdvancedPtz() ? ViewportMovement : NoMovement;

                    if (m_movement == NoMovement)
                    {
                        if (!m_noAdvancedPtzWarning)
                        {
                            m_noAdvancedPtzWarning = SceneBanner::show(
                                tr("This camera does not support advanced PTZ"));
                        }

                        return true;
                    }
                }
                else
                {
                    m_movement = data.hasContinuousPanOrTilt() ? ContinuousMovement : NoMovement;
                }
            }
        }

        if (m_movement == NoMovement)
            return false;
    }

    if (m_movement == ContinuousMovement)
    {
        m_movementOrientations = {};
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
    selectionItem()->setElementSize(Workbench::kUnitSize / 64.0);
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
    data.capabilitiesConnection = connect(widget->ptzController().get(),
        &QnAbstractPtzController::changed,
        this,
        [this, widget](DataFields fields)
        {
            if (fields.testFlag(DataField::capabilities))
                updateCapabilities(widget);
            if (fields.testFlag(DataField::auxiliaryTraits))
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

    const PtzData& data = m_dataByWidget[widget];
    if (!data.overlayWidget)
        return;

    updatePromo(widget);

    const auto isPromoVisible = data.promoOverlay && data.promoOverlay->isVisible();
    const QnResourceWidget::OverlayVisibility visibility = ptzModeEnabled && !isPromoVisible
        ? QnResourceWidget::AutoVisible
        : QnResourceWidget::Invisible;

    widget->setOverlayWidgetVisibility(data.overlayWidget, visibility, animate);
    if (NX_ASSERT(data.cursorOverlay))
        widget->setOverlayWidgetVisibility(data.cursorOverlay, visibility, false);

    const bool isFisheyeEnabled = widget->dewarpingParams().enabled;

    const bool canMove = data.hasCapabilities(Ptz::ContinuousPanCapability)
        || data.hasCapabilities(Ptz::ContinuousTiltCapability);
    const bool hasZoom = data.hasCapabilities(Ptz::ContinuousZoomCapability);
    const bool hasFocus = data.hasCapabilities(Ptz::ContinuousFocusCapability);
    const bool hasAutoFocus = data.traits.contains(Ptz::ManualAutoFocusPtzTrait);
    const bool showManipulator = canMove && appContext()->localSettings()->ptzAimOverlayEnabled();

    data.overlayWidget->manipulatorWidget()->setVisible(showManipulator);
    data.overlayWidget->zoomInButton()->setVisible(hasZoom);
    data.overlayWidget->zoomOutButton()->setVisible(hasZoom);
    data.overlayWidget->focusInButton()->setVisible(hasFocus);
    data.overlayWidget->focusOutButton()->setVisible(hasFocus);

    updateOverlayCursor(widget);

    const bool autoFocusWasVisible = data.overlayWidget->focusAutoButton()->isVisible();
    data.overlayWidget->focusAutoButton()->setVisible(hasAutoFocus);

    if (data.isFisheye())
    {
        int panoAngle = widget->item()
            ? 90 * widget->item()->dewarpingParams().panoFactor
            : 90;
        data.overlayWidget->modeButton()->setText(QString::number(panoAngle));
    }

    data.overlayWidget->modeButton()->setVisible(data.isFisheye() && isFisheyeEnabled);
    data.overlayWidget->setMarkersMode(appContext()->localSettings()->ptzAimOverlayEnabled()
        ? capabilitiesToMode(data.capabilities)
        : Qt::Orientations());

    if (autoFocusWasVisible != hasAutoFocus)
        data.overlayWidget->forceUpdateLayout();
}

void PtzInstrument::updateOverlayCursor(QnMediaResourceWidget* widget)
{
    const PtzData& data = m_dataByWidget[widget];
    if (!data.cursorOverlay)
        return;

    if (data.promoOverlay && data.promoOverlay->isVisible())
    {
        data.cursorOverlay->unsetCursor();
        return;
    }

    const bool canMove = data.hasContinuousPanOrTilt();
    const bool showManipulator = canMove && appContext()->localSettings()->ptzAimOverlayEnabled();

    const bool advancedPtzDragEnabled = appContext()->localSettings()->ptzAimOverlayEnabled()
        || KeyboardModifiersWatcher::instance()->modifiers().testFlag(Qt::ShiftModifier);

    if (data.isFisheye() && !appContext()->localSettings()->ptzAimOverlayEnabled())
        data.cursorOverlay->setCursor(Qt::OpenHandCursor);
    else if (data.hasAdvancedPtz() && !data.isFisheye() && advancedPtzDragEnabled)
        data.cursorOverlay->setCursor(CustomCursors::cross);
    else if (canMove && !showManipulator)
        data.cursorOverlay->setCursor(CustomCursors::sizeAll);
    else
        data.cursorOverlay->unsetCursor();
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

void PtzInstrument::ptzMoveTo(QnMediaResourceWidget* widget, const QRectF& rect, bool unzooming)
{
    handlePtzRedirect(widget);
    const qreal aspectRatio = Geometry::aspectRatio(widget->size());
    const QRectF viewport = Geometry::cwiseDiv(rect, widget->size());
    widget->ptzController()->viewportMove(aspectRatio, viewport, 1.0);

    if (m_dataByWidget.value(widget).isFisheye())
        return;

    widget->setActionText(unzooming ? tr("Zooming out..."): tr("Moving..."));
    widget->clearActionText(2s);
}

void PtzInstrument::ptzUnzoom(QnMediaResourceWidget* widget)
{
    QSizeF size = widget->size() * 100;
    ptzMoveTo(widget, QRectF(widget->rect().center() - Geometry::toPoint(size) / 2, size),
        /*unzooming*/ true);
}

QString PtzInstrument::actionText(QnMediaResourceWidget* widget) const
{
    if (!NX_ASSERT(widget))
        return {};

    const auto data = m_dataByWidget[widget];
    if (data.requestedSpeed.isNull())
        return {};

    if (!qFuzzyIsNull(data.requestedSpeed.zoom) != 0
        && !qFuzzyIsNull(data.requestedSpeed.zoom - data.currentSpeed.zoom))
    {
        return data.requestedSpeed.zoom > 0.0 ? tr("Zooming in...") : tr("Zooming out...");
    }

    return tr("Moving...");
}

void PtzInstrument::updateActionText(QnMediaResourceWidget* widget)
{
    if (!NX_ASSERT(widget))
        return;

    const auto text = actionText(widget);
    if (text.isEmpty())
        widget->clearActionText(2s);
    else
        widget->setActionText(text);
}

void PtzInstrument::ptzMove(QnMediaResourceWidget* widget, const Vector& speed, bool instant)
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
        NX_VERBOSE(this, "PTZ continuous move: %1, %2, %3", data.requestedSpeed.pan,
            data.requestedSpeed.tilt, data.requestedSpeed.zoom);

        widget->ptzController()->continuousMove(data.requestedSpeed);

        if (!data.isFisheye())
            updateActionText(widget);

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
    NX_VERBOSE(this, "PTZ continuous focus: %1", speed);

    widget->ptzController()->continuousFocus(speed);

    if (m_dataByWidget.value(widget).isFisheye())
        return;

    if (qFuzzyIsNull(speed))
        updateActionText(widget);
    else
        widget->setActionText(tr("Focusing..."));
}

void PtzInstrument::focusAuto(QnMediaResourceWidget* widget)
{
    widget->ptzController()->runAuxiliaryCommand(
        Ptz::ManualAutoFocusPtzTrait,
        QString());

    if (m_dataByWidget.value(widget).isFisheye())
        return;

    widget->setActionText(tr("Focusing..."));
    widget->clearActionText(2s);
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
}

void PtzInstrument::startDrag(DragInfo* info)
{
    m_isClick = false;
    m_isDoubleClick = false;
    m_ptzStartedEmitted = false;

    if (!target() || target()->isTitleUnderMouse())
    {
        // Target is destroyed, or Move instrument should be used instead of Ptz.
        reset();
        return;
    }

    emit ptzProcessStarted(target());

    ensureOverlayWidget(target());

    const auto data = m_dataByWidget[target()];
    qApp->setOverrideCursor(
        data.isFisheye() && !appContext()->localSettings()->ptzAimOverlayEnabled()
            ? QCursor(Qt::ClosedHandCursor)
            : data.cursorOverlay->cursor());

    switch (m_movement)
    {
        case ContinuousMovement:
            m_movementFilter.reset(target()->dewarpingParams().enabled
                ? nullptr
                : new MovementFilter(target(), this));

            ensureElementsWidget();

            if (appContext()->localSettings()->ptzAimOverlayEnabled())
            {
                opacityAnimator(elementsWidget()->arrowItem())->animateTo(1.0);
            }
            else if (m_movement != VirtualMovement)
            {
                elementsWidget()->newArrowItem()->setDirection({});
                elementsWidget()->newArrowItem()->setPos(elementsWidget()->mapFromItem(target(),
                    info->mouseItemPos()));

                opacityAnimator(elementsWidget()->newArrowItem(), 2.0)->animateTo(1.0);
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

            QPointF currentPos = info->mouseItemPos();
            const QPointF startPos = info->mousePressItemPos();

            if (!(m_movementOrientations & Qt::Horizontal))
                currentPos.setX(startPos.x());
            if (!(m_movementOrientations & Qt::Vertical))
                currentPos.setY(startPos.y());

            QPointF sensitivity(1.0, 1.0);
            if (!target()->dewarpingParams().enabled)
            {
                if (const auto camera = target()->resource().dynamicCast<QnClientCameraResource>())
                    sensitivity = camera->ptzPanTiltSensitivity();
            }

            const QPointF delta = currentPos - startPos;
            const QSizeF size = target()->size();
            const qreal scale = qMax(size.width(), size.height()) / 2.0;
            const QPointF speed(qBound(-1.0, delta.x() / scale * sensitivity.x(), 1.0),
                qBound(-1.0, -delta.y() * sensitivity.y() / scale, 1.0));

            const qreal speedMagnitude = Geometry::length(speed);
            const qreal arrowSize = 12.0 * (1.0 + 3.0 * speedMagnitude);

            ensureElementsWidget();

            if (appContext()->localSettings()->ptzAimOverlayEnabled())
            {
                auto arrowItem = elementsWidget()->arrowItem();
                arrowItem->moveTo(elementsWidget()->mapFromItem(target(), target()->rect().center()),
                    elementsWidget()->mapFromItem(target(), currentPos));
                arrowItem->setSize(QSizeF(arrowSize, arrowSize));
            }
            else
            {
                elementsWidget()->newArrowItem()->setDirection((
                    elementsWidget()->mapFromItem(target(), currentPos)
                        - elementsWidget()->newArrowItem()->pos()) / 2.0);
            }

            if (m_movementFilter)
                m_movementFilter->updateFilteringSpeed(Vector(speed));
            else
                ptzMove(target(), Vector(speed));

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
            const QPointF delta = mouseItemPos - info->lastMouseItemPos();

            const qreal scale = target()->size().width() / 2.0;
            const QPointF shift(delta.x() / scale, -delta.y() / scale);

            Vector position;
            target()->ptzController()->getPosition(&position, CoordinateSpace::logical);

            const qreal speed = 0.5 * position.zoom
                * (appContext()->localSettings()->ptzAimOverlayEnabled() ? 1 : -1);

            const Vector positionDelta(shift.x() * speed, shift.y() * speed, 0.0, 0.0);
            target()->ptzController()->absoluteMove(
                CoordinateSpace::logical,
                position + positionDelta,
                /*instant movement*/ 2.0);

            if (appContext()->localSettings()->ptzAimOverlayEnabled())
            {
                ensureElementsWidget();
                auto arrowItem = elementsWidget()->arrowItem();
                arrowItem->moveTo(elementsWidget()->mapFromItem(target(), target()->rect().center()),
                    elementsWidget()->mapFromItem(target(), mouseItemPos));
                const qreal arrowSize = 24.0;
                arrowItem->setSize(QSizeF(arrowSize, arrowSize));
            }

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

    if (!m_ptzStartedEmitted)
        return;

    qApp->restoreOverrideCursor();
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
                    ptzMove(target(), Vector());
                break;

            case ViewportMovement:
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
    statisticsModule()->controls()->registerClick("ptz_overlay_mode");

    PtzImageButtonWidget* button = checked_cast<PtzImageButtonWidget*>(sender());

    if (QnMediaResourceWidget* widget = button->target())
    {
        ensureOverlayWidget(widget);

        nx::vms::api::dewarping::ViewData iparams = widget->item()->dewarpingParams();
        nx::vms::api::dewarping::MediaData mparams = widget->dewarpingParams();

        const QList<int> allowed = mparams.allowedPanoFactorValues();

        int idx = (allowed.indexOf(iparams.panoFactor) + 1) % allowed.size();
        iparams.panoFactor = allowed[idx];
        widget->item()->setDewarpingParams(iparams);

        updateOverlayWidgetInternal(widget);
    }
}

void PtzInstrument::at_zoomInButton_pressed()
{
    statisticsModule()->controls()->registerClick("ptz_overlay_zoom_in");
    at_zoomButton_activated(1.0);
}

void PtzInstrument::at_zoomInButton_released()
{
    at_zoomButton_activated(0.0);
}

void PtzInstrument::at_zoomOutButton_pressed()
{
    statisticsModule()->controls()->registerClick("ptz_overlay_zoom_out");
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
        ptzMove(widget, Vector(0.0, 0.0, 0.0, zoomSpeed), true);
}

void PtzInstrument::at_focusInButton_pressed()
{
    statisticsModule()->controls()->registerClick("ptz_overlay_focus_in");
    at_focusButton_activated(1.0);
}

void PtzInstrument::at_focusInButton_released()
{
    at_focusButton_activated(0.0);
}

void PtzInstrument::at_focusOutButton_pressed()
{
    statisticsModule()->controls()->registerClick("ptz_overlay_focus_out");
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
    statisticsModule()->controls()->registerClick("ptz_overlay_focus_auto");

    PtzImageButtonWidget* button = checked_cast<PtzImageButtonWidget*>(sender());

    if (QnMediaResourceWidget* widget = button->target())
        focusAuto(widget);
}

bool PtzInstrument::PtzData::hasCapabilities(Ptz::Capabilities value) const
{
    return (capabilities & value) == value;
}

bool PtzInstrument::PtzData::isFisheye() const
{
    return hasCapabilities(Ptz::VirtualPtzCapability);
}

bool PtzInstrument::PtzData::hasContinuousPanOrTilt() const
{
    return hasCapabilities(Ptz::ContinuousPanCapability)
        || hasCapabilities(Ptz::ContinuousTiltCapability);
}

bool PtzInstrument::PtzData::hasAdvancedPtz() const
{
    return hasCapabilities(Ptz::ViewportPtzCapability);
}

bool PtzInstrument::supportsContinuousPtz(
    QnMediaResourceWidget* widget, DirectionFlag direction) const
{
    if (!widget || (!widget->supportsBasicPtz() && !widget->options().testFlag(
        QnResourceWidget::DisplayDewarped)))
    {
        return false;
    }
    const auto caps = widget->ptzController()->getCapabilities();
    return caps.testFlag(requiredCapability(direction));
}

void PtzInstrument::toggleContinuousPtz(
    QnMediaResourceWidget* widget, DirectionFlag direction, bool on)
{
    if (dragProcessor()->isRunning()
        || !supportsContinuousPtz(widget, direction)
        || m_externalPtzDirections.testFlag(direction) == on
        || (!m_dataByWidget.value(widget).isFisheye() && !checkPlayingLive(widget)))
    {
        return;
    }

    NX_VERBOSE(this, "Toggle continuous PTZ: %1 %2",
        QMetaEnum::fromType<DirectionFlag>().valueToKey(int(direction)),
        on ? "on" : "off");

    setTarget(widget);

    m_externalPtzDirections.setFlag(direction, on);
    if (on)
        m_externalPtzDirections.setFlag(oppositeDirection(direction), false);

    if (direction == DirectionFlag::zoomIn || direction == DirectionFlag::zoomOut)
    {
        m_wheelZoomDirection = 0;
        m_wheelZoomTimer->stop();
    }

    updateExternalPtzSpeed();
}

void PtzInstrument::at_display_widgetAboutToBeChanged(Qn::ItemRole role)
{
    if (role != Qn::CentralRole)
        return;

    if (const auto* mediaWidget = qobject_cast<QnMediaResourceWidget*>(display()->widget(role)))
    {
        QnPtzObject ptzObject;
        auto ptzController = mediaWidget->ptzController();
        ptzController->getActiveObject(&ptzObject);

        const bool ptzStateActive = ptzObject.type != Qn::InvalidPtzObject;
        if (ptzStateActive)
            return;

        m_externalPtzDirections = {};

        // Forcefully stop ptz movement on a camera.
        if (mediaWidget->item()->layout())
        {
            if (const auto camera = mediaWidget->resource().dynamicCast<QnClientCameraResource>();
                camera && camera->autoSendPtzStopCommand())
            {
                menu()->triggerIfPossible(ui::action::PtzContinuousMoveAction, ui::action::Parameters()
                    .withArgument(Qn::ItemDataRole::PtzSpeedRole, QVariant::fromValue(QVector3D()))
                    .withArgument(Qn::ItemDataRole::ForceRole, true));
            }
        }
        else
        {
            // TODO: #sivanov Make sure stopping PTZ should occur in this case.
            // ptzController->continuousMove({});
        }
    }
}

void PtzInstrument::updateExternalPtzSpeed()
{
    if (!target() || !target()->item())
        return;

    const QVector3D directionMultiplier(
        -int(m_externalPtzDirections.testFlag(DirectionFlag::panRight))
        + int(m_externalPtzDirections.testFlag(DirectionFlag::panLeft)),
        -int(m_externalPtzDirections.testFlag(DirectionFlag::tiltUp))
        + int(m_externalPtzDirections.testFlag(DirectionFlag::tiltDown)),
        -int(m_externalPtzDirections.testFlag(DirectionFlag::zoomOut))
        + int(m_externalPtzDirections.testFlag(DirectionFlag::zoomIn)));

    const auto rotation = target()->item()->rotation()
        + (target()->item()->data<bool>(Qn::ItemFlipRole, false) ? 0.0 : 180.0);

    const auto camera = target()->resource().dynamicCast<QnSecurityCamResource>();
    const QPointF cameraSensitivity = camera ? camera->ptzPanTiltSensitivity() : QPointF();
    const QVector3D sensitivityVector(cameraSensitivity.x(), cameraSensitivity.y(), 1);
    const auto speed = applyRotation(
        kKeyboardPtzSensitivity * sensitivityVector * directionMultiplier, rotation);

    static constexpr qreal kWheelZoomSpeedFactor = 1.0;
    const auto wheelZoomSpeed = m_wheelZoomDirection * kWheelZoomSpeedFactor;

    // Stop PTZ before setting a new speed. Otherwise it works unexpectedly.
    ptzMove(target(), {}, true);

    ptzMove(target(), {speed.x(), speed.y(), 0, speed.z() + wheelZoomSpeed}, true);
}

QnResourceWidget* PtzInstrument::findPtzWidget(const QPointF& scenePos) const
{
    const auto isSuitableWidget =
        [scenePos](QnResourceWidget* widget) -> bool
        {
            return widget->options().testFlag(QnResourceWidget::Option::ControlPtz)
                && widget->rect().contains(widget->mapFromScene(scenePos));
        };

    auto widget = display()->widget(Qn::ZoomedRole);
    if (widget)
        return isSuitableWidget(widget) ? widget : nullptr;

    widget = display()->widget(Qn::RaisedRole);
    if (widget && widget->rect().contains(widget->mapFromScene(scenePos)))
        return widget->options().testFlag(QnResourceWidget::Option::ControlPtz) ? widget : nullptr;

    const auto widgets = display()->widgets();
    const auto iter = std::find_if(widgets.cbegin(), widgets.cend(), isSuitableWidget);
    return iter != widgets.cend() ? *iter : nullptr;
}

bool PtzInstrument::wheelEvent(QGraphicsScene* /*scene*/, QGraphicsSceneWheelEvent* event)
{
    const auto widget = qobject_cast<QnMediaResourceWidget*>(findPtzWidget(event->scenePos()));
    if (!widget)
        return false;

    const bool isFisheye = m_dataByWidget.value(widget).isFisheye();

    // Wheel events should not be delivered here in non-live mode,
    // but for additional safety this check is added.
    if (!isFisheye && !checkPlayingLive(widget))
        return true;

    widget->selectThisWidget();
    setTarget(widget);

    if (isFisheye)
    {
        Vector position;
        target()->ptzController()->getPosition(&position, CoordinateSpace::logical);

        position.zoom -= event->delta() / 120.0 * kWheelFisheyeZoomSensitivity;

        target()->ptzController()->absoluteMove(
            CoordinateSpace::logical,
            position,
            /*instant movement*/ 2.0);

        return true;
    }

    const auto remainingTime = m_wheelZoomTimer->isActive()
        ? m_wheelZoomTimer->remainingTimeAsDuration()
        : 0ms;

    static constexpr auto kMaxWheelDuration = 1000ms;
    m_wheelZoomTimer->start(qMin(remainingTime + 200ms, milliseconds(kMaxWheelDuration)));

    const int newDirection = (event->delta() > 0) ? 1 : -1;
    if (m_wheelZoomDirection == newDirection)
        return true;

    m_wheelZoomDirection = newDirection;
    updateExternalPtzSpeed();
    return true;
}

bool PtzInstrument::checkPlayingLive(QnMediaResourceWidget* widget) const
{
    if (widget->isPlayingLive())
        return true;

    if (!m_ptzInArchiveMessageBox)
        m_ptzInArchiveMessageBox = SceneBanner::show(tr("PTZ can only be used in the live mode"));

    return false;
}
