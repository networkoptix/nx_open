// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "manager_hid.h"

#include <QtGui/QGuiApplication>

#include <nx/utils/log/log_main.h>

#include "device_hid.h"
#include "osal/osal_driver.h"

namespace nx::vms::client::desktop::joystick {

ManagerHid::ManagerHid(QObject* parent):
    base_type(parent)
{
    connect(OsalDriver::getDriver(), &OsalDriver::deviceListChanged,
        this, &ManagerHid::onHidDeviceListChanged);

    connect(qApp, &QGuiApplication::applicationStateChanged,
        [this](Qt::ApplicationState state)
        {
            if (state == Qt::ApplicationActive)
            {
                NX_DEBUG(this, "Application became active. Resuming the joystick driver.");

                OsalDriver::getDriver()->resume();
            }
            else
            {
                NX_DEBUG(this, "Application became inactive. Halting the joystick driver.");

                OsalDriver::getDriver()->halt();
            }
        });
}

void ManagerHid::onHidDeviceListChanged()
{
    if (!isActive())
    {
        NX_DEBUG(this, "Joystick devices are updated, but the manager is not active. Skipping.");
        return;
    }

    QSet<QString> foundDevices;

    const auto& deviceList = OsalDriver::getDriver()->deviceList();

    for (const auto& deviceInfo: deviceList)
    {
        foundDevices << deviceInfo.path;

        if (m_devices.contains(deviceInfo.path))
            continue;

        NX_DEBUG(this,
            "A new Joystick has been found. "
            "Manufacturer: %1, model: %2, id: %3, path:\n%4",
            deviceInfo.manufacturerName, deviceInfo.modelName, deviceInfo.id, deviceInfo.path);

        const auto config = getDeviceDescription(deviceInfo.modelName);
        if (isGeneralJoystickConfig(config))
        {
            NX_VERBOSE(this, "Unsupported device. Model: %1, path:\n%2", deviceInfo.modelName,
                deviceInfo.path);

            continue;
        }

        auto device = new DeviceHid(config, deviceInfo.path, this);

        initializeDevice(DevicePtr(device), config);
    }

    removeUnpluggedJoysticks(foundDevices);
}

} // namespace nx::vms::client::desktop::joystick
