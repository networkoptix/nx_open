// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <hidapi/hidapi.h>

#include <QtCore/QObject>
#include <QtCore/QBitArray>
#include <QtCore/QString>
#include <QtCore/QTimer>

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
    DeviceHid(
        const JoystickDescriptor& modelInfo,
        const QString& path,
        QTimer* pollTimer,
        QObject* parent = 0);

    virtual ~DeviceHid() override;

    virtual bool isValid() const override;

protected:
    virtual State getNewState() override;
    virtual AxisLimits parseAxisLimits(
        const AxisDescriptor& descriptor,
        const AxisLimits& oldLimits) const override;

private:
    ParsedFieldLocation parseLocation(const FieldLocation& location);
    QBitArray parseData(const QBitArray& buffer, const ParsedFieldLocation& location);

private:
    hid_device* m_dev = nullptr;

    int m_bufferSize = 0;
    QScopedArrayPointer<unsigned char> m_buffer;

    QBitArray m_reportData;

    int m_bitCount = 0;
    std::vector<ParsedFieldLocation> m_axisLocations;
    std::vector<ParsedFieldLocation> m_buttonLocations;
};

} // namespace nx::vms::client::desktop::joystick
