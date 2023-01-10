// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "data_macros.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API VideoWallLicenseOverflowData
{
    bool value = false;
    qint64 time = 0;
};
#define VideoWallLicenseOverflowData_Fields (value)(time)
NX_VMS_API_DECLARE_STRUCT(VideoWallLicenseOverflowData)

} // namespace api
} // namespace vms
} // namespace nx
