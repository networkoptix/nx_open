// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "joystick_device.h"

#include <nx/utils/log/log.h>

namespace nx::vms::client::desktop::joystick {

JoystickDevice::JoystickDevice(OsHidDeviceInfo deviceInfo, QObject* parent):
    QObject(parent),
    m_deviceInfo(deviceInfo)
{
    OsHidDriver::getDriver()->setupDeviceSubscriber(m_deviceInfo.path, this);

    NX_DEBUG(this, "Joystick device subscriber created for device: %1 %2 (%3)",
        m_deviceInfo.manufacturerName, m_deviceInfo.modelName, m_deviceInfo.id);
}

JoystickDevice::~JoystickDevice()
{
    OsHidDriver::getDriver()->removeDeviceSubscriber(this);

    NX_DEBUG(this, "Joystick device subscriber destroyed for device: %1 %2 (%3)",
        m_deviceInfo.manufacturerName, m_deviceInfo.modelName, m_deviceInfo.id);
}

QObject* JoystickDevice::parent() const
{
    return QObject::parent();
}

void JoystickDevice::onStateChanged(const QBitArray& newState)
{
    QList<bool> state;

    for (int i = 0; i < newState.size(); ++i)
        state.append(newState[i]);

    emit stateChanged(state);
}

} // namespace nx::vms::client::desktop::joystick
