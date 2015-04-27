#include "volume_slider.h"

#include <openal/qtvaudiodevice.h>

#include <utils/common/scoped_value_rollback.h>

QnVolumeSlider::QnVolumeSlider(QGraphicsItem *parent): 
    base_type(parent),
    m_updating(false)
{
    /* Supress QtvAudioDevice reset from constructor. */
    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    setRange(0, 100);
    setSliderPosition(QtvAudioDevice::instance()->volume() * 100);

    /* Update tooltip text. */
    sliderChange(SliderValueChange);

    connect(QtvAudioDevice::instance(), &QtvAudioDevice::volumeChanged, this, [this] {
        QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
        setSliderPosition(QtvAudioDevice::instance()->volume() * 100);
    });
}

QnVolumeSlider::~QnVolumeSlider() {
    return;
}

bool QnVolumeSlider::isMute() const {
    return QtvAudioDevice::instance()->isMute();
}

void QnVolumeSlider::setMute(bool mute) {
    QtvAudioDevice::instance()->setMute(mute);

    setSliderPosition(qMax(
        static_cast<int>(QtvAudioDevice::instance()->volume() * 100), 
        mute ? 0 : 1 /* So that switching mute -> non-mute will result in non-zero volume. */
    ));
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
        qint64 value = this->value();

        if (!m_updating)
            QtvAudioDevice::instance()->setVolume(value / 100.0);

        setToolTip(QtvAudioDevice::instance()->isMute() ? tr("Muted") : tr("%1%").arg(value));
    }
}

