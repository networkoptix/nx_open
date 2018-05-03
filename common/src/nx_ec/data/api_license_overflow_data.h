#ifndef __API_LICENSE_OVERFLOW_DATA_H_
#define __API_LICENSE_OVERFLOW_DATA_H_

#include "api_globals.h"
#include "api_data.h"

namespace ec2
{
    struct ApiLicenseOverflowData: nx::vms::api::Data
    {
        bool   value = false;
        qint64 time = 0;
    };
#define ApiLicenseOverflowData_Fields (value)(time)
}

#endif // __API_LICENSE_OVERFLOW_DATA_H_
