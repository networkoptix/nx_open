#include <gtest/gtest.h>

#include <chrono>

#include <nx/core/ptz/test_support/test_ptz_controller.h>
#include <nx/core/ptz/test_support/test_position_tracker.h>
#include <nx/core/ptz/relative/relative_continuous_move_mapping.h>
#include <nx/core/ptz/relative/relative_move_workaround_controller.h>
#include <nx/core/ptz/relative/relative_continuous_move_sequence_maker.h>
#include <nx/core/ptz/utils/continuous_move_sequence_executor.h>

#include <nx/fusion/model_functions.h>

using namespace std::chrono;
using namespace std::chrono_literals;

namespace nx {
namespace core {
namespace ptz {

namespace {

static const double kEpsilon = 5.0;

struct ExtensionTestCase
{
    Ptz::Capabilities capabilities;
    bool canBeExtendedWithRelativeMovement = false;
};

struct RelativeContinuousTestCase
{
    ptz::Vector initialPosition;
    ptz::Vector movementVector;
    ptz::Vector resultPosition;
};

static const std::vector<RelativeContinuousTestCase> kRelativeContinuousMoveTestCases = {
    {{0, 0, 0, 0, 0}, {0.25, 0, 0, 0, 0}, {50, 0, 0, 0, 0}},
    {{0, -100, 0, 0, 0}, {0, 0.7, 0, 0, 0}, {0, 40, 0, 0, 0}},
    {{0, 0, 70, 0, 0}, {0, 0, 0.3, 0, 0}, {0, 0, 100, 0, 0}},
    {{0, 0, 0, 100, 0}, {0, 0, 0, -0.25, 0}, {0, 0, 0, 75, 0}},
    {{0, 0, -100, 80, 0}, {-0.25, 0.3, 0.8, -0.25, 0}, {-50, 60, 60, 55, 0}}
};

} // namespace

// TODO: #dmishin tests on relativeFocus and mixed (relative/absolute) movement.

TEST(RelativeMoveWorkaround, extending)
{
    static const std::vector<ExtensionTestCase> kTestCases = {
        {Ptz::NoPtzCapabilities, false},
        {
            Ptz::PresetsPtzCapability
            | Ptz::ToursPtzCapability
            | Ptz::HomePtzCapability
            | Ptz::ActivityPtzCapability
            | Ptz::AuxiliaryPtzCapability
            | Ptz::AsynchronousPtzCapability
            | Ptz::SynchronizedPtzCapability
            | Ptz::LogicalPositioningPtzCapability
            | Ptz::DevicePositioningPtzCapability
            | Ptz::NativePresetsPtzCapability
            | Ptz::NoNxPresetsPtzCapability
            | Ptz::FlipPtzCapability
            | Ptz::LimitsPtzCapability
            | Ptz::VirtualPtzCapability,
            false
        },
        {Ptz::ContinuousPanCapability, true},
        {Ptz::ContinuousTiltCapability, true},
        {Ptz::ContinuousZoomCapability, true},
        {Ptz::ContinuousRotationCapability, true},
        {Ptz::ContinuousFocusCapability, true},
        {Ptz::AbsolutePanCapability, true},
        {Ptz::AbsoluteTiltCapability, true},
        {Ptz::AbsoluteZoomCapability, true},
        {Ptz::AbsoluteRotationCapability, true},
        {Ptz::ContinuousPanCapability | Ptz::RelativePanCapability, false},
        {Ptz::ContinuousTiltCapability | Ptz::RelativeTiltCapability, false},
        {Ptz::ContinuousZoomCapability | Ptz::RelativeZoomCapability, false},
        {Ptz::ContinuousRotationCapability | Ptz::RelativeRotationCapability, false},
        {Ptz::ContinuousFocusCapability | Ptz::RelativeFocusCapability, false},
        {Ptz::AbsolutePanCapability | Ptz::RelativePanCapability, false},
        {Ptz::AbsoluteTiltCapability | Ptz::RelativeTiltCapability, false},
        {Ptz::AbsoluteZoomCapability | Ptz::RelativeZoomCapability, false},
        {Ptz::AbsoluteRotationCapability | Ptz::RelativeRotationCapability, false}
    };

    for (const auto& testCase: kTestCases)
    {
        ASSERT_EQ(
            testCase.canBeExtendedWithRelativeMovement,
            RelativeMoveWorkaroundController::extends(testCase.capabilities));
    }
}

TEST(RelativeMoveWorkaround, relativeMoveProxy)
{
    QSharedPointer<test_support::TestPtzController> controller(
        new test_support::TestPtzController());

    controller->setCapabilities(Ptz::AbsolutePanCapability
        | Ptz::RelativeTiltCapability
        | Ptz::RelativeRotationCapability
        | Ptz::RelativeZoomCapability
        | Ptz::RelativeFocusCapability);

    const ptz::Vector moveDirection(0, 0.1, 0.2, 0.3);
    ptz::Vector moveResult;
    controller->setRelativeMoveExecutor(
        [&moveResult](const ptz::Vector& direction, const ptz::Options& options)
        {
            moveResult = direction + moveResult;
            return true;
        });

    const qreal focusDirection = -1.0;
    qreal focusResult = 0.0;
    controller->setRelativeFocusExecutor(
        [&focusResult](qreal direction, const ptz::Options& options)
        {
            focusResult += direction;
            return true;
        });

    auto threadPool = std::make_unique<QThreadPool>();
    QnPtzControllerPtr relativeMoveController(
        new ptz::RelativeMoveWorkaroundController(
            controller,
            std::make_shared<ptz::RelativeContinuousMoveSequenceMaker>(
                RelativeContinuousMoveMapping()),
            std::make_shared<ptz::ContinuousMoveSequenceExecutor>(
                controller.data(),
                threadPool.get())));

    bool result = relativeMoveController->relativeMove(moveDirection);
    ASSERT_TRUE(result);
    ASSERT_TRUE(qFuzzyEquals(moveDirection, moveResult));

    result = relativeMoveController->relativeFocus(focusDirection);
    ASSERT_TRUE(result);
    ASSERT_TRUE(qFuzzyEquals(focusDirection, focusResult));
}

TEST(RelativeMoveWorkaround, relativeMoveViaAbsoluteMove)
{
    QSharedPointer<test_support::TestPtzController> controller(
        new test_support::TestPtzController());

    controller->setCapabilities(Ptz::LogicalPositioningPtzCapability
        | Ptz::AbsolutePanCapability
        | Ptz::AbsoluteTiltCapability
        | Ptz::AbsoluteRotationCapability
        | Ptz::AbsoluteZoomCapability);

    QnPtzLimits limits;
    limits.minPan = -10;
    limits.maxPan = 10;
    limits.minTilt = -10;
    limits.maxTilt = 10;
    limits.minRotation = -10;
    limits.maxRotation = 10;
    limits.minFov = -10;
    limits.maxFov = 10;
    controller->setLimits(limits);

    ptz::Vector position;
    controller->setAbsoluteMoveExecutor(
        [&position](
            Qn::PtzCoordinateSpace /*space*/,
            const ptz::Vector& direction,
            qreal /*speed*/,
            const ptz::Options& /*options*/)
        {
            position = direction;
            return true;
        });

    controller->setGetPositionExecutor(
        [&position](
            Qn::PtzCoordinateSpace /*space*/,
            ptz::Vector* outPosition,
            const ptz::Options& /*options*/)
        {
            *outPosition = position;
            return true;
        });

    auto threadPool = std::make_unique<QThreadPool>();
    QnPtzControllerPtr relativeMoveController(
        new ptz::RelativeMoveWorkaroundController(
            controller,
            std::make_shared<ptz::RelativeContinuousMoveSequenceMaker>(
                RelativeContinuousMoveMapping()),
            std::make_shared<ptz::ContinuousMoveSequenceExecutor>(
                controller.data(),
                threadPool.get())));

    const ptz::Vector moveDirection(0.1, 0.2, 0.3, 0.4);
    const ptz::Vector expectedMoveResult(2, 4, 6, 8);
    bool result = relativeMoveController->relativeMove(moveDirection);

    ptz::Vector moveResult;
    controller->getPosition(
        Qn::LogicalPtzCoordinateSpace,
        &moveResult,
        {nx::core::ptz::Type::operational});

    ASSERT_TRUE(result);
    ASSERT_TRUE(qFuzzyEquals(expectedMoveResult, moveResult));
}

TEST(RelativeMoveWorkaround, DISABLED_relativeMoveViaContinuousMove)
{
    QSharedPointer<test_support::TestPtzController> controller(
        new test_support::TestPtzController());

    controller->setCapabilities(Ptz::ContinuousPtrzCapabilities | Ptz::ContinuousFocusCapability);

    QnPtzLimits limits;
    limits.minPan = -100;
    limits.maxPan = 100;
    limits.minTilt = -100;
    limits.maxTilt = 100;
    limits.minRotation = -100;
    limits.maxRotation = 100;
    limits.minFov = 0;
    limits.maxFov = 100;
    limits.minFocus = 0;
    limits.maxFocus = 100;
    controller->setLimits(limits);

    RelativeContinuousMoveMapping mapping({1s, 1s, 1s, 1s, 1s});

    auto speedCoefficient =
        [&limits, &mapping](ptz::Component component)
        {
            const auto componentMapping = mapping.componentMapping(component);
            return limits.componentRange(component)
                / duration_cast<seconds>(componentMapping.workingSpeed.cycleDuration).count();
        };

    test_support::TestPositionTracker tracker;
    tracker.setLimits(limits);
    tracker.setSpeedCoefficientsUnitPerSecond({
        speedCoefficient(ptz::Component::pan),
        speedCoefficient(ptz::Component::tilt),
        speedCoefficient(ptz::Component::rotation),
        speedCoefficient(ptz::Component::zoom),
        speedCoefficient(ptz::Component::focus)
    });

    controller->setContinuousMoveExecutor(
        [&tracker](
            const ptz::Vector& speedVector,
            const ptz::Options& /*options*/)
        {
            tracker.continuousMove(speedVector);
            return true;
        });

    controller->setContinuousFocusExecutor(
        [&tracker](
            double speed,
            const ptz::Options& /*options*/)
        {
            tracker.continuousFocus(speed);
            return true;
        });

    controller->setGetPositionExecutor(
        [&tracker](
            Qn::PtzCoordinateSpace space,
            ptz::Vector* outPosition,
            const ptz::Options& options)
        {
            *outPosition = tracker.position();
            return true;
        });

    auto threadPool = std::make_unique<QThreadPool>();
    QSharedPointer<RelativeMoveWorkaroundController> relativeMoveController(
        new ptz::RelativeMoveWorkaroundController(
            controller,
            std::make_shared<ptz::RelativeContinuousMoveSequenceMaker>(mapping),
            std::make_shared<ptz::ContinuousMoveSequenceExecutor>(
                controller.data(),
                threadPool.get())));

    for (const auto& testCase: kRelativeContinuousMoveTestCases)
    {
        tracker.setPosition(testCase.initialPosition);
        nx::utils::promise<ptz::Vector> donePromise;

        relativeMoveController->setRelativeMoveDoneCallback(
            [relativeMoveController, &donePromise]()
            {
                ptz::Vector realPosition;
                relativeMoveController->getPosition(
                    Qn::DevicePtzCoordinateSpace,
                    &realPosition,
                    ptz::Options());

                donePromise.set_value(realPosition);
            });

        relativeMoveController->relativeMove(testCase.movementVector, ptz::Options());
        auto fut = donePromise.get_future();
        fut.wait();
        const auto realPosition = fut.get();
        ASSERT_LT(
            (testCase.resultPosition - realPosition).length(),
            kEpsilon);
    }
}

} // namespace ptz
} // namespace core
} // namespace nx

