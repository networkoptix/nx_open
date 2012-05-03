#include "speed_slider.h"

#include <QtCore/QPropertyAnimation>

#include <QtGui/QGraphicsSceneMouseEvent>

#include <utils/common/warnings.h>
#include <utils/common/math.h>

namespace {
    inline qint64 speedToPosition(qreal speed, qreal minimalStep) {
        if(speed < -minimalStep) {
            return static_cast<qint64>((std::log(speed / -minimalStep) + 1.0) * -1000.0);
        } else if(speed < 1.0) {
            return static_cast<qint64>((speed / minimalStep) * 1000.0);
        } else {
            return static_cast<qint64>((std::log(speed /  minimalStep) + 1.0) *  1000.0);
        }
    }

    inline qreal positionToSpeed(qint64 position, qreal minimalStep) {
        if(position < -1000) {
            return std::exp(position / -1000.0 - 1.0) * -minimalStep;
        } else if(position < 1000) {
            return position / 1000.0 * minimalStep;
        } else {
            return std::exp(position /  1000.0 - 1.0) *  minimalStep;
        }
    }


} // anonymous namespace


QnSpeedSlider::QnSpeedSlider(QGraphicsItem *parent): 
    base_type(parent),
    m_animation(0),
    m_wheelStuckedTimerId(0),
    m_wheelStucked(false)
{
    setTickPosition(GraphicsSlider::NoTicks);

    /* Update tooltip text. */
    sliderChange(SliderValueChange);
}

QnSpeedSlider::~QnSpeedSlider() {
    if (m_animation) {
        m_animation->stop();
        delete m_animation;
    }

    if (m_wheelStuckedTimerId) {
        killTimer(m_wheelStuckedTimerId);
        m_wheelStuckedTimerId = 0;
    }
}

qreal QnSpeedSlider::speed() const {
    return positionToSpeed(value(), m_minimalSpeedStep);
}

qreal QnSpeedSlider::roundedSpeed() const {
    return positionToSpeed(qRound(value(), 1000ll), m_minimalSpeedStep);
}

void QnSpeedSlider::setSpeed(qreal speed) {
    setValue(speedToPosition(speed, m_minimalSpeedStep));
}

void QnSpeedSlider::resetSpeed() {
    setSpeed(m_defaultSpeed);
}

qreal QnSpeedSlider::minimalSpeed() const {
    return positionToSpeed(minimum(), m_minimalSpeedStep);
}

void QnSpeedSlider::setMinimalSpeed(qreal minimalSpeed) {
    setSpeedRange(minimalSpeed, qMax(maximalSpeed(), minimalSpeed));
}

qreal QnSpeedSlider::maximalSpeed() const {
    return positionToSpeed(maximum(), m_minimalSpeedStep);
}

void QnSpeedSlider::setMaximalSpeed(qreal maximalSpeed) {
    setSpeedRange(qMin(minimalSpeed(), maximalSpeed), maximalSpeed);
}

void QnSpeedSlider::setSpeedRange(qreal minimalSpeed, qreal maximalSpeed) {
    maximalSpeed = qMax(minimalSpeed, maximalSpeed);

    setRange(speedToPosition(minimalSpeed, m_minimalSpeedStep), speedToPosition(maximalSpeed, m_minimalSpeedStep));
}

qreal QnSpeedSlider::defaultSpeed() const {
    return m_defaultSpeed;
}

void QnSpeedSlider::setDefaultSpeed(qreal defaultSpeed) {
    defaultSpeed = qBound(minimalSpeed(), defaultSpeed, maximalSpeed());

    m_defaultSpeed = defaultSpeed;
}

qreal QnSpeedSlider::minimalSpeedStep() const {
    return m_minimalSpeedStep;
}

void QnSpeedSlider::setMinimalSpeedStep(qreal minimalSpeedStep) {
    if(minimalSpeedStep <= 0.0 || qFuzzyIsNull(minimalSpeedStep)) {
        qnWarning("Invalid minimal speed step '%1'.", minimalSpeedStep);
        return;
    }
    
    if(qFuzzyCompare(m_minimalSpeedStep, minimalSpeedStep))
        return;

    qreal oldMinimalSpeed = minimalSpeed();
    qreal oldMaximalSpeed = maximalSpeed();
    qreal oldSpeed = speed();

    m_minimalSpeedStep = minimalSpeedStep;

    setSpeedRange(oldMinimalSpeed, oldMaximalSpeed);
    setSpeed(oldSpeed);
}

void QnSpeedSlider::sliderChange(SliderChange change) {
    base_type::sliderChange(change);

    if (change == SliderValueChange) {
        qreal speed = roundedSpeed();
        setToolTip(!qFuzzyIsNull(speed) ? tr("%1x").arg(speed) : tr("Paused"));
        emit speedChanged(speed);
    }
}

void QnSpeedSlider::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    base_type::mouseReleaseEvent(event);

#if 0
    if (event->isAccepted()) {
        if (m_animation)
            delete m_animation;

        m_animation = new QPropertyAnimation(this, "value", this);
        m_animation->setEasingCurve(QEasingCurve::OutQuad);
        m_animation->setDuration(500);
        m_animation->setEndValue(idx2pos(presets[m_precision].defaultIndex));
        m_animation->start();
    }
#endif
}

void QnSpeedSlider::wheelEvent(QGraphicsSceneWheelEvent *e) {
#if 0
    e->accept();

    if (m_precision == HighPrecision && (e->modifiers() & Qt::ControlModifier) == 0) {
        int delta = e->delta();
        // in Qt scrolling to the right gives negative values.
        if (e->orientation() == Qt::Horizontal)
            delta = -delta;
        for (int i = qAbs(delta / 120); i > 0; --i)
            QMetaObject::invokeMethod(this, delta < 0 ? "frameBackward" : "frameForward", Qt::QueuedConnection); // TODO: wtf?
        return;
    }

    if (m_wheelStucked)
        return;

    // make a gap around the default value
    setTracking(false);
    const int oldPosition = sliderPosition();

    base_type::wheelEvent(e);

    const int newPosition = sliderPosition();
    const int defaultPosition = idx2pos(presets[m_precision].defaultIndex);
    if ((oldPosition < defaultPosition && defaultPosition <= newPosition)
        || (oldPosition > defaultPosition && defaultPosition >= newPosition)) {
        setSliderPosition(defaultPosition);
        m_wheelStucked = true;
        m_wheelStuckedTimerId = startTimer(500);
    }
    setTracking(true);
    triggerAction(SliderMove);
#endif
}

void QnSpeedSlider::timerEvent(QTimerEvent *event) {
#if 0
    if (event->timerId() == m_wheelStuckedTimerId) {
        killTimer(m_wheelStuckedTimerId);
        m_wheelStuckedTimerId = 0;

        m_wheelStucked = false;
    }
#endif

    base_type::timerEvent(event);
}

