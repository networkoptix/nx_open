#pragma once

#include <plugins/io_device/joystick/joystick_types_fwd.h>
#include <plugins/io_device/joystick/joystick_common.h>

namespace nx {
namespace joystick {
namespace driver {

class AbstractJoystickDriver 
{
public:
    virtual ~AbstractJoystickDriver() {};

    virtual std::vector<nx::joystick::JoystickPtr> enumerateJoysticks() = 0;
    virtual bool setControlState(
        const QString& joystickId,
        const QString& controlId,
        const nx::joystick::State& state) = 0;

    virtual bool captureJoystick(nx::joystick::JoystickPtr& joystickToCapture) = 0;
    virtual bool releaseJoystick(nx::joystick::JoystickPtr& joystickToRelease) = 0;
};

} // namespace driver
} // namespace joystick
} // namespace nx