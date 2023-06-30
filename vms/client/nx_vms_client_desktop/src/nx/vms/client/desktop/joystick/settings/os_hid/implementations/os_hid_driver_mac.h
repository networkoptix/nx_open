// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

#include "../os_hid_driver.h"

namespace nx::vms::client::desktop::joystick {

class OsHidDriverMac: public OsHidDriver
{
public:
    OsHidDriverMac();
    virtual ~OsHidDriverMac() override;

    virtual QList<OsHidDeviceInfo> deviceList() override;
    virtual void setupDeviceSubscriber(const QString& path,
        const OsHidDeviceSubscriber* subscriber) override;
    virtual void removeDeviceSubscriber(const OsHidDeviceSubscriber* subscriber) override;

    void attachVirtualDevice(OsHidDevice* device);
    void detachVirtualDevice(OsHidDevice* device);

private:
    virtual void enumerateDevices();

    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::joystick
