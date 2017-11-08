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

StateElement Stick::x() const
{
    QnMutexLocker lock(&m_mutex);
    return m_state[DegreeOfFreedom::x];
}

StateElement Stick::y() const
{
    QnMutexLocker lock(&m_mutex);
    return m_state[DegreeOfFreedom::y];
}

StateElement Stick::z() const
{
    QnMutexLocker lock(&m_mutex);
    return m_state[DegreeOfFreedom::z];
}

StateElement Stick::rX() const
{
    QnMutexLocker lock(&m_mutex);
    return m_state[DegreeOfFreedom::rX];
}

StateElement Stick::rY() const
{
    QnMutexLocker lock(&m_mutex);
    return m_state[DegreeOfFreedom::rY];
}

StateElement Stick::rZ() const
{
    QnMutexLocker lock(&m_mutex);
    return m_state[DegreeOfFreedom::rZ];
}

std::vector<Range> Stick::ranges() const
{
    QnMutexLocker lock(&m_mutex);
    return m_ranges;
}

void Stick::setRanges(Ranges ranges)
{
    NX_ASSERT(ranges.size() == kMaxDegreesOfFreedom);
    QnMutexLocker lock(&m_mutex);
    m_ranges = ranges;
}

Ranges Stick::outputRanges() const
{
    QnMutexLocker lock(&m_mutex);
    return m_outputRanges;
}

void Stick::setOutputRanges(Ranges outputRanges)
{
    NX_ASSERT(outputRanges.size() == kMaxDegreesOfFreedom);
    QnMutexLocker lock(&m_mutex);
    m_outputRanges = outputRanges;
}

std::vector<bool> Stick::inversions() const
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
    m_inversions[DegreeOfFreedom::x] = invert;
}

bool Stick::isXInverted() const
{
    QnMutexLocker lock(&m_mutex);
    return m_inversions[DegreeOfFreedom::x];
}

void Stick::invertY(bool invert)
{
    QnMutexLocker lock(&m_mutex);
    m_inversions[DegreeOfFreedom::y] = invert;
}

bool Stick::isYInverted() const
{
    QnMutexLocker lock(&m_mutex);
    return m_inversions[DegreeOfFreedom::y];
}

void Stick::invertZ(bool invert)
{
    QnMutexLocker lock(&m_mutex);
    m_inversions[DegreeOfFreedom::z] = invert;
}

bool Stick::isZInverted() const
{
    QnMutexLocker lock(&m_mutex);
    return m_inversions[DegreeOfFreedom::z];
}

void Stick::invertRx(bool invert)
{
    QnMutexLocker lock(&m_mutex);
    m_inversions[DegreeOfFreedom::rX] = invert;
}

bool Stick::isRxInverted() const
{
    QnMutexLocker lock(&m_mutex);
    return m_inversions[DegreeOfFreedom::rX];
}

void Stick::invertRy(bool invert)
{
    QnMutexLocker lock(&m_mutex);
    m_inversions[DegreeOfFreedom::rY] = invert;
}

bool Stick::isRyInverted() const
{
    QnMutexLocker lock(&m_mutex);
    return m_inversions[DegreeOfFreedom::rY];
}

void Stick::invertRz(bool invert)
{
    QnMutexLocker lock(&m_mutex);
    m_inversions[DegreeOfFreedom::rZ] = invert;
}

bool Stick::isRzInverted() const
{
    QnMutexLocker lock(&m_mutex);
    return m_inversions[DegreeOfFreedom::rZ];
}

Range Stick::xRange() const
{
    QnMutexLocker lock(&m_mutex);
    return m_ranges[DegreeOfFreedom::x];
}

void Stick::setStickXRange(StateElement min, StateElement max)
{
    NX_ASSERT(max > min);
    QnMutexLocker lock(&m_mutex);
    m_ranges[DegreeOfFreedom::x] = Range(min, max);
}

void Stick::setStickXRange(const Range& range)
{
    NX_ASSERT(range.max > range.min);
    QnMutexLocker lock(&m_mutex);
    m_ranges[DegreeOfFreedom::x] = range;
}

Range Stick::yRange() const
{
    QnMutexLocker lock(&m_mutex);
    return m_ranges[DegreeOfFreedom::y];
}

