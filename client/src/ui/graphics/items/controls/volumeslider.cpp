#include "volumeslider.h"

#include "ui/style/proxy_style.h"

#include "openal/qtvaudiodevice.h"

QnVolumeSlider::QnVolumeSlider(QGraphicsItem *parent): 
    base_type(parent)
{
    setRange(0, 100);
    setSliderPosition(QtvAudioDevice::instance()->volume() * 100);

    /* Update tooltip text. */
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

