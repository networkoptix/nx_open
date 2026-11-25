// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QFlags>

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::client::core {

/** Names double as INI tokens and cloud feature flag keys. */
NX_REFLECTION_ENUM_CLASS(CloudFeature,
    // The next three need cloud services, see CloudServiceChecker.
    vmsClientCloudLayouts = 1 << 0,
    vmsClientPushNotifications = 1 << 1,
    vmsClientCrossSiteNotifications = 1 << 2,

    vmsClientCrashReporting = 1 << 3,
    vmsClientQrCodeDeployment = 1 << 4);

Q_DECLARE_FLAGS(CloudFeatures, CloudFeature)
Q_DECLARE_OPERATORS_FOR_FLAGS(CloudFeatures)

NX_VMS_CLIENT_CORE_API CloudFeatures allCloudFeatures();

} // namespace nx::vms::client::core
