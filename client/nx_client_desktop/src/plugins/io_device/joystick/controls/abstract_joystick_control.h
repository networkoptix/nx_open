#pragma once

#include <plugins/io_device/joystick/joystick_common.h>
#include <plugins/io_device/joystick/joystick_config.h>

namespace nx {
namespace client {
namespace plugins {
namespace io_device {
namespace joystick {

class AbstractJoystick;

namespace controls {

class AbstractControl 
{

public:
    virtual ~AbstractControl() {};

    virtual QString getId() const = 0;
    virtual void setId(const QString& id) = 0;

    virtual AbstractJoystick* getParentDevice() const = 0;
    virtual void setParentDevice(AbstractJoystick* parentJoystick) = 0;

    virtual QString getDescription() const = 0;
    virtual void setDescription(const QString& description) = 0;

    virtual ControlCapabilities getCapabilities() const = 0;
    virtual void setCapabilities(ControlCapabilities capabilities) = 0;

    virtual State getState() const = 0;
    virtual void setState(const State& state) = 0;

    virtual State fromRawToNormalized(const State& raw) const = 0;
    virtual State fromNormalizedToRaw(const State& raw) const = 0;

    virtual void notifyControlStateChanged(const State& state) = 0;

    virtual void applyOverride(const QString overrideName, const QString& overrideValue) = 0;

    virtual bool addEventHandler(
        EventType eventType,
        EventHandler handler) = 0;
};

} // namespace controls
} // namespace joystick
} // namespace io_device
} // namespace plugins
} // namespace client
} // namespace nx