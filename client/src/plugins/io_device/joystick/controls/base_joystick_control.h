#pragma once

#include "abstract_joystick_control.h"

#include <set>

#include <utils/common/log.h>
#include <plugins/io_device/joystick/joystick_common.h>

namespace nx {
namespace joystick {

class AbstractJoystick;

namespace controls {

struct Limits
{
    Limits(): min(0), max(0) {};
    Limits(nx::joystick::StateAtom minimum, nx::joystick::StateAtom maximum):
        min(minimum),
        max(maximum)
    {};

    nx::joystick::StateAtom min;
    nx::joystick::StateAtom max;
};

typedef std::vector<Limits> LimitsVector;

class BaseControl: public AbstractControl
{
public:
    virtual QString getId() const override;
    virtual void setId(const QString& id) override;

    virtual nx::joystick::AbstractJoystick* getParentDevice() const override;
    virtual void setParentDevice(nx::joystick::AbstractJoystick* joytsick) override;

    virtual QString getDescription() const override;
    virtual void setDescription(const QString& description) override;

    virtual nx::joystick::ControlCapabilities getCapabilities() const override;
    virtual void setCapabilities(nx::joystick::ControlCapabilities capabilities) override;

    virtual nx::joystick::State getState() const override;
    virtual void setState(const nx::joystick::State& state) override;

    virtual void updateStateWithRawValue(const nx::joystick::State& state) override;
    virtual bool addEventHandler(nx::joystick::EventType eventType, nx::joystick::EventHandler handler) override;

protected:
    typedef std::map<nx::joystick::EventType, std::vector<EventHandler>> EventHandlerMap;
    typedef std::set<nx::joystick::EventType> EventSet; 

    virtual bool isEventTypeSupported(EventType eventType) const;
    virtual EventSet checkForEventsUnsafe() const;
    virtual void fireEventsUnsafe(const EventSet& eventSet);
    virtual nx::joystick::EventParameters makeParametersForEvent(
        nx::joystick::EventType eventType) const;

    virtual nx::joystick::State fromRawToNormalized(const nx::joystick::State& raw) const;
    virtual nx::joystick::State fromNormalizedToRaw(const nx::joystick::State& normalized) const;

private:
    void setStateAndFireEventsUnsafe(const nx::joystick::State& state);

protected:
    QString m_id;
    QString m_description;
    nx::joystick::AbstractJoystick* m_parentDevice;
    nx::joystick::ControlCapabilities m_capabilities;
    State m_state;
    
    EventHandlerMap m_eventHandlers;
   
    mutable QnMutex m_mutex;
};

} // namespace controls
} // namespace joystick
} // namespace nx