#pragma once

#include "base_joystick_control.h"

namespace nx {
namespace joystick {
namespace controls {

enum class ButtonState
{
    released,
    pressed
};

class Button: public BaseControl
{
    typedef BaseControl base_type;

public:
    Button();
    virtual ButtonState getButtonState() const;
    virtual void notifyButtonStateChanged(ButtonState buttonState);
    virtual void notifyButtonStateChanged(const nx::joystick::State& state);

protected:
    virtual bool isEventTypeSupported(nx::joystick::EventType eventType) const override;
    virtual EventSet checkForEventsUnsafe() const override;
    virtual void setStateUnsafe(const nx::joystick::State& state);

private:
    nx::joystick::StateElement fromButtonStateToStateElement(ButtonState buttonState) const;
    ButtonState fromStateElementToButtonState(nx::joystick::StateElement StateElement) const;

    ButtonState getButtonStateUnsafe() const;

    bool didButtonUpEventOccurUnsafe() const;
    bool didButtonDownEventOccurUnsafe() const;
    bool didButtonPressedEventOccurUnsafe() const;
    bool didButtonDoublePressedEventOccurUnsafe() const;

private:
    ButtonState m_previousState;
};

typedef std::shared_ptr<Button> ButtonPtr;

} // namespace controls
} // namespace joystick
} // namespace nx

