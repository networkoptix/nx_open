// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "data_macros.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API LicenseOverflowData
{
    bool value = false;
    qint64 time = 0;
};
#define LicenseOverflowData_Fields (value)(time)
NX_VMS_API_DECLARE_STRUCT(LicenseOverflowData)

} // namespace api
} // namespace vms
} // namespace nx