void Stick::setStickYRange(StateElement min, StateElement max)
{
    NX_ASSERT(max > min);
    QnMutexLocker lock(&m_mutex);
    m_ranges[DegreeOfFreedom::y] = Range(min, max);
}

void Stick::setStickYRange(const Range& range)
{
    NX_ASSERT(range.max > range.min);
    QnMutexLocker lock(&m_mutex);
    m_ranges[DegreeOfFreedom::y] = range;
}

Range Stick::zRange() const
{    
    QnMutexLocker lock(&m_mutex);
    return m_ranges[DegreeOfFreedom::z];
}

void Stick::setStickZRange(StateElement min, StateElement max)
{
    NX_ASSERT(max > min);
    QnMutexLocker lock(&m_mutex);
    m_ranges[DegreeOfFreedom::z] = Range(min, max);
}

void Stick::setStickZRange(const Range& range)
{
    NX_ASSERT(range.max > range.min);
    QnMutexLocker lock(&m_mutex);
    m_ranges[DegreeOfFreedom::z] = range;
}

Range Stick::rXRange() const
{
    QnMutexLocker lock(&m_mutex);
    return m_ranges[DegreeOfFreedom::rX];
}

void Stick::setStickRxRange(StateElement min, StateElement max)
{
    NX_ASSERT(max > min);
    QnMutexLocker lock(&m_mutex);
    m_ranges[DegreeOfFreedom::rX] = Range(min, max);
}

void Stick::setStickRxRange(const Range& range)
{
    NX_ASSERT(range.max > range.min);
    QnMutexLocker lock(&m_mutex);
    m_ranges[DegreeOfFreedom::rX] = range;
}

Range Stick::rYRange() const
{
    QnMutexLocker lock(&m_mutex);
    return m_ranges[DegreeOfFreedom::rY];
}

void Stick::setStickRyRange(StateElement min, StateElement max)
{
    NX_ASSERT(max > min);
    QnMutexLocker lock(&m_mutex);
    m_ranges[DegreeOfFreedom::rY] = Range(min, max);
}

void Stick::setStickRyRange(const Range& range)
{
    NX_ASSERT(range.max > range.min);
    QnMutexLocker lock(&m_mutex);
    m_ranges[DegreeOfFreedom::rY] = range;
}

Range Stick::rZRange() const
{
    QnMutexLocker lock(&m_mutex);
    return m_ranges[DegreeOfFreedom::rZ];
}

void Stick::setStickRzRange(StateElement min, StateElement max)
{
    NX_ASSERT(max > min);
    QnMutexLocker lock(&m_mutex);
    m_ranges[DegreeOfFreedom::rZ] = Range(min, max);
}

void Stick::setStickRzRange(const Range& range)
{
    NX_ASSERT(range.max > range.min);
    QnMutexLocker lock(&m_mutex);
    m_ranges[DegreeOfFreedom::rZ] = range;
}

Range Stick::outputXRange() const
{
    QnMutexLocker lock(&m_mutex);
    return m_outputRanges[DegreeOfFreedom::x];
}

void Stick::setStickOutputXRange(StateElement min, StateElement max)
{
    NX_ASSERT(max > min);
    QnMutexLocker lock(&m_mutex);
    m_outputRanges[DegreeOfFreedom::x] = Range(min, max);
}

void Stick::setStickOutputXRange(const Range& range)
{
    NX_ASSERT(range.max > range.min);
    QnMutexLocker lock(&m_mutex);
    m_outputRanges[DegreeOfFreedom::x] = range;
}

Range Stick::outputYRange() const
{
    QnMutexLocker lock(&m_mutex);
    return m_outputRanges[DegreeOfFreedom::y];
}

void Stick::setStickOutputYRange(StateElement min, StateElement max)
{
    NX_ASSERT(max > min);
    QnMutexLocker lock(&m_mutex);
    m_outputRanges[DegreeOfFreedom::y] = Range(min, max);
}

void Stick::setStickOutputYRange(const Range& range)
{
    NX_ASSERT(range.max > range.min);
    QnMutexLocker lock(&m_mutex);
    m_outputRanges[DegreeOfFreedom::y] = range;
}

