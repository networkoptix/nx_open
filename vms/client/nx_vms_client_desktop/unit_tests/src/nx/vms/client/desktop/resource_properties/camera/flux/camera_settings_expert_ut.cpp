// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include "camera_settings_test_fixture.h"

namespace nx::vms::client::desktop::test {

using namespace nx::vms::api;

class CameraSettingsDialogExpertTest: public CameraSettingsTestFixture
{
protected:
    void thenSecondaryRecordingEnabled(bool value)
    {
        EXPECT_TRUE(state().expert.secondaryRecordingDisabled.hasValue());
        EXPECT_EQ(state().expert.secondaryRecordingDisabled.valueOr(false), !value);
    }

    void whenOptimizationEnabled(bool value)
    {
        dispatch(Reducer::setSettingsOptimizationEnabled, value);
    }

    void whenKeepCameraTimeSettings(bool value)
    {
        dispatch(Reducer::setKeepCameraTimeSettings, value);
    }

    void whenRemoteArchiveSynchronizationEnable(bool value)
    {
        dispatch(Reducer::setRemoteArchiveSynchronizationEnabled, value);
    }

    void thenKeepCameraTimeSettingsIsDefault(bool isDefault)
    {
        EXPECT_EQ(state().expert.isKeepCameraTimeSettingsDefault, isDefault);
    }

    void thenSettingsOptimizationEnabled(bool value)
    {
        EXPECT_EQ(state().settingsOptimizationEnabled, value);
    }

    void thenKeepCameraTimeSettings(bool value)
    {
        EXPECT_TRUE(state().expert.keepCameraTimeSettings.hasValue());
        EXPECT_EQ(state().expert.keepCameraTimeSettings.valueOr(false), value);
    }

    void thenKeepCameraTimeSettingsDoesNotHaveValue()
    {
        EXPECT_FALSE(state().expert.keepCameraTimeSettings.hasValue());
    }

    void thenFirstCameraKeepCameraTimeSettings(bool value)
    {
        EXPECT_EQ(cameras().at(0)->keepCameraTimeSettings(), value);
    }

    void thenSecondCameraKeepCameraTimeSettings(bool value)
    {
        EXPECT_EQ(cameras().at(1)->keepCameraTimeSettings(), value);
    }

    void thenRemoteArchiveSynchronizationEnable(bool value)
    {
        EXPECT_TRUE(state().expert.remoteArchiveSynchronizationEnabled.hasValue());
        EXPECT_EQ(state().expert.remoteArchiveSynchronizationEnabled.valueOr(false), value);
    }

    void thenRemoteArchiveSynchronizationEnableDoesNotHaveValue()
    {
        EXPECT_FALSE(state().expert.remoteArchiveSynchronizationEnabled.hasValue());
    }

    void thenFirstCameraRemoteArchiveSynchronizationEnable(bool value)
    {
        EXPECT_EQ(cameras().at(0)->isRemoteArchiveSynchronizationEnabled(), value);
    }

