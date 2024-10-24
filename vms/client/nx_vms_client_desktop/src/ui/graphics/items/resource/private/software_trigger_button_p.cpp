// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "software_trigger_button_p.h"

#include <cmath>

#include <QtCore/QScopedValueRollback>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsDropShadowEffect>

#include <ui/animation/opacity_animator.h>
#include <ui/graphics/items/generic/slider_tooltip_widget.h>
#include <ui/processors/hover_processor.h>
#include <nx/vms/client/core/skin/icon.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/software_trigger_pixmaps.h>
#include <ui/workaround/sharp_pixmap_painting.h>
#include <utils/common/delayed.h>
#include <utils/common/event_processors.h>
#include <utils/common/scoped_painter_rollback.h>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/common/widgets/busy_indicator.h>

namespace {

/* Duration of success and failure notifications staying up: */
static constexpr int kNotificationDurationMs = 1200;

/* Opacity for disabled button: */
static const qreal kDisabledTriggerOpacity = nx::style::Hints::kDisabledItemOpacity;

/* A short delay in State::Waiting before busy indicator appears: */
static constexpr int kBusyIndicatorDelayMs = 150;

/* Geometric adjustments: */
static constexpr qreal kBusyIndicatorDotRadius = 2.5;
static constexpr unsigned int kBusyIndicatorDotSpacing = 3;

/* Prolonged trigger frame pulsating opacity parameters: */
static constexpr int kAnimationPeriodMs = 1200;
static constexpr qreal kAnimationMinimumOpacity = 0.15;

/* Tooltip adjustments: */
static constexpr int kHoverEnterDelayMs = 0;
static constexpr int kHoverLeaveDelayMs = 25;
static constexpr qreal kToolTipFadeTimeMs = 25;
static constexpr qreal kToolTipRoundingRadius = 2.0;
static constexpr qreal kToolTipPadding = 8.0;
static constexpr qreal kToolTipShadowBlurRadius = 5.0;
static constexpr qreal kToolTipShadowOpacity = 0.36;
static const QPointF kToolTipShadowOffset(0.0, 3.0);

static Qt::Edge invertEdge(Qt::Edge edge)
{
    switch (edge)
    {
        case Qt::LeftEdge:
            return Qt::RightEdge;

        case Qt::RightEdge:
            return Qt::LeftEdge;

        case Qt::TopEdge:
            return Qt::BottomEdge;

        case Qt::BottomEdge:
            return Qt::TopEdge;

        default:
            NX_ASSERT(false);
            return Qt::LeftEdge;
    }
}

static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kLight1Theme = {
    {QnIcon::Normal, {.primary = "light1"}},
};

NX_DECLARE_COLORIZED_ICON(kGoToLiveIcon, "24x24/Solid/go_to_live.svg", kLight1Theme)

} // namespace

