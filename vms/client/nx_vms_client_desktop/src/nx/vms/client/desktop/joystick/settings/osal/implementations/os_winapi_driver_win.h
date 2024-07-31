// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

#include "../osal_driver.h"

namespace nx::vms::client::desktop::joystick {

class OsWinApiDriver: public OsalDriver
{
public:
    OsWinApiDriver();
    virtual ~OsWinApiDriver() override;

    virtual QList<JoystickDeviceInfo> deviceList() override;
    virtual void setupDeviceListener(
        const QString& path,
        const OsalDeviceListener* listener) override;
    virtual void removeDeviceListener(const OsalDeviceListener* listener) override;

    virtual void halt() override;
    virtual void resume() override;

private:
    void enumerateDevices();

private:
    class Worker;
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::joystick
