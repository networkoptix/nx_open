// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/client/core/cloud_features/ini_cloud_feature_source.h>

#include "test_cloud_features.h"

namespace nx::vms::client::core::test {

TEST(IniCloudFeatureSource, disablesListedFeaturesOnly)
{
    // Unknown names, numbers and other letter cases are ignored.
    const IniCloudFeatureSource source(
        "vmsClientCloudLayouts, vmsClientCrashReporting;unknown;3;VmsClientPushNotifications");
    EXPECT_EQ(source.enabledFeatures(), kPush | kCrossSite | kQrCode);

    EXPECT_EQ(IniCloudFeatureSource("").enabledFeatures(), kAllFeatures);
}

} // namespace nx::vms::client::core::test
