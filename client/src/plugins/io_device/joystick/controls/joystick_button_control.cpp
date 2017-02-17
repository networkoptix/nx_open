#include "joystick_button_control.h"

namespace {

const nx::joystick::State::size_type kButtonStateSize = 1;

const nx::joystick::StateElement kReleasedButtonState = 0;
const nx::joystick::StateElement kPressedButtonState = 1;

const QString kWrongStateSizeMessage = lit("Wrong button state size.");

} // namespace 


namespace nx {
namespace joystick {
namespace controls {

Button::Button():
    base_type(kButtonStateSize),
    m_previousState(ButtonState::released)
{
}

ButtonState Button::getButtonState() const
{
    QnMutexLocker lock(&m_mutex);
    return getButtonStateUnsafe();
}

void Button::notifyButtonStateChanged(ButtonState buttonState)
{
    {
        QnMutexLocker lock(&m_mutex);
        Q_ASSERT(m_state.size() == 1);
    }

    State newState;
    newState.push_back(fromButtonStateToStateElement(buttonState));

    notifyControlStateChanged(newState);
}

void Button::notifyButtonStateChanged(const nx::joystick::State& state)
{
    {
        QnMutexLocker lock(&m_mutex);
        Q_ASSERT(m_state.size() == 1);
    }

    notifyControlStateChanged(state);
}

bool Button::isEventTypeSupported(EventType eventType) const
{
    return eventType == EventType::buttonUp 
        || eventType == EventType::buttonDown
        || eventType == EventType::buttonPressed
        || eventType == EventType::buttonDoublePressed;
}

BaseControl::EventSet Button::checkForEventsUnsafe() const 
{
    Q_ASSERT(m_state.size() == 1);
    EventSet eventSet;

    if (didButtonUpEventOccurUnsafe())
        eventSet.insert(EventType::buttonUp);

    if(didButtonDownEventOccurUnsafe())
        eventSet.insert(EventType::buttonDown);

    if(didButtonPressedEventOccurUnsafe())
        eventSet.insert(EventType::buttonPressed);

    if(didButtonDoublePressedEventOccurUnsafe())
        eventSet.insert(EventType::buttonDoublePressed);

    return eventSet;
}

void Button::setStateUnsafe(const nx::joystick::State& state)
{
    m_previousState = fromStateElementToButtonState(m_state[0]);
    m_state = state;
}

nx::joystick::StateElement Button::fromButtonStateToStateElement(ButtonState buttonState) const
{
    if (buttonState == ButtonState::released)
        return kReleasedButtonState;

    return kPressedButtonState;
}

ButtonState Button::fromStateElementToButtonState(nx::joystick::StateElement StateElement) const
{
    if (StateElement == kReleasedButtonState)
        return ButtonState::released;

    return ButtonState::pressed;
}

ButtonState Button::getButtonStateUnsafe() const
{
    Q_ASSERT(m_state.size() == 1);
    Q_ASSERT(m_state[0] == kReleasedButtonState || m_state[0] == kPressedButtonState);

    return fromStateElementToButtonState(m_state[0]);
}

bool Button::didButtonUpEventOccurUnsafe() const
{
    const auto currentState = getButtonStateUnsafe();

    return currentState == ButtonState::released
        && m_previousState == ButtonState::pressed;
}

bool Button::didButtonDownEventOccurUnsafe() const
{
    const auto currentState = getButtonStateUnsafe();

    return currentState == ButtonState::pressed
        && m_previousState == ButtonState::released;
}

bool Button::didButtonPressedEventOccurUnsafe() const
{
    const auto currentState = getButtonStateUnsafe();

    return currentState == ButtonState::released
        && m_previousState == ButtonState::pressed;
}

bool Button::didButtonDoublePressedEventOccurUnsafe() const
{
    return false; // TODO #dmishin add support for this event
}

} // namespace controls
} // namespace joystick
} // namespace nx