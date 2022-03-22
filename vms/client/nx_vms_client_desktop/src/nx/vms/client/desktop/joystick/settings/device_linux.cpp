// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_linux.h"

#include <fcntl.h>
#include <linux/joystick.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <QtCore/QPair>

#include "descriptors.h"

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
    QObject* parent)
    :
    base_type(modelInfo, path, pollTimer, parent)
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

Device::State DeviceLinux::getNewState()
{
    if (!isValid())
        return {};

    JoystickEvent event;
    Device::StickPositions newStickPositions = m_stickPosition;
    Device::ButtonStates newButtonStates = m_buttonStates;

    while (readData(m_dev, &event.data))
    {
        if (event.isAxis())
        {
            if (!NX_ASSERT(event.axisOrButtonIndex() < newStickPositions.size()))
                continue;

            newStickPositions[event.axisOrButtonIndex()] =
                mapAxisState(event.axisOrButtonState(), m_axisLimits[event.axisOrButtonIndex()]);
        }
        else if (event.isButton())
        {
            if (!NX_ASSERT(event.axisOrButtonIndex() < newButtonStates.size()))
                continue;

            newButtonStates[event.axisOrButtonIndex()] = event.axisOrButtonState();
        }
    }

    return {newStickPositions, newButtonStates};
}

Device::AxisLimits DeviceLinux::parseAxisLimits(const AxisDescriptor& descriptor)
{
    AxisLimits result;
    result.min = JoystickEvent::MIN_AXES_VALUE;
    result.max = JoystickEvent::MAX_AXES_VALUE;
    result.bounce = (result.max - result.min) / 100.0 * 2; //< 2% central point bounce.
    result.mid = (result.min + result.max) / 2;
    result.sensitivity = descriptor.sensitivity.toDouble(/*ok*/ nullptr);
    return result;
}

} // namespace nx::vms::client::desktop::joystick
