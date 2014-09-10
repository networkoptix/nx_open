#ifndef __API_LICENSE_OVERFLOW_DATA_H_
#define __API_LICENSE_OVERFLOW_DATA_H_

#include "api_globals.h"
#include "api_data.h"

namespace ec2
{
    struct ApiLicenseOverflowData : ApiData
    {
        ApiLicenseOverflowData(): value(false), time(0) {}

        bool   value;
        qint64 time;
    };
#define ApiLicenseOverflowData_Fields (value)(time)
}

#endif // __API_LICENSE_OVERFLOW_DATA_H_
