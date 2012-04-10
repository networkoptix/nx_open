#include "volumeslider.h"

#include "ui/style/proxy_style.h"

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


QnVolumeSlider::QnVolumeSlider(QGraphicsItem *parent): 
    base_type(parent)
{
    setStyle(new VolumeSliderProxyStyle(this));

    setRange(0, 100);
    setSliderPosition(QtvAudioDevice::instance()->volume() * 100);

    connect(this, SIGNAL(valueChanged(int)), this, SLOT(onValueChanged(int)));

    sliderChange(SliderValueChange);
}

QnVolumeSlider::~QnVolumeSlider() {
    return;
}

bool QnVolumeSlider::isMute() const {
    return QtvAudioDevice::instance()->isMute();
}

void QnVolumeSlider::setMute(bool mute) {
    QtvAudioDevice::instance()->setMute(mute);
    setSliderPosition(QtvAudioDevice::instance()->volume() * 100);
}

void QnVolumeSlider::stepBackward() {
    triggerAction(SliderPageStepSub);
}

void QnVolumeSlider::stepForward() {
    triggerAction(SliderPageStepAdd);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnVolumeSlider::sliderChange(SliderChange change) {
    base_type::sliderChange(change);
    
    if(change == SliderValueChange) {
        int value = this->value();

        QtvAudioDevice::instance()->setVolume(value / 100.0);
        setToolTip(QtvAudioDevice::instance()->isMute() ? tr("Muted") : tr("%1%").arg(value));
    }
}

