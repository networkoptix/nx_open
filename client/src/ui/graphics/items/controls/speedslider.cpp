#include "speedslider.h"

#include <QtCore/QPropertyAnimation>

#include <QtGui/QGraphicsSceneMouseEvent>

#include "ui/style/proxy_style.h"

static const struct Preset {
    int size;
    int defaultIndex;
    float preset[10];
} presets[] = {
    { 10, 5, { -16.0, -8.0, -4.0, -2.0, -1.0, 1.0, 2.0, 4.0, 8.0, 16.0 } },  // LowPrecision
    { 9, 4, { -2.0, -1.0, -0.5, -0.25, 0.0, 0.25, 0.5, 1.0, 2.0 } }          // HighPrecision
};
static inline int idx2pos(int idx) { return idx * 10 + 5; }
static inline int pos2idx(int pos) { return pos / 10; }

QnSpeedSlider::QnSpeedSlider(QGraphicsItem *parent): 
    base_type(parent),
    m_precision(HighPrecision),
    m_animation(0),
    m_wheelStuckedTimerId(0),
    m_wheelStucked(false)
{
    setSingleStep(5);
    setPageStep(10);
    setTickInterval(10);
    setTickPosition(GraphicsSlider::NoTicks);

    setPrecision(LowPrecision);

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

QnSpeedSlider::Precision QnSpeedSlider::precision() const {
    return m_precision;
}

void QnSpeedSlider::setPrecision(QnSpeedSlider::Precision precision) {
    if (m_precision == precision)
        return;

    m_precision = precision;

    // power of 10 in order to allow slider animation
    setRange(idx2pos(0), idx2pos(presets[int(m_precision)].size - 1));
    resetSpeed();
}

void QnSpeedSlider::resetSpeed() {
    setSliderPosition(idx2pos(presets[int(m_precision)].defaultIndex));
}

void QnSpeedSlider::stepBackward() {
    triggerAction(SliderPageStepSub);
}

void QnSpeedSlider::stepForward() {
    triggerAction(SliderPageStepAdd);
}

void QnSpeedSlider::sliderChange(SliderChange change) {
    base_type::sliderChange(change);

    if (change == SliderValueChange || change == SliderRangeChange) {
        const Preset &preset = presets[int(m_precision)];

        int idx = pos2idx(value());
        Q_ASSERT(idx >= 0 && idx < preset.size);
        float newSpeed = preset.preset[idx];

        int defaultIndex = preset.defaultIndex;
        if ((defaultIndex == 0 || newSpeed > preset.preset[defaultIndex - 1])
            && (defaultIndex == preset.size - 1 || newSpeed < preset.preset[defaultIndex + 1])) {
            newSpeed = preset.preset[defaultIndex]; // make a gap around the default value
        }

        setToolTip(!qFuzzyIsNull(newSpeed) ? tr("%1x").arg(newSpeed) : tr("Paused"));

        emit speedChanged(newSpeed);
    }
}

void QnSpeedSlider::setSpeed(int value) {
    const Preset &preset = presets[int(m_precision)];
    for (int i = 0; i < preset.size; i++) {
        if (preset.preset[i] == value) {
            int pos = idx2pos(i);
            setValue(pos);
            return;
        }
    }
    qWarning() << "Unsupported value" << value << "for speed slider";
    return;
}

void QnSpeedSlider::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    base_type::mouseReleaseEvent(event);

    if (event->isAccepted() && value() != 10) {
        if (m_animation)
            delete m_animation;

        m_animation = new QPropertyAnimation(this, "value", this);
        m_animation->setEasingCurve(QEasingCurve::OutQuad);
        m_animation->setDuration(500);
        m_animation->setEndValue(idx2pos(presets[int(m_precision)].defaultIndex));
        m_animation->start();
    }
}

void QnSpeedSlider::wheelEvent(QGraphicsSceneWheelEvent *e) {
    e->accept();

    if (m_precision == HighPrecision && (e->modifiers() & Qt::ControlModifier) == 0) {
        int delta = e->delta();
        // in Qt scrolling to the right gives negative values.
        if (e->orientation() == Qt::Horizontal)
            delta = -delta;
        for (int i = qAbs(delta / 120); i > 0; --i)
            QMetaObject::invokeMethod(this, delta < 0 ? "frameBackward" : "frameForward", Qt::QueuedConnection);
        return;
    }

    if (m_wheelStucked)
        return;

    // make a gap around the default value
    setTracking(false);
    const int oldPosition = sliderPosition();

    base_type::wheelEvent(e);

    const int newPosition = sliderPosition();
    const int defaultPosition = idx2pos(presets[int(m_precision)].defaultIndex);
    if ((oldPosition < defaultPosition && defaultPosition <= newPosition)
        || (oldPosition > defaultPosition && defaultPosition >= newPosition)) {
        setSliderPosition(defaultPosition);
        m_wheelStucked = true;
        m_wheelStuckedTimerId = startTimer(500);
    }
    setTracking(true);
    triggerAction(SliderMove);
}

void QnSpeedSlider::timerEvent(QTimerEvent *event) {
    if (event->timerId() == m_wheelStuckedTimerId) {
        killTimer(m_wheelStuckedTimerId);
        m_wheelStuckedTimerId = 0;

        m_wheelStucked = false;
    }

    base_type::timerEvent(event);
}

