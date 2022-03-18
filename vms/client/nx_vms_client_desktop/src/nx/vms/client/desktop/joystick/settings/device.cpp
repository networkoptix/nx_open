// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device.h"

#include "descriptors.h"

namespace {

// QString.toInt() could autodetect integer base by their format (0x... for hex numbers, etc.).
constexpr int kAutoDetectBase = 0;

} // namespace

namespace nx::vms::client::desktop::joystick {

Device::Device(
    const JoystickDescriptor& modelInfo,
    const QString& path,
    QTimer* pollTimer,
    QObject* parent)
    :
    QObject(parent),
    m_id(modelInfo.id),
    m_modelName(modelInfo.model),
    m_path(path),
    m_buttonStates(modelInfo.buttons.size(), false)
{
    connect(pollTimer, &QTimer::timeout, this, &Device::pollData);
}

Device::~Device()
{
}

QString Device::id() const
{
    return m_id;
}

QString Device::modelName() const
{
    return m_modelName;
}

QString Device::path() const
{
    return m_path;
}

void Device::updateStickAxisLimits(const JoystickDescriptor& modelInfo)
{
    m_axisLimits[xIndex] = parseAxisLimits(modelInfo.xAxis);
    m_axisLimits[yIndex] = parseAxisLimits(modelInfo.yAxis);
    m_axisLimits[zIndex] = parseAxisLimits(modelInfo.zAxis);
    // Initialize stick position by zeroes (all three axes are in neutral position).
    m_stickPosition.fill(0);
}

Device::AxisLimits Device::parseAxisLimits(const AxisDescriptor& descriptor)
{
    Device::AxisLimits result;
    result.min = descriptor.min.toInt(/*ok*/ nullptr, kAutoDetectBase);
    result.max = descriptor.max.toInt(/*ok*/ nullptr, kAutoDetectBase);
    result.mid = descriptor.mid.toInt(/*ok*/ nullptr, kAutoDetectBase);
    result.bounce = descriptor.bounce.toInt(/*ok*/ nullptr, kAutoDetectBase);
    if (result.mid == 0)
        result.mid = (result.min + result.max) / 2;
    result.sensitivity = descriptor.sensitivity.toDouble(/*ok*/ nullptr);
    return result;
}

double Device::mapAxisState(int rawValue, const AxisLimits& limits)
{
    if (rawValue < limits.mid - limits.bounce)
    {
        // "Negative" part of the axis.
        return -1. * limits.sensitivity * (limits.mid - rawValue) / (limits.mid - limits.min);
    }
    else if (rawValue > limits.mid + limits.bounce)
    {
        // "Positive" part of the axis.
        return 1. * limits.sensitivity * (rawValue - limits.mid) / (limits.max - limits.mid);
    }
    else
    {
        // Neutral point.
        return 0;
    }
}

void Device::pollData()
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    const auto newState = getNewState();

    if (newState.buttonStates.empty())
        return;

    bool deviceStateChanged = false;

    if (newState.stickPositions != m_stickPosition)
    {
        m_stickPosition = newState.stickPositions;
        deviceStateChanged = true;
        emit stickMoved(
            newState.stickPositions[xIndex],
            newState.stickPositions[yIndex],
            newState.stickPositions[zIndex]);
    }

    NX_ASSERT(m_buttonStates.size() == newState.buttonStates.size());

    if (newState.buttonStates != m_buttonStates)
    {
        for (int i = 0; i < m_buttonStates.size() && i < newState.buttonStates.size(); ++i)
        {
            if (m_buttonStates[i] == newState.buttonStates[i])
                continue;

            if (m_buttonStates[i])
                emit buttonReleased(i);
            else
                emit buttonPressed(i);
        }

        m_buttonStates = newState.buttonStates;
        deviceStateChanged = true;
    }

    if (deviceStateChanged)
        emit stateChanged(newState.stickPositions, newState.buttonStates);
}

} // namespace nx::vms::client::desktop::joystick
