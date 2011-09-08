#include "speedslider.h"

#include <QtCore/QPropertyAnimation>

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


static const int fixedSpeedSet[] = { -160, -80, -40, -20, -15, -10, -5, 5, 10, 15, 20, 40, 80, 160 }; // x10
static const int fixedSpeedSetSize = sizeof(fixedSpeedSet) / sizeof(fixedSpeedSet[0]);
static const int fsIndex1x = 8;

//static const int sliderHeigth = 12;

SpeedSlider::SpeedSlider(Qt::Orientation orientation, QGraphicsItem *parent)
    : GraphicsSlider(orientation, parent),
      m_precision(LowPrecision),
      m_animation(0)
{
//    setMinimumHeight(sliderHeigth + 2);

    setStyle(new SpeedSliderProxyStyle);
    setRange(0, fixedSpeedSetSize * 10 - 1);
    setSingleStep(10);
    setPageStep(10);
    setTickInterval(10);
    setTickPosition(GraphicsSlider::NoTicks);

    resetSpeed();
}

SpeedSlider::~SpeedSlider()
{
    if (m_animation)
    {
        m_animation->stop();
        delete m_animation;
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

    update();
}

void SpeedSlider::resetSpeed()
{
    setValue(fsIndex1x * 10); // 1x
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

    int idx = value() / 10; // truncation!
    Q_ASSERT(idx >= 0 && idx < fixedSpeedSetSize);
    int value = fixedSpeedSet[idx]; // map to speed * 10
    if (value > 5 && value < 15)
        value = 10; // make a gap around 1x

    const float newSpeed = float(value) / 10.0;
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
        m_animation->setEndValue(fsIndex1x * 10);
        m_animation->start();
    }
}
