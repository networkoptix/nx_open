// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>

#include "../os_hid_device.h"

namespace nx::vms::client::desktop::joystick {

class OsHidDeviceMac: public OsHidDevice
{
public:
    OsHidDeviceMac(const OsHidDeviceInfo& info);
    virtual ~OsHidDeviceMac() override;

    virtual OsHidDeviceInfo info() const override;
    virtual bool isValid() const override;

    void stall();

protected:
    void poll();
    virtual int read(unsigned char* buffer, int bufferSize) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::joystick
