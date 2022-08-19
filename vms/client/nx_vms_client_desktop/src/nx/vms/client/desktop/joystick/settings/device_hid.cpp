// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_hid.h"

#include <QtCore/QBitArray>
#include <QtCore/QPair>

namespace {

bool mapButtonState(const QBitArray& state)
{
    // We assume that button has only two states: either pressed or not.
    return state.count(true) > 0;
}

} // namespace

namespace nx::vms::client::desktop::joystick {

DeviceHid::DeviceHid(
    const JoystickDescriptor& modelInfo,
    const QString& path,
    QTimer* pollTimer,
    QObject* parent)
    :
    base_type(modelInfo, path, pollTimer, parent)
{
    m_dev = hid_open_path(path.toStdString().c_str());

    m_bitCount = modelInfo.reportSize.toInt(/*ok*/ nullptr, kAutoDetectBase);
    m_bufferSize = (m_bitCount + 7) / 8;
    m_buffer.reset(new unsigned char[m_bufferSize]);

    // Initialize stick axes (x, y, z).
    m_axisLocations.push_back(parseLocation(modelInfo.xAxis.bits));
    m_axisLocations.push_back(parseLocation(modelInfo.yAxis.bits));
    m_axisLocations.push_back(parseLocation(modelInfo.zAxis.bits));
    for (const auto& button: modelInfo.buttons)
        m_buttonLocations.push_back(parseLocation(button.bits));

    updateStickAxisLimits(modelInfo);
}

DeviceHid::~DeviceHid()
{
    if (m_dev)
        hid_close(m_dev);
}

bool DeviceHid::isValid() const
{
    return m_dev;
}

Device::State DeviceHid::getNewState()
{
    if (!isValid())
        return {};

    int bytesRead = hid_read(m_dev, m_buffer.data(), m_bufferSize);
    if (bytesRead < m_bufferSize)
        return {};

    const auto newReportData = QBitArray::fromBits((char *)(m_buffer.data()), m_bitCount);
    if (m_reportData == newReportData)
        return {};
    m_reportData = newReportData;

    StickPosition newStickPosition;
    newStickPosition.fill(0);
    for (size_t i = 0; i < m_axisLocations.size() && i < newStickPosition.max_size(); ++i)
    {
        const auto state = parseData(m_reportData, m_axisLocations[i]);

        int rawValue = 0;
        for (int j = 0; j < state.size(); ++j)
        {
            if (state.at(j))
                rawValue |= 1 << j;
        }

        newStickPosition[i] = mapAxisState(rawValue, m_axisLimits[i]);
    }

    ButtonStates newButtonStates;
    for (const auto& location: m_buttonLocations)
    {
        newButtonStates.push_back(mapButtonState(parseData(m_reportData, location)));
    }

    return {newStickPosition, newButtonStates};
}

Device::AxisLimits DeviceHid::parseAxisLimits(
    const AxisDescriptor& descriptor,
    const AxisLimits& /*oldLimits*/) const
{
    Device::AxisLimits result;
    result.min = descriptor.min.toInt(/*ok*/ nullptr, kAutoDetectBase);
    result.max = descriptor.max.toInt(/*ok*/ nullptr, kAutoDetectBase);
    result.mid = descriptor.mid.toInt(/*ok*/ nullptr, kAutoDetectBase);

    result.bounce = descriptor.bounce.toInt(/*ok*/ nullptr, kAutoDetectBase);

    const int minBounce = (result.max - result.min) / 100.0 * kMinBounceInPercentages;
    if (minBounce > result.bounce)
        result.bounce = minBounce;

    if (result.mid == 0)
        result.mid = (result.min + result.max) / 2;
    result.sensitivity = descriptor.sensitivity.toDouble(/*ok*/ nullptr);
    return result;
}

DeviceHid::ParsedFieldLocation DeviceHid::parseLocation(const FieldLocation& location)
{
    DeviceHid::ParsedFieldLocation result;

    for (const auto& interval: location.split(',', Qt::SkipEmptyParts))
    {
        auto borders = interval.split('-');

        // Check for intervals with omitted 'last' part.
        if (borders.size() == 1)
            borders << borders.first();

        if (borders.size() != 2)
            return {};

        bool ok1 = false;
        bool ok2 = false;
        int first = borders[0].toInt(&ok1, kAutoDetectBase);
        int last = borders[1].toInt(&ok2, kAutoDetectBase);

        if (!ok1 || !ok2)
            return {};

        result.intervals.push_back({first, last});
        // Calc and store the field size.
        // It never changes after initialization, so there's no need for a separate function.
        result.size += last - first + 1;
    }

    return result;
}

QBitArray DeviceHid::parseData(const QBitArray& buffer, const ParsedFieldLocation& location)
{
    QBitArray result(location.size);

    int pos = 0;
    for (const auto& interval: location.intervals)
    {
        // Iterate over data bits. TODO: support inverted bit order if some devices would use it.
        for (int i = interval.first; i <= interval.second; ++i)
        {
            result.setBit(pos, buffer.at(i));
            ++pos;
        }
    }

    return result;
}

} // namespace nx::vms::client::desktop::joystick
