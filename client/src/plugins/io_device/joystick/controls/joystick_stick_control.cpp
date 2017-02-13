#include "joystick_stick_control.h"
#include <utils/math/fuzzy.h>

namespace {

const int kMaxDegreesOfFreedom = 6;
const nx::joystick::StateAtom kZeroPositionArray[] = {0, 0, 0, 0, 0, 0};
const std::vector<nx::joystick::StateAtom> kZeroPositionState(
    kZeroPositionArray,
    kZeroPositionArray + sizeof(kZeroPositionArray) / sizeof(nx::joystick::StateAtom));

} // namespace

namespace nx {
namespace joystick {
namespace controls {

Stick::Stick()
{
    m_state.resize(kMaxDegreesOfFreedom);
    m_previousPosition.resize(kMaxDegreesOfFreedom);
    m_limits.resize(kMaxDegreesOfFreedom);
    m_outputLimits.resize(kMaxDegreesOfFreedom);
}

LimitsVector Stick::getOutputLimits() const
{
    QnMutexLocker lock(&m_mutex);
    return m_outputLimits;
}

void Stick::setOutputLimits(LimitsVector outputLimits)
{
    QnMutexLocker lock(&m_mutex);
    m_outputLimits = outputLimits;
}

nx::joystick::StateAtom Stick::getStickX() const
{
    QnMutexLocker lock(&m_mutex);
    return m_state[kXindex];
}

nx::joystick::StateAtom Stick::getStickY() const
{
    QnMutexLocker lock(&m_mutex);
    return m_state[kYindex];
}

nx::joystick::StateAtom Stick::getStickZ() const
{
    QnMutexLocker lock(&m_mutex);
    return m_state[kZindex];
}

nx::joystick::StateAtom Stick::getStickRx() const
{
    QnMutexLocker lock(&m_mutex);
    return m_state[kRxIndex];
}

nx::joystick::StateAtom Stick::getStickRy() const
{
    QnMutexLocker lock(&m_mutex);
    return m_state[kRyIndex];
}

nx::joystick::StateAtom Stick::getStickRz() const
{
    QnMutexLocker lock(&m_mutex);
    return m_state[kRzIndex];
}

Limits Stick::getStickXLimits() const
{
    QnMutexLocker lock(&m_mutex);
    return m_limits[kXindex];
}

void Stick::setStickXLimits(nx::joystick::StateAtom min, nx::joystick::StateAtom max)
{
    QnMutexLocker lock(&m_mutex);
    m_limits[kXindex] = Limits(min, max);
}

void Stick::setStickXLimits(Limits limits)
{
    QnMutexLocker lock(&m_mutex);
    m_limits[kXindex] = limits;
}

Limits Stick::getStickYLimits() const
{
    QnMutexLocker lock(&m_mutex);
    return m_limits[kYindex];
}

void Stick::setStickYLimits(nx::joystick::StateAtom min, nx::joystick::StateAtom max)
{
    QnMutexLocker lock(&m_mutex);
    m_limits[kYindex] = Limits(min, max);
}

void Stick::setStickYLimits(Limits limits)
{
    QnMutexLocker lock(&m_mutex);
    m_limits[kYindex] = limits;
}

Limits Stick::getStickZLimits() const
{
    QnMutexLocker lock(&m_mutex);
    return m_limits[kZindex];
}

void Stick::setStickZLimits(nx::joystick::StateAtom min, nx::joystick::StateAtom max)
{
    QnMutexLocker lock(&m_mutex);
    m_limits[kZindex] = Limits(min, max);
}

void Stick::setStickZLimits(Limits limits)
{
    QnMutexLocker lock(&m_mutex);
    m_limits[kZindex] = limits;
}

Limits Stick::getStickRxLimits() const
{
    QnMutexLocker lock(&m_mutex);
    return m_limits[kRxIndex];
}

void Stick::setStickRxLimits(nx::joystick::StateAtom min, nx::joystick::StateAtom max)
{
    QnMutexLocker lock(&m_mutex);
    m_limits[kRxIndex] = Limits(min, max);
}

void Stick::setStickRxLimits(Limits limits)
{
    QnMutexLocker lock(&m_mutex);
    m_limits[kRxIndex] = limits;
}

Limits Stick::getStickRyLimits() const
{
    QnMutexLocker lock(&m_mutex);
    return m_limits[kRyIndex];
}

void Stick::setStickRyLimits(nx::joystick::StateAtom min, nx::joystick::StateAtom max)
{
    QnMutexLocker lock(&m_mutex);
    m_limits[kRyIndex] = Limits(min, max);
}

void Stick::setStickRyLimits(Limits limits)
{
    QnMutexLocker lock(&m_mutex);
    m_limits[kRyIndex] = limits;
}

Limits Stick::getStickRzLimits() const
{
    QnMutexLocker lock(&m_mutex);
    return m_limits[kRzIndex];
}

void Stick::setStickRzLimits(nx::joystick::StateAtom min, nx::joystick::StateAtom max)
{
    QnMutexLocker lock(&m_mutex);
    m_limits[kRzIndex] = Limits(min, max);
}

void Stick::setStickRzLimits(Limits limits)
{
    QnMutexLocker lock(&m_mutex);
    m_limits[kRzIndex] = limits;
}

std::vector<Limits> Stick::getLimits() const
{
    QnMutexLocker lock(&m_mutex);
    return m_limits;
}

void Stick::setLimits(std::vector<Limits> limits)
{
    QnMutexLocker lock(&m_mutex);
    m_limits = limits;
}

bool Stick::isEventTypeSupported(nx::joystick::EventType eventType) const
{
    return eventType == EventType::stickNonZero
        || eventType == EventType::stickZero
        || eventType == EventType::stickPushUp
        || eventType == EventType::stickPushDown
        || eventType == EventType::stickPushLeft
        || eventType == EventType::stickPushRight;
}


BaseControl::EventSet Stick::checkForEventsUnsafe() const 
{
    // TODO: #dmishin something is wrong with this method and zero/nonZero events

    Q_ASSERT(m_state.size() == kMaxDegreesOfFreedom);
    EventSet eventsToBeFired;
    if (isZeroPosition(m_state) && !isZeroPosition(m_previousPosition))
    {
        eventsToBeFired.insert(nx::joystick::EventType::stickZero);
        return eventsToBeFired;
    }

    if (distance(m_previousPosition, m_state) > m_threshold)
        eventsToBeFired.insert(nx::joystick::EventType::stickNonZero);
    
    return eventsToBeFired;
}

nx::joystick::State Stick::fromRawToNormalized(const nx::joystick::State& raw) const 
{
    nx::joystick::State normalizedState;

    for (auto i = 0; i < raw.size(); ++i)
    {
        auto normalizedAxisState = translatePoint(raw[i], m_limits[i], m_outputLimits[i]);
        normalizedState.push_back(normalizedAxisState);
    }

    return normalizedState;
}

nx::joystick::State Stick::fromNormalizedToRaw(const nx::joystick::State& normalized) const 
{
    nx::joystick::State rawState;

    for (auto i = 0; i < normalized.size(); ++i)
    {
        auto rawAxisState = translatePoint(normalized[i], m_outputLimits[i], m_limits[i]);
        rawState.push_back(rawAxisState);
    }

    return rawState;
}


nx::joystick::StateAtom Stick::distance(const State& position1, const State& position2) const
{
    auto xDiff = position1[kXindex] - position2[kXindex];
    auto yDiff = position1[kYindex] - position2[kYindex];
    auto zDiff = position1[kZindex] - position2[kZindex];

    return static_cast<StateAtom>(std::sqrt(xDiff * xDiff + yDiff * yDiff + zDiff * zDiff) + 0.5);
}

nx::joystick::StateAtom Stick::translatePoint(
    nx::joystick::StateAtom point,
    const Limits& originalBounds,
    const Limits& targetBounds) const
{
    auto translated = static_cast<nx::joystick::StateAtom>(
        targetBounds.min 
        + static_cast<double>((point - originalBounds.min)) / (originalBounds.max - originalBounds.min) 
        * (targetBounds.max - targetBounds.min) + 0.5);

    if (translated > targetBounds.max)
        return targetBounds.max;

    if (translated < targetBounds.min)
        return targetBounds.min;

    return translated;
}

bool Stick::isZeroPosition(const State& position) const
{
    Q_ASSERT(position.size() == 6, lit("Wrong state size"));

    return distance(position, kZeroPositionState) < m_threshold;
}

bool Stick::hasStickBeenPushedToArea(StickArea area) const
{
    return false; //< TODO: #dmishin implement push events.
}

} // namespace controls
} // namespace joystick
} // namespace nx