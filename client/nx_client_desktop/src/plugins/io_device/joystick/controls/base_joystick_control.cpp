#include "base_joystick_control.h"

#include <plugins/io_device/joystick/abstract_joystick.h>

namespace nx {
namespace client {
namespace plugins {
namespace io_device {
namespace joystick {
namespace controls {

BaseControl::BaseControl(State::size_type stateSize)
{
    m_state.resize(stateSize);
}

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

AbstractJoystick* BaseControl::getParentDevice() const 
{
    QnMutexLocker lock(&m_mutex);
    return m_parentDevice;
}

void BaseControl::setParentDevice(AbstractJoystick* parentJoystick)
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

ControlCapabilities BaseControl::getCapabilities() const 
{
    QnMutexLocker lock(&m_mutex);
    return m_capabilities;
}

void BaseControl::setCapabilities(ControlCapabilities capabilities)
{
    QnMutexLocker lock(&m_mutex);
    m_capabilities = capabilities;
}

State BaseControl::getState() const 
{
    QnMutexLocker lock(&m_mutex);
    return m_state;
}

void BaseControl::setState(const State& state)
{
    decltype(m_eventHandlers) handlers;
    EventSet eventsToBeFired;
    {
        QnMutexLocker lock(&m_mutex);
        if (!m_parentDevice)
            return;

        if (state == m_state)
            return;

        if (!m_parentDevice->setControlState(m_id, state))
            return;

        setStateUnsafe(state);

        eventsToBeFired = checkForEventsUnsafe();
        if (eventsToBeFired.empty())
            return;

        handlers = m_eventHandlers;
    }

    for (const auto& eventType: eventsToBeFired) 
    {
        if (handlers[eventType].empty())
            continue;

        auto parameters = makeParametersForEvent(eventType);

        for (auto& handler: handlers[eventType])
            handler(eventType, parameters);
    }  
}

void BaseControl::notifyControlStateChanged(const State& state)
{
    decltype(m_eventHandlers) handlers;
    EventSet eventsToBeFired;

    {
        QnMutexLocker lock(&m_mutex);
        if (m_state == state)
            return;

        setStateUnsafe(state);
        eventsToBeFired = checkForEventsUnsafe();
        if (eventsToBeFired.empty())
            return;

        handlers = m_eventHandlers;
    }

    for (const auto& eventType: eventsToBeFired) 
    {
        if (handlers[eventType].empty())
            continue;

        auto parameters = makeParametersForEvent(eventType);

        for (auto& handler: handlers[eventType])
            handler(eventType, parameters);
    }
}

bool BaseControl::addEventHandler(
    EventType eventType,
    EventHandler handler)
{
    if (!isEventTypeSupported(eventType))
        return false;

    QnMutexLocker lock(&m_mutex);
    m_eventHandlers[eventType].push_back(handler);

    return true;
}

void BaseControl::applyOverride(const QString overrideName, const QString& overrideValue)
{
    // Do nothing.
}

bool BaseControl::isEventTypeSupported(EventType eventType) const
{
    return false;
}

BaseControl::EventSet BaseControl::checkForEventsUnsafe() const
{
    return EventSet();
}

void BaseControl::setStateUnsafe(const State& state)
{
    m_state = state;
}

EventParameters BaseControl::makeParametersForEvent(
    EventType eventType) const
{
    EventParameters eventParameters;
    eventParameters.controlId = m_id;
    eventParameters.state = m_state;

    return eventParameters;
}

State BaseControl::fromRawToNormalized(const State& raw) const
{
    // No translation by default.
    return raw;
}

State BaseControl::fromNormalizedToRaw(const State& normalized) const
{
    // No translation by default.
    return normalized;
}

} // namespace controls
} // namespace joystick
} // namespace io_device
} // namespace plugins
} // namespace client
} // namespace nx