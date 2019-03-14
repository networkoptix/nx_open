#pragma once

#include <chrono>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/core/ptz/vector.h>

namespace nx {
namespace core {
namespace ptz {
namespace test_support {

class CyclingSettings
{
public:
    CyclingSettings() = default;

    CyclingSettings(
        bool isPanCycled,
        bool isTiltCycled,
        bool isRotationCycled,
        bool isZoomCycled = false,
        bool isFocusCycled = false);

    bool isComponentCycled(nx::core::ptz::Component component) const;
    void setIsComponentCycled(nx::core::ptz::Component component, bool isCycled);

private:
    std::map<nx::core::ptz::Component, bool> m_cyclingByComponent;
};

class TestPositionTracker
{
public:
    TestPositionTracker() = default;
    TestPositionTracker(const Vector& position);

    bool continuousMove(const Vector& movementVector);
    bool continuousFocus(double focusSpeed);

    bool absoluteMove(const Vector& movementVector);
    bool relativeMove(const Vector& movementVector);
    bool relativeFocus(double movementDirection);

    void setPosition(const Vector& position);
    Vector position() const;

    void setSpeedCoefficientsUnitPerSecond(const Vector& speedCoefficients);
    void setCyclingSettings(const CyclingSettings& settings);
    void setLimits(const QnPtzLimits& ptzLimits);

    Vector shortestVectorTo(const Vector& position) const;

private:
    bool genericContinuousMove(const Vector& movementVector);
    bool genericRelativeMovement(const Vector& movementVector);

    Vector positionUnsafe() const;

private:
    mutable QnMutex m_mutex;

    QnPtzLimits m_limits;
    CyclingSettings m_cyclingSettings;
    Vector m_speedCoefficientsUnitPerSecond;

    Vector m_lastStaticPosition;
    Vector m_continuousMovementSpeed;
    nx::utils::ElapsedTimer m_continuousMoveTimer;
};

} // namespace test_support
} // namespace ptz
} // namespace core
} // namespace nx
