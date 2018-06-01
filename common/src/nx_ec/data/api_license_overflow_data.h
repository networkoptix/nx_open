#pragma once

#include "api_globals.h"
#include "api_data.h"

namespace ec2 {

struct ApiLicenseOverflowData: nx::vms::api::Data
{
    bool value = false;
    qint64 time = 0;
};
#define ApiLicenseOverflowData_Fields (value)(time)

} // namespace ec2
