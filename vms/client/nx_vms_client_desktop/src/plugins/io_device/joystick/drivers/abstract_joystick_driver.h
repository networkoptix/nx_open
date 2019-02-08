#pragma once

#include <plugins/io_device/joystick/joystick_types_fwd.h>
#include <plugins/io_device/joystick/joystick_common.h>

namespace nx {
namespace client {
namespace plugins {
namespace io_device {
namespace joystick {
namespace driver {

class AbstractJoystickDriver 
{
public:
    virtual ~AbstractJoystickDriver() {};

    virtual std::vector<JoystickPtr> enumerateJoysticks() = 0;
    virtual bool setControlState(
        const QString& joystickId,
        const QString& controlId,
        const State& state) = 0;

    virtual bool captureJoystick(JoystickPtr& joystickToCapture) = 0;
    virtual bool releaseJoystick(JoystickPtr& joystickToRelease) = 0;
};

} // namespace driver
} // namespace joystick
} // namespace io_device
} // namespace plugins
} // namespace client
} // namespace nx