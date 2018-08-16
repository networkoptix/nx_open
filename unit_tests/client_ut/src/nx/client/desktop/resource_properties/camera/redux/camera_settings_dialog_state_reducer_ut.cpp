#include <gtest/gtest.h>

#include <core/resource/layout_resource.h>

#include <nx/client/desktop/resource_properties/camera/redux/camera_settings_dialog_state.h>
#include <nx/client/desktop/resource_properties/camera/redux/camera_settings_dialog_state_reducer.h>

#include <ui/style/globals.h>

namespace nx {
namespace client {
namespace desktop {

class CameraSettingsDialogStateReducerTest: public testing::Test
{
public:
    using State = CameraSettingsDialogState;
    using Reducer = CameraSettingsDialogStateReducer;

    // TODO: #vkutin #gdm Implement more tests.
    // Currently only fixed archive length validation is done.
};

TEST_F(CameraSettingsDialogStateReducerTest, fixedArchiveLengthValidation)
{
    static constexpr int kTestDaysBig = nx::vms::api::kDefaultMaxArchiveDays + 1;

    // Unchecking automatic max days when no fixed value is set: sets fixed value to default.
    State s;
    s = Reducer::setMaxRecordingDaysAutomatic(std::move(s), false);
    ASSERT_FALSE(s.recording.maxDays.automatic.valueOr(true));
    ASSERT_EQ(s.recording.maxDays.value.get(), nx::vms::api::kDefaultMaxArchiveDays);

    // Unchecking automatic max days when fixed value is set: keeps fixed value.
    s = {};
    s.recording.maxDays.value.setBase(kTestDaysBig);
    s = Reducer::setMaxRecordingDaysAutomatic(std::move(s), false);
    ASSERT_FALSE(s.recording.maxDays.automatic.valueOr(true));
    ASSERT_EQ(s.recording.maxDays.value.get(), kTestDaysBig);

    // Unchecking automatic max days when no fixed value is set: sets fixed value to default.
    // Checks that max days value stays greater or equal than fixed min days value.
    s = {};
    s.recording.minDays.automatic.setBase(false);
    s.recording.minDays.value.setBase(kTestDaysBig);
    s = Reducer::setMaxRecordingDaysAutomatic(std::move(s), false);
    ASSERT_FALSE(s.recording.maxDays.automatic.valueOr(true));
    ASSERT_GE(s.recording.maxDays.value.get(), s.recording.minDays.value.get());

    // Unchecking automatic max days when fixed value is set: keeps fixed value.
    // Checks that max days value stays greater or equal than fixed min days value.
    s = {};
    s.recording.maxDays.value.setBase(kTestDaysBig);
    s.recording.minDays.automatic.setBase(false);
    s.recording.minDays.value.setBase(kTestDaysBig + 1);
    s = Reducer::setMaxRecordingDaysAutomatic(std::move(s), false);
    ASSERT_FALSE(s.recording.maxDays.automatic.valueOr(true));
    ASSERT_GE(s.recording.maxDays.value.get(), s.recording.minDays.value.get());

    // Unchecking automatic min days when no fixed value is set: sets fixed value to default.
    s = {};
    s = Reducer::setMinRecordingDaysAutomatic(std::move(s), false);
    ASSERT_FALSE(s.recording.minDays.automatic.valueOr(true));
    ASSERT_EQ(s.recording.minDays.value.get(), nx::vms::api::kDefaultMinArchiveDays);

    // Unchecking automatic min days when fixed value is set: keeps fixed value.
    s = {};
    s.recording.minDays.value.setBase(kTestDaysBig);
    s = Reducer::setMinRecordingDaysAutomatic(std::move(s), false);
    ASSERT_FALSE(s.recording.minDays.automatic.valueOr(true));
    ASSERT_EQ(s.recording.minDays.value.get(), kTestDaysBig);

    // Unchecking automatic min days when fixed value is set: keeps fixed value.
    // Checks that min days value stays lesser or equal than fixed max days value.
    s = {};
    s.recording.minDays.value.setBase(kTestDaysBig);
    s.recording.maxDays.automatic.setBase(false);
    s.recording.maxDays.value.setBase(kTestDaysBig - 1);
    s = Reducer::setMinRecordingDaysAutomatic(std::move(s), false);
    ASSERT_FALSE(s.recording.minDays.automatic.valueOr(true));
    ASSERT_LE(s.recording.minDays.value.get(), s.recording.maxDays.value.get());

    // Setting fixed max days less than fixed min days pushes min days down.
    s = {};
    s.recording.minDays.automatic.setBase(false);
    s.recording.maxDays.automatic.setBase(false);
    s.recording.minDays.value.setBase(kTestDaysBig + 1);
    s = Reducer::setMaxRecordingDaysValue(std::move(s), kTestDaysBig);
    ASSERT_EQ(s.recording.maxDays.value.get(), kTestDaysBig);
    ASSERT_GE(s.recording.maxDays.value.get(), s.recording.minDays.value.get());

    // Setting fixed min days greater than fixed max days pushes max days up.
    s = {};
    s.recording.minDays.automatic.setBase(false);
    s.recording.maxDays.automatic.setBase(false);
    s.recording.maxDays.value.setBase(kTestDaysBig - 1);
    s = Reducer::setMinRecordingDaysValue(std::move(s), kTestDaysBig);
    ASSERT_EQ(s.recording.minDays.value.get(), kTestDaysBig);
    ASSERT_LE(s.recording.minDays.value.get(), s.recording.maxDays.value.get());
}

} // namespace desktop
} // namespace client
} // namespace nx
