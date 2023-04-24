// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QBitArray>
#include <QtCore/QObject>
#include <QtCore/QString>

#include "descriptors.h"
#include "device.h"

namespace nx::vms::client::desktop::joystick {

struct JoystickDescriptor;

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
    DeviceHid(const JoystickDescriptor& modelInfo, const QString& path, QObject* parent = nullptr);

    virtual ~DeviceHid() override;

    virtual bool isValid() const override;

public slots:
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
    int m_bufferSize = 0;
    QScopedArrayPointer<unsigned char> m_buffer;

    int m_bitCount = 0;
    std::vector<ParsedFieldLocation> m_axisLocations;
    std::vector<ParsedFieldLocation> m_buttonLocations;
};

} // namespace nx::vms::client::desktop::joystick
