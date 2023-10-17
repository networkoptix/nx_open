// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "speed_slider.h"

#include <QtCore/QtMath>
#include <QtGui/QWheelEvent>
#include <QtQuickWidgets/QQuickWidget>
#include <QtWidgets/QStyle>

#include <nx/utils/math/math.h>
#include <nx/vms/client/desktop/style/style.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/timeline/slider_tooltip.h>
#include <nx/vms/client/desktop/workbench/workbench_animations.h>
#include <ui/animation/variant_animator.h>
#include <ui/animation/widget_opacity_animator.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/workbench/workbench_display.h>
#include <utils/common/event_processors.h>

namespace nx::vms::client::desktop::workbench::timeline {

namespace {

using namespace std::chrono;

static constexpr int kAutoHideTimerIntervalMs = 100;
static constexpr int kAutoHideAfterSliderChangePeriodMs = 2500;
static constexpr int kTooltipYShift = -5;

inline int speedToPosition(qreal speed, qreal minimalStep)
{
    if (speed < -minimalStep)
        return static_cast<int>((std::log(speed / -minimalStep) / M_LN2 + 1.0) * -1000.0);

    if (speed < 1.0)
        return static_cast<int>((speed / minimalStep) * 1000.0);

    return static_cast<int>((std::log(speed / minimalStep) / M_LN2 + 1.0) *  1000.0);
}

inline qreal positionToSpeed(qint64 position, qreal minimalStep)
{
    if (position < -1000)
        return std::exp(M_LN2 * (position / -1000.0 - 1.0)) * -minimalStep;

    if (position < 1000)
        return position / 1000.0 * minimalStep;

    return std::exp(M_LN2 * (position / 1000.0 - 1.0)) *  minimalStep;
}

} // namespace

SpeedSlider::SpeedSlider(WindowContext* context, QWidget* parent):
    base_type(Qt::Horizontal, parent),
    m_animator(new VariantAnimator(this)),
    m_tooltip(new SliderToolTip(context))
{
    setSpeedRange(-16.0, 16.0);
    setSpeed(1.0);
    setPageStep(1000);

    /* Set up animator. */
    m_animator->setTargetObject(this);
    m_animator->setAccessor(new PropertyAccessor("value"));
    m_animator->setTimeLimit(500);
    m_animator->setSpeed(5000.0);
    m_animator->setTimer(context->display()->instrumentManager()->animationTimer());

    connect(this, &QSlider::sliderReleased, this, &SpeedSlider::restartSpeedAnimation);

    m_hideTooltipTimer.setSingleShot(true);

    connect(this, &QSlider::valueChanged,
        [this](int value)
        {
            if (m_restricted && (value < m_restrictValue))
            {
                setValue(m_restrictValue);
                emit sliderRestricted();
            }

            updateTooltipPos();
            showToolTip();
        });

    connect(&m_hideTooltipTimer, &QTimer::timeout,
        [this]
        {
            if (!calculateToolTipAutoVisibility())
                hideToolTip();
        });

    installEventHandler({parentWidget()}, {QEvent::Resize, QEvent::Move},
        this, &SpeedSlider::updateTooltipPos);

    /* Make sure that tooltip text is updated. */
    sliderChange(SliderValueChange);
}

SpeedSlider::~SpeedSlider()
{
}

void SpeedSlider::setTooltipsVisible(bool enabled)
{
    m_tooltipsEnabled = enabled;
    if (!m_tooltipsEnabled)
        hideToolTip();
}

qreal SpeedSlider::speed() const
{
    return positionToSpeed(value(), m_minimalSpeedStep);
}

qreal SpeedSlider::roundedSpeed() const
{
    return positionToSpeed(qRound(value(), 1000ll), m_minimalSpeedStep);
}

void SpeedSlider::setSpeed(qreal speed)
{
    if (m_restricted && (speed < m_restrictSpeed))
    {
        setValue(speedToPosition(m_restrictSpeed, m_minimalSpeedStep));
        return;
    }

    setValue(speedToPosition(speed, m_minimalSpeedStep));
}

void SpeedSlider::resetSpeed()
{
    setSpeed(m_defaultSpeed);
}

qreal SpeedSlider::minimalSpeed() const
{
    return positionToSpeed(minimum(), m_minimalSpeedStep);
}

void SpeedSlider::setMinimalSpeed(qreal minimalSpeed)
{
    setSpeedRange(minimalSpeed, qMax(maximalSpeed(), minimalSpeed));
}

qreal SpeedSlider::maximalSpeed() const
{
    return positionToSpeed(maximum(), m_minimalSpeedStep);
}

void SpeedSlider::setMaximalSpeed(qreal maximalSpeed)
{
    setSpeedRange(qMin(minimalSpeed(), maximalSpeed), maximalSpeed);
}

void SpeedSlider::setSpeedRange(qreal minimalSpeed, qreal maximalSpeed)
{
    maximalSpeed = qMax(minimalSpeed, maximalSpeed);
    setRange(speedToPosition(minimalSpeed, m_minimalSpeedStep),
        speedToPosition(maximalSpeed, m_minimalSpeedStep));
}

qreal SpeedSlider::defaultSpeed() const
{
    return m_defaultSpeed;
}

void SpeedSlider::setDefaultSpeed(qreal defaultSpeed)
{
    m_defaultSpeed = qBound(minimalSpeed(), defaultSpeed, maximalSpeed());

    if (m_animator->isRunning())
        restartSpeedAnimation();
}

qreal SpeedSlider::minimalSpeedStep() const
{
    return m_minimalSpeedStep;
}

void SpeedSlider::setMinimalSpeedStep(qreal minimalSpeedStep)
{
    if (minimalSpeedStep <= 0.0 || qFuzzyIsNull(minimalSpeedStep))
    {
        NX_ASSERT(false, "Invalid minimal speed step '%1'.", minimalSpeedStep);
        return;
    }

    if (qFuzzyCompare(m_minimalSpeedStep, minimalSpeedStep))
        return;

    qreal oldMinimalSpeed = minimalSpeed();
    qreal oldMaximalSpeed = maximalSpeed();
    qreal oldSpeed = speed();

    m_minimalSpeedStep = minimalSpeedStep;

    setSpeedRange(oldMinimalSpeed, oldMaximalSpeed);
    setSpeed(oldSpeed);

    if (m_animator->isRunning())
        restartSpeedAnimation();
}

void SpeedSlider::setRestrictSpeed(qreal restrictSpeed)
{
    m_restrictSpeed = restrictSpeed;
    m_restrictValue = speedToPosition(m_restrictSpeed, m_minimalSpeedStep);
}

void SpeedSlider::setRestrictEnable(bool enabled)
{
    if (m_restricted == enabled)
        return;

    m_restricted = enabled;
    if (m_restricted && (value() < m_restrictValue))
    {
        setValue(m_restrictValue);
        emit sliderRestricted();
    }
}

void SpeedSlider::hideToolTip()
{
    m_tooltip->hide();
}

void SpeedSlider::showToolTip()
{
    if (m_tooltipsEnabled)
        m_tooltip->show();
}

void SpeedSlider::finishAnimations()
{
    if (m_animator->isRunning())
    {
        m_animator->stop();
        setSpeed(m_defaultSpeed);
    }
}

void SpeedSlider::speedUp()
{
    triggerAction(SliderPageStepAdd);
}

void SpeedSlider::speedDown()
{
    triggerAction(SliderPageStepSub);
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void SpeedSlider::sliderChange(SliderChange change)
{
    base_type::sliderChange(change);

    if (change == SliderValueChange)
    {
        emit speedChanged(speed());

        m_hideTooltipTimer.setInterval(kAutoHideAfterSliderChangePeriodMs);
        m_hideTooltipTimer.start();

        qreal roundedSpeed = this->roundedSpeed();
        if (!qFuzzyEquals(roundedSpeed, m_roundedSpeed))
        {
            m_roundedSpeed = roundedSpeed;
            m_tooltip->setText(!qFuzzyIsNull(roundedSpeed) ? tr("%1x").arg(roundedSpeed) : tr("Paused"));

            emit roundedSpeedChanged(roundedSpeed);
        }
    }
}

void SpeedSlider::wheelEvent(QWheelEvent* event)
{
    emit wheelMoved(event->angleDelta().y() > 0);

    event->accept();
}

bool SpeedSlider::eventFilter(QObject* watched, QEvent* event)
{
    switch (event->type())
    {
        case QEvent::ToolTip:
            return true; // default tooltip shouldn't be displayed
        default:
            break;
    }

    return base_type::eventFilter(watched, event);
}

void SpeedSlider::restartSpeedAnimation()
{
    m_animator->animateTo(speedToPosition(m_defaultSpeed, m_minimalSpeedStep));
}

void SpeedSlider::updateTooltipPos()
{
    if (!m_tooltip)
        return;

    const int sliderHandleSize = style()->pixelMetric(QStyle::PM_SliderControlThickness);

    const QPoint localTooltipPos(QStyle::sliderPositionFromValue(minimum(), maximum(), value(),
        width() - sliderHandleSize), kTooltipYShift);

    m_tooltip->setTarget(QRect(mapToGlobal(localTooltipPos),
        QSize(sliderHandleSize, sliderHandleSize)));

    const QRect parentWindowGeometry = parentWidget()->window()->geometry().adjusted(1, 1, -1, -1);
    m_tooltip->setEnclosingRect(parentWindowGeometry);
}

void SpeedSlider::enterEvent(QEnterEvent* /*event*/)
{
    m_hideTooltipTimer.stop();
    updateTooltipPos();
    showToolTip();
}

void SpeedSlider::leaveEvent(QEvent* /*event*/)
{
    m_hideTooltipTimer.setInterval(kAutoHideTimerIntervalMs);
    m_hideTooltipTimer.start();
}

bool SpeedSlider::calculateToolTipAutoVisibility() const
{
    // Always show playback speed tooltip if speed is different from 0 or 1.
    const bool alwaysShowTooltip = !qFuzzyIsNull(m_roundedSpeed)
        && !qFuzzyEquals(m_roundedSpeed, 1.0);

    return alwaysShowTooltip || underMouse();
}

} // namespace nx::vms::client::desktop::workbench::timeline
