// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "data_macros.h"

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::api {


NX_REFLECTION_ENUM(GracePeriodType,
    perpetualLicense,
    videoWallLicense,
    tier);

struct NX_VMS_API GracePeriodExpirationData
{
    std::chrono::milliseconds expirationTimeMs{0};
    GracePeriodType type{perpetualLicense};

};
#define GracePeriodExpirationData_Fields (expirationTimeMs)(type)
NX_VMS_API_DECLARE_STRUCT(GracePeriodExpirationData)

} // namespace nx::vms::api
