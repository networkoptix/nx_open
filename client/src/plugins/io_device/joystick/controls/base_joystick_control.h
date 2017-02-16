#pragma once

#include "abstract_joystick_control.h"

#include <set>

#include <utils/common/log.h>
#include <plugins/io_device/joystick/joystick_common.h>

namespace nx {
namespace joystick {

class AbstractJoystick;

namespace controls {


struct Range
{
    static const int kDefaultMinRangeValue = 100;
    static const int kDefaultMaxRangeValue = -100;

    Range(): 
        min(kDefaultMinRangeValue),
        max(kDefaultMaxRangeValue) 
    {};

    Range(nx::joystick::StateElement minimum, nx::joystick::StateElement maximum):
        min(minimum),
        max(maximum)
    {};

    nx::joystick::StateElement min;
    nx::joystick::StateElement max;
};

typedef std::vector<Range> Ranges;

class BaseControl: public AbstractControl
{
public:
    BaseControl(nx::joystick::State::size_type stateSize);

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

    virtual void notifyControlStateChanged(const nx::joystick::State& state) override;
    virtual bool addEventHandler(
        nx::joystick::EventType eventType,
        nx::joystick::EventHandler handler) override;

    virtual void applyOverride(
        const QString overrideName,
        const QString& overrideValue) override;

protected:
    typedef std::map<nx::joystick::EventType, std::vector<EventHandler>> EventHandlerMap;
    typedef std::set<nx::joystick::EventType> EventSet; 

    virtual bool isEventTypeSupported(EventType eventType) const;
    virtual EventSet checkForEventsUnsafe() const;
    virtual void setStateUnsafe(const nx::joystick::State& state);
    virtual nx::joystick::EventParameters makeParametersForEvent(
        nx::joystick::EventType eventType) const;

    virtual nx::joystick::State fromRawToNormalized(const nx::joystick::State& raw) const;
    virtual nx::joystick::State fromNormalizedToRaw(const nx::joystick::State& normalized) const;

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