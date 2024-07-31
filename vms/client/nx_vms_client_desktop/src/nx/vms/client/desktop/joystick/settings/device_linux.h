// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QTimer>

#include "device.h"

namespace nx::vms::client::desktop::joystick {

struct JoystickDescriptor;
class Manager;

class DeviceLinux: public Device
{
    Q_OBJECT

    using base_type = Device;

public:
    DeviceLinux(
        const JoystickDescriptor& modelInfo,
        const QString& path,
        QTimer* pollTimer,
        Manager* manager);

    virtual ~DeviceLinux() override;

    virtual bool isValid() const override;

    void setFoundControlsNumber(int axesNumber, int buttonsNumber);

protected:
    virtual State getNewState() override;
    virtual AxisLimits parseAxisLimits(
        const AxisDescriptor& descriptor,
        const AxisLimits& oldLimits) const override;

private:
    int m_dev = -1;
};

} // namespace nx::vms::client::desktop::joystick
