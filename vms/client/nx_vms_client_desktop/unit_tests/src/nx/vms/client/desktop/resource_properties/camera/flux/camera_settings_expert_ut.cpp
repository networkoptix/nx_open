// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include "camera_settings_test_fixture.h"

namespace nx::vms::client::desktop::test {

using namespace nx::vms::api;

class CameraSettingsDialogExpertTest: public CameraSettingsTestFixture
{
protected:
    void whenKeepCameraTimeSettings(bool value)
    {
        dispatch(Reducer::setKeepCameraTimeSettings, value);
    }

    void whenOptimizationEnabled(bool value)
    {
        dispatch(Reducer::setSettingsOptimizationEnabled, value);
    }

    void thenSettingsOptimizationEnabled(bool value)
    {
        EXPECT_EQ(state().settingsOptimizationEnabled, value);
    }

    void thenKeepCameraTimeSettings(bool value)
    {
        EXPECT_EQ(state().expert.keepCameraTimeSettings, value);
    }

    void thenSecondaryRecordingEnabled(bool value)
    {
        EXPECT_EQ(state().expert.secondaryRecordingDisabled, !value);
    }
};

TEST_F(CameraSettingsDialogExpertTest, checkKeepCameraTimeSettings)
{
    givenCamera();

    whenOptimizationEnabled(true);
    whenKeepCameraTimeSettings(true);
    thenKeepCameraTimeSettings(true);

    whenKeepCameraTimeSettings(false);
    thenKeepCameraTimeSettings(false);
}

// When we turn on the second stream (even if the camera does not have an internal second stream),
// then by default the recording from the second stream should be enabled.
TEST_F(CameraSettingsDialogExpertTest, checkSecondaryRecordingEnabled)
{
    auto camera = givenCamera();
    camera->setHasDualStreaming(false);
    whenCamerasAreLoaded();

    whenRecordingEnabled(true);
    whenDualStreamingEnabled(true);

    thenSecondaryRecordingEnabled(true);
}

} // namespace nx::vms::client::desktop::test
