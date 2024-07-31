// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device.h"

#include <QtCore/QTimer>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>

#include "descriptors.h"
#include "manager.h"

namespace nx::vms::client::desktop::joystick {

Device::Device(
    const JoystickDescriptor& modelInfo,
    const QString& path,
    QTimer* pollTimer,
    Manager* manager)
    :
    QObject(manager),
    m_manager(manager),
    m_id(modelInfo.id),
    m_modelName(modelInfo.model),
    m_path(path),
    m_buttonStates(modelInfo.buttons.size(), false)
{
    if (pollTimer)
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

    emit stateChanged(m_stickPosition, m_buttonStates);
}

int Device::zAxisIsInitialized() const
{
    return m_zAxisInitialized;
}

qsizetype Device::initializedButtonsNumber() const
{
    return m_initializedButtonsNumber;
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

void Device::processNewState(const State& newState)
{
    bool deviceStateChanged = false;

    if (newState.stickPosition != m_stickPosition)
    {
        m_stickPosition = newState.stickPosition;
        deviceStateChanged = true;
    }

    if (newState.buttonStates != m_buttonStates)
    {
        for (int i = 0; i < m_buttonStates.size() && i < newState.buttonStates.size(); ++i)
        {
            if (m_buttonStates[i] == newState.buttonStates[i])
                continue;

            m_buttonStates[i] = newState.buttonStates[i];

            if (m_buttonStates[i])
                emit buttonPressed(i);
            else
                emit buttonReleased(i);
        }

        deviceStateChanged = true;
    }

    if (deviceStateChanged)
        emit stateChanged(m_stickPosition, m_buttonStates);
}

void Device::pollData()
{
    NX_TRACE(this, "Polling joystick device...");

    const auto newState = getNewState();

    if (newState.buttonStates.empty())
        return;

    processNewState(newState);
}

bool Device::axisIsInitialized(AxisIndexes index) const
{
    switch (index)
    {
        case xIndex:
            return m_xAxisInitialized;
        case yIndex:
            return m_yAxisInitialized;
        case zIndex:
            return m_zAxisInitialized;
        default: // Any other axes are not important.
            return false;
    }
}

void Device::setAxisInitialized(AxisIndexes index, bool isInitialized)
{
    switch (index)
    {
        case xIndex:
        {
            m_xAxisInitialized = isInitialized;
            break;
        }
        case yIndex:
        {
            m_yAxisInitialized = isInitialized;
            break;
        }
        case zIndex:
        {
            m_zAxisInitialized = isInitialized;
            break;
        }
        default:
        {
            // Any other axes are not important.
            return;
        }
    }

    emit axisIsInitializedChanged(index);
}

void Device::setInitializedButtonsNumber(int number)
{
    if (m_initializedButtonsNumber == number)
        return;

    m_initializedButtonsNumber = number;
    emit initializedButtonsNumberChanged();
}

} // namespace nx::vms::client::desktop::joystick
