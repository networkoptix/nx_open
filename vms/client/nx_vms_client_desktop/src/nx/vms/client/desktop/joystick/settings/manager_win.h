// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>

#include "device_win.h"
#include "manager.h"

#ifndef DIRECTINPUT_VERSION
    #define DIRECTINPUT_VERSION 0x0800
#endif
#include <dinput.h>

namespace nx::vms::client::desktop::joystick {

class ManagerWindows: public Manager
{
    Q_OBJECT

    using base_type = Manager;

    struct FoundDeviceInfo
    {
        LPDIRECTINPUTDEVICE8 directInputDeviceObject;

        QString model;
        QString guid;
    };

public:
    ManagerWindows(QObject* parent = 0);
    ~ManagerWindows();

protected:
    virtual void removeUnpluggedJoysticks(const QSet<QString>& foundDevicePaths) override;

private:
    void enumerateDevices();
    virtual void updateSearchState() override;

    DeviceWindowsPtr createDevice(
        const JoystickDescriptor& deviceConfig,
        const QString& path,
        LPDIRECTINPUTDEVICE8 directInputDeviceObject);
    std::pair<QString, QString> getDeviceModelAndGuid(LPDIRECTINPUTDEVICE8 inputDevice) const;

    static bool enumDevicesCallback(LPCDIDEVICEINSTANCE deviceInstance, LPVOID managerPtr);

    void onDeviceFailed(const QString& path);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::joystick
