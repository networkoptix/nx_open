#include <gtest/gtest.h>

#include <nx/core/ptz/test_support/test_ptz_controller.h>
#include <nx/core/ptz/realtive/relative_continuous_move_mapping.h>
#include <nx/core/ptz/realtive/relative_move_workaround_controller.h>

#include <nx/fusion/model_functions.h>

namespace nx {
namespace core {
namespace ptz {

namespace {

struct TestCase
{
    Ptz::Capabilities capabilities;
    bool canBeExtendedWithRelativeMovement = false;
};

} // namespace

TEST(RelativeMoveWorkaround, extending)
{
    static const std::vector<TestCase> kTestCases = {
        {Ptz::NoPtzCapabilities, false},
        {
            Ptz::PresetsPtzCapability
            | Ptz::ToursPtzCapability
            | Ptz::HomePtzCapability
            | Ptz::ActivityPtzCapability
            | Ptz::AuxilaryPtzCapability
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
    QSharedPointer<TestPtzController> controller(new TestPtzController());
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
            RelativeContinuousMoveMapping(),
            threadPool.get()));

    bool result = relativeMoveController->relativeMove(moveDirection, ptz::Options());
    ASSERT_TRUE(result);
    ASSERT_TRUE(qFuzzyEquals(moveDirection, moveResult));

    result = relativeMoveController->relativeFocus(focusDirection, ptz::Options());
    ASSERT_TRUE(result);
    ASSERT_TRUE(qFuzzyEquals(focusDirection, focusResult));
}

TEST(RelativeMoveWorkaround, relativeMoveViaAbsoluteMove)
{
    QSharedPointer<TestPtzController> controller(new TestPtzController());
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
            RelativeContinuousMoveMapping(),
            threadPool.get()));

    const ptz::Vector moveDirection(0.1, 0.2, 0.3, 0.4);
    const ptz::Vector expectedMoveResult(2, 4, 6, 8);
    bool result = relativeMoveController->relativeMove(moveDirection, ptz::Options());

    ptz::Vector moveResult;
    controller->getPosition(
        Qn::LogicalPtzCoordinateSpace,
        &moveResult,
        ptz::Options());

    ASSERT_TRUE(result);
    ASSERT_TRUE(qFuzzyEquals(expectedMoveResult, moveResult));
}

} // namespace ptz
} // namespace core
} // namespace nx

