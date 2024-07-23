// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>

#ifndef DIRECTINPUT_VERSION
    #define DIRECTINPUT_VERSION 0x0800
#endif
#include <dinput.h>

#include "../osal_device.h"

namespace nx::vms::client::desktop::joystick {

class OsWinApiDeviceWin: public OsalDevice
{
public:
    struct Device
    {
        LPDIRECTINPUTDEVICE8 inputDevice;
        JoystickDeviceInfo info;
    };

    OsWinApiDeviceWin(const Device& device, QTimer* pollingTimer);
    virtual ~OsWinApiDeviceWin() override;

    virtual JoystickDeviceInfo info() const override;

    void release();

protected:
    void poll();
    static bool enumerateObjectsCallback(LPCDIDEVICEOBJECTINSTANCE deviceObject, LPVOID devicePtr);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::joystick
