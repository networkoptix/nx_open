// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "manager.h"

namespace nx::vms::client::desktop::joystick {

class ManagerLinux: public Manager
{
    Q_OBJECT

    using base_type = Manager;

public:
    ManagerLinux(QObject* parent = 0);

private:
    virtual void enumerateDevices() override;

    QString findDeviceModel(const QString& modelAndManufacturer) const;
};

} // namespace nx::vms::client::desktop::joystick
