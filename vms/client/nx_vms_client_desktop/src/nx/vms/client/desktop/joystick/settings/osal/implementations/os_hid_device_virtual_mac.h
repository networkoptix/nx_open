// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../os_hid_device.h"

namespace nx::vms::client::desktop::joystick {

struct JoystickDescriptor;

class OsHidDeviceVirtual: public OsHidDevice
{
public:
    OsHidDeviceVirtual();

    virtual JoystickDeviceInfo info() const override;

    virtual int read(unsigned char* buffer, int bufferSize) override;

    virtual bool isOpened() const override { return true; }
    virtual bool open() override { return true; }

    // TODO: #alevenkov Try to avoid empty implementations in the next refactoring.
    virtual void stall() override { }
    // This is not needed for virtual devices, because they are only used for halting the joystick
    // during the application inactivity. But when the application is inactive, a user can't
    // interact with the virtual joystick anyway.
    virtual void halt() override { }
    virtual void resume() override { }

    QBitArray state() const { return m_state; }
    void setState(const QBitArray& newState);

    static JoystickDescriptor* joystickConfig();
    static JoystickDeviceInfo deviceInfo();

protected:
    QBitArray m_state;
};

} // namespace nx::vms::client::desktop::joystick
