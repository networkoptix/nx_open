// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/client/core/cloud_features/cloud_feature.h>

#include "test_cloud_features.h"

namespace nx::vms::client::core::test {

TEST(CloudFeature, allCloudFeaturesAreExactlyEnumItems)
{
    EXPECT_EQ(allCloudFeatures(), kAllFeatures);
}

} // namespace nx::vms::client::core::test
