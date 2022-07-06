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

Device::StickPosition Device::currentStickPosition() const
{
    return m_stickPosition;
}

void Device::resetState()
{
    for (int i = 0; i < m_buttonStates.size(); ++i)
    {
        if (m_buttonStates[i])
            emit buttonReleased(i);
    }
    std::fill(m_buttonStates.begin(), m_buttonStates.end(), false);

    m_stickPosition.fill(0);
    emit stickMoved(0, 0, 0);

    emit stateChanged(m_stickPosition, m_buttonStates);
}

void Device::updateStickAxisLimits(const JoystickDescriptor& modelInfo)
{
    m_axisLimits[xIndex] = parseAxisLimits(modelInfo.xAxis, m_axisLimits[xIndex]);
    m_axisLimits[yIndex] = parseAxisLimits(modelInfo.yAxis, m_axisLimits[yIndex]);
    m_axisLimits[zIndex] = parseAxisLimits(modelInfo.zAxis, m_axisLimits[zIndex]);
    // Initialize stick position by zeroes (all three axes are in neutral position).
    m_stickPosition.fill(0);
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
    const auto newState = getNewState();

    if (newState.buttonStates.empty())
        return;

    bool deviceStateChanged = false;

    if (newState.stickPosition != m_stickPosition)
    {
        m_stickPosition = newState.stickPosition;
        deviceStateChanged = true;
        emit stickMoved(
            newState.stickPosition[xIndex],
            newState.stickPosition[yIndex],
            newState.stickPosition[zIndex]);
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
        emit stateChanged(newState.stickPosition, newState.buttonStates);
}

} // namespace nx::vms::client::desktop::joystick
