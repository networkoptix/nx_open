// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/cloud_features/cloud_feature.h>

namespace nx::vms::client::core::test {

inline constexpr CloudFeatures kLayouts = CloudFeature::vmsClientCloudLayouts;
inline constexpr CloudFeatures kPush = CloudFeature::vmsClientPushNotifications;
inline constexpr CloudFeatures kCrossSite = CloudFeature::vmsClientCrossSiteNotifications;
inline constexpr CloudFeatures kCrashReporting = CloudFeature::vmsClientCrashReporting;
inline constexpr CloudFeatures kQrCode = CloudFeature::vmsClientQrCodeDeployment;
inline constexpr CloudFeatures kAllFeatures =
    kLayouts | kPush | kCrossSite | kCrashReporting | kQrCode;

} // namespace nx::vms::client::core::test