Range Stick::outputZRange() const
{
    QnMutexLocker lock(&m_mutex);
    return m_outputRanges[DegreeOfFreedom::z];
}

void Stick::setStickOutputZRange(StateElement min, StateElement max)
{
    NX_ASSERT(max > min);
    QnMutexLocker lock(&m_mutex);
    m_outputRanges[DegreeOfFreedom::z] = Range(min, max);
}

void Stick::setStickOutputZRange(const Range& range)
{
    NX_ASSERT(range.max > range.min);
    QnMutexLocker lock(&m_mutex);
    m_outputRanges[DegreeOfFreedom::z] = range;
}

Range Stick::outputRxRange() const
{
    QnMutexLocker lock(&m_mutex);
    return m_outputRanges[DegreeOfFreedom::rX];
}

void Stick::setStickOutputRxRange(StateElement min, StateElement max)
{
    NX_ASSERT(max > min);
    QnMutexLocker lock(&m_mutex);
    m_outputRanges[DegreeOfFreedom::rX] = Range(min, max);
}

void Stick::setStickOutputRxRange(const Range& range)
{
    NX_ASSERT(range.max > range.min);
    QnMutexLocker lock(&m_mutex);
    m_outputRanges[DegreeOfFreedom::rX] = range;
}

Range Stick::outputRyRange() const
{
    QnMutexLocker lock(&m_mutex);
    return m_outputRanges[DegreeOfFreedom::rY];
}

void Stick::setStickOutputRyRange(StateElement min, StateElement max)
{
    NX_ASSERT(max > min);
    QnMutexLocker lock(&m_mutex);
    m_outputRanges[DegreeOfFreedom::rY] = Range(min, max);
}

void Stick::setStickOutputRyRange(const Range& range)
{
    NX_ASSERT(range.max > range.min);
    QnMutexLocker lock(&m_mutex);
    m_outputRanges[DegreeOfFreedom::rY] = range;
}

Range Stick::outputRzRange() const
{
    QnMutexLocker lock(&m_mutex);
    return m_outputRanges[DegreeOfFreedom::rZ];
}

void Stick::setStickOutputRzRange(StateElement min, StateElement max)
{
    NX_ASSERT(max > min);
    QnMutexLocker lock(&m_mutex);
    m_outputRanges[DegreeOfFreedom::rZ] = Range(min, max);
}

void Stick::setStickOutputRzRange(const Range& range)
{
    NX_ASSERT(range.max > range.min);
    QnMutexLocker lock(&m_mutex);
    m_outputRanges[DegreeOfFreedom::rZ] = range;
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
    NX_ASSERT(m_state.size() == kMaxDegreesOfFreedom);
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
    fixState(&m_state);
}

State Stick::fromRawToNormalized(const State& raw) const 
{
    NX_ASSERT(raw.size() == kMaxDegreesOfFreedom);

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
    NX_ASSERT(normalized.size() == kMaxDegreesOfFreedom);
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
    NX_ASSERT(position1.size() >= 3 && position2.size() >= 3);
    auto xDiff = position1[DegreeOfFreedom::x] - position2[DegreeOfFreedom::x];
    auto yDiff = position1[DegreeOfFreedom::y] - position2[DegreeOfFreedom::y];
    auto zDiff = position1[DegreeOfFreedom::z] - position2[DegreeOfFreedom::z];

    return static_cast<StateElement>(std::sqrt(xDiff * xDiff + yDiff * yDiff + zDiff * zDiff) + 0.5);
}

StateElement Stick::translatePoint(
    StateElement point,
    const Range& originalBounds,
    const Range& targetBounds) const
{
    NX_ASSERT(originalBounds.max > originalBounds.min && targetBounds.max > targetBounds.min);
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
    NX_ASSERT(position.size() == kMaxDegreesOfFreedom);

    return distance(position, kZeroPositionState) < m_threshold;
}

void Stick::fixState(State* inOutState)
{
    auto& state = *inOutState;
    for (auto i = 0; i < state.size(); ++i)
    {
        if (std::abs(state[i]) < m_threshold)
            state[i] = 0;
    }
}

} // namespace controls
} // namespace joystick
} // namespace io_device
} // namespace plugins
} // namespace client
} // namespace nx