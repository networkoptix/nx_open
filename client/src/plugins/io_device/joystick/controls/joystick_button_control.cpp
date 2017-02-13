#include "joystick_button_control.h"

namespace {

const nx::joystick::StateAtom kReleasedButtonState = 0;
const nx::joystick::StateAtom kPressedButtonState = 1;

const QString kWrongStateSizeMessage = lit("Wrong button state size.");

} // namespace 


namespace nx {
namespace joystick {
namespace controls {

Button::Button():
    m_previousState(ButtonState::released)
{
    m_state.resize(1);
}

ButtonState Button::getButtonState() const
{
    QnMutexLocker lock(&m_mutex);
    return getButtonStateUnsafe();
}

void Button::updateButtonState(ButtonState buttonState)
{
    State newState;

    {
        QnMutexLocker lock(&m_mutex);
        Q_ASSERT(m_state.size() == 1, kWrongStateSizeMessage);

        // TODO: #dmishin normalization is needed here
        m_previousState = fromStateAtomToButtonState(m_state[0]);
        newState.push_back(fromButtonStateToStateAtom(buttonState));
    }

    updateStateWithRawValue(newState);
}

void Button::updateButtonState(nx::joystick::State state)
{
    {
        QnMutexLocker lock(&m_mutex);
        Q_ASSERT(m_state.size() == 1, kWrongStateSizeMessage);
    }

    updateStateWithRawValue(state);
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

nx::joystick::StateAtom Button::fromButtonStateToStateAtom(ButtonState buttonState) const
{
    if (buttonState == ButtonState::released)
        return kReleasedButtonState;

    return kPressedButtonState;
}

ButtonState Button::fromStateAtomToButtonState(nx::joystick::StateAtom stateAtom) const
{
    if (stateAtom == kReleasedButtonState)
        return ButtonState::released;

    return ButtonState::pressed;
}

ButtonState Button::getButtonStateUnsafe() const
{
    Q_ASSERT(m_state.size() == 1, kWrongStateSizeMessage);
    Q_ASSERT(
        m_state[0] == kReleasedButtonState || m_state[0] == kPressedButtonState,
        lit("Wrong button state"));

    return fromStateAtomToButtonState(m_state[0]);
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