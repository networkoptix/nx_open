#include "base_joystick_control.h"

#include <plugins/io_device/joystick/abstract_joystick.h>

namespace nx {
namespace joystick {
namespace controls {

QString BaseControl::getId() const
{
    QnMutexLocker lock(&m_mutex);
    return m_id;
}

void BaseControl::setId(const QString& id)
{
    QnMutexLocker lock(&m_mutex);
    m_id = id;
}

nx::joystick::AbstractJoystick* BaseControl::getParentDevice() const 
{
    QnMutexLocker lock(&m_mutex);
    return m_parentDevice;
}

void BaseControl::setParentDevice(nx::joystick::AbstractJoystick* parentJoystick)
{
    QnMutexLocker lock(&m_mutex);
    m_parentDevice = parentJoystick;
}

QString BaseControl::getDescription() const 
{
    QnMutexLocker lock(&m_mutex);
    return m_description;
}

void BaseControl::setDescription(const QString& description)
{
    QnMutexLocker lock(&m_mutex);
    m_description = description;
}

nx::joystick::ControlCapabilities BaseControl::getCapabilities() const 
{
    QnMutexLocker lock(&m_mutex);
    return m_capabilities;
}

void BaseControl::setCapabilities(nx::joystick::ControlCapabilities capabilities)
{
    QnMutexLocker lock(&m_mutex);
    m_capabilities = capabilities;
}

nx::joystick::State BaseControl::getState() const 
{
    QnMutexLocker lock(&m_mutex);
    return m_state;
}

void BaseControl::setState(const nx::joystick::State& state)
{
    QnMutexLocker lock(&m_mutex);
    if (!m_parentDevice)
        return;

    if (state == m_state)
        return;

    if (m_parentDevice->setControlState(m_id, state))
        setStateAndFireEventsUnsafe(state);
}

void BaseControl::updateStateWithRawValue(const nx::joystick::State& state)
{
    QnMutexLocker lock(&m_mutex);

    auto normalizedState = fromRawToNormalized(state);
    if (m_state == normalizedState)
        return;
    qDebug() << "SETTING STATE AND FIRING EVENTS";
    setStateAndFireEventsUnsafe(normalizedState);
}

bool BaseControl::addEventHandler(
    nx::joystick::EventType eventType,
    nx::joystick::EventHandler handler)
{
    if (!isEventTypeSupported(eventType))
        return false;

    QnMutexLocker lock(&m_mutex);
    m_eventHandlers[eventType].push_back(handler);

    return true;
}

bool BaseControl::isEventTypeSupported(nx::joystick::EventType eventType) const
{
    return false;
}

BaseControl::EventSet BaseControl::checkForEventsUnsafe() const
{
    return EventSet();
}

void BaseControl::fireEventsUnsafe(const EventSet& eventsToBeFired)
{
    for (const auto& eventType: eventsToBeFired) 
    {
        if (m_eventHandlers[eventType].empty())
            continue;

        auto parameters = makeParametersForEvent(eventType);
        for (auto& handler: m_eventHandlers[eventType])
            handler(eventType, parameters);
    }
}

nx::joystick::EventParameters BaseControl::makeParametersForEvent(
    nx::joystick::EventType eventType) const
{
    nx::joystick::EventParameters eventParameters;
    eventParameters.state = m_state;

    return eventParameters;
}

nx::joystick::State BaseControl::fromRawToNormalized(const nx::joystick::State& raw) const
{
    // No translation by default.
    return raw;
}

nx::joystick::State BaseControl::fromNormalizedToRaw(const nx::joystick::State& normalized) const
{
    // No translation by default.
    return normalized;
}

void BaseControl::setStateAndFireEventsUnsafe(const nx::joystick::State& state)
{
    m_state = state;
    auto eventsToBeFired = checkForEventsUnsafe();
    if (!eventsToBeFired.empty())
        fireEventsUnsafe(eventsToBeFired);
}

} // namespace controls
} // namespace joystick
} // namespace nx