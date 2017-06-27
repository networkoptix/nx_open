#include "volume_slider.h"

#include <nx/audio/audiodevice.h>

#include <ui/style/helper.h>

#include <utils/common/scoped_value_rollback.h>

namespace {

static const qreal kMouseWheelFactor = 2.0;

} // namespace


QnVolumeSlider::QnVolumeSlider(QGraphicsItem *parent):
    base_type(parent),
    m_updating(false)
{
    /* Supress AudioDevice reset from constructor. */
    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    setProperty(style::Properties::kSliderFeatures, static_cast<int>(style::SliderFeature::FillingUp));

    setWheelFactor(kMouseWheelFactor);

    setRange(0, 100);
    setSliderPosition(nx::audio::AudioDevice::instance()->volume() * 100);

    /* Update tooltip text. */
    sliderChange(SliderValueChange);

    connect(nx::audio::AudioDevice::instance(), &nx::audio::AudioDevice::volumeChanged, this,
        [this]
        {
            QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
            setSliderPosition(nx::audio::AudioDevice::instance()->volume() * 100);
        });
}

QnVolumeSlider::~QnVolumeSlider() {
    return;
}

bool QnVolumeSlider::isMute() const {
    return nx::audio::AudioDevice::instance()->isMute();
}

void QnVolumeSlider::setMute(bool mute) {
    nx::audio::AudioDevice::instance()->setMute(mute);

    setSliderPosition(qMax(
        static_cast<int>(nx::audio::AudioDevice::instance()->volume() * 100),
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
            nx::audio::AudioDevice::instance()->setVolume(value / 100.0);

        setToolTip(
            nx::audio::AudioDevice::instance()->isMute() ? tr("Muted") : lit("%1%").arg(value));
    }
}

