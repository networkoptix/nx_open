#pragma once

#include "abstract_joystick_control.h"

#include <set>

#include <nx/utils/log/log.h>
#include <plugins/io_device/joystick/joystick_common.h>

namespace nx {
namespace client {
namespace plugins {
namespace io_device {
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

    Range(StateElement minimum, StateElement maximum):
        min(minimum),
        max(maximum)
    {};

    StateElement min;
    StateElement max;
};

typedef std::vector<Range> Ranges;

class BaseControl: public AbstractControl
{
public:
    BaseControl(State::size_type stateSize);

    virtual QString getId() const override;
    virtual void setId(const QString& id) override;

    virtual AbstractJoystick* getParentDevice() const override;
    virtual void setParentDevice(AbstractJoystick* joytsick) override;

    virtual QString getDescription() const override;
    virtual void setDescription(const QString& description) override;

    virtual ControlCapabilities getCapabilities() const override;
    virtual void setCapabilities(ControlCapabilities capabilities) override;

    virtual State getState() const override;
    virtual void setState(const State& state) override;

    virtual void notifyControlStateChanged(const State& state) override;
    virtual bool addEventHandler(EventType eventType, EventHandler handler) override;

    virtual void applyOverride(
        const QString overrideName,
        const QString& overrideValue) override;

protected:
    typedef std::map<EventType, std::vector<EventHandler>> EventHandlerMap;
    typedef std::set<EventType> EventSet;

    virtual bool isEventTypeSupported(EventType eventType) const;
    virtual EventSet checkForEventsUnsafe() const;
    virtual void setStateUnsafe(const State& state);

    virtual EventParameters makeParametersForEvent(EventType eventType) const;

    virtual State fromRawToNormalized(const State& raw) const;
    virtual State fromNormalizedToRaw(const State& normalized) const;

protected:
    QString m_id;
    QString m_description;
    AbstractJoystick* m_parentDevice;
    ControlCapabilities m_capabilities;
    State m_state;

    EventHandlerMap m_eventHandlers;

    mutable QnMutex m_mutex;
};

} // namespace controls
} // namespace joystick
} // namespace io_device
} // namespace plugins
} // namespace client
} // namespace nx