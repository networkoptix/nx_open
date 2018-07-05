#pragma once

#include <chrono>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/core/ptz/vector.h>

namespace nx {
namespace core {
namespace ptz {
namespace test_support {

struct CyclingSettings
{
    bool isPanCycled = false;
    bool isTiltCycled = false;
    bool isRotationCycled = false;
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

    void setLimits(const QnPtzLimits& limits);
    void setSpeedCoefficients(const Vector& speedCoefficients);

    void setCyclingSettings(const CyclingSettings& settings);

    Vector shortestVectorTo(const Vector& position) const;

private:
    bool genericContinuousMove(const Vector& movementVector);
    bool genericRelativeMovement(const Vector& movementVector);

private:
    mutable QnMutex m_mutex;
    QnPtzLimits m_limits;
    CyclingSettings m_cyclingSettings;
    Vector m_speedCoefficients;

    Vector m_lastStaticPosition;
    nx::utils::ElapsedTimer m_continuousMoveTimer;
};

} // namespace test_support
} // namespace ptz
} // namespace core
} // namespace nx
