#pragma once

#include "joystick_types_fwd.h"
#include "joystick_common.h"
#include <plugins/io_device/joystick/controls/abstract_joystick_control.h>

namespace nx {
namespace joystick {

class AbstractJoystick 
{

public:
    virtual ~AbstractJoystick(){};

    virtual void setId(const QString& id) = 0;
    virtual QString getId() const = 0;

    virtual QString getModel() const = 0;
    virtual void setModel(const QString& model) = 0;

    virtual QString getVendor() const = 0;
    virtual void setVendor(const QString& vendor) = 0;

    virtual std::vector<controls::ControlPtr> getControls() const = 0;
    virtual controls::ControlPtr getControlById(const QString& id) const = 0;
    virtual void setControls(std::vector<controls::ControlPtr> joystickControls) = 0;

    virtual bool setControlState(const QString& controlId, const State& state) = 0;
    virtual void updateControlStateWithRawValue(const QString& controlId, const State& state) = 0;

    virtual nx::joystick::driver::AbstractJoystickDriver* getDriver() const = 0;
    virtual void setDriver(nx::joystick::driver::AbstractJoystickDriver* driver) = 0;
};

typedef std::shared_ptr<AbstractJoystick> JoystickPtr;

} // namespace joystick
} // namespace nx