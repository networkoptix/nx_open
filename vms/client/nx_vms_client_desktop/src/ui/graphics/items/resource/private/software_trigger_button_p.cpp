// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "software_trigger_button_p.h"

#include <numbers>

#include <QtCore/QScopedValueRollback>
#include <QtWidgets/QApplication>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/common/widgets/busy_indicator.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/software_trigger_pixmaps.h>
#include <ui/workaround/sharp_pixmap_painting.h>
#include <utils/common/delayed.h>
#include <utils/common/event_processors.h>
#include <utils/common/scoped_painter_rollback.h>

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

} // namespace

namespace nx::vms::client::desktop {

using State = CameraButton::State;

SoftwareTriggerButtonPrivate::SoftwareTriggerButtonPrivate(SoftwareTriggerButton* main):
    base_type(nullptr),
    q_ptr(main)
{
    Q_Q(SoftwareTriggerButton);

    connect(main, &SoftwareTriggerButton::pressed, this,
        [q]() { q->setState(State::Default); });

    installEventHandler(main, QEvent::PaletteChange, this,
        [this, q]()
        {
            m_imagesDirty = true;
            q->update();
        });

    connect(q, &CameraButton::iconChanged, this, [this]() { m_imagesDirty = true; });
    connect(q, &CameraButton::stateChanged, this, [this]() { updateState(); });
}

SoftwareTriggerButtonPrivate::~SoftwareTriggerButtonPrivate()
{
    cancelScheduledChange();
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
            if (q->prolonged())
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
            if (q->prolonged())
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
            if (q->prolonged())
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
    if (q->prolonged() && q->isPressed() && state == State::Default)
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
    Q_Q(SoftwareTriggerButton);

    if (!q->isLive())
    {
        q->base_type::paint(painter, option, widget);
        return;
    }

    ensureImages();

    const auto state = q->state();
    const auto effectiveState =
        q->prolonged() && q->isPressed() && state != State::Failure
            ? State::Default
            : state;

    const auto currentOpacity =
        [this]() -> qreal
        {
            static const qreal kTwoPi = std::numbers::pi * 2.0;
            const auto angle = m_animationTime.elapsed() * kTwoPi / kAnimationPeriodMs;
            const auto value = (cos(angle) + 1.0) / 2.0; // [0...1]
            return kAnimationMinimumOpacity + value * (1.0 - kAnimationMinimumOpacity);
        };

    QnScopedPainterOpacityRollback opacityRollback(painter);

    if (q->isDisabled())
        painter->setOpacity(painter->opacity() * kDisabledTriggerOpacity);

    switch (effectiveState)
    {
        case State::Default:
        {
            q->base_type::paint(painter, option, widget);
            if (q->prolonged() && q->isPressed())
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
            if (q->prolonged() && !q->isPressed())
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
}

} // namespace nx::vms::client::desktop
