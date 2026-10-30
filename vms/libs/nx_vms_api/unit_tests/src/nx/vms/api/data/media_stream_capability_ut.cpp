// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>
#include <nx/reflect/json.h>
#include <nx/vms/api/data/media_stream_capability.h>

namespace nx::vms::api::test {

static const CameraStreamCapability kCapability{.availableFps = {1, 1.33, 3, 3.33, 6.67}};

TEST(CameraStreamCapability, availableFpsJsonHasNoFloatNoise)
{
    for (const auto& json:
        {nx::reflect::json::serialize(kCapability), QJson::serialized(kCapability).toStdString()})
    {
        EXPECT_NE(json.find("1.33,"), std::string::npos) << json;
        EXPECT_NE(json.find("3.33,"), std::string::npos) << json;
        EXPECT_NE(json.find("6.67]"), std::string::npos) << json;
        EXPECT_EQ(json.find("0000"), std::string::npos) << json;
    }
}

TEST(CameraStreamCapability, strictFpsMatchesFloatOfAvailableFps)
{
    EXPECT_EQ(kCapability.strictFps(3.33f), 3.33f); //< 3.33f < 3.33.
    EXPECT_EQ(kCapability.strictFps(6.67f), 6.67f); //< 6.67f > 6.67.
    EXPECT_EQ(kCapability.strictFps(3.2f), 3.0f);
    EXPECT_EQ(kCapability.strictFps(0.5f), 1.0f);
    EXPECT_EQ(kCapability.strictFps(30.0f), 6.67f);
}

} // namespace nx::vms::api::test
