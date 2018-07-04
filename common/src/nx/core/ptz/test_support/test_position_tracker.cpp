#include "test_position_tracker.h"

namespace nx {
namespace core {
namespace ptz {
namespace test_support {

namespace {

Vector calculatePosition(
    const Vector& lastStaticPosition,
    const Vector& speedCoefficients,
    const std::chrono::milliseconds& continuousMoveTime,
    const QnPtzLimits& limits,
    const CyclingSettings m_cyclingSettings)
{
    // TODO: #dmishin implement.
    return Vector();
}

Vector calculateRelativeMovementPosition(
    const Vector& currentPosition,
    const Vector& movementVector,
    const QnPtzLimits& limits,
    const CyclingSettings& cyclingSettings)
{
    // TODO: #dmishin implement.
    return Vector();
}

} // namespace

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
    m_lastStaticPosition = movementVector;

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

    if (!m_continuousMoveTimer.isValid())
        return m_lastStaticPosition;

    return calculatePosition(
        m_lastStaticPosition,
        m_speedCoefficients,
        m_continuousMoveTimer.elapsed(),
        m_limits,
        m_cyclingSettings);
}

void TestPositionTracker::setLimits(const QnPtzLimits& limits)
{
    QnMutexLocker lock(&m_mutex);
    m_limits = limits;
}

void TestPositionTracker::setSpeedCoefficients(const Vector& speedCoefficients)
{
    m_speedCoefficients = speedCoefficients;
}

void TestPositionTracker::setCyclingSettings(const CyclingSettings& settings)
{
    m_cyclingSettings = settings;
}

Vector TestPositionTracker::shortestVectorTo(const Vector& position) const
{
    // TODO: #dmishin implement
    return Vector();
}

bool TestPositionTracker::genericContinuousMove(const Vector& movementVector)
{
    QnMutexLocker lock(&m_mutex);
    if (m_continuousMoveTimer.isValid())
    {
        m_lastStaticPosition = calculatePosition(
            m_lastStaticPosition,
            m_speedCoefficients,
            m_continuousMoveTimer.elapsed(),
            m_limits,
            m_cyclingSettings);
    }

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
        m_speedCoefficients,
        m_continuousMoveTimer.elapsed(),
        m_limits,
        m_cyclingSettings);

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
