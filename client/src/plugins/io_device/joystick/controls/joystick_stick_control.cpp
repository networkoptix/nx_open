#include "joystick_stick_control.h"

#include <cmath>

namespace {

const int kMaxDegreesOfFreedom = 6;
const nx::client::plugins::io_device::joystick::StateElement kDefaultThreshold = 2;
const nx::client::plugins::io_device::joystick::StateElement kZeroPositionArray[] = {0, 0, 0, 0, 0, 0};
const nx::client::plugins::io_device::joystick::State kZeroPositionState(
    kZeroPositionArray,
    kZeroPositionArray 
    + sizeof(kZeroPositionArray) 
        / sizeof(nx::client::plugins::io_device::joystick::StateElement));

const QString kInvertXaxisOverrideName = lit("invertX");
const QString kInvertYaxisOverrideName = lit("invertY");
const QString kInvertZaxisOverrideName = lit("invertZ");
const QString kInvertRxAxisOverrideName = lit("invertRx");
const QString kInvertRyAxisOverrideName = lit("invertRy");
const QString kInvertRzAxisOverrideName = lit("invertRz");

const QString kStringTrueValue = lit("true");
const QString kStringFalseValue = lit("false");

} // namespace

namespace nx {
namespace client {
namespace plugins {
namespace io_device {
namespace joystick {
namespace controls {

Stick::Stick():
    base_type(kMaxDegreesOfFreedom),
    m_threshold(kDefaultThreshold),
    m_previousPosition(kMaxDegreesOfFreedom, 0),
    m_ranges(kMaxDegreesOfFreedom, Range()),
    m_outputRanges(kMaxDegreesOfFreedom, Range()),
    m_inversions(kMaxDegreesOfFreedom, false)
{
}

StateElement Stick::getStickX() const
{
    QnMutexLocker lock(&m_mutex);
    return m_state[kXindex];
}

StateElement Stick::getStickY() const
{
    QnMutexLocker lock(&m_mutex);
    return m_state[kYindex];
}

StateElement Stick::getStickZ() const
{
    QnMutexLocker lock(&m_mutex);
    return m_state[kZindex];
}

StateElement Stick::getStickRx() const
{
    QnMutexLocker lock(&m_mutex);
    return m_state[kRxIndex];
}

StateElement Stick::getStickRy() const
{
    QnMutexLocker lock(&m_mutex);
    return m_state[kRyIndex];
}

StateElement Stick::getStickRz() const
{
    QnMutexLocker lock(&m_mutex);
    return m_state[kRzIndex];
}

std::vector<Range> Stick::getRanges() const
{
    QnMutexLocker lock(&m_mutex);
    return m_ranges;
}

void Stick::setRanges(Ranges ranges)
{
    Q_ASSERT(ranges.size() == kMaxDegreesOfFreedom);
    QnMutexLocker lock(&m_mutex);
    m_ranges = ranges;
}

Ranges Stick::getOutputRanges() const
{
    QnMutexLocker lock(&m_mutex);
    return m_outputRanges;
}

void Stick::setOutputRanges(Ranges outputRanges)
{
    Q_ASSERT(outputRanges.size() == kMaxDegreesOfFreedom);
    QnMutexLocker lock(&m_mutex);
    m_outputRanges = outputRanges;
}

std::vector<bool> Stick::getInversions() const
{
    QnMutexLocker lock(&m_mutex);
    return m_inversions;
}

void Stick::setInversions(const std::vector<bool>& inverions)
{
    QnMutexLocker lock(&m_mutex);
    m_inversions = inverions;
}

void Stick::invertX(bool invert)
{
    QnMutexLocker lock(&m_mutex);
    m_inversions[kXindex] = invert;
}

bool Stick::isXInverted() const
{
    QnMutexLocker lock(&m_mutex);
    return m_inversions[kXindex];
}

void Stick::invertY(bool invert)
{
    QnMutexLocker lock(&m_mutex);
    m_inversions[kYindex] = invert;
}

bool Stick::isYInverted() const
{
    QnMutexLocker lock(&m_mutex);
    return m_inversions[kYindex];
}

void Stick::invertZ(bool invert)
{
    QnMutexLocker lock(&m_mutex);
    m_inversions[kZindex] = invert;
}

bool Stick::isZInverted() const
{
    QnMutexLocker lock(&m_mutex);
    return m_inversions[kZindex];
}

void Stick::invertRx(bool invert)
{
    QnMutexLocker lock(&m_mutex);
    m_inversions[kRxIndex] = invert;
}

bool Stick::isRxInverted() const
{
    QnMutexLocker lock(&m_mutex);
    return m_inversions[kRxIndex];
}

void Stick::invertRy(bool invert)
{
    QnMutexLocker lock(&m_mutex);
    m_inversions[kRyIndex] = invert;
}

bool Stick::isRyInverted() const
{
    QnMutexLocker lock(&m_mutex);
    return m_inversions[kRyIndex];
}

void Stick::invertRz(bool invert)
{
    QnMutexLocker lock(&m_mutex);
    m_inversions[kRzIndex] = invert;
}

bool Stick::isRzInverted() const
{
    QnMutexLocker lock(&m_mutex);
    return m_inversions[kRzIndex];
}

Range Stick::getStickXRange() const
{
    QnMutexLocker lock(&m_mutex);
    return m_ranges[kXindex];
}

void Stick::setStickXRange(StateElement min, StateElement max)
{
    Q_ASSERT(max > min);
    QnMutexLocker lock(&m_mutex);
    m_ranges[kXindex] = Range(min, max);
}

void Stick::setStickXRange(const Range& range)
{
    Q_ASSERT(range.max > range.min);
    QnMutexLocker lock(&m_mutex);
    m_ranges[kXindex] = range;
}

Range Stick::getStickYRange() const
{
    QnMutexLocker lock(&m_mutex);
    return m_ranges[kYindex];
}

void Stick::setStickYRange(StateElement min, StateElement max)
{
    Q_ASSERT(max > min);
    QnMutexLocker lock(&m_mutex);
    m_ranges[kYindex] = Range(min, max);
}

void Stick::setStickYRange(const Range& range)
{
    Q_ASSERT(range.max > range.min);
    QnMutexLocker lock(&m_mutex);
    m_ranges[kYindex] = range;
}

Range Stick::getStickZRange() const
{    
    QnMutexLocker lock(&m_mutex);
    return m_ranges[kZindex];
}

void Stick::setStickZRange(StateElement min, StateElement max)
{
    Q_ASSERT(max > min);
    QnMutexLocker lock(&m_mutex);
    m_ranges[kZindex] = Range(min, max);
}

void Stick::setStickZRange(const Range& range)
{
    Q_ASSERT(range.max > range.min);
    QnMutexLocker lock(&m_mutex);
    m_ranges[kZindex] = range;
}

Range Stick::getStickRxRange() const
{
    QnMutexLocker lock(&m_mutex);
    return m_ranges[kRxIndex];
}

void Stick::setStickRxRange(StateElement min, StateElement max)
{
    Q_ASSERT(max > min);
    QnMutexLocker lock(&m_mutex);
    m_ranges[kRxIndex] = Range(min, max);
}

void Stick::setStickRxRange(const Range& range)
{
    Q_ASSERT(range.max > range.min);
    QnMutexLocker lock(&m_mutex);
    m_ranges[kRxIndex] = range;
}

Range Stick::getStickRyRange() const
{
    QnMutexLocker lock(&m_mutex);
    return m_ranges[kRyIndex];
}

void Stick::setStickRyRange(StateElement min, StateElement max)
{
    Q_ASSERT(max > min);
    QnMutexLocker lock(&m_mutex);
    m_ranges[kRyIndex] = Range(min, max);
}

void Stick::setStickRyRange(const Range& range)
{
    Q_ASSERT(range.max > range.min);
    QnMutexLocker lock(&m_mutex);
    m_ranges[kRyIndex] = range;
}

Range Stick::getStickRzRange() const
{
    QnMutexLocker lock(&m_mutex);
    return m_ranges[kRzIndex];
}

void Stick::setStickRzRange(StateElement min, StateElement max)
{
    Q_ASSERT(max > min);
    QnMutexLocker lock(&m_mutex);
    m_ranges[kRzIndex] = Range(min, max);
}

void Stick::setStickRzRange(const Range& range)
{
    Q_ASSERT(range.max > range.min);
    QnMutexLocker lock(&m_mutex);
    m_ranges[kRzIndex] = range;
}

Range Stick::getStickOutputXRange() const
{
    QnMutexLocker lock(&m_mutex);
    return m_outputRanges[kXindex];
}

void Stick::setStickOutputXRange(StateElement min, StateElement max)
{
    Q_ASSERT(max > min);
    QnMutexLocker lock(&m_mutex);
    m_outputRanges[kXindex] = Range(min, max);
}

void Stick::setStickOutputXRange(const Range& range)
{
    Q_ASSERT(range.max > range.min);
    QnMutexLocker lock(&m_mutex);
    m_outputRanges[kXindex] = range;
}

Range Stick::getStickOutputYRange() const
{
    QnMutexLocker lock(&m_mutex);
    return m_outputRanges[kYindex];
}

void Stick::setStickOutputYRange(StateElement min, StateElement max)
{
    Q_ASSERT(max > min);
    QnMutexLocker lock(&m_mutex);
    m_outputRanges[kYindex] = Range(min, max);
}

void Stick::setStickOutputYRange(const Range& range)
{
    Q_ASSERT(range.max > range.min);
    QnMutexLocker lock(&m_mutex);
    m_outputRanges[kYindex] = range;
}

Range Stick::getStickOutputZRange() const
{
    QnMutexLocker lock(&m_mutex);
    return m_outputRanges[kZindex];
}

void Stick::setStickOutputZRange(StateElement min, StateElement max)
{
    Q_ASSERT(max > min);
    QnMutexLocker lock(&m_mutex);
    m_outputRanges[kZindex] = Range(min, max);
}

void Stick::setStickOutputZRange(const Range& range)
{
    Q_ASSERT(range.max > range.min);
    QnMutexLocker lock(&m_mutex);
    m_outputRanges[kZindex] = range;
}

Range Stick::getStickOutputRxRange() const
{
    QnMutexLocker lock(&m_mutex);
    return m_outputRanges[kRxIndex];
}

void Stick::setStickOutputRxRange(StateElement min, StateElement max)
{
    Q_ASSERT(max > min);
    QnMutexLocker lock(&m_mutex);
    m_outputRanges[kRxIndex] = Range(min, max);
}

void Stick::setStickOutputRxRange(const Range& range)
{
    Q_ASSERT(range.max > range.min);
    QnMutexLocker lock(&m_mutex);
    m_outputRanges[kRxIndex] = range;
}

Range Stick::getStickOutputRyRange() const
{
    QnMutexLocker lock(&m_mutex);
    return m_outputRanges[kRyIndex];
}

void Stick::setStickOutputRyRange(StateElement min, StateElement max)
{
    Q_ASSERT(max > min);
    QnMutexLocker lock(&m_mutex);
    m_outputRanges[kRyIndex] = Range(min, max);
}

void Stick::setStickOutputRyRange(const Range& range)
{
    Q_ASSERT(range.max > range.min);
    QnMutexLocker lock(&m_mutex);
    m_outputRanges[kRyIndex] = range;
}

Range Stick::getStickOutputRzRange() const
{
    QnMutexLocker lock(&m_mutex);
    return m_outputRanges[kRzIndex];
}

void Stick::setStickOutputRzRange(StateElement min, StateElement max)
{
    Q_ASSERT(max > min);
    QnMutexLocker lock(&m_mutex);
    m_outputRanges[kRzIndex] = Range(min, max);
}

void Stick::setStickOutputRzRange(const Range& range)
{
    Q_ASSERT(range.max > range.min);
    QnMutexLocker lock(&m_mutex);
    m_outputRanges[kRzIndex] = range;
}

void Stick::applyOverride(const QString overrideName, const QString& overrideValue)
{
    bool isTrueValue = overrideValue.toLower() == kStringTrueValue;

    if (overrideName == kInvertXaxisOverrideName)
        invertX(isTrueValue);
    else if (overrideName == kInvertYaxisOverrideName)
        invertY(isTrueValue);
    else if(overrideName == kInvertZaxisOverrideName)
        invertZ(isTrueValue);
    else if(overrideName == kInvertRxAxisOverrideName)
        invertRx(isTrueValue);
    else if(overrideName == kInvertRyAxisOverrideName)
        invertRy(isTrueValue);
    else if(overrideName == kInvertRzAxisOverrideName)
        invertRz(isTrueValue);
}

bool Stick::isEventTypeSupported(EventType eventType) const
{
    return eventType == EventType::stickMove;
}

BaseControl::EventSet Stick::checkForEventsUnsafe() const 
{
    Q_ASSERT(m_state.size() == kMaxDegreesOfFreedom);
    EventSet eventsToBeFired;

    if (distance(m_previousPosition, m_state) > m_threshold || 
        (isZeroPosition(m_state) && !isZeroPosition(m_previousPosition)))
    {
        eventsToBeFired.insert(EventType::stickMove);
    }
    
    return eventsToBeFired;
}

void Stick::setStateUnsafe(const State& state)
{
    m_previousPosition = m_state;
    m_state = state;
}

State Stick::fromRawToNormalized(const State& raw) const 
{
    Q_ASSERT(raw.size() == kMaxDegreesOfFreedom);

    QnMutexLocker lock(&m_mutex);
    State normalizedState;
    std::vector<bool> inversions;

    for (auto i = 0; i < raw.size(); ++i)
    {
        auto normalizedAxisState = translatePoint(raw[i], m_ranges[i], m_outputRanges[i]);
        if (m_inversions[i])
            normalizedAxisState *= -1;

        normalizedState.push_back(normalizedAxisState);
    }

    if (distance(normalizedState, kZeroPositionState) < m_threshold)
        return kZeroPositionState;

    return normalizedState;
}

State Stick::fromNormalizedToRaw(const State& normalized) const 
{
    Q_ASSERT(normalized.size() == kMaxDegreesOfFreedom);
    State rawState;
    for (auto i = 0; i < normalized.size(); ++i)
    {
        auto rawAxisState = translatePoint(normalized[i], m_outputRanges[i], m_ranges[i]);
        rawState.push_back(rawAxisState);
    }

    return rawState;
}

StateElement Stick::distance(const State& position1, const State& position2) const
{
    Q_ASSERT(position1.size() >= 3 && position2.size() >= 3);
    auto xDiff = position1[kXindex] - position2[kXindex];
    auto yDiff = position1[kYindex] - position2[kYindex];
    auto zDiff = position1[kZindex] - position2[kZindex];

    return static_cast<StateElement>(std::sqrt(xDiff * xDiff + yDiff * yDiff + zDiff * zDiff) + 0.5);
}

StateElement Stick::translatePoint(
    StateElement point,
    const Range& originalBounds,
    const Range& targetBounds) const
{
    Q_ASSERT(originalBounds.max > originalBounds.min && targetBounds.max > targetBounds.min);
    if (originalBounds.max == originalBounds.min)
        return targetBounds.min;

    auto translated = static_cast<StateElement>(
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
    Q_ASSERT(position.size() == kMaxDegreesOfFreedom);

    return distance(position, kZeroPositionState) < m_threshold;
}

} // namespace controls
} // namespace joystick
} // namespace io_device
} // namespace plugins
} // namespace client
} // namespace nx