// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_hid.h"

#include <QtCore/QBitArray>
#include <QtCore/QPair>

#include "os_hid/os_hid_driver.h"

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
    QObject* parent)
    :
    base_type(modelInfo, path, nullptr, parent)
{
    OsHidDriver::getDriver()->setupDeviceSubscriber(path, this);

    m_bitCount = modelInfo.reportSize.toInt(/*ok*/ nullptr, kAutoDetectBase);
    m_bufferSize = (m_bitCount + 7) / 8;
    m_buffer.reset(new unsigned char[m_bufferSize]);

    for (int axisIndex = 0; axisIndex < Device::axisIndexCount; ++axisIndex)
        setAxisInitialized((Device::AxisIndexes)axisIndex, true);

    setInitializedButtonsNumber(modelInfo.buttons.size());

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
    OsHidDriver::getDriver()->removeDeviceSubscriber(this);
}

bool DeviceHid::isValid() const
{
    // We don't need this method with OsHidDriver.
    return true;
}

void DeviceHid::onStateChanged(const QBitArray& newOsHidLevelState)
{
    const State newState = parseOsHidLevelState(newOsHidLevelState);

    processNewState(newState);
}

Device::State DeviceHid::getNewState()
{
    NX_ASSERT(false, "This method should no be used in case of non-polling device.");
    return {};
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

Device::State DeviceHid::parseOsHidLevelState(const QBitArray& osHidLevelState)
{
    StickPosition newStickPosition;
    newStickPosition.fill(0);
    for (size_t i = 0; i < m_axisLocations.size() && i < newStickPosition.max_size(); ++i)
    {
        const auto state = parseData(osHidLevelState, m_axisLocations[i]);

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
        newButtonStates.push_back(mapButtonState(parseData(osHidLevelState, location)));

    return {newStickPosition, newButtonStates};
}

} // namespace nx::vms::client::desktop::joystick
