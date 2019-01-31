#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include <nx/core/ptz/test_support/test_position_tracker.h>

using namespace std::chrono;
using namespace std::chrono_literals;

namespace nx {
namespace core {
namespace ptz {

namespace {

static const double kEpsilon = 1.0;

struct TestContinuousMovement
{
    milliseconds duration = 0ms;
    Vector movementVector;
    Vector resultPosition;
};

struct ContinuousMovementTestInput
{
    QnPtzLimits limits;
    Vector speedCoefficeients;
    std::vector<TestContinuousMovement> movementSequence;
};

static const ContinuousMovementTestInput kContinuousMoveTestInput = {
    {},
    {10.0, 10.0, 10.0, 10.0, 10.0},
    {
        {100ms, {1.0, 0.0, 0.0, 0.0, 0.0}, {1.0, 0.0, 0.0, 0.0, 0.0}},
        {500ms, {0.0, 0.5, 0.0, 0.0, 0.0}, {1.0, 2.5, 0.0, 0.0, 0.0}},
        {500ms, {0.0, 0.0, 0.2, 0.0, 0.0}, {1.0, 2.5, 1.0, 0.0, 0.0}},
        {50ms, {0.0, 0.0, 0.0, 0.8, 0.0}, {1.0, 2.5, 1.0, 0.4, 0.0}},

        {50ms, {0.0, 0.0, 0.0, -0.8, 0.0}, {1.0, 2.5, 1.0, 0.0, 0.0 }},
        {100ms, {-1.0, 0.0, 0.0, 0.0, 0.0}, {0.0, 2.5, 1.0, 0.0, 0.0}},
        {500ms, {0.0, -0.5, 0.0, 0.0, 0.0}, {0.0, 0.0, 1.0, 0.0, 0.0}},
        {500ms, {0.0, 0.0, -0.2, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0, 0.0}},

        {200ms, {1.0, -0.2, 0.3, 0.5, 0.0}, {2.0, -0.4, 0.6, 1.0, 0.0}},
        {100ms, {0.0, 0.0, 0.0, 0.0, 0.0}, {2.0, -0.4, 0.6, 1.0, 0.0}},
        {500ms, {-1.0, -1.0, 0.0, 0.5, 0.0}, {0.0, -5.4, 0.6, 3.5, 0.0}}
    }
};

struct TestAbsoluteMovement
{
    Vector inputVector;
    Vector resultVector;
};

struct AbsoluteMovementTestInput
{
    QnPtzLimits limits;
    std::vector<TestAbsoluteMovement> movementSequence;
};

static const AbsoluteMovementTestInput kAbsoluteMovementTestInput = {
    {},
    {
        {{120, 50, 20, 20, 0.5}, {120, 50, 20, 20, 0.5}},
        {{420, 50, 20, 20, 0.5}, {360, 50, 20, 20, 0.5}},
        {{120, 100, 20, 20, 0.5}, {120, 90, 20, 20, 0.5}},
        {{120, 50, 20, -10, 0.5}, {120, 50, 20, 0, 0.5}},
        {{120, 50, 20, 20, 12}, {120, 50, 20, 20, 1}}
    }
};

static const AbsoluteMovementTestInput kRelativeMovementTestInput = {
    {},
    {
        {{0.5, 0, 0, 0, 0}, {180, 0, 0, 0, 0}},
        {{0, -0.2, 0, 0, 0}, {180, -36, 0, 0, 0}},
        {{0, 0, 1.0, 0, 0}, {180, -36, 360, 0, 0}},
        {{0, 0, 0, -0.4, 0}, {180, -36, 360, 0, 0}},
        {{-0.5, 0.2, -1, 0.4, 0}, {0, 0, 0, 144, 0}}
    }
};

} // namespace

// TODO: #dmishin add tests for cycled mode and for focus movement.

TEST(TestPositionTrackerTest, continuousMovement)
{
    test_support::TestPositionTracker tracker;
    tracker.setSpeedCoefficientsUnitPerSecond(kContinuousMoveTestInput.speedCoefficeients);
    tracker.setLimits(kContinuousMoveTestInput.limits);

    for (const auto& movement: kContinuousMoveTestInput.movementSequence)
    {
        tracker.continuousMove(movement.movementVector);
        std::this_thread::sleep_for(movement.duration);
        ASSERT_LT(tracker.shortestVectorTo(movement.resultPosition).length(), kEpsilon);
        tracker.setPosition(movement.resultPosition); //< To avoid error accumulation.
    }
}

TEST(TestPositionTrackerTest, absoluteMovement)
{
    test_support::TestPositionTracker tracker;
    tracker.setLimits(kAbsoluteMovementTestInput.limits);

    for (const auto& position: kAbsoluteMovementTestInput.movementSequence)
    {
        tracker.absoluteMove(position.inputVector);
        ASSERT_EQ(tracker.position(), position.resultVector);
    }
}

TEST(TestPositionTrackerTest, relativeMovement)
{
    test_support::TestPositionTracker tracker;
    tracker.setLimits(kRelativeMovementTestInput.limits);

    for (const auto& position : kRelativeMovementTestInput.movementSequence)
    {
        tracker.relativeMove(position.inputVector);
        ASSERT_EQ(tracker.position(), position.resultVector);
    }
}

} // namespace ptz
} // namespace core
} // namespace nx
