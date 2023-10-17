// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "volume_slider.h"

#include <QtCore/QScopedValueRollback>
#include <QtCore/QtMath>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QStyle>

#include <nx/audio/audiodevice.h>
#include <nx/utils/math/math.h>
#include <nx/vms/client/desktop/style/style.h>
#include <nx/vms/client/desktop/workbench/timeline/slider_tooltip.h>
#include <nx/vms/client/desktop/workbench/workbench_animations.h>
#include <ui/animation/variant_animator.h>
#include <ui/animation/widget_opacity_animator.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/workbench/workbench_display.h>
#include <utils/common/event_processors.h>

namespace nx::vms::client::desktop::workbench::timeline {

namespace {

static constexpr int kAutoHideTimerIntervalMs = 100;
static constexpr int kAutoHideAfterSliderChangePeriodMs = 2500;
static constexpr int kTooltipYShift = -5;

} // namespace

VolumeSlider::VolumeSlider(WindowContext* context, QWidget* parent):
    base_type(Qt::Horizontal, parent),
    m_tooltip(new SliderToolTip(context))
{
    setRange(0, 100);
    //setWheelFactor(kMouseWheelFactor);

    setSliderPosition(nx::audio::AudioDevice::instance()->volume() * 100);
    sliderChange(SliderValueChange);

    connect(nx::audio::AudioDevice::instance(), &nx::audio::AudioDevice::volumeChanged, this,
        [this]
        {
            QScopedValueRollback<bool> guard(m_updating, true);
            setSliderPosition(nx::audio::AudioDevice::instance()->volume() * 100);
        });

    m_hideTooltipTimer.setSingleShot(true);

    connect(this, &QSlider::valueChanged,
        [this]
        {
            updateTooltipPos();
            showToolTip();
        });

    connect(&m_hideTooltipTimer, &QTimer::timeout, this, &VolumeSlider::hideToolTip);

    installEventHandler({parentWidget()}, {QEvent::Resize, QEvent::Move},
        this, &VolumeSlider::updateTooltipPos);

    /* Make sure that tooltip text is updated. */
    sliderChange(SliderValueChange);
}

VolumeSlider::~VolumeSlider()
{
}

bool VolumeSlider::isMute() const
{
    return nx::audio::AudioDevice::instance()->isMute();
}

void VolumeSlider::setTooltipsVisible(bool enabled)
{
    m_tooltipsEnabled = enabled;
    if (!m_tooltipsEnabled)
        hideToolTip();
}

void VolumeSlider::setMute(bool mute)
{
    nx::audio::AudioDevice::instance()->setMute(mute);

    setSliderPosition(qMax(
        static_cast<int>(nx::audio::AudioDevice::instance()->volume() * 100),
        mute ? 0 : 1 /* So that switching mute -> non-mute will result in non-zero volume. */
    ));
}

void VolumeSlider::stepBackward()
{
    triggerAction(SliderPageStepSub);
}

void VolumeSlider::stepForward()
{
    triggerAction(SliderPageStepAdd);
}

void VolumeSlider::hideToolTip()
{
    m_tooltip->hide();
}

void VolumeSlider::showToolTip()
{
    if (m_tooltipsEnabled)
        m_tooltip->show();
}

void VolumeSlider::sliderChange(SliderChange change)
{
    base_type::sliderChange(change);

    if (change == SliderValueChange)
    {
        qint64 value = this->value();

        if (!m_updating)
            nx::audio::AudioDevice::instance()->setVolume(value / 100.0f);

        m_tooltip->setText(nx::audio::AudioDevice::instance()->isMute()
            ? tr("Muted")
            : lit("%1%").arg(value));

        m_hideTooltipTimer.setInterval(kAutoHideAfterSliderChangePeriodMs);
        m_hideTooltipTimer.start();
    }
}

bool VolumeSlider::eventFilter(QObject* watched, QEvent* event)
{
    switch (event->type())
    {
        case QEvent::ToolTip:
            return true; // default tooltip shouldn't be displayed
        default:
            break;
    }

    return base_type::eventFilter(watched, event);
}

void VolumeSlider::wheelEvent(QWheelEvent* event)
{
    setValue(value() + event->angleDelta().y() / 120 * 2);
}

void VolumeSlider::updateTooltipPos()
{
    if (!m_tooltip)
        return;

    const int sliderHandleSize = style()->pixelMetric(QStyle::PM_SliderControlThickness);

    const QPoint localTooltipPos(QStyle::sliderPositionFromValue(minimum(), maximum(), value(),
        width() - sliderHandleSize), kTooltipYShift);

    m_tooltip->setTarget(
        QRect(mapToGlobal(localTooltipPos), QSize(sliderHandleSize, sliderHandleSize)));

    const QRect parentWindowGeometry = parentWidget()->window()->geometry().adjusted(1, 1, -1, -1);
    m_tooltip->setEnclosingRect(parentWindowGeometry);
}

void VolumeSlider::enterEvent(QEnterEvent* /*event*/)
{
    m_hideTooltipTimer.stop();
    updateTooltipPos();
    showToolTip();
}

void VolumeSlider::leaveEvent(QEvent* /*event*/)
{
    m_hideTooltipTimer.setInterval(kAutoHideTimerIntervalMs);
    m_hideTooltipTimer.start();
}

} // namespace nx::vms::client::desktop::workbench::timeline
