// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QBitArray>
#include <QtCore/QObject>
#include <QtCore/QVariant>

#include "joystick_device_info.h"

namespace nx::vms::client::desktop::joystick {

class OsalDevice: public QObject
{
    Q_OBJECT

public:
    virtual JoystickDeviceInfo info() const = 0;

    struct State
    {
        bool isAxisXInitialized;
        bool isAxisYInitialized;
        bool isAxisZInitialized;

        int x, minX, maxX;
        int y, minY, maxY;
        int z, minZ, maxZ;

        QBitArray buttons;

        QVariant rawData;
    };

signals:
    void stateChanged(const nx::vms::client::desktop::joystick::OsalDevice::State& state);
    void failed();
};

} // namespace nx::vms::client::desktop::joystick

Q_DECLARE_METATYPE(nx::vms::client::desktop::joystick::OsalDevice::State)
