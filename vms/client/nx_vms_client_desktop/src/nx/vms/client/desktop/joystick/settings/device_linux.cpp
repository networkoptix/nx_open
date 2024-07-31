// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_linux.h"

#include <algorithm>

#include <fcntl.h>
#include <linux/joystick.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <QtCore/QPair>

#include "descriptors.h"
#include "manager.h"

namespace {

constexpr int kInvalidDeviceId = -1;

struct JoystickEvent
{
    /** Minimum value of axes range. */
    static constexpr short MIN_AXES_VALUE = -32768;

    /** Maximum value of axes range. */
    static constexpr short MAX_AXES_VALUE = 32767;

    js_event data;

    /**
     * Returns true if this event is the result of a button press.
     */
    bool isButton() const
    {
        return (data.type & JS_EVENT_BUTTON) != 0;
    }

    /**
     * Returns true if this event is the result of an axis movement.
     */
    bool isAxis() const
    {
        return (data.type & JS_EVENT_AXIS) != 0;
    }

    /**
     * Returns true if this event is part of the initial state obtained when
     * the joystick is first connected to.
     */
    bool isInitialState() const
    {
        return (data.type & JS_EVENT_INIT) != 0;
    }

    int axisOrButtonIndex() const
    {
        return data.number;
    }

    int axisOrButtonState() const
    {
        return data.value;
    }
};

bool readData(int dev, js_event* event)
{
    int bytes = read(dev, event, sizeof(*event));

    if (bytes == -1)
        return false;

    // If this condition is not met, we're probably out of sync
    // and the joystick instance is likely unusable.
    return bytes == sizeof(*event);
}

} // namespace

namespace nx::vms::client::desktop::joystick {

DeviceLinux::DeviceLinux(
    const JoystickDescriptor& modelInfo,
    const QString& path,
    QTimer* pollTimer,
    Manager* manager)
    :
    base_type(modelInfo, path, pollTimer, manager)
{
    m_dev = open(path.toStdString().c_str(), O_RDONLY | O_NONBLOCK);
    updateStickAxisLimits(modelInfo);
}

DeviceLinux::~DeviceLinux()
{
    if (m_dev != kInvalidDeviceId)
        close(m_dev);
}

bool DeviceLinux::isValid() const
{
    return m_dev != kInvalidDeviceId;
}

void DeviceLinux::setFoundControlsNumber(int axesNumber, int buttonsNumber)
{
    for (int axisIndex = 0;
        axisIndex < std::min(axesNumber, (int)Device::axisIndexCount);
        ++axisIndex)
    {
        setAxisInitialized((Device::AxisIndexes)axisIndex, true);
    }

    setInitializedButtonsNumber(buttonsNumber);
}

Device::State DeviceLinux::getNewState()
{
    if (!isValid())
        return {};

    JoystickEvent event;
    Device::StickPosition newStickPosition = m_stickPosition;
    Device::ButtonStates newButtonStates = m_buttonStates;

    while (readData(m_dev, &event.data))
    {
        if (event.isAxis())
        {
            if (event.axisOrButtonIndex() >= newStickPosition.size())
                continue; // We don't use additional axes.

            newStickPosition[event.axisOrButtonIndex()] =
                mapAxisState(event.axisOrButtonState(), m_axisLimits[event.axisOrButtonIndex()]);
        }
        else if (event.isButton())
        {
            if (event.axisOrButtonIndex() >= newButtonStates.size())
                continue; // We don't use more buttons, than in joystick config.

            newButtonStates[event.axisOrButtonIndex()] = event.axisOrButtonState();
        }
    }

    return {newStickPosition, newButtonStates};
}

Device::AxisLimits DeviceLinux::parseAxisLimits(
    const AxisDescriptor& descriptor,
    const AxisLimits& /*oldLimits*/) const
{
    AxisLimits result;
    result.min = JoystickEvent::MIN_AXES_VALUE;
    result.max = JoystickEvent::MAX_AXES_VALUE;
    result.mid = (result.min + result.max) / 2;
    result.bounce = descriptor.bounce.toInt(/*ok*/ nullptr, kAutoDetectBase);

    const int minBounce = (result.max - result.min) / 100.0 * kMinBounceInPercentages;
    if (minBounce > result.bounce)
        result.bounce = minBounce;

    result.sensitivity = descriptor.sensitivity.toDouble(/*ok*/ nullptr);
    return result;
}

} // namespace nx::vms::client::desktop::joystick
