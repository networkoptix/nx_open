// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/utils/impl_ptr.h>

#include "../os_hid_device.h"
#include "../osal_driver.h"

namespace nx::vms::client::desktop::joystick {

class OsHidDriver: public OsalDriver
{
public:
    OsHidDriver();
    virtual ~OsHidDriver() override;

    virtual QList<JoystickDeviceInfo> deviceList() override;
    virtual void setupDeviceListener(
        const QString& path,
        const OsalDeviceListener* listener) override;
    virtual void removeDeviceListener(const OsalDeviceListener* listener) override;

    virtual void halt() override;
    virtual void resume() override;

public:
    void attachVirtualDevice(OsHidDevice* device);
    void detachVirtualDevice(OsHidDevice* device);

private:
    virtual void enumerateDevices();

    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::joystick
