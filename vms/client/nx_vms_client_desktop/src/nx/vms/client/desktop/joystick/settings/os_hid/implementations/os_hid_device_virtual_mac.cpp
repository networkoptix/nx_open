// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "os_hid_device_virtual_mac.h"

#include <algorithm>

#include <nx/reflect/json.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/joystick/settings/descriptors.h>

namespace {

const auto virtualJoystickConfigPath = ":/hid_configs/virtual.json";

} // namespace

namespace nx::vms::client::desktop::joystick {

OsHidDeviceVirtual::OsHidDeviceVirtual()
{
    m_state = QBitArray(joystickConfig()->reportSize.toInt());
}

OsHidDeviceInfo OsHidDeviceVirtual::info() const
{
    return OsHidDeviceVirtual::deviceInfo();
}

int OsHidDeviceVirtual::read(unsigned char* buffer, int bufferSize)
{
    const int size = std::min((int)(m_state.size() / 8), bufferSize);

    memcpy(buffer, m_state.bits(), size);

    return size;
}

void OsHidDeviceVirtual::setState(const QBitArray& newState)
{
    m_state = newState;
    emit stateChanged(newState);
}

JoystickDescriptor* OsHidDeviceVirtual::joystickConfig()
{
    static JoystickDescriptor* m_config = nullptr;

    if (!m_config)
    {
        QFile file(virtualJoystickConfigPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            NX_ERROR(typeid(OsHidDeviceVirtual), "Failed to open virtual joystick config.");

        m_config = new JoystickDescriptor();

        if (!nx::reflect::json::deserialize<JoystickDescriptor>(
            file.readAll().toStdString(), m_config))
        {
            NX_ERROR(typeid(OsHidDeviceVirtual), "Failed to deserialize virtual joystick config.");
        }
    }

    return m_config;
}

OsHidDeviceInfo OsHidDeviceVirtual::deviceInfo()
{
    return OsHidDeviceInfo{
        .id = joystickConfig()->id,
        .path = "/not/a/real/path/virtual",
        .modelName = joystickConfig()->model,
        .manufacturerName = joystickConfig()->manufacturer,
    };
}

} // namespace nx::vms::client::desktop::joystick
