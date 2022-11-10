// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "manager.h"

namespace nx::vms::client::desktop::joystick {

/** This implementation cannot support general joystick config. */
class ManagerHid: public Manager
{
    Q_OBJECT

    using base_type = Manager;

public:
    ManagerHid(QObject* parent = 0);
    virtual ~ManagerHid() override;

private:
    virtual void enumerateDevices() override;
};

} // namespace nx::vms::client::desktop::joystick
