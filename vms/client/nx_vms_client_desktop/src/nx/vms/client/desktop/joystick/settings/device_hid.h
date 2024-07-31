// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QBitArray>
#include <QtCore/QObject>
#include <QtCore/QString>

#include <nx/utils/impl_ptr.h>

#include "descriptors.h"
#include "device.h"

namespace nx::vms::client::desktop::joystick {

struct JoystickDescriptor;
class Manager;

class DeviceHid: public Device
{
    Q_OBJECT

    using base_type = Device;

    struct ParsedFieldLocation
    {
        int size = 0;
        std::vector<QPair<int, int>> intervals;
    };

public:
    DeviceHid(const JoystickDescriptor& modelInfo, const QString& path, Manager* manager);

    virtual ~DeviceHid() override;

    virtual bool isValid() const override;

public:
    void onStateChanged(const QBitArray& newState);

protected:
    virtual State getNewState() override;
    virtual AxisLimits parseAxisLimits(
        const AxisDescriptor& descriptor,
        const AxisLimits& oldLimits) const override;

    friend class ManagerHid;

private:
    QBitArray parseData(const QBitArray& buffer, const ParsedFieldLocation& location);
    State parseOsHidLevelState(const QBitArray& osHidLevelState);

public:
    static ParsedFieldLocation parseLocation(const FieldLocation& location);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::joystick