    void thenSecondCameraRemoteArchiveSynchronizationEnable(bool value)
    {
        EXPECT_EQ(cameras().at(1)->isRemoteArchiveSynchronizationEnabled(), value);
    }
};

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

// keepCameraTimeSettings, camera hasn't RemoteArchive capability

TEST_F(CameraSettingsDialogExpertTest, resetDefaultKeepCameraTimeSettings)
{
    givenCamera();
    whenOptimizationEnabled(false); // set and reset do nothing.
    thenKeepCameraTimeSettings(true);
    thenFirstCameraKeepCameraTimeSettings(true);
    thenKeepCameraTimeSettingsIsDefault(true);

    whenKeepCameraTimeSettings(false);
    thenKeepCameraTimeSettings(true);
    whenChangesAreSaved();
    thenFirstCameraKeepCameraTimeSettings(true);
    thenKeepCameraTimeSettingsIsDefault(true);

    dispatch(Reducer::resetExpertSettings);
    thenKeepCameraTimeSettings(true);
    whenChangesAreSaved();
    thenFirstCameraKeepCameraTimeSettings(true);
    thenKeepCameraTimeSettingsIsDefault(true);
}

TEST_F(CameraSettingsDialogExpertTest, resetDefaultKeepCameraTimeSettingsWithOptimization)
{
    givenCamera();
    whenOptimizationEnabled(true); // set and reset work as expected.
    thenKeepCameraTimeSettings(true);
    thenFirstCameraKeepCameraTimeSettings(true);
    thenKeepCameraTimeSettingsIsDefault(true);

    whenKeepCameraTimeSettings(false);
    thenKeepCameraTimeSettings(false);
    whenChangesAreSaved();
    thenFirstCameraKeepCameraTimeSettings(false);
    thenKeepCameraTimeSettingsIsDefault(false);

    dispatch(Reducer::resetExpertSettings);
    thenKeepCameraTimeSettings(true);
    whenChangesAreSaved();
    thenFirstCameraKeepCameraTimeSettings(true);
    thenKeepCameraTimeSettingsIsDefault(true);
}

// keepCameraTimeSettings, camera has RemoteArchive capability

TEST_F(CameraSettingsDialogExpertTest, resetDefaultKeepCameraTimeSettingsWithRemoteArchive)
{
    givenCamera(CameraFeature::remoteArchive);
    whenOptimizationEnabled(false); // set and reset do nothing.
    thenKeepCameraTimeSettings(false);
    thenFirstCameraKeepCameraTimeSettings(false);
    thenKeepCameraTimeSettingsIsDefault(true);

    whenKeepCameraTimeSettings(true);
    thenKeepCameraTimeSettings(false);
    whenChangesAreSaved();
    thenFirstCameraKeepCameraTimeSettings(false);
    thenKeepCameraTimeSettingsIsDefault(true);

    dispatch(Reducer::resetExpertSettings);
    thenKeepCameraTimeSettings(false);
    whenChangesAreSaved();
    thenFirstCameraKeepCameraTimeSettings(false);
    thenKeepCameraTimeSettingsIsDefault(true);
}

TEST_F(CameraSettingsDialogExpertTest, resetDefaultKeepCameraTimeSettingsWithOptimizationWithRemoteArchive)
{
    givenCamera(CameraFeature::remoteArchive);
    whenOptimizationEnabled(true); // set and reset work as expected.
    thenKeepCameraTimeSettings(false);
    thenFirstCameraKeepCameraTimeSettings(false);
    thenKeepCameraTimeSettingsIsDefault(true);

    whenKeepCameraTimeSettings(true);
    thenKeepCameraTimeSettings(true);
    whenChangesAreSaved();
    thenFirstCameraKeepCameraTimeSettings(true);
    thenKeepCameraTimeSettingsIsDefault(false);

    dispatch(Reducer::resetExpertSettings);
    thenKeepCameraTimeSettings(false);
    whenChangesAreSaved();
    thenFirstCameraKeepCameraTimeSettings(false);
    thenKeepCameraTimeSettingsIsDefault(true);
}

// keepCameraTimeSettings, 2 cameras

TEST_F(CameraSettingsDialogExpertTest, resetDefaultKeepCameraTimeSettings2Cameras)
{
    givenCamera();
    givenCamera(CameraFeature::remoteArchive);
    whenOptimizationEnabled(false); // set and reset do nothing.
    thenKeepCameraTimeSettingsDoesNotHaveValue();
    thenFirstCameraKeepCameraTimeSettings(true);
    thenSecondCameraKeepCameraTimeSettings(false);
    thenKeepCameraTimeSettingsIsDefault(true);

    whenKeepCameraTimeSettings(false);
    thenKeepCameraTimeSettingsDoesNotHaveValue();
    whenChangesAreSaved();
    thenFirstCameraKeepCameraTimeSettings(true);
    thenSecondCameraKeepCameraTimeSettings(false);
    thenKeepCameraTimeSettingsIsDefault(true);

    dispatch(Reducer::resetExpertSettings);
    thenKeepCameraTimeSettingsDoesNotHaveValue();
    whenChangesAreSaved();
    thenFirstCameraKeepCameraTimeSettings(true);
    thenSecondCameraKeepCameraTimeSettings(false);
    thenKeepCameraTimeSettingsIsDefault(true);
}

TEST_F(CameraSettingsDialogExpertTest, resetDefaultKeepCameraTimeSettingsWithOptimization2Cameras)
{
    givenCamera();
    givenCamera(CameraFeature::remoteArchive);
    whenOptimizationEnabled(true); // set and reset work as expected.
    thenKeepCameraTimeSettingsDoesNotHaveValue();
    thenFirstCameraKeepCameraTimeSettings(true);
    thenSecondCameraKeepCameraTimeSettings(false);
    thenKeepCameraTimeSettingsIsDefault(true);

    whenKeepCameraTimeSettings(true);
    thenKeepCameraTimeSettings(true);
    whenChangesAreSaved();
    thenFirstCameraKeepCameraTimeSettings(true);
    thenSecondCameraKeepCameraTimeSettings(true);
    thenKeepCameraTimeSettingsIsDefault(false);

    dispatch(Reducer::resetExpertSettings);
    thenKeepCameraTimeSettingsDoesNotHaveValue();
    whenChangesAreSaved();
    thenFirstCameraKeepCameraTimeSettings(true);
    thenSecondCameraKeepCameraTimeSettings(false);
    thenKeepCameraTimeSettingsIsDefault(true);
}

//remoteArchiveSynchronizationEnabled

TEST_F(CameraSettingsDialogExpertTest, resetDefaultRemoteArchiveSynchronizationEnabled)
{
    givenCamera();
    thenRemoteArchiveSynchronizationEnable(false);
    thenFirstCameraRemoteArchiveSynchronizationEnable(false);

    whenRemoteArchiveSynchronizationEnable(true);
    thenRemoteArchiveSynchronizationEnable(true);
    whenChangesAreSaved();
    thenFirstCameraRemoteArchiveSynchronizationEnable(false);

    whenRemoteArchiveSynchronizationEnable(false);
    thenRemoteArchiveSynchronizationEnable(false);
    whenChangesAreSaved();
    thenFirstCameraRemoteArchiveSynchronizationEnable(false);

    whenRemoteArchiveSynchronizationEnable(true);
    thenRemoteArchiveSynchronizationEnable(true);
    dispatch(Reducer::resetExpertSettings);
    thenRemoteArchiveSynchronizationEnable(false);
    whenChangesAreSaved();
    thenFirstCameraRemoteArchiveSynchronizationEnable(false);
}

TEST_F(CameraSettingsDialogExpertTest, resetDefaultRemoteArchiveSynchronizationEnabledWithRemoteArchive)
{
    givenCamera(CameraFeature::remoteArchive);
    thenRemoteArchiveSynchronizationEnable(true);
    thenFirstCameraRemoteArchiveSynchronizationEnable(true);

    whenRemoteArchiveSynchronizationEnable(false);
    thenRemoteArchiveSynchronizationEnable(false);
    whenChangesAreSaved();
    thenFirstCameraRemoteArchiveSynchronizationEnable(false);

    whenRemoteArchiveSynchronizationEnable(true);
    thenRemoteArchiveSynchronizationEnable(true);
    whenChangesAreSaved();
    thenFirstCameraRemoteArchiveSynchronizationEnable(true);

    whenRemoteArchiveSynchronizationEnable(false);
    thenRemoteArchiveSynchronizationEnable(false);
    dispatch(Reducer::resetExpertSettings);
    thenRemoteArchiveSynchronizationEnable(true);
    whenChangesAreSaved();
    thenFirstCameraRemoteArchiveSynchronizationEnable(true);
}

// remoteArchiveSynchronizationEnabled, 2 cameras

TEST_F(CameraSettingsDialogExpertTest, resetDefaultRemoteArchiveSynchronizationEnabled2Cameras)
{
    givenCamera();
    givenCamera(CameraFeature::remoteArchive);
    thenRemoteArchiveSynchronizationEnableDoesNotHaveValue();
    thenFirstCameraRemoteArchiveSynchronizationEnable(false);
    thenSecondCameraRemoteArchiveSynchronizationEnable(true);

    whenRemoteArchiveSynchronizationEnable(true);
    thenRemoteArchiveSynchronizationEnable(true);
    whenChangesAreSaved();
    thenFirstCameraRemoteArchiveSynchronizationEnable(false);
    thenSecondCameraRemoteArchiveSynchronizationEnable(true);

    whenRemoteArchiveSynchronizationEnable(false);
    thenRemoteArchiveSynchronizationEnable(false);
    whenChangesAreSaved();
    thenFirstCameraRemoteArchiveSynchronizationEnable(false);
    thenSecondCameraRemoteArchiveSynchronizationEnable(false);

    dispatch(Reducer::resetExpertSettings);
    thenRemoteArchiveSynchronizationEnableDoesNotHaveValue();
    whenChangesAreSaved();
    thenFirstCameraRemoteArchiveSynchronizationEnable(false);
    thenSecondCameraRemoteArchiveSynchronizationEnable(true);
}

} // namespace nx::vms::client::desktop::test
