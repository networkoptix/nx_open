#include "test_position_tracker.h"

#include <cmath>

#include <nx/utils/std/optional.h>

using namespace std::chrono;
using namespace std::chrono_literals;

namespace nx {
namespace core {
namespace ptz {
namespace test_support {

namespace {

Vector calculatePosition(
    const Vector& lastStaticPosition,
    const Vector& continuousMoveSpeed,
    const Vector& speedCoefficientsUnitsPerSecond,
    const milliseconds& continuousMoveTime,
    const QnPtzLimits& limits,
    const CyclingSettings& cyclingSettings)
{
    Vector result;
    const auto effectiveSpeed = continuousMoveSpeed * speedCoefficientsUnitsPerSecond;
    for (const auto component: kAllComponents)
    {
        const auto speed = effectiveSpeed.component(component);
        const auto lastPosition = lastStaticPosition.component(component);
        if (qFuzzyIsNull(speed))
        {
            result.setComponent(lastPosition, component);
            continue;
        }

        const auto minBound = speed > 0
            ? limits.minComponentValue(component)
            : limits.maxComponentValue(component);

        const auto maxBound = speed > 0
            ? limits.maxComponentValue(component)
            : limits.minComponentValue(component);

        const auto range = limits.componentRange(component);
        const auto period = range / effectiveSpeed.component(component);
        const auto periodCount = duration_cast<milliseconds>(continuousMoveTime).count()
            / (double) duration_cast<milliseconds>(1s).count()
            / period;

        const auto effectivePeriodCount = periodCount - (int) periodCount
             + ((std::abs(periodCount) > 1) ? copysign(1, periodCount) : 0);
        const auto effectiveMovement = effectivePeriodCount * range;
        const bool isComponentOutOfRange = !cyclingSettings.isComponentCycled(component)
            && (speed > 0
                ? lastPosition + effectiveMovement >= maxBound
                : lastPosition + effectiveMovement <= maxBound);

        if (isComponentOutOfRange)
        {
            result.setComponent(maxBound, component);
            continue;
        }

        result.setComponent(
            minBound + std::fmod(lastPosition + effectiveMovement - minBound, range),
            component);
    }

    return result;
}

Vector calculateRelativeMovementPosition(
    const Vector& currentPosition,
    const Vector& movementVector,
    const QnPtzLimits& limits,
    const CyclingSettings& cyclingSettings)
{
    Vector result;
    for (const auto component: kAllComponents)
    {
        const auto componentPosition = currentPosition.component(component);
        const auto movementComponent = movementVector.component(component);

        const auto minBound = movementComponent > 0
            ? limits.minComponentValue(component)
            : limits.maxComponentValue(component);

        const auto maxBound = movementComponent > 0
            ? limits.maxComponentValue(component)
            : limits.minComponentValue(component);

        const auto range = limits.componentRange(component);
        const auto movement = range * movementComponent;
        const auto position = componentPosition + movement;

        const bool isOutOfRange = !cyclingSettings.isComponentCycled(component)
            && movementComponent > 0 ? position >= maxBound : position <= maxBound;

        if (isOutOfRange)
        {
            result.setComponent(maxBound, component);
            continue;
        }

        result.setComponent(
            minBound + std::fmod(position - minBound, range),
            component);

    }

    return result;
}

} // namespace

CyclingSettings::CyclingSettings(
    bool isPanCycled,
    bool isTiltCycled,
    bool isRotationCycled,
    bool isZoomCycled,
    bool isFocusCycled)
    :
    m_cyclingByComponent({
        {Component::pan, isPanCycled},
        {Component::tilt, isTiltCycled},
        {Component::rotation, isRotationCycled},
        {Component::zoom, isZoomCycled},
        {Component::focus, isFocusCycled}})
{
}

bool CyclingSettings::isComponentCycled(nx::core::ptz::Component component) const
{
    const auto componentItr = m_cyclingByComponent.find(component);
    if (componentItr != m_cyclingByComponent.cend())
        return componentItr->second;

    return false;
}

void CyclingSettings::setIsComponentCycled(nx::core::ptz::Component component, bool isCycled)
{
    m_cyclingByComponent[component] = isCycled;
}

TestPositionTracker::TestPositionTracker(const Vector& position):
    m_lastStaticPosition(position)
{
}

bool TestPositionTracker::continuousMove(const Vector& movementVector)
{
    return genericContinuousMove(movementVector);
}

bool TestPositionTracker::continuousFocus(double focusSpeed)
{
    return genericContinuousMove(Vector(0.0, 0.0, 0.0, 0.0, focusSpeed));
}

bool TestPositionTracker::absoluteMove(const Vector& movementVector)
{
    QnMutexLocker lock(&m_mutex);
    m_continuousMoveTimer.invalidate();
    m_lastStaticPosition = movementVector.restricted(m_limits, LimitsType::position);

    return true;
}

bool TestPositionTracker::relativeMove(const Vector& movementVector)
{
    return genericRelativeMovement(movementVector);
}

bool TestPositionTracker::relativeFocus(double focusMovementDirection)
{
    return genericRelativeMovement(Vector(0, 0, 0, 0, focusMovementDirection));
}

void TestPositionTracker::setPosition(const Vector& position)
{
    absoluteMove(position);
}

Vector TestPositionTracker::position() const
{
    QnMutexLocker lock(&m_mutex);
    return positionUnsafe();
}

Vector TestPositionTracker::positionUnsafe() const
{
    if (!m_continuousMoveTimer.isValid())
        return m_lastStaticPosition;

    return calculatePosition(
        m_lastStaticPosition,
        m_continuousMovementSpeed,
        m_speedCoefficientsUnitPerSecond,
        m_continuousMoveTimer.elapsed(),
        m_limits,
        m_cyclingSettings);
}

void TestPositionTracker::setLimits(const QnPtzLimits& limits)
{
    QnMutexLocker lock(&m_mutex);
    m_limits = limits;
}

void TestPositionTracker::setSpeedCoefficientsUnitPerSecond(const Vector& speedCoefficients)
{
    m_speedCoefficientsUnitPerSecond = speedCoefficients;
}

void TestPositionTracker::setCyclingSettings(const CyclingSettings& settings)
{
    m_cyclingSettings = settings;
}

Vector TestPositionTracker::shortestVectorTo(const Vector& destination) const
{
    QnMutexLocker lock(&m_mutex);
    Vector result;
    const auto currentPosition = positionUnsafe();
    for (const auto component: kAllComponents)
    {
        const auto sourceComponentPosition = currentPosition.component(component);
        const auto destinationComponentPosition = destination.component(component);
        double componentPart = destinationComponentPosition - sourceComponentPosition;

        const bool isComponentCycled = m_cyclingSettings.isComponentCycled(component);
        if (isComponentCycled)
        {
            const auto minBound = (destinationComponentPosition >= sourceComponentPosition)
                ? m_limits.minComponentValue(component)
                : m_limits.maxComponentValue(component);

            const auto maxBound = (destinationComponentPosition >= sourceComponentPosition)
                ? m_limits.maxComponentValue(component)
                : m_limits.minComponentValue(component);

            double alternativeComponentPart = (minBound - sourceComponentPosition)
                + (maxBound - destinationComponentPosition);

            if (std::abs(alternativeComponentPart) < std::abs(componentPart))
                componentPart = alternativeComponentPart;
        }

        result.setComponent(componentPart, component);
    }

    return result;
}

bool TestPositionTracker::genericContinuousMove(const Vector& movementVector)
{
    QnMutexLocker lock(&m_mutex);
    if (m_continuousMoveTimer.isValid())
    {
        m_lastStaticPosition = calculatePosition(
            m_lastStaticPosition,
            m_continuousMovementSpeed,
            m_speedCoefficientsUnitPerSecond,
            m_continuousMoveTimer.elapsed(),
            m_limits,
            m_cyclingSettings);
    }

    m_continuousMovementSpeed = movementVector;
    if (qFuzzyIsNull(movementVector))
        m_continuousMoveTimer.invalidate();
    else
        m_continuousMoveTimer.restart();

    return true;
}

bool TestPositionTracker::genericRelativeMovement(const Vector& movementVector)
{
    QnMutexLocker lock(&m_mutex);
    m_lastStaticPosition = calculatePosition(
        m_lastStaticPosition,
        m_continuousMovementSpeed,
        m_speedCoefficientsUnitPerSecond,
        m_continuousMoveTimer.elapsed(),
        m_limits,
        m_cyclingSettings);

    m_continuousMovementSpeed = Vector();
    m_continuousMoveTimer.invalidate();

    m_lastStaticPosition = calculateRelativeMovementPosition(
        m_lastStaticPosition,
        movementVector,
        m_limits,
        m_cyclingSettings);

    return true;
}

} // namespace test_support
} // namespace ptz
} // namespace core
} // namespace nx
