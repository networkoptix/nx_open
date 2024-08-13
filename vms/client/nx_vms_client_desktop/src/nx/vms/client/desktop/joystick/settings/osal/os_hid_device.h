// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "osal_device.h"

namespace nx::vms::client::desktop::joystick {

class OsHidDevice: public OsalDevice
{
public:
    virtual bool isOpened() const = 0;
    virtual bool open() = 0;

    virtual void stall() = 0;
    virtual void halt() = 0;
    virtual void resume() = 0;

protected:
    virtual int read(unsigned char* buffer, int bufferSize) = 0;
};

} // namespace nx::vms::client::desktop::joystick
