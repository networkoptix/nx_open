// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device.h"

#include "descriptors.h"

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
    m_axisLimits.clear();
    m_axisLimits.push_back(parseAxisLimits(modelInfo.xAxis));
    m_axisLimits.push_back(parseAxisLimits(modelInfo.yAxis));
    m_axisLimits.push_back(parseAxisLimits(modelInfo.zAxis));
    // Initialize stick position by zeroes (all three axes are in neutral position).
    m_stickPosition.assign(3, 0);
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
    const std::vector<double>& newStickPosition = newState.first;
    const std::vector<bool>& newButtonStates = newState.second;

    if (newStickPosition.empty() || newButtonStates.empty())
        return;

    bool deviceStateChanged = false;

    if (newStickPosition != m_stickPosition)
    {
        m_stickPosition = newStickPosition;
        deviceStateChanged = true;
        emit stickMoved(
            newStickPosition[0],
            newStickPosition[1],
            newStickPosition[2]);
    }

    if (newButtonStates != m_buttonStates)
    {
        for (int i = 0; i < m_buttonStates.size(); ++i)
        {
            if (m_buttonStates[i] == newButtonStates[i])
                continue;

            if (m_buttonStates[i])
                emit buttonReleased(i);
            else
                emit buttonPressed(i);
        }
        m_buttonStates = newButtonStates;
        deviceStateChanged = true;
    }

    if (deviceStateChanged)
        emit stateChanged(newStickPosition, newButtonStates);
}

} // namespace nx::vms::client::desktop::joystick
