// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../os_hid_device.h"

namespace nx::vms::client::desktop::joystick {

struct JoystickDescriptor;
class OsHidDriverVirtual;

class OsHidDeviceVirtual: public OsHidDevice
{
public:
    OsHidDeviceVirtual();

    virtual OsHidDeviceInfo info() const override;

    virtual int read(unsigned char* buffer, int bufferSize) override;

    virtual bool isOpened() const override { return true; }
    virtual bool open() override { return true; }

    virtual void stall() override { }

    QBitArray state() const { return m_state; }
    void setState(const QBitArray& newState);

    static JoystickDescriptor* joystickConfig();
    static OsHidDeviceInfo deviceInfo();

protected:
    QBitArray m_state;
};

} // namespace nx::vms::client::desktop::joystick
