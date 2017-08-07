#include "speed_slider.h"

#include <QtWidgets/QGraphicsSceneMouseEvent>

#include <ui/animation/variant_animator.h>
#include <nx/client/desktop/ui/workbench/workbench_animations.h>

#include <utils/common/warnings.h>
#include <utils/math/math.h>

namespace {

inline qint64 speedToPosition(qreal speed, qreal minimalStep)
{
    if (speed < -minimalStep)
        return static_cast<qint64>((std::log(speed / -minimalStep) / M_LN2 + 1.0) * -1000.0);

    if (speed < 1.0)
        return static_cast<qint64>((speed / minimalStep) * 1000.0);

    return static_cast<qint64>((std::log(speed / minimalStep) / M_LN2 + 1.0) *  1000.0);
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


QnSpeedSlider::QnSpeedSlider(QGraphicsItem *parent):
    base_type(parent),
    m_roundedSpeed(0.0),
    m_minimalSpeedStep(1.0),
    m_defaultSpeed(1.0),
    m_animator(new VariantAnimator(this)),
    m_wheelStuck(false)
{
    setTickPosition(GraphicsSlider::NoTicks);
    setSpeedRange(-16.0, 16.0);
    setSpeed(1.0);
    setPageStep(1000);

    /* Set up animator. */
    m_animator->setTargetObject(this);
    m_animator->setAccessor(new PropertyAccessor("value"));
    m_animator->setTimeLimit(500);
    m_animator->setSpeed(5000.0);
    registerAnimation(m_animator);

    /* Set up handlers. */
    connect(this, &AbstractGraphicsSlider::sliderReleased, this,
        &QnSpeedSlider::restartSpeedAnimation);

    /* Make sure that tooltip text is updated. */
    sliderChange(SliderValueChange);
}

QnSpeedSlider::~QnSpeedSlider()
{
}

qreal QnSpeedSlider::speed() const
{
    return positionToSpeed(value(), m_minimalSpeedStep);
}

qreal QnSpeedSlider::roundedSpeed() const
{
    return positionToSpeed(qRound(value(), 1000ll), m_minimalSpeedStep);
}

void QnSpeedSlider::setSpeed(qreal speed)
{
    setValue(speedToPosition(speed, m_minimalSpeedStep));
}

void QnSpeedSlider::resetSpeed()
{
    setSpeed(m_defaultSpeed);
}

qreal QnSpeedSlider::minimalSpeed() const
{
    return positionToSpeed(minimum(), m_minimalSpeedStep);
}

void QnSpeedSlider::setMinimalSpeed(qreal minimalSpeed)
{
    setSpeedRange(minimalSpeed, qMax(maximalSpeed(), minimalSpeed));
}

qreal QnSpeedSlider::maximalSpeed() const
{
    return positionToSpeed(maximum(), m_minimalSpeedStep);
}

void QnSpeedSlider::setMaximalSpeed(qreal maximalSpeed)
{
    setSpeedRange(qMin(minimalSpeed(), maximalSpeed), maximalSpeed);
}

void QnSpeedSlider::setSpeedRange(qreal minimalSpeed, qreal maximalSpeed)
{
    maximalSpeed = qMax(minimalSpeed, maximalSpeed);
    setRange(speedToPosition(minimalSpeed, m_minimalSpeedStep),
        speedToPosition(maximalSpeed, m_minimalSpeedStep));
}

qreal QnSpeedSlider::defaultSpeed() const
{
    return m_defaultSpeed;
}

void QnSpeedSlider::setDefaultSpeed(qreal defaultSpeed)
{
    m_defaultSpeed = qBound(minimalSpeed(), defaultSpeed, maximalSpeed());

    if (m_animator->isRunning())
        restartSpeedAnimation();
}

qreal QnSpeedSlider::minimalSpeedStep() const
{
    return m_minimalSpeedStep;
}

void QnSpeedSlider::setMinimalSpeedStep(qreal minimalSpeedStep)
{
    if (minimalSpeedStep <= 0.0 || qFuzzyIsNull(minimalSpeedStep))
    {
        qnWarning("Invalid minimal speed step '%1'.", minimalSpeedStep);
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

void QnSpeedSlider::restartSpeedAnimation()
{
    m_animator->animateTo(speedToPosition(m_defaultSpeed, m_minimalSpeedStep));
}

void QnSpeedSlider::finishAnimations()
{
    if (m_animator->isRunning())
    {
        m_animator->stop();
        setSpeed(m_defaultSpeed);
    }
}

void QnSpeedSlider::speedUp()
{
    triggerAction(SliderPageStepAdd);
}

void QnSpeedSlider::speedDown()
{
    triggerAction(SliderPageStepSub);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnSpeedSlider::sliderChange(SliderChange change)
{
    base_type::sliderChange(change);

    if (change == SliderValueChange)
    {
        emit speedChanged(speed());

        qreal roundedSpeed = this->roundedSpeed();
        if (!qFuzzyEquals(roundedSpeed, m_roundedSpeed))
        {
            m_roundedSpeed = roundedSpeed;
            setToolTip(!qFuzzyIsNull(roundedSpeed) ? tr("%1x").arg(roundedSpeed) : tr("Paused"));
            updateToolTipAutoVisibility();

            emit roundedSpeedChanged(roundedSpeed);
        }
    }
}

void QnSpeedSlider::wheelEvent(QGraphicsSceneWheelEvent *event)
{
    /*
    * This logic is currently unused. Timeline speed slider mouse wheel
    * events are handled in QnNavigationItem::at_speedSlider_wheelEvent.
    */

    event->accept();

    if (m_wheelStuck)
        return;

    setTracking(false);

    /* Wheel should snap to default speed. */
    const qint64 oldPosition = sliderPosition();
    base_type::wheelEvent(event);
    const qint64 newPosition = sliderPosition();

    const qint64 defaultPosition = speedToPosition(m_defaultSpeed, m_minimalSpeedStep);
    if ((oldPosition < defaultPosition && defaultPosition <= newPosition) || (oldPosition > defaultPosition && defaultPosition >= newPosition))
    {
        setSliderPosition(defaultPosition);
        m_wheelStuck = true;
        m_wheelStuckTimer.start(500, this);
    }

    setTracking(true);
    triggerAction(SliderMove);
}

bool QnSpeedSlider::calculateToolTipAutoVisibility() const
{
    // Always show playback speed tooltip if speed is different from 0 or 1.
    const bool alwaysShowTooltip = !qFuzzyIsNull(m_roundedSpeed)
        && !qFuzzyEquals(m_roundedSpeed, 1.0);

    return alwaysShowTooltip || base_type::calculateToolTipAutoVisibility();
}

void QnSpeedSlider::setupShowAnimator(VariantAnimator* animator) const
{
    using namespace nx::client::desktop::ui::workbench;
    qnWorkbenchAnimations->setupAnimator(animator,
        Animations::Id::SpeedTooltipShow);
}

void QnSpeedSlider::setupHideAnimator(VariantAnimator* animator) const
{
    using namespace nx::client::desktop::ui::workbench;
    qnWorkbenchAnimations->setupAnimator(animator,
        Animations::Id::SpeedTooltipHide);
}

void QnSpeedSlider::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_wheelStuckTimer.timerId())
    {
        m_wheelStuckTimer.stop();
        m_wheelStuck = false;
    }

    base_type::timerEvent(event);
}
