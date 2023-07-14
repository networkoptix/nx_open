// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "non_blocking_manager_win.h"

#include <QtCore/QDir>

#include <nx/utils/log/log_main.h>

#include "non_blocking_device_win.h"
#include "osal/osal_driver.h"

namespace nx::vms::client::desktop::joystick {

NonBlockingManagerWin::NonBlockingManagerWin(QObject* parent):
    base_type(parent)
{
    connect(OsalDriver::getDriver(), &OsalDriver::deviceListChanged,
        this, &NonBlockingManagerWin::onDeviceListChanged);
}

void NonBlockingManagerWin::onDeviceListChanged()
{
    const auto deviceInfos = OsalDriver::getDriver()->deviceList();

    for (auto deviceInfo: deviceInfos)
    {
        if (m_devices.contains(deviceInfo.id))
            continue;

        const auto config = createDeviceDescription(deviceInfo.modelName);

        auto device = new NonBlockingDeviceWin(config, deviceInfo.id, this);

        initializeDevice(DevicePtr(device), config);
    }
}

} // namespace nx::vms::client::desktop::joystick
