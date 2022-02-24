// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>

#include <gtest/gtest.h>

#include <core/resource/motion_window.h>
#include <nx/reflect/enum_string_conversion.h>
#include <nx/reflect/to_string.h>

#include "camera_settings_test_fixture.h"

namespace nx::vms::client::desktop {

std::ostream& operator<<(std::ostream& os, CameraSettingsDialogState::MotionHint value)
{
    switch (value)
    {
        case CameraSettingsDialogState::MotionHint::sensitivityChanged:
            os << "sensitivityChanged";
            break;
        case CameraSettingsDialogState::MotionHint::completelyMaskedOut:
            os << "completelyMaskedOut";
            break;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os,
    std::optional<CameraSettingsDialogState::MotionHint> value)
{
    if (value)
        os << *value;
    else
        os << "std::nullopt";
    return os;
}

namespace test {

using namespace nx::vms::api;

class CameraSettingsDialogMotionTest: public CameraSettingsTestFixture
{
protected:
    void whenMotionRegionSet(QnMotionRegion region)
    {
        dispatch(Reducer::setMotionRegionList, QList<QnMotionRegion>() << region);
    }

    void thenMotionHintIs(std::optional<State::MotionHint> value)
    {
        EXPECT_EQ(state().motionHint, value);
    }

    void whenCurrentMotionSensitivityChanged(int value)
    {
        dispatch(Reducer::setCurrentMotionSensitivity, value);
    }
};

// When user selects motion sensitivity, hint should appear only if there is only one rect.
TEST_F(CameraSettingsDialogMotionTest, motionHintIsNotShownWhenThereAreSeveralRegions)
{
    QnMotionRegion motionRegion; //< Filled with default sensitivity.

    givenCamera();
    whenMotionRegionSet(motionRegion);
    thenMotionHintIs({});
    whenCurrentMotionSensitivityChanged(QnMotionRegion::kDefaultSensitivity + 1);
    thenMotionHintIs(State::MotionHint::sensitivityChanged);

    motionRegion.addRect(QnMotionRegion::kDefaultSensitivity + 1, QRect(0, 0, 2, 2));
    whenMotionRegionSet(motionRegion);
    whenCurrentMotionSensitivityChanged(QnMotionRegion::kDefaultSensitivity + 2);
    thenMotionHintIs({});
}

} // namespace test
} // namespace nx::vms::client::desktop
