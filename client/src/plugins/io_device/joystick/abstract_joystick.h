#pragma once

#include "joystick_types_fwd.h"
#include "joystick_common.h"
#include <plugins/io_device/joystick/controls/abstract_joystick_control.h>

namespace nx {
namespace client {
namespace plugins {
namespace io_device {
namespace joystick {


class AbstractJoystick 
{

public:
    virtual ~AbstractJoystick(){};

    virtual QString id() const = 0;
    virtual void setId(const QString& id) = 0;

    virtual QString model() const = 0;
    virtual void setModel(const QString& model) = 0;

    virtual QString vendor() const = 0;
    virtual void setVendor(const QString& vendor) = 0;

    virtual std::vector<controls::ControlPtr> controls() const = 0;
    virtual controls::ControlPtr controlById(const QString& id) const = 0;
    virtual void setControls(std::vector<controls::ControlPtr> joystickControls) = 0;

    virtual bool setControlState(const QString& controlId, const State& state) = 0;
    virtual void notifyControlStateChanged(const QString& controlId, const State& state) = 0;

    virtual driver::AbstractJoystickDriver* driver() const = 0;
    virtual void setDriver(driver::AbstractJoystickDriver* driver) = 0;
};

} // namespace joystick
} // namespace io_device
} // namespace plugins
} // namespace client
} // namespace nx