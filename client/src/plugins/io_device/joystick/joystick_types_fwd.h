#pragma once

namespace nx {
namespace joystick {

class AbstractJoystick;
typedef std::shared_ptr<AbstractJoystick> JoystickPtr;

namespace driver {

class AbstractJoystickDriver;

} // namespace driver

namespace controls {

class AbstractControl;
typedef std::shared_ptr<AbstractControl> ControlPtr;

} // namespace controls

} // namespace joystick
} // namespace nx