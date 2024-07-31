// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>

#include "../os_hid_device.h"

namespace nx::vms::client::desktop::joystick {

class OsHidDeviceMac: public OsHidDevice
{
public:
    OsHidDeviceMac(const JoystickDeviceInfo& info);
    virtual ~OsHidDeviceMac() override;

    virtual JoystickDeviceInfo info() const override;

    virtual bool isOpened() const override;
    virtual bool open() override;

    virtual void stall() override;
    virtual void halt() override;
    virtual void resume() override;

protected:
    void poll();
    virtual int read(unsigned char* buffer, int bufferSize) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::joystick