namespace nx::vms::client::desktop {

using State = CameraButton::State;

SoftwareTriggerButtonPrivate::SoftwareTriggerButtonPrivate(SoftwareTriggerButton* main):
    base_type(nullptr),
    q_ptr(main),
    m_toolTip(new QnSliderTooltipWidget(main)),
    m_toolTipHoverProcessor(new HoverFocusProcessor(main))
{
    Q_Q(SoftwareTriggerButton);

    connect(main, &SoftwareTriggerButton::geometryChanged,
        this, &SoftwareTriggerButtonPrivate::updateToolTipPosition);

    connect(main, &SoftwareTriggerButton::pressed, this,
        [q]() { q->setState(State::Default); });

    installEventHandler(main, QEvent::PaletteChange, this,
        [this, q]()
        {
            m_imagesDirty = true;
            q->update();
        });

    m_toolTipHoverProcessor->addTargetItem(main);
    m_toolTipHoverProcessor->setHoverEnterDelay(kHoverEnterDelayMs);
    m_toolTipHoverProcessor->setHoverLeaveDelay(kHoverLeaveDelayMs);

    connect(m_toolTipHoverProcessor, &HoverFocusProcessor::hoverEntered,
        this, &SoftwareTriggerButtonPrivate::updateToolTipVisibility);
    connect(m_toolTipHoverProcessor, &HoverFocusProcessor::hoverLeft,
        this, &SoftwareTriggerButtonPrivate::updateToolTipVisibility);

    m_toolTip->setOpacity(0.0);
    m_toolTip->setRoundingRadius(kToolTipRoundingRadius);
    m_toolTip->setFlag(QGraphicsItem::ItemIgnoresTransformations, false);

    auto shadowEffect = new QGraphicsDropShadowEffect(m_toolTip);
    auto shadowColor = QPalette().color(QPalette::Shadow);
    shadowColor.setAlphaF(kToolTipShadowOpacity);
    shadowEffect->setColor(shadowColor);
    shadowEffect->setOffset(kToolTipShadowOffset);
    shadowEffect->setBlurRadius(kToolTipShadowBlurRadius);
    m_toolTip->setGraphicsEffect(shadowEffect);

    updateToolTipTailEdge();

    connect(q, &CameraButton::iconChanged, this, [this]() { m_imagesDirty = true; });

    const auto updateTooltipVisibilityConnections =
        [this, q]()
        {
            m_imagesDirty = true;
            if (q->prolonged())
            {
                connect(q, &SoftwareTriggerButton::pressed,
                    this, &SoftwareTriggerButtonPrivate::updateToolTipVisibility);
                connect(q, &SoftwareTriggerButton::released,
                    this, &SoftwareTriggerButtonPrivate::updateToolTipVisibility);
            }
            else
            {
                disconnect(q, &SoftwareTriggerButton::pressed,
                    this, &SoftwareTriggerButtonPrivate::updateToolTipVisibility);
                disconnect(q, &SoftwareTriggerButton::released,
                    this, &SoftwareTriggerButtonPrivate::updateToolTipVisibility);
            }
            if (q->scene())
                updateToolTipVisibility();
        };

    connect(q, &CameraButton::prolongedChanged, this, updateTooltipVisibilityConnections);
    updateTooltipVisibilityConnections();

    connect(q, &CameraButton::stateChanged, this, [this]() { updateState(); });
    connect(q, &CameraButton::isLiveChanged, this,
        [this, q]()
        {
            updateToolTipText();
            q->update();
        });
}

SoftwareTriggerButtonPrivate::~SoftwareTriggerButtonPrivate()
{
    cancelScheduledChange();
}

QString SoftwareTriggerButtonPrivate::toolTip() const
{
    return m_toolTip->text();
}

void SoftwareTriggerButtonPrivate::setToolTip(const QString& toolTip)
{
    if (m_toolTipText == toolTip)
        return;

    m_toolTipText = toolTip;
    updateToolTipText();
}

void SoftwareTriggerButtonPrivate::updateToolTipText()
{
    m_toolTip->setText(m_live
        ? m_toolTipText
        : SoftwareTriggerButton::tr("Go to Live"));

    updateToolTipPosition();
}

Qt::Edge SoftwareTriggerButtonPrivate::toolTipEdge() const
{
    return m_toolTipEdge;
}

void SoftwareTriggerButtonPrivate::setToolTipEdge(Qt::Edge edge)
{
    if (m_toolTipEdge == edge)
        return;

    m_toolTipEdge = edge;
    updateToolTipTailEdge();
}

void SoftwareTriggerButtonPrivate::updateToolTipVisibility()
{
    static constexpr qreal kTransparent = 0.0;
    static constexpr qreal kOpaque = 1.0;

    Q_Q(const SoftwareTriggerButton);

    const bool showToolTip = m_toolTipHoverProcessor->isHovered()
        && !m_toolTip->text().isEmpty()
        && !(m_prolonged && q->isPressed());

    static constexpr qreal kToolTipAnimationSpeedFactor = 1000.0 / kToolTipFadeTimeMs;

    const qreal targetOpacity = showToolTip ? kOpaque : kTransparent;
    opacityAnimator(m_toolTip, kToolTipAnimationSpeedFactor)->animateTo(targetOpacity);
}

void SoftwareTriggerButtonPrivate::updateToolTipTailEdge()
{
    m_toolTip->setTailEdge(invertEdge(m_toolTipEdge));
    updateToolTipPosition();
}

void SoftwareTriggerButtonPrivate::updateToolTipPosition()
{
    Q_Q(const SoftwareTriggerButton);
    const auto size = q->size();

    switch (m_toolTipEdge)
    {
        case Qt::LeftEdge:
            m_toolTip->pointTo({ -kToolTipPadding, size.height() / 2 });
            break;

        case Qt::RightEdge:
            m_toolTip->pointTo({ size.width() + kToolTipPadding - 1, size.height() / 2 });
            break;

        case Qt::TopEdge:
            m_toolTip->pointTo({ size.width() / 2, -kToolTipPadding });
            break;

        case Qt::BottomEdge:
            m_toolTip->pointTo({ size.width() / 2, size.height() + kToolTipPadding - 1 });
            break;

        default:
            NX_ASSERT(false);
    }
}

void SoftwareTriggerButtonPrivate::updateState()
{
    if (m_updatingState)
        return;

    Q_Q(SoftwareTriggerButton);

    const auto state = q->state();

    const QScopedValueRollback updatingStateRollback(m_updatingState, true);

    cancelScheduledChange();
    m_busyIndicator.reset(nullptr);

    const auto setDefaultState =
        [q]()
        {
            q->setState(State::Default);
        };

    q->update();

    switch (state)
    {
        case State::Default:
            break;

        case State::WaitingForActivation:
        case State::WaitingForDeactivation:
        {
            if (m_prolonged)
            {
                /* Immediately reset to normal for released prolonged triggers: */
                if (!q->isPressed())
                    setDefaultState();

                /* Don't show busy indicator for prolonged triggers. */
                break;
            }

            const auto showBusyIndicator =
                [this]()
                {
                    Q_Q(SoftwareTriggerButton);
                    m_busyIndicator.reset(new BusyIndicatorGraphicsWidget(q));
                    m_busyIndicator->setPreferredSize(q->size());
                    m_busyIndicator->dots()->setDotRadius(kBusyIndicatorDotRadius);
                    m_busyIndicator->dots()->setDotSpacing(kBusyIndicatorDotSpacing);
                };

            scheduleChange(showBusyIndicator, kBusyIndicatorDelayMs);
            break;
        }

        case State::Activate:
        {
            if (m_prolonged)
            {
                /* Immediately reset to normal for prolonged triggers: */
                setDefaultState();
                break;
            }

            scheduleChange(setDefaultState, kNotificationDurationMs);
            break;
        }

        case State::Failure:
        {
            if (m_prolonged)
            {
                if (q->isPressed())
                {
                    /* Release pressed prolonged trigger in case of error: */
                    q->ungrabMouse();
                }
                else
                {
                    /* Restart animation timer: */
                    m_animationTime.start();

                    /* Don't do timed reset to normal for released prolonged triggers: */
                    break;
                }
            }

            scheduleChange(setDefaultState, kNotificationDurationMs);
            break;
        }

        default:
        {
            NX_ASSERT(false); // should never get here
            break;
        }
    }

    /* Restart animation timer for active prolonged trigger: */
    if (m_prolonged && q->isPressed() && state == State::Default)
        m_animationTime.start();
}

void SoftwareTriggerButtonPrivate::scheduleChange(std::function<void()> callback, int delayMs)
{
    if (m_scheduledChangeTimer)
    {
        m_scheduledChangeTimer->stop();
        m_scheduledChangeTimer->deleteLater();
    }

    if (callback)
        m_scheduledChangeTimer = executeDelayedParented(callback, delayMs, this);
}

void SoftwareTriggerButtonPrivate::cancelScheduledChange()
{
    scheduleChange(std::function<void()>(), 0);
}

void SoftwareTriggerButtonPrivate::paint(QPainter* painter,
    const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    ensureImages();

    Q_Q(SoftwareTriggerButton);
    const auto state = q->state();
    if (!m_live && (q->isHovered() || q->isPressed()))
    {
        paintPixmapSharp(painter, q->isPressed()
            ? m_goToLivePixmapPressed
            : m_goToLivePixmap);
        return;
    }

    const auto effectiveState =
        m_prolonged && q->isPressed() && state != State::Failure
            ? State::Default
            : state;

    const auto currentOpacity =
        [this]() -> qreal
        {
            static const qreal kTwoPi = 3.1415926 * 2.0;
            const auto angle = m_animationTime.elapsed() * kTwoPi / kAnimationPeriodMs;
            const auto value = (cos(angle) + 1.0) / 2.0; // [0...1]
            return kAnimationMinimumOpacity + value * (1.0 - kAnimationMinimumOpacity);
        };

    QnScopedPainterOpacityRollback opacityRollback(painter);

    if (q->isDisabled() || !m_live)
        painter->setOpacity(painter->opacity() * kDisabledTriggerOpacity);

    switch (effectiveState)
    {
        case State::Default:
        {
            q->SoftwareTriggerButton::base_type::paint(painter, option, widget);
            if (m_prolonged && q->isPressed())
            {
                painter->setOpacity(painter->opacity() * currentOpacity());
                paintPixmapSharp(painter, m_activityFramePixmap);
            }
            break;
        }

        case State::WaitingForActivation:
        case State::WaitingForDeactivation:
        {
            paintPixmapSharp(painter, m_waitingPixmap);
            break;
        }

        case State::Activate:
        {
            paintPixmapSharp(painter, m_successPixmap); //< already with frame
            break;
        }

        case State::Failure:
        {
            paintPixmapSharp(painter, m_failurePixmap);
            if (m_prolonged && !q->isPressed())
                painter->setOpacity(painter->opacity() * currentOpacity());

            paintPixmapSharp(painter, m_failureFramePixmap);
            break;
        }

        default:
        {
            NX_ASSERT(false); // should never get here
            break;
        }
    }
}

void SoftwareTriggerButtonPrivate::ensureImages()
{
    if (!m_imagesDirty)
        return;

    m_imagesDirty = false;

    Q_Q(SoftwareTriggerButton);

    static const QColor kErrorBackgroundColor = core::colorTheme()->color("red", 128);
    static const QColor kErrorFrameColor = core::colorTheme()->color("red");

    m_waitingPixmap = q->generatePixmap(
        q->pressedColor(),
        QColor(),
        QPixmap());

    m_successPixmap = q->generatePixmap(
        q->pressedColor(),
        q->palette().color(QPalette::Text),
        qnSkin->pixmap("24x24/Outline/yes.svg"));

    m_failurePixmap = q->generatePixmap(
        kErrorBackgroundColor,
        QColor(),
        qnSkin->pixmap("24x24/Outline/no.svg"));

    m_failureFramePixmap = q->generatePixmap(
        QColor(),
        kErrorFrameColor,
        QPixmap());

    m_activityFramePixmap = q->generatePixmap(
        QColor(),
        q->palette().color(QPalette::Text),
        QPixmap());

    const auto goToLivePixmap = qnSkin->icon(kGoToLiveIcon).pixmap(24, 24);

    m_goToLivePixmap = q->generatePixmap(
        q->activeColor(),
        QColor(),
        goToLivePixmap);

    m_goToLivePixmapPressed = q->generatePixmap(
        q->pressedColor(),
        QColor(),
        goToLivePixmap);
}

} // namespace nx::vms::client::desktop
