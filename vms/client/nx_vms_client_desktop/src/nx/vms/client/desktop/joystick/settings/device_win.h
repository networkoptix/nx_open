// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#ifndef DIRECTINPUT_VERSION
    #define DIRECTINPUT_VERSION 0x0800
#endif
#include <dinput.h>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QTimer>

#include "device.h"

namespace nx::vms::client::desktop::joystick {

struct JoystickDescriptor;
class Manager;

class DeviceWindows: public Device
{
    Q_OBJECT

    using base_type = Device;

public:
    DeviceWindows(
        LPDIRECTINPUTDEVICE8 inputDevice,
        const JoystickDescriptor& modelInfo,
        const QString& path,
        QTimer* pollTimer,
        Manager* manager);

    virtual ~DeviceWindows() override;

    virtual bool isValid() const override;

    bool isInitialized() const;

protected:
    virtual State getNewState() override;
    virtual AxisLimits parseAxisLimits(
        const AxisDescriptor& descriptor,
        const AxisLimits& oldLimits) const override;
    static bool enumObjectsCallback(LPCDIDEVICEOBJECTINSTANCE deviceObject, LPVOID devicePtr);
    static void formAxisLimits(int min, int max, Device::AxisLimits* limits);

private:
    LPDIRECTINPUTDEVICE8 m_inputDevice;
};


using DeviceWindowsPtr = QSharedPointer<DeviceWindows>;

} // namespace nx::vms::client::desktop::joystick
