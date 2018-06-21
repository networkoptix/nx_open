#include <gtest/gtest.h>

#include <nx/core/ptz/test_ptz_controller.h>
#include <nx/core/ptz/realtive/relative_move_workaround_controller.h>

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

} // namespace ptz
} // namespace core
} // namespace nx

