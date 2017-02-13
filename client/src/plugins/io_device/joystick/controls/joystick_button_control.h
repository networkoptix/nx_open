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

public:
    Button();
    virtual ButtonState getButtonState() const;
    virtual void updateButtonState(ButtonState buttonState);
    virtual void updateButtonState(nx::joystick::State state);

protected:
    virtual bool isEventTypeSupported(nx::joystick::EventType eventType) const override;
    virtual EventSet checkForEventsUnsafe() const override;

private:
    nx::joystick::StateAtom fromButtonStateToStateAtom(ButtonState buttonState) const;
    ButtonState fromStateAtomToButtonState(nx::joystick::StateAtom stateAtom) const;

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

