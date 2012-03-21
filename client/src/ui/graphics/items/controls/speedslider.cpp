#include "speedslider.h"

#include <QtCore/QPropertyAnimation>

#include "ui/style/proxy_style.h"

#include "ui/graphics/items/tool_tip_item.h"

class SpeedSliderProxyStyle : public QnProxyStyle
{
public:
    SpeedSliderProxyStyle(QObject *parent = 0) : QnProxyStyle(0, parent) {}

    int styleHint(StyleHint hint, const QStyleOption *option = 0, const QWidget *widget = 0, QStyleHintReturn *returnData = 0) const
    {
        if (hint == QStyle::SH_Slider_AbsoluteSetButtons)
            return Qt::LeftButton;
        return QnProxyStyle::styleHint(hint, option, widget, returnData);
    }

    QRect subControlRect(ComplexControl cc, const QStyleOptionComplex *opt, SubControl sc, const QWidget *widget = 0) const
    {
        QRect r = QnProxyStyle::subControlRect(cc, opt, sc, widget);
        if (cc == CC_Slider && sc == SC_SliderHandle) {
            int side = qMin(r.width(), r.height());
            if (qstyleoption_cast<const QStyleOptionSlider *>(opt)->orientation == Qt::Horizontal)
                r.setWidth(side);
            else
                r.setHeight(side);
        }
        return r;
    }
};


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

SpeedSlider::SpeedSlider(Qt::Orientation orientation, QGraphicsItem *parent)
    : GraphicsSlider(orientation, parent),
      m_precision(HighPrecision),
      m_animation(0),
      m_toolTipTimerId(0),
      m_wheelStuckedTimerId(0),
      m_wheelStucked(false)
{
    m_toolTip = new QnStyledToolTipItem(this);
    m_toolTip->setVisible(false);

    setStyle(new SpeedSliderProxyStyle(this));

    setSingleStep(5);
    setPageStep(10);
    setTickInterval(10);
    setTickPosition(GraphicsSlider::NoTicks);

    setPrecision(LowPrecision);

    connect(this, SIGNAL(speedChanged(float)), this, SLOT(onSpeedChanged(float)));
}

SpeedSlider::~SpeedSlider()
{
    if (m_animation)
    {
        m_animation->stop();
        delete m_animation;
    }

    if (m_toolTipTimerId)
    {
        killTimer(m_toolTipTimerId);
        m_toolTipTimerId = 0;
    }

    if (m_wheelStuckedTimerId)
    {
        killTimer(m_wheelStuckedTimerId);
        m_wheelStuckedTimerId = 0;
    }
}

SpeedSlider::Precision SpeedSlider::precision() const
{
    return m_precision;
}

void SpeedSlider::setPrecision(SpeedSlider::Precision precision)
{
    if (m_precision == precision)
        return;

    m_precision = precision;

    // power of 10 in order to allow slider animation
    setRange(idx2pos(0), idx2pos(presets[int(m_precision)].size - 1));
    resetSpeed();
}

void SpeedSlider::setToolTipItem(QnToolTipItem *toolTip)
{
    if (m_toolTip == toolTip)
        return;

    delete m_toolTip;

    m_toolTip = toolTip;

    if (m_toolTip) {
        m_toolTip->setParentItem(this);
        m_toolTip->setVisible(false);
    }
}

void SpeedSlider::resetSpeed()
{
    setSliderPosition(idx2pos(presets[int(m_precision)].defaultIndex));
}

void SpeedSlider::stepBackward()
{
    triggerAction(SliderPageStepSub);
}

void SpeedSlider::stepForward()
{
    triggerAction(SliderPageStepAdd);
}

void SpeedSlider::sliderChange(SliderChange change)
{
    GraphicsSlider::sliderChange(change);

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

        emit speedChanged(newSpeed);
    }
}

void SpeedSlider::setSpeed(int value)
{
    const Preset &preset = presets[int(m_precision)];
    for (int i = 0; i < preset.size; i++)
    {
        if (preset.preset[i] == value) 
        {
            int pos = idx2pos(i);
            GraphicsSlider::setValue(pos);
            return;
        }
    }
    qWarning() << "Unsupported value" << value << "for speed slider";
    return;
}

void SpeedSlider::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    GraphicsSlider::mouseReleaseEvent(event);

    if (event->isAccepted() && value() != 10)
    {
        if (m_animation)
            delete m_animation;

        m_animation = new QPropertyAnimation(this, "value", this);
        m_animation->setEasingCurve(QEasingCurve::OutQuad);
        m_animation->setDuration(500);
        m_animation->setEndValue(idx2pos(presets[int(m_precision)].defaultIndex));
        m_animation->start();
    }
}

#ifndef QT_NO_WHEELEVENT
void SpeedSlider::wheelEvent(QGraphicsSceneWheelEvent *e)
{
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

    GraphicsSlider::wheelEvent(e);

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
#endif

void SpeedSlider::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_toolTipTimerId)
    {
        killTimer(m_toolTipTimerId);
        m_toolTipTimerId = 0;

        if (m_toolTip)
            m_toolTip->setVisible(false);
    }
    else if (event->timerId() == m_wheelStuckedTimerId)
    {
        killTimer(m_wheelStuckedTimerId);
        m_wheelStuckedTimerId = 0;

        m_wheelStucked = false;
    }

    GraphicsSlider::timerEvent(event);
}

void SpeedSlider::onSpeedChanged(float newSpeed)
{
    if (m_toolTipTimerId)
        killTimer(m_toolTipTimerId);
    m_toolTipTimerId = startTimer(2500);

    if (m_toolTip) {
        QStyleOptionSlider opt;
        initStyleOption(&opt);
        const QRectF sliderRect = QRectF(style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle));
        m_toolTip->setPos(sliderRect.center().x(), sliderRect.top());
        m_toolTip->setText(!qFuzzyIsNull(newSpeed) ? tr("%1x").arg(newSpeed) : tr("Paused"));
        m_toolTip->setVisible(true);
    }
}
