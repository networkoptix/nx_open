#include "speedslider.h"

#include <QtCore/QPropertyAnimation>

#include "tooltipitem.h"

#include "ui/videoitem/timeslider.h" // SliderProxyStyle

class SpeedSliderProxyStyle : public SliderProxyStyle
{
public:
    SpeedSliderProxyStyle(QStyle *baseStyle = 0) : SliderProxyStyle(baseStyle) {}

    QRect subControlRect(ComplexControl cc, const QStyleOptionComplex *opt, SubControl sc, const QWidget *widget = 0) const
    {
        QRect r = SliderProxyStyle::subControlRect(cc, opt, sc, widget);
        if (cc == CC_Slider && sc == SC_SliderHandle)
        {
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

SpeedSlider::SpeedSlider(Qt::Orientation orientation, QGraphicsItem *parent)
    : GraphicsSlider(orientation, parent),
      m_precision(HighPrecision),
      m_animation(0),
      m_timerId(0)
{
    m_toolTip = new StyledToolTipItem(this);
    m_toolTip->setVisible(false);

    setStyle(new SpeedSliderProxyStyle);
    setSingleStep(10);
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

    if (m_timerId)
    {
        killTimer(m_timerId);
        m_timerId = 0;
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
    setRange(0, (presets[int(m_precision)].size - 1) * 10);
    resetSpeed();
    update();
}

void SpeedSlider::setToolTipItem(ToolTipItem *toolTip)
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
    setValue(presets[int(m_precision)].defaultIndex * 10);
}

void SpeedSlider::stepBackward()
{
    setValue(value() - singleStep());
}

void SpeedSlider::stepForward()
{
    setValue(value() + singleStep());
}

void SpeedSlider::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    GraphicsSlider::paint(painter, option, widget); return;/*
    QPainter p(this);
    p.fillRect(rect(), QColor(0, 0, 0, 0));

    QRect r = contentsRect();
    static const int sliderHeigth = 12;
    if (r.height() > sliderHeigth)
    {
        r.moveTop((r.height() - sliderHeigth) / 2);
        r.setHeight(sliderHeigth);
    }
    r.adjust(0, 0, -1, -1);

    const float handlePos = float(value() - minimum()) / (maximum() - minimum());

    QLinearGradient linearGrad(QPointF(0, 0), QPointF(r.width(), 0));
    linearGrad.setColorAt(0, QColor(0, 43, 130));
    linearGrad.setColorAt(handlePos, QColor(186, 239, 255));
    if (!qFuzzyCompare(handlePos, 1.0f))
    {
        linearGrad.setColorAt(handlePos + 0.001, QColor(0, 0, 0, 0));
        linearGrad.setColorAt(1, QColor(0, 0, 0, 0));
    }

    p.setPen(QPen(Qt::darkGray, 1));
    p.setBrush(linearGrad);
    p.drawRect(r);*/
}

void SpeedSlider::sliderChange(SliderChange change)
{
    GraphicsSlider::sliderChange(change);

    const Preset &preset = presets[int(m_precision)];

    int idx = value() / 10;
    Q_ASSERT(idx >= 0 && idx < preset.size);
    float newSpeed = preset.preset[idx];

    int defaultIndex = preset.defaultIndex;
    if ((presets == 0 || newSpeed > preset.preset[defaultIndex - 1])
        && (defaultIndex == preset.size - 1 || newSpeed < preset.preset[defaultIndex + 1])) {
        newSpeed = preset.preset[defaultIndex]; // make a gap around default value
    }

    emit speedChanged(newSpeed);
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
        m_animation->setEndValue(presets[int(m_precision)].defaultIndex * 10);
        m_animation->start();
    }
}

void SpeedSlider::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_timerId)
    {
        killTimer(m_timerId);
        m_timerId = 0;

        if (m_toolTip)
            m_toolTip->setVisible(false);
    }

    GraphicsSlider::timerEvent(event);
}

void SpeedSlider::onSpeedChanged(float newSpeed)
{
    if (m_timerId)
        killTimer(m_timerId);
    m_timerId = startTimer(2500);

    if (m_toolTip) {
        QStyleOptionSlider opt;
        initStyleOption(&opt);
        const QRect sliderRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle);
        m_toolTip->setPos(sliderRect.center().x(), sliderRect.top() - m_toolTip->boundingRect().height());
        m_toolTip->setText(!qFuzzyIsNull(newSpeed) ? tr("%1x").arg(newSpeed) : tr("Paused"));
        m_toolTip->setVisible(true);
    }
}
