// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "non_blocking_manager_win.h"

#include <QtGui/QGuiApplication>

#include <nx/utils/log/log.h>

#include "non_blocking_device_win.h"
#include "osal/osal_driver.h"

namespace nx::vms::client::desktop::joystick {

NonBlockingManagerWin::NonBlockingManagerWin(QObject* parent):
    base_type(parent)
{
    connect(OsalDriver::getDriver(), &OsalDriver::deviceListChanged,
        this, &NonBlockingManagerWin::onDeviceListChanged);

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
