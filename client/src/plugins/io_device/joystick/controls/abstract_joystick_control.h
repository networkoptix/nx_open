#pragma once

#include <plugins/io_device/joystick/joystick_common.h>
#include <plugins/io_device/joystick/joystick_mapping.h>

namespace nx {
namespace joystick {

class AbstractJoystick;

namespace controls {

class AbstractControl 
{

public:
    virtual ~AbstractControl() {};

    virtual QString getId() const = 0;
    virtual void setId(const QString& id) = 0;

    virtual nx::joystick::AbstractJoystick* getParentDevice() const = 0;
    virtual void setParentDevice(nx::joystick::AbstractJoystick* parentJoystick) = 0;

    virtual QString getDescription() const = 0;
    virtual void setDescription(const QString& description) = 0;

    virtual nx::joystick::ControlCapabilities getCapabilities() const = 0;
    virtual void setCapabilities(nx::joystick::ControlCapabilities capabilities) = 0;

    virtual nx::joystick::State getState() const = 0;
    virtual void setState(const nx::joystick::State& state) = 0;

    virtual nx::joystick::State fromRawToNormalized(const nx::joystick::State& raw) const = 0;
    virtual nx::joystick::State fromNormalizedToRaw(const nx::joystick::State& raw) const = 0;

    virtual void notifyControlStateChanged(const nx::joystick::State& state) = 0;

    virtual void applyOverride(const QString overrideName, const QString& overrideValue) = 0;

    virtual bool addEventHandler(
        nx::joystick::EventType eventType,
        nx::joystick::EventHandler handler) = 0;
};

} // namespace controls
} // namespace joystick
} // namespace nx