// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

namespace nx::vms::client::desktop::joystick {

struct JoystickDeviceInfo
{
    QString id;
    QString path;
    QString modelName;
    QString manufacturerName;
};

} // namespace nx::vms::client::desktop::joystick
