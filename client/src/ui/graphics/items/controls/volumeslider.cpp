#include "volumeslider.h"

#include "ui/style/proxy_style.h"

#include "ui/graphics/items/tool_tip_item.h"

#include "openal/qtvaudiodevice.h"

class VolumeSliderProxyStyle : public QnProxyStyle
{
public:
    VolumeSliderProxyStyle(QObject *parent = 0) : QnProxyStyle(0, parent) {}

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
            int side = qMin(r.width(), r.height());//3;
            if (qstyleoption_cast<const QStyleOptionSlider *>(opt)->orientation == Qt::Horizontal)
                r.setWidth(side);
            else
                r.setHeight(side);
        }
        return r;
    }
};


VolumeSlider::VolumeSlider(Qt::Orientation orientation, QGraphicsItem *parent)
    : GraphicsSlider(orientation, parent),
      m_toolTipTimerId(0)
{
    m_toolTip = new QnStyledToolTipItem(this);
    m_toolTip->setVisible(false);

    setStyle(new VolumeSliderProxyStyle(this));

    setRange(0, 100);
    setSliderPosition(QtvAudioDevice::instance()->volume() * 100);

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
    return QtvAudioDevice::instance()->isMute();
}

void VolumeSlider::setMute(bool mute)
{
    QtvAudioDevice::instance()->setMute(mute);
    setSliderPosition(QtvAudioDevice::instance()->volume() * 100);
}

void VolumeSlider::stepBackward() {
    triggerAction(SliderPageStepSub);
}

void VolumeSlider::stepForward() {
    triggerAction(SliderPageStepAdd);
}

void VolumeSlider::setToolTipItem(QnToolTipItem *toolTip)
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

#if 0
void VolumeSlider::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
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
#endif

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
    QtvAudioDevice::instance()->setVolume(value / 100.0);

    if (m_toolTipTimerId)
        killTimer(m_toolTipTimerId);
    m_toolTipTimerId = startTimer(2500);

    if (m_toolTip) {
        QStyleOptionSlider opt;
        initStyleOption(&opt);
        const QRectF sliderRect = QRectF(style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle));
        m_toolTip->setPos(sliderRect.center().x(), sliderRect.top());
        m_toolTip->setText(!QtvAudioDevice::instance()->isMute() ? tr("%1%").arg(value) : tr("Muted"));
        m_toolTip->setVisible(true);
    }
}
