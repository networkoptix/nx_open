#include "volumeslider.h"

#include "tooltipitem.h"

#include "openal/qtvaudiodevice.h"

#include "ui/videoitem/timeslider.h" // SliderProxyStyle

class VolumeSliderProxyStyle : public SliderProxyStyle
{
public:
    VolumeSliderProxyStyle(QStyle *baseStyle = 0) : SliderProxyStyle(baseStyle) {}

    QRect subControlRect(ComplexControl cc, const QStyleOptionComplex *opt, SubControl sc, const QWidget *widget = 0) const
    {
        QRect r = SliderProxyStyle::subControlRect(cc, opt, sc, widget);
        if (cc == CC_Slider && sc == SC_SliderHandle)
        {
            if (qstyleoption_cast<const QStyleOptionSlider *>(opt)->orientation == Qt::Horizontal)
                r.setWidth(3);
            else
                r.setHeight(3);
        }
        return r;
    }
};


VolumeSlider::VolumeSlider(Qt::Orientation orientation, QGraphicsItem *parent)
    : GraphicsSlider(orientation, parent),
      m_toolTipTimerId(0)
{
    m_toolTip = new StyledToolTipItem(this);
    m_toolTip->setVisible(false);

    setStyle(new VolumeSliderProxyStyle);

    setRange(0, 100);
    setSliderPosition(QtvAudioDevice::instance().volume() * 100);

    connect(this, SIGNAL(valueChanged(int)), this, SLOT(onValueChanged(int)));
}

VolumeSlider::~VolumeSlider()
{
    if (m_toolTipTimerId)
    {
        killTimer(m_toolTipTimerId);
        m_toolTipTimerId = 0;
    }
}

bool VolumeSlider::isMute() const
{
    return QtvAudioDevice::instance().isMute();
}

void VolumeSlider::setMute(bool mute)
{
    QtvAudioDevice::instance().setMute(mute);
    setSliderPosition(QtvAudioDevice::instance().volume() * 100);
}

void VolumeSlider::setToolTipItem(ToolTipItem *toolTip)
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

void VolumeSlider::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    GraphicsSlider::paint(painter, option, widget); return;
    Q_UNUSED(option)
    Q_UNUSED(widget)

    painter->fillRect(rect(), QColor(0, 0, 0, 0));

    QRectF r = contentsRect();
    /*static const int sliderHeigth = 12;
    if (r.height() > sliderHeigth)
    {
        r.moveTop((r.height() - sliderHeigth) / 2);
        r.setHeight(sliderHeigth);
    }*/
    r.adjust(0, 0, -1, -1);

    const float handlePos = float(value() - minimum()) / (maximum() - minimum());

    QLinearGradient linearGrad;
    if (orientation() == Qt::Horizontal)
    {
        linearGrad.setStart(QPointF(0, 0));
        linearGrad.setFinalStop(QPointF(r.width(), 0));
    }
    else
    {
        linearGrad.setStart(QPointF(0, r.height()));
        linearGrad.setFinalStop(QPointF(0, 0));
    }
    linearGrad.setColorAt(0, QColor(0, 43, 130));
    linearGrad.setColorAt(handlePos, QColor(186, 239, 255));
    if (!qFuzzyCompare(handlePos, 1.0f))
    {
        linearGrad.setColorAt(handlePos + 0.001, QColor(0, 0, 0, 0));
        linearGrad.setColorAt(1, QColor(0, 0, 0, 0));
    }

    QPointF p = r.center();
    if (orientation() == Qt::Horizontal)
        p.setX(r.width() * handlePos);
    else
        p.setY(r.height() * (1 - handlePos));
    qreal radius = qMin(r.width(), r.height()) / 2;
    painter->setPen(QPen(QColor(100, 78, 74), 1));
    painter->setBrush(QColor(166, 39, 36));
    painter->drawEllipse(p, radius, radius);

    painter->setPen(QPen(Qt::darkGray, 1));
    painter->setBrush(linearGrad);
    painter->drawRect(r);
}

void VolumeSlider::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_toolTipTimerId)
    {
        killTimer(m_toolTipTimerId);
        m_toolTipTimerId = 0;

        if (m_toolTip)
            m_toolTip->setVisible(false);
    }

    GraphicsSlider::timerEvent(event);
}

void VolumeSlider::onValueChanged(int value)
{
    QtvAudioDevice::instance().setVolume(value / 100.0);

    if (m_toolTipTimerId)
        killTimer(m_toolTipTimerId);
    m_toolTipTimerId = startTimer(2500);

    if (m_toolTip) {
        QStyleOptionSlider opt;
        initStyleOption(&opt);
        const QRect sliderRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle);
        m_toolTip->setPos(sliderRect.center().x(), sliderRect.top() - m_toolTip->boundingRect().height());
        m_toolTip->setText(!QtvAudioDevice::instance().isMute() ? tr("%1%").arg(value) : tr("Muted"));
        m_toolTip->setVisible(true);
    }
}
