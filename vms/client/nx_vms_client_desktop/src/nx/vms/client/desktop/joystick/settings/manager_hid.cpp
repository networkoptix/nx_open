// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "manager_hid.h"

#include <QtCore/QDir>

#include <nx/utils/log/log_main.h>

#include "device_hid.h"
#include "osal/osal_driver.h"

namespace nx::vms::client::desktop::joystick {

ManagerHid::ManagerHid(QObject* parent):
    base_type(parent)
{
    connect(OsalDriver::getDriver(), &OsalDriver::deviceListChanged,
        this, &ManagerHid::onHidDeviceListChanged);
}


void ManagerHid::onHidDeviceListChanged()
{
    QSet<QString> foundDevices;

    const auto& deviceList = OsalDriver::getDriver()->deviceList();

    for (const auto& deviceInfo: deviceList)
    {
        foundDevices << deviceInfo.path;

        if (m_devices.contains(deviceInfo.path))
            continue;

        NX_DEBUG(this,
            "A new Joystick has been found. "
            "Manufacturer: %1, model: %2, id: %3, path: %4",
            deviceInfo.manufacturerName, deviceInfo.modelName, deviceInfo.id, deviceInfo.path);

        const auto config = getDeviceDescription(deviceInfo.modelName);
        if (isGeneralJoystickConfig(config))
        {
            NX_VERBOSE(this, "Unsupported device. Model: %1, path: %2", deviceInfo.modelName,
                deviceInfo.path);

            continue;
        }

        auto device = new DeviceHid(config, deviceInfo.path, pollTimer());

        initializeDevice(DevicePtr(device), config);
    }

    removeUnpluggedJoysticks(foundDevices);
}

} // namespace nx::vms::client::desktop::joystick
