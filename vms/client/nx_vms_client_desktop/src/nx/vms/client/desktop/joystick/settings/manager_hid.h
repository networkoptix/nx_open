// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "manager.h"

namespace nx::vms::client::desktop::joystick {

class ManagerHid: public Manager
{
    Q_OBJECT

    using base_type = Manager;

public:
    ManagerHid(QObject* parent = 0);
    virtual ~ManagerHid() override;

private:
    virtual DevicePtr createDevice(
        const JoystickDescriptor& deviceConfig,
        const QString& path) override;
    virtual void enumerateDevices() override;
};

} // namespace nx::vms::client::desktop::joystick
