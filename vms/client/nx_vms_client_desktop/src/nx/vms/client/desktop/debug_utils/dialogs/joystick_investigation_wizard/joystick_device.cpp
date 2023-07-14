// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "joystick_device.h"

#include <nx/utils/log/log.h>

namespace nx::vms::client::desktop::joystick {

JoystickDevice::JoystickDevice(JoystickDeviceInfo deviceInfo, QObject* parent):
    OsalDeviceListener(parent),
    m_deviceInfo(deviceInfo)
{
    OsalDriver::getDriver()->setupDeviceListener(m_deviceInfo.path, this);

    NX_DEBUG(this, "Joystick device subscriber created for device: %1 %2 (%3)",
        m_deviceInfo.manufacturerName, m_deviceInfo.modelName, m_deviceInfo.id);
}

JoystickDevice::~JoystickDevice()
{
    OsalDriver::getDriver()->removeDeviceListener(this);

    NX_DEBUG(this, "Joystick device subscriber destroyed for device: %1 %2 (%3)",
        m_deviceInfo.manufacturerName, m_deviceInfo.modelName, m_deviceInfo.id);
}

void JoystickDevice::onStateChanged(const OsalDevice::State& newState)
{
    QList<bool> state;

    if (!NX_ASSERT(newState.rawData.canConvert<QBitArray>()))
        return;

    const QBitArray bitState = newState.rawData.toBitArray();

    for (int i = 0; i < bitState.size(); ++i)
        state.append(bitState[i]);

    emit stateChanged(state);
}

} // namespace nx::vms::client::desktop::joystick
